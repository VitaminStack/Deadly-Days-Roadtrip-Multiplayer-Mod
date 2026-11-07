#include <winsock2.h>
#include <ws2tcpip.h>

#include <doctest.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "FakeSteam.h"
#include "Net/Transport.h"
#include "Net/Net.h"

#pragma comment(lib, "ws2_32.lib")

using namespace Net;

namespace
{
constexpr uint64_t kHostId = 1001;
constexpr uint64_t kClientId = 2002;

// Eigene Ports, damit ein laufendes Spiel oder ein zweiter Testlauf nicht stört.
TransportOptions TestPorts(uint16_t Base)
{
    TransportOptions Options;
    Options.GamePort = Base;
    Options.ClientPort = static_cast<uint16_t>(Base + 1);
    Options.LoopbackPort = static_cast<uint16_t>(Base + 2);
    Options.StatsIntervalMs = 100;
    Options.TotalsIntervalMs = 20;
    // Diese Tests prüfen den Tunnel wie mit dem Testschalter -ddcoop_transport, ohne Lobby. Die Lobby-Prüfung steht in
    // AuthorizationTests.cpp.
    Options.AllowLoopback = true;
    Options.Peers = PeerPolicy::AnyAccount;
    return Options;
}

bool WaitFor(const std::function<bool()>& Condition, int Milliseconds = 3000)
{
    const auto Deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(Milliseconds);
    while (std::chrono::steady_clock::now() < Deadline)
    {
        if (Condition())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return Condition();
}

// UDP-Socket wie das Spiel: Host auf dem Spielport, Client auf einem freien Port.
struct GameSocket
{
    explicit GameSocket(uint16_t Port)
    {
        WSADATA Data;
        WSAStartup(MAKEWORD(2, 2), &Data);
        Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in Address{};
        Address.sin_family = AF_INET;
        Address.sin_port = htons(Port);
        Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        Bound = bind(Socket, reinterpret_cast<sockaddr*>(&Address), sizeof(Address)) == 0;
        u_long NonBlocking = 1;
        ioctlsocket(Socket, FIONBIO, &NonBlocking);
    }
    ~GameSocket()
    {
        closesocket(Socket);
        WSACleanup();
    }

    void SendTo(uint16_t Port, const std::string& Text)
    {
        sockaddr_in Address{};
        Address.sin_family = AF_INET;
        Address.sin_port = htons(Port);
        Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sendto(Socket, Text.data(), static_cast<int>(Text.size()), 0, reinterpret_cast<sockaddr*>(&Address), sizeof(Address));
    }

    void Reply(const std::string& Text)
    {
        sendto(Socket, Text.data(), static_cast<int>(Text.size()), 0, reinterpret_cast<sockaddr*>(&LastFrom), sizeof(LastFrom));
    }

    std::string Receive()
    {
        char Buffer[2048];
        int Size = sizeof(LastFrom);
        const int Length = recvfrom(Socket, Buffer, sizeof(Buffer), 0, reinterpret_cast<sockaddr*>(&LastFrom), &Size);
        return Length > 0 ? std::string(Buffer, Length) : std::string();
    }

    std::string WaitReceive()
    {
        std::string Text;
        WaitFor([&] { return !(Text = Receive()).empty(); });
        return Text;
    }

    SOCKET Socket = INVALID_SOCKET;
    sockaddr_in LastFrom{};
    bool Bound = false;
};

// Schickt, bis die Gegenseite antwortet: Pakete vor dem Aufbau verwirft der Tunnel wie bei echtem UDP.
std::string Exchange(GameSocket& From, uint16_t Port, const std::string& Text, GameSocket& To)
{
    std::string Got;
    WaitFor([&] {
        From.SendTo(Port, Text);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        Got = To.Receive();
        return !Got.empty();
    });
    return Got;
}
}

TEST_CASE("Testschalter aus der Kommandozeile")
{
    CHECK_FALSE(ParseTransportRequest(L"DDSurvivors.exe -windowed").has_value());

    const auto Host = ParseTransportRequest(L"game.exe -windowed -ddcoop_transport=host -nosplash");
    REQUIRE(Host);
    CHECK(Host->Role == TransportRequest::Kind::Host);

    const auto Loopback = ParseTransportRequest(L"game.exe -ddcoop_transport=loopback");
    REQUIRE(Loopback);
    CHECK(Loopback->Role == TransportRequest::Kind::Loopback);

    const auto Client = ParseTransportRequest(L"\"game.exe\" -ddcoop_transport=76561190000000001 -UserDir=\"x\"");
    REQUIRE(Client);
    CHECK(Client->Role == TransportRequest::Kind::Client);
    CHECK(Client->HostSteamId == 76561190000000001ull);

    CHECK_FALSE(ParseTransportRequest(L"game.exe -ddcoop_transport=").has_value());
    CHECK_FALSE(ParseTransportRequest(L"game.exe -ddcoop_transport=gast").has_value());
    CHECK_FALSE(ParseTransportRequest(L"game.exe -ddcoop_transport=0").has_value());
    CHECK_FALSE(ParseTransportRequest(L"game.exe -ddcoop_transport=123456789012345678901").has_value());
}

TEST_CASE("Adressen werden maskiert, die Route wird kurz zusammengefasst")
{
    CHECK(MaskAddresses("Primary router: fra#364 (155.133.1.2:27126) via 127.0.0.1") ==
          "Primary router: fra#364 (<ip>:27126) via 127.0.0.1");
    CHECK(MaskAddresses("keine Adresse") == "keine Adresse");
    CHECK(RouteSummary("Ping:44ms\nPrimary router: fra#364 (<ip>:27126)\nBackup router: ams#86") == "Relay fra");
    CHECK(RouteSummary("Ping:3ms\nTransport: direct") == "ohne Relay");
    CHECK(RouteSummary("") == "unbekannt");
}

TEST_CASE("Host und Client tunneln Spielpakete per P2P in beide Richtungen")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = TestPorts(47710);
    GameSocket HostGame(Options.GamePort);
    GameSocket ClientGame(0);
    REQUIRE(HostGame.Bound);

    Transport Host(Network->Account(kHostId), Options);
    Transport Client(Network->Account(kClientId), Options);
    REQUIRE(Host.StartHost());
    CHECK(Host.Stats().State == TransportState::Listening);
    REQUIRE(Client.StartClient(kHostId));

    CHECK(WaitFor([&] { return Client.Stats().State == TransportState::Connected; }));
    CHECK(WaitFor([&] { return Host.Stats().State == TransportState::Connected; }));

    CHECK(Exchange(ClientGame, Options.ClientPort, "hallo host", HostGame) == "hallo host");
    HostGame.Reply("hallo client");
    CHECK(ClientGame.WaitReceive() == "hallo client");

    CHECK(WaitFor([&] { return Client.Stats().PacketsFromSteam >= 1 && Host.Stats().Connections == 1; }));
    const TransportStats Stats = Client.Stats();
    CHECK(Stats.PingMs == 42);
    CHECK(Stats.Route == "Relay fra");
    CHECK_FALSE(Stats.IsHost);
    CHECK(Host.Stats().IsHost);
}

TEST_CASE("Zwei Instanzen auf einem PC verbinden sich über 127.0.0.1")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = TestPorts(47720);
    GameSocket HostGame(Options.GamePort);
    GameSocket ClientGame(0);

    Transport Host(Network->Account(kHostId), Options);
    Transport Client(Network->Account(kClientId), Options);
    REQUIRE(Host.StartHost());
    REQUIRE(Client.StartLoopbackClient());
    CHECK(WaitFor([&] { return Client.Stats().State == TransportState::Connected; }));
    CHECK(Exchange(ClientGame, Options.ClientPort, "loopback", HostGame) == "loopback");
}

TEST_CASE("Abgelehnte Verbindungen melden Failed und geben den Port frei")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = TestPorts(47730);

    SUBCASE("P2P zum eigenen Konto lehnt Steam ab")
    {
        Transport Host(Network->Account(kHostId), Options);
        REQUIRE(Host.StartHost());
        Transport Self(Network->Account(kHostId + 1), Options);
        CHECK_FALSE(Self.StartClient(kHostId + 1));
        CHECK(Self.Stats().State == TransportState::Failed);
        CHECK(Self.Stats().Message == "Steam hat die Verbindung abgelehnt");
    }
    SUBCASE("Ohne Host")
    {
        Transport Client(Network->Account(kClientId), Options);
        CHECK_FALSE(Client.StartClient(kHostId));
        CHECK(Client.Stats().State == TransportState::Failed);
    }
    SUBCASE("SteamID 0")
    {
        Transport Client(Network->Account(kClientId), Options);
        CHECK_FALSE(Client.StartClient(0));
    }
    // Nach jedem Fall ist der Client-Port wieder frei.
    GameSocket Probe(Options.ClientPort);
    CHECK(Probe.Bound);
}

TEST_CASE("Belegter Client-Port bricht den Start ab")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = TestPorts(47740);
    GameSocket Blocker(Options.ClientPort);
    REQUIRE(Blocker.Bound);
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());
    Transport Client(Network->Account(kClientId), Options);
    CHECK_FALSE(Client.StartClient(kHostId));
    CHECK(Client.Stats().State == TransportState::Failed);
    CHECK_FALSE(Client.Running());
}

TEST_CASE("Ende der Verbindung: Client meldet Failed, Host wartet weiter")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = TestPorts(47750);
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());

    SUBCASE("Host geht")
    {
        Transport Client(Network->Account(kClientId), Options);
        REQUIRE(Client.StartClient(kHostId));
        REQUIRE(WaitFor([&] { return Client.Stats().State == TransportState::Connected; }));
        Host.Stop();
        CHECK(Host.Stats().State == TransportState::Stopped);
        CHECK(WaitFor([&] { return Client.Stats().State == TransportState::Failed; }));
        CHECK(Client.Stats().Message == "Der Host hat die Sitzung verlassen");
    }
    SUBCASE("Client geht")
    {
        {
            Transport Client(Network->Account(kClientId), Options);
            REQUIRE(Client.StartClient(kHostId));
            REQUIRE(WaitFor([&] { return Host.Stats().Connections == 1; }));
        }
        CHECK(WaitFor([&] { return Host.Stats().State == TransportState::Listening && Host.Stats().Connections == 0; }));
        CHECK(Host.Stats().Message == "Der Mitspieler hat die Sitzung verlassen");
        // Ein neuer Client kommt wieder herein.
        Transport Again(Network->Account(kClientId), Options);
        REQUIRE(Again.StartClient(kHostId));
        CHECK(WaitFor([&] { return Host.Stats().Connections == 1; }));
    }
}

TEST_CASE("Abbruch der Leitung: beide Seiten melden den Verlust, der Host nimmt danach neue Clients an")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = TestPorts(47870);
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());
    {
        Transport Client(Network->Account(kClientId), Options);
        REQUIRE(Client.StartClient(kHostId));
        REQUIRE(WaitFor([&] { return Host.Stats().Connections == 1 && Client.Stats().State == TransportState::Connected; }));
        Network->LoseConnectionsOf(kClientId);
        REQUIRE(WaitFor([&] { return Client.Stats().State == TransportState::Failed; }));
        CHECK(Client.Stats().Message == "Verbindung zum Host verloren");
        CHECK(Client.ControlPeers().empty());
        REQUIRE(WaitFor([&] { return Host.Stats().State == TransportState::Listening && Host.Stats().Connections == 0; }));
        CHECK(Host.Stats().Message == "Verbindung zum Mitspieler verloren");
        CHECK(Host.ControlPeers().empty());
    }
    Transport Again(Network->Account(kClientId + 1), Options);
    REQUIRE(Again.StartClient(kHostId));
    CHECK(WaitFor([&] { return Again.Stats().State == TransportState::Connected && Host.Stats().Connections == 1; }));
}

TEST_CASE("Host antwortet nie: Abbruch vor dem Aufbau heißt Host nicht erreichbar")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = TestPorts(47880);
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());
    Network->SilenceAccount(kHostId);
    Transport Client(Network->Account(kClientId), Options);
    REQUIRE(Client.StartClient(kHostId));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    CHECK(Client.Stats().State == TransportState::Connecting);
    CHECK(Host.Stats().Connections == 0);
    Network->LoseConnectionsOf(kClientId);
    REQUIRE(WaitFor([&] { return Client.Stats().State == TransportState::Failed; }));
    CHECK(Client.Stats().Message == "Host nicht erreichbar");
}

TEST_CASE("Ein zweiter Client wird mit Grund abgewiesen, der erste bleibt verbunden")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = TestPorts(47890);
    TransportOptions SecondPorts = Options;
    SecondPorts.ClientPort = static_cast<uint16_t>(Options.ClientPort + 3);
    Transport Host(Network->Account(kHostId), Options);
    Transport First(Network->Account(kClientId), Options);
    Transport Second(Network->Account(kClientId + 1), SecondPorts);
    REQUIRE(Host.StartHost());
    REQUIRE(First.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Host.Stats().Connections == 1; }));
    REQUIRE(Second.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Second.Stats().State == TransportState::Failed; }));
    CHECK(Second.Stats().Message == "Der Host hat schon einen Mitspieler");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    CHECK(First.Stats().State == TransportState::Connected);
    CHECK(Host.Stats().Connections == 1);
}

TEST_CASE("Meldungen zum Ende einer Verbindung")
{
    using State = SteamConnectionState;
    CHECK(EndMessage(false, State::ClosedByPeer, EndReason::HostLeft, true) == "Der Host hat die Sitzung verlassen");
    CHECK(EndMessage(true, State::ClosedByPeer, EndReason::ClientLeft, true) == "Der Mitspieler hat die Sitzung verlassen");
    CHECK(EndMessage(false, State::ClosedByPeer, EndReason::Busy, false) == "Der Host hat schon einen Mitspieler");
    CHECK(EndMessage(false, State::ClosedByPeer, EndReason::Rejected, true) ==
          "Der Host hat die Verbindung abgelehnt (andere Mod-Version oder anderer Spiel-Build)");
    CHECK(EndMessage(true, State::ClosedByPeer, EndReason::Rejected, true) == "Der Mitspieler hat die Verbindung abgelehnt");
    // Andere Gründe, etwa wenn Steam beim Beenden des Spiels schließt.
    CHECK(EndMessage(false, State::ClosedByPeer, 0, true) == "Der Host hat die Verbindung beendet");
    CHECK(EndMessage(true, State::ClosedByPeer, 5001, true) == "Der Mitspieler hat die Verbindung beendet");
    CHECK(EndMessage(false, State::ProblemDetectedLocally, 5003, true) == "Verbindung zum Host verloren");
    CHECK(EndMessage(false, State::ProblemDetectedLocally, 4001, false) == "Host nicht erreichbar");
    CHECK(EndMessage(true, State::ProblemDetectedLocally, 5003, true) == "Verbindung zum Mitspieler verloren");
}

TEST_CASE("Stop ist wiederholbar und ein Neustart danach möglich")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = TestPorts(47760);
    Transport Host(Network->Account(kHostId), Options);
    Transport Client(Network->Account(kClientId), Options);
    REQUIRE(Host.StartHost());
    CHECK_FALSE(Host.StartHost());
    REQUIRE(Client.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Client.Stats().State == TransportState::Connected; }));
    Client.Stop();
    Client.Stop();
    CHECK_FALSE(Client.Running());
    CHECK(Client.Stats().State == TransportState::Stopped);
    REQUIRE(Client.StartClient(kHostId));
    CHECK(WaitFor([&] { return Client.Stats().State == TransportState::Connected; }));
}

TEST_CASE("Ohne Steam startet nichts")
{
    Transport Missing(nullptr);
    CHECK_FALSE(Missing.StartHost());
    CHECK_FALSE(Missing.Running());
}

TEST_CASE("Netzsimulation verzögert Spielpakete und Kontrollnachrichten im Tunnel")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    TransportOptions Options = TestPorts(47950);
    Options.Sim = {200, 0, 0};
    GameSocket HostGame(Options.GamePort);
    GameSocket ClientGame(0);
    REQUIRE(HostGame.Bound);

    Transport Host(Network->Account(kHostId), Options);
    Transport Client(Network->Account(kClientId), Options);
    REQUIRE(Host.StartHost());
    REQUIRE(Client.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Client.Stats().State == TransportState::Connected; }));
    REQUIRE(Exchange(ClientGame, Options.ClientPort, "erstes", HostGame) == "erstes");
    // Exchange sendet alle 10 ms, weitere Kopien liegen noch in der Verzögerung.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    while (!HostGame.Receive().empty()) {}

    // Hin und zurück: je Seite 100 ms.
    const auto Start = std::chrono::steady_clock::now();
    ClientGame.SendTo(Options.ClientPort, "ping");
    REQUIRE(HostGame.WaitReceive() == "ping");
    HostGame.Reply("pong");
    REQUIRE(ClientGame.WaitReceive() == "pong");
    const auto RoundTrip = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Start);
    CHECK(RoundTrip.count() >= 195);
    CHECK(RoundTrip.count() < 1000);

    REQUIRE(WaitFor([&] { return Host.ControlPeers().size() == 1; }));
    const Protocol::Control Value{1, 1, 1, 9, {4, 2}};
    const auto Sent = std::chrono::steady_clock::now();
    REQUIRE(Host.SendControl(Host.ControlPeers()[0], Value));
    std::vector<ControlMessage> Got;
    REQUIRE(WaitFor([&] { Got = Client.TakeControlMessages(); return !Got.empty(); }));
    const auto OneWay = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - Sent);
    CHECK(Got[0].Value == Value);
    CHECK(OneWay.count() >= 95);
}

TEST_CASE("Kontrollkanal funktioniert ohne Spieladresse und bleibt von UDP getrennt")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const auto Options = TestPorts(47770);
    GameSocket HostGame(Options.GamePort);
    Transport Host(Network->Account(kHostId), Options);
    Transport Client(Network->Account(kClientId), Options);
    CHECK_FALSE(Client.SendControl(1, {1, 1, 1, 1}));
    REQUIRE(Host.StartHost());
    REQUIRE(Client.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Host.ControlPeers().size() == 1 && Client.ControlPeers().size() == 1; }));
    const auto HostPeer = Host.ControlPeers()[0];
    const auto ClientPeer = Client.ControlPeers()[0];
    const Protocol::Control Value{10, 2, 3, 7, {0, 255, 0, 17}};
    CHECK_FALSE(Host.SendControl(HostPeer + 100, Value));
    REQUIRE(Host.SendControl(HostPeer, Value));
    std::vector<ControlMessage> Got;
    REQUIRE(WaitFor([&] { Got = Client.TakeControlMessages(); return !Got.empty(); }));
    REQUIRE(Got.size() == 1);
    CHECK(Got[0].Value == Value);
    CHECK(Got[0].Peer == ClientPeer);
    CHECK(HostGame.Receive().empty());
    REQUIRE(Client.SendControl(ClientPeer, Value));
    REQUIRE(WaitFor([&] { Got = Host.TakeControlMessages(); return !Got.empty(); }));
    CHECK(Got[0].Peer == HostPeer);
    CHECK(Got[0].Value == Value);
    Client.Stop();
    REQUIRE(WaitFor([&] { return Host.ControlPeers().empty(); }));
    CHECK_FALSE(Host.SendControl(HostPeer, Value));
    REQUIRE(Client.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return !Host.ControlPeers().empty(); }));
    CHECK(Host.ControlPeers()[0] != HostPeer);
    CHECK(Client.TakeControlMessages().empty());
}

TEST_CASE("Falscher Build oder Mod darf keine Spielpakete weiterleiten")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    auto Options = TestPorts(47780);
    auto BadOptions = Options;
    SUBCASE("Spiel-Build") { ++BadOptions.Identity.Build; }
    SUBCASE("Mod-Version") { ++BadOptions.Identity.Mod; }
    GameSocket HostGame(Options.GamePort);
    GameSocket ClientGame(0);
    Transport Host(Network->Account(kHostId), Options);
    Transport Client(Network->Account(kClientId), BadOptions);
    REQUIRE(Host.StartHost());
    REQUIRE(Client.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Client.Stats().State == TransportState::Failed; }));
    CHECK(Host.ControlPeers().empty());
    CHECK(Client.ControlPeers().empty());
    ClientGame.SendTo(Options.ClientPort, "kein ungeprueftes UE-Paket");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    CHECK(HostGame.Receive().empty());
}

TEST_CASE("Fehlendes Hello laeuft aus, danach kann ein kompatibler Client verbinden")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    auto Options = TestPorts(47790);
    Options.HandshakeTimeoutMs = 50;
    Transport Host(Network->Account(kHostId), Options);
    REQUIRE(Host.StartHost());
    auto OldClient = Network->Account(kClientId);
    const auto Connection = OldClient->ConnectP2P(kHostId, Options.VirtualPort);
    REQUIRE(Connection != 0);
    REQUIRE(WaitFor([&] { return Host.Stats().Message == "Zeitlimit beim Versions-Handshake"; }));
    CHECK(Host.ControlPeers().empty());
    OldClient.reset();
    Options.HandshakeTimeoutMs = 5000;
    Transport Client(Network->Account(kClientId + 1), Options);
    REQUIRE(WaitFor([&] { return Client.StartClient(kHostId); }));
    REQUIRE(WaitFor([&] { return !Host.ControlPeers().empty(); }));
}

TEST_CASE("Sync findet den Lobbytransport ohne ihn nach Sitzungsende am Leben zu halten")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    auto Host = std::make_shared<Transport>(Network->Account(kHostId), TestPorts(47800));
    REQUIRE(Host->StartHost());
    UseSessionTransport(Host);
    CHECK(CurrentTransport() == Host);
    Host->Stop();
    CHECK_FALSE(CurrentTransport());
    REQUIRE(Host->StartHost());
    CHECK(CurrentTransport() == Host);
    std::weak_ptr<Transport> Weak = Host;
    Host.reset();
    CHECK(Weak.expired());
    CHECK_FALSE(CurrentTransport());
}

TEST_CASE("Kontrollnachrichten eines Typs lassen sich einzeln abholen, die übrigen bleiben in Reihenfolge")
{
    auto Network = std::make_shared<FakeSteamNetwork>();
    const auto Options = TestPorts(47790);
    GameSocket HostGame(Options.GamePort);
    Transport Host(Network->Account(kHostId), Options);
    Transport Client(Network->Account(kClientId), Options);
    REQUIRE(Host.StartHost());
    REQUIRE(Client.StartClient(kHostId));
    REQUIRE(WaitFor([&] { return Client.ControlPeers().size() == 1; }));
    const auto Peer = Client.ControlPeers()[0];
    REQUIRE(Client.SendControl(Peer, {1, 1, 1, 0x100, {1}}));
    REQUIRE(Client.SendControl(Peer, {1, 1, 2, 0x500, {2}}));
    REQUIRE(Client.SendControl(Peer, {1, 1, 3, 0x101, {3}}));
    REQUIRE(Client.SendControl(Peer, {1, 1, 4, 0x500, {4}}));
    std::vector<ControlMessage> Typed;
    REQUIRE(WaitFor([&] {
        for (auto& Message : Host.TakeControlMessages(0x500))
            Typed.push_back(std::move(Message));
        return Typed.size() == 2;
    }));
    CHECK(Typed[0].Value.Revision == 2);
    CHECK(Typed[1].Value.Revision == 4);
    std::vector<ControlMessage> Rest;
    REQUIRE(WaitFor([&] {
        for (auto& Message : Host.TakeControlMessages())
            Rest.push_back(std::move(Message));
        return Rest.size() == 2;
    }));
    CHECK(Rest[0].Value.Revision == 1);
    CHECK(Rest[1].Value.Revision == 3);
    CHECK(Host.TakeControlMessages(0x500).empty());
}
