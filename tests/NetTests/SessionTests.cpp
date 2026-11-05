#include <doctest.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "FakeSteam.h"
#include "Net/Session.h"

using namespace Net;

namespace
{
constexpr uint64_t kHostId = 3003;
constexpr uint64_t kClientId = 4004;

// Nachgebaute Lobbys von Steam: Aufrufe sind beim nächsten Abfragen fertig, wie nach einem Frame im Spiel.
struct FakeLobbyService
{
    struct Lobby
    {
        uint64_t Owner = 0;
        std::set<uint64_t> Members;
        std::map<std::string, std::string> Data;
    };
    struct Call
    {
        bool Create = false;
        uint64_t Account = 0;
        uint64_t Lobby = 0;
    };

    std::map<uint64_t, Lobby> Lobbies;
    std::map<SteamApiCall, Call> Calls;
    std::map<uint64_t, std::vector<uint64_t>> JoinRequests;
    SteamApiCall NextCall = 1;
    uint64_t NextLobby = 109775240000000001ull;
    bool RejectCreate = false;
    bool FailCreate = false;
    // Steam antwortet nicht auf Erstellen oder Beitreten.
    bool NoAnswer = false;
    int InviteDialogs = 0;

    void Leave(uint64_t Account, uint64_t Id)
    {
        auto Found = Lobbies.find(Id);
        if (Found == Lobbies.end())
            return;
        Found->second.Members.erase(Account);
        if (Found->second.Members.empty())
            Lobbies.erase(Found);
        else if (Found->second.Owner == Account)
            Found->second.Owner = *Found->second.Members.begin();
    }
};

class FakeLobbyApi : public SteamLobbyApi
{
public:
    FakeLobbyApi(std::shared_ptr<FakeLobbyService> ServiceIn, uint64_t AccountIn) : Service(std::move(ServiceIn)), Account(AccountIn) {}

    uint64_t OwnSteamId() override { return Account; }
    SteamApiCall CreateFriendsLobby(int) override
    {
        if (Service->RejectCreate)
            return 0;
        const SteamApiCall Call = Service->NextCall++;
        Service->Calls[Call] = {true, Account, 0};
        return Call;
    }
    SteamApiCall JoinLobby(uint64_t Lobby) override
    {
        const SteamApiCall Call = Service->NextCall++;
        Service->Calls[Call] = {false, Account, Lobby};
        return Call;
    }
    void LeaveLobby(uint64_t Lobby) override { Service->Leave(Account, Lobby); }
    uint64_t LobbyOwner(uint64_t Lobby) override
    {
        const auto Found = Service->Lobbies.find(Lobby);
        return Found == Service->Lobbies.end() ? 0 : Found->second.Owner;
    }
    int MemberCount(uint64_t Lobby) override
    {
        const auto Found = Service->Lobbies.find(Lobby);
        return Found == Service->Lobbies.end() ? 0 : static_cast<int>(Found->second.Members.size());
    }
    bool SetLobbyData(uint64_t Lobby, const std::string& Key, const std::string& Value) override
    {
        auto Found = Service->Lobbies.find(Lobby);
        if (Found == Service->Lobbies.end() || Found->second.Owner != Account)
            return false;
        Found->second.Data[Key] = Value;
        return true;
    }
    std::string LobbyData(uint64_t Lobby, const std::string& Key) override
    {
        const auto Found = Service->Lobbies.find(Lobby);
        if (Found == Service->Lobbies.end())
            return {};
        const auto Value = Found->second.Data.find(Key);
        return Value == Found->second.Data.end() ? std::string() : Value->second;
    }
    bool OpenInviteDialog(uint64_t Lobby) override
    {
        if (!Service->Lobbies.count(Lobby))
            return false;
        ++Service->InviteDialogs;
        return true;
    }
    std::optional<LobbyCreatedResult> PollCreated(SteamApiCall Id) override
    {
        if (Service->NoAnswer)
            return std::nullopt;
        const auto Found = Service->Calls.find(Id);
        if (Found == Service->Calls.end() || !Found->second.Create)
            return std::nullopt;
        Service->Calls.erase(Found);
        if (Service->FailCreate)
            return LobbyCreatedResult{false, 0, 2};
        const uint64_t Lobby = Service->NextLobby++;
        Service->Lobbies[Lobby] = {Account, {Account}, {}};
        return LobbyCreatedResult{true, Lobby, 1};
    }
    std::optional<LobbyEnteredResult> PollEntered(SteamApiCall Id) override
    {
        if (Service->NoAnswer)
            return std::nullopt;
        const auto Found = Service->Calls.find(Id);
        if (Found == Service->Calls.end() || Found->second.Create)
            return std::nullopt;
        const uint64_t Lobby = Found->second.Lobby;
        Service->Calls.erase(Found);
        auto Target = Service->Lobbies.find(Lobby);
        if (Target == Service->Lobbies.end())
            return LobbyEnteredResult{false, Lobby, 2};
        Target->second.Members.insert(Account);
        return LobbyEnteredResult{true, Lobby, 1};
    }
    std::vector<uint64_t> TakeJoinRequests() override
    {
        auto Requests = std::move(Service->JoinRequests[Account]);
        Service->JoinRequests[Account].clear();
        return Requests;
    }
    std::string LaunchCommandLine() override { return {}; }

private:
    std::shared_ptr<FakeLobbyService> Service;
    uint64_t Account;
};

TransportOptions SessionPorts(uint16_t Base)
{
    TransportOptions Options;
    Options.GamePort = Base;
    Options.ClientPort = static_cast<uint16_t>(Base + 1);
    Options.LoopbackPort = static_cast<uint16_t>(Base + 2);
    Options.StatsIntervalMs = 100;
    return Options;
}

// Was die Sitzung im Spiel ausgelöst hätte.
struct GameCalls
{
    int Listen = 0;
    std::vector<uint16_t> Connects;
    int Disconnects = 0;
    // Verbindung des Spiels zum Host, wie sie die Mod aus der Welt liest. nullopt: unbekannt, etwa beim Laden.
    std::optional<bool> InGame;

    SessionHooks Hooks()
    {
        return {[this] { ++Listen; }, [this](uint16_t Port) { Connects.push_back(Port); }, [this] { ++Disconnects; },
                [this] { return InGame; }};
    }
};

struct Player
{
    Player(std::shared_ptr<FakeLobbyService> Service, std::shared_ptr<FakeSteamNetwork> Network, uint64_t LobbyAccount,
           uint64_t SocketAccount, TransportOptions Options, std::string Version = "1")
        : Lobby(std::make_shared<FakeLobbyApi>(Service, LobbyAccount)),
          Run(Lobby,
              [Network, SocketAccount, Options] { return std::make_shared<Transport>(Network->Account(SocketAccount), Options); },
              Game.Hooks(), Options, std::move(Version))
    {
    }

    std::shared_ptr<FakeLobbyApi> Lobby;
    GameCalls Game;
    Session Run;
};

// Tickt beide Seiten wie der Game Thread, bis die Bedingung gilt.
bool TickUntil(const std::vector<Session*>& Sessions, const std::function<bool()>& Condition, int Milliseconds = 3000)
{
    const auto Deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(Milliseconds);
    while (std::chrono::steady_clock::now() < Deadline)
    {
        for (Session* Each : Sessions)
            Each->Tick();
        if (Condition())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return Condition();
}
}

TEST_CASE("Host erstellt eine Lobby, wird Listen-Server und kann einladen")
{
    auto Service = std::make_shared<FakeLobbyService>();
    auto Network = std::make_shared<FakeSteamNetwork>();
    Player Host(Service, Network, kHostId, kHostId, SessionPorts(47810));

    CHECK(Host.Run.Status().CanCreate);
    CHECK_FALSE(Host.Run.Invite());
    REQUIRE(Host.Run.Create());
    REQUIRE(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Hosting; }));

    const SessionStatus Status = Host.Run.Status();
    CHECK(Status.IsHost);
    CHECK(Status.Lobby != 0);
    CHECK(Status.Members == 1);
    CHECK(Status.CanInvite);
    CHECK(Status.CanLeave);
    CHECK_FALSE(Status.CanCreate);
    CHECK_FALSE(Status.CanJoin);
    CHECK(Host.Game.Listen == 1);
    CHECK(Service->Lobbies[Status.Lobby].Data[std::string(Session::kModKey)] == "1");

    CHECK(Host.Run.Invite());
    CHECK(Service->InviteDialogs == 1);
    CHECK_FALSE(Host.Run.Create());

    Host.Run.Leave();
    CHECK(Host.Run.Status().Phase == SessionPhase::Idle);
    CHECK(Host.Run.Status().CanCreate);
    CHECK(Service->Lobbies.empty());
    CHECK(Host.Game.Disconnects == 0);
}

TEST_CASE("Client tritt bei und verbindet sich per P2P mit dem Besitzer der Lobby")
{
    auto Service = std::make_shared<FakeLobbyService>();
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = SessionPorts(47820);
    Player Host(Service, Network, kHostId, kHostId, Options);
    Player Client(Service, Network, kClientId, kClientId, Options);

    REQUIRE(Host.Run.Create());
    REQUIRE(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Hosting; }));
    const uint64_t Lobby = Host.Run.Status().Lobby;

    REQUIRE(Client.Run.Join(Lobby));
    CHECK_FALSE(Client.Run.Status().CanJoin);
    REQUIRE(TickUntil({&Host.Run, &Client.Run}, [&] {
        return Client.Run.Status().Phase == SessionPhase::Playing && Host.Run.Status().Phase == SessionPhase::Playing;
    }));
    CHECK(Client.Game.Connects == std::vector<uint16_t>{Options.ClientPort});
    CHECK(Client.Game.Listen == 0);
    CHECK_FALSE(Client.Run.Status().IsHost);
    CHECK(Client.Run.Status().Route == "Relay fra");
    CHECK(Client.Run.Status().Members == 2);
    CHECK(Host.Run.Status().Message == "Mitspieler verbunden");

    // Weitere Ticks verbinden das Spiel nicht noch einmal.
    TickUntil({&Host.Run, &Client.Run}, [] { return false; }, 100);
    CHECK(Client.Game.Connects.size() == 1);

    SUBCASE("Client verlässt die Sitzung")
    {
        Client.Run.Leave();
        CHECK(Client.Game.Disconnects == 1);
        CHECK(Service->Lobbies[Lobby].Members.size() == 1);
        CHECK(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Hosting; }));
        CHECK(Host.Run.Status().Message ==
              "Der Mitspieler hat die Sitzung verlassen. Lobby offen, Freunde über Einladen holen");
        CHECK(Host.Run.Status().CanInvite);
    }
    SUBCASE("Host beendet die Sitzung")
    {
        Host.Run.Leave();
        REQUIRE(TickUntil({&Client.Run}, [&] { return Client.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Client.Game.Disconnects == 1);
        CHECK(Client.Run.Status().Message == "Der Host hat die Sitzung verlassen");
        CHECK(Client.Run.Status().CanJoin);
        CHECK(Service->Lobbies.empty());
    }
}

TEST_CASE("Zweite Instanz unter demselben Konto verbindet sich über 127.0.0.1")
{
    auto Service = std::make_shared<FakeLobbyService>();
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = SessionPorts(47830);
    // Beide Spiele laufen unter demselben Steam-Konto. Die Sockets des Hosts liegen unter einer anderen Kennung, P2P zum
    // Konto der Lobby fände also niemanden, nur der Weg über 127.0.0.1 kommt an.
    Player Host(Service, Network, kHostId, 9009, Options);
    Player Client(Service, Network, kHostId, kClientId, Options);

    REQUIRE(Host.Run.Create());
    REQUIRE(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Hosting; }));
    REQUIRE(Client.Run.Join(Host.Run.Status().Lobby));
    CHECK(TickUntil({&Host.Run, &Client.Run}, [&] {
        return Client.Run.Status().Phase == SessionPhase::Playing && Host.Run.Status().Phase == SessionPhase::Playing;
    }));
}

TEST_CASE("Beitritt scheitert sauber")
{
    auto Service = std::make_shared<FakeLobbyService>();
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = SessionPorts(47840);
    Player Client(Service, Network, kClientId, kClientId, Options);

    SUBCASE("Lobby gibt es nicht")
    {
        REQUIRE(Client.Run.Join(12345));
        REQUIRE(TickUntil({&Client.Run}, [&] { return Client.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Client.Run.Status().Message == "Beitritt abgelehnt (Antwort 2)");
    }
    SUBCASE("Andere Mod-Version")
    {
        Player Host(Service, Network, kHostId, kHostId, Options, "2");
        REQUIRE(Host.Run.Create());
        REQUIRE(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Hosting; }));
        const uint64_t Lobby = Host.Run.Status().Lobby;
        REQUIRE(Client.Run.Join(Lobby));
        REQUIRE(TickUntil({&Client.Run}, [&] { return Client.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Client.Run.Status().Message == "Andere Mod-Version beim Host");
        CHECK(Service->Lobbies[Lobby].Members.size() == 1);
    }
    SUBCASE("Lobby ohne Mod")
    {
        const uint64_t Lobby = 777;
        Service->Lobbies[Lobby] = {kHostId, {kHostId}, {}};
        REQUIRE(Client.Run.Join(Lobby));
        REQUIRE(TickUntil({&Client.Run}, [&] { return Client.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Client.Run.Status().Message == "Die Lobby gehört nicht zur Coop-Mod");
        CHECK(Service->Lobbies[Lobby].Members.size() == 1);
    }
    SUBCASE("Host antwortet nicht")
    {
        Network->RejectConnectionsTo(kHostId);
        const uint64_t Lobby = 778;
        Service->Lobbies[Lobby] = {kHostId, {kHostId}, {{std::string(Session::kModKey), "1"}}};
        REQUIRE(Client.Run.Join(Lobby));
        REQUIRE(TickUntil({&Client.Run}, [&] { return Client.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Client.Game.Connects.empty());
        CHECK(Client.Game.Disconnects == 0);
    }
    CHECK(Client.Run.Status().CanJoin);
    CHECK(Client.Run.Status().CanCreate);
    CHECK_FALSE(Client.Run.Status().CanLeave);
    CHECK_FALSE(Client.Run.Join(0));
}

TEST_CASE("Erstellen scheitert sauber und ist danach wieder möglich")
{
    auto Service = std::make_shared<FakeLobbyService>();
    auto Network = std::make_shared<FakeSteamNetwork>();
    Player Host(Service, Network, kHostId, kHostId, SessionPorts(47850));

    SUBCASE("Steam lehnt den Aufruf ab")
    {
        Service->RejectCreate = true;
        CHECK_FALSE(Host.Run.Create());
        CHECK(Host.Run.Status().Phase == SessionPhase::Failed);
    }
    SUBCASE("Steam meldet einen Fehler")
    {
        Service->FailCreate = true;
        REQUIRE(Host.Run.Create());
        REQUIRE(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Host.Run.Status().Message == "Steam konnte keine Lobby erstellen (Fehler 2)");
    }
    CHECK(Host.Game.Listen == 0);
    Service->RejectCreate = Service->FailCreate = false;
    REQUIRE(Host.Run.Create());
    CHECK(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Hosting; }));
}

TEST_CASE("Eine angenommene Einladung verlässt die alte Sitzung und tritt der neuen bei")
{
    auto Service = std::make_shared<FakeLobbyService>();
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = SessionPorts(47860);
    Player Host(Service, Network, kHostId, kHostId, Options);
    Player Client(Service, Network, kClientId, kClientId, Options);

    // Der Client hostet gerade selbst eine Lobby.
    REQUIRE(Client.Run.Create());
    REQUIRE(TickUntil({&Client.Run}, [&] { return Client.Run.Status().Phase == SessionPhase::Hosting; }));
    const uint64_t Own = Client.Run.Status().Lobby;

    REQUIRE(Host.Run.Create());
    REQUIRE(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Hosting; }));
    Service->JoinRequests[kClientId].push_back(Host.Run.Status().Lobby);

    REQUIRE(TickUntil({&Host.Run, &Client.Run}, [&] { return Client.Run.Status().Phase == SessionPhase::Playing; }));
    CHECK_FALSE(Client.Run.Status().IsHost);
    CHECK(Client.Run.Status().Lobby == Host.Run.Status().Lobby);
    CHECK(Service->Lobbies.count(Own) == 0);
}

namespace
{
// Host und Client im Spiel, für die Fälle nach dem Aufbau.
struct Pair
{
    explicit Pair(uint16_t Base)
        : Service(std::make_shared<FakeLobbyService>()), Network(std::make_shared<FakeSteamNetwork>()), Options(SessionPorts(Base)),
          Host(Service, Network, kHostId, kHostId, Options), Client(Service, Network, kClientId, kClientId, Options)
    {
    }

    // Lobby erstellen und beitreten, ohne auf die Verbindung zu warten.
    bool Start()
    {
        return Host.Run.Create() &&
               TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Hosting; }) &&
               Client.Run.Join(Host.Run.Status().Lobby);
    }

    bool Connect()
    {
        if (!Start())
            return false;
        return TickUntil({&Host.Run, &Client.Run}, [&] {
            return Client.Run.Status().Phase == SessionPhase::Playing && Host.Run.Status().Phase == SessionPhase::Playing;
        });
    }

    std::shared_ptr<FakeLobbyService> Service;
    std::shared_ptr<FakeSteamNetwork> Network;
    TransportOptions Options;
    Player Host;
    Player Client;
};
}

TEST_CASE("Abbruch der Leitung: Client trennt das Spiel, Host bleibt offen und sagt warum")
{
    Pair Game(47900);
    REQUIRE(Game.Connect());
    Game.Network->LoseConnectionsOf(kClientId);
    REQUIRE(TickUntil({&Game.Host.Run, &Game.Client.Run}, [&] {
        return Game.Client.Run.Status().Phase == SessionPhase::Failed && Game.Host.Run.Status().Phase == SessionPhase::Hosting;
    }));
    CHECK(Game.Client.Run.Status().Message == "Verbindung zum Host verloren");
    CHECK(Game.Client.Game.Disconnects == 1);
    CHECK(Game.Client.Run.Status().CanJoin);
    CHECK(Game.Host.Run.Status().Message ==
          "Verbindung zum Mitspieler verloren. Lobby offen, Freunde über Einladen holen");
    CHECK(Game.Host.Game.Disconnects == 0);

    // Der Client kann derselben Lobby wieder beitreten, die Meldung beim Host verschwindet.
    REQUIRE(Game.Client.Run.Join(Game.Host.Run.Status().Lobby));
    REQUIRE(TickUntil({&Game.Host.Run, &Game.Client.Run}, [&] {
        return Game.Client.Run.Status().Phase == SessionPhase::Playing && Game.Host.Run.Status().Phase == SessionPhase::Playing;
    }));
    CHECK(Game.Host.Run.Status().Message == "Mitspieler verbunden");
}

TEST_CASE("Zeitlimits, wenn Steam oder der Host nicht antworten")
{
    auto Service = std::make_shared<FakeLobbyService>();
    auto Network = std::make_shared<FakeSteamNetwork>();
    const TransportOptions Options = SessionPorts(47910);
    SessionTimeouts Short;
    Short.Lobby = std::chrono::milliseconds(100);
    Short.Connect = std::chrono::milliseconds(150);

    SUBCASE("Lobby erstellen")
    {
        Player Host(Service, Network, kHostId, kHostId, Options);
        Host.Run.SetTimeouts(Short);
        Service->NoAnswer = true;
        REQUIRE(Host.Run.Create());
        REQUIRE(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Host.Run.Status().Message == "Zeitlimit: Steam hat die Lobby nicht erstellt");
        CHECK(Host.Game.Listen == 0);
        CHECK(Host.Run.Status().CanCreate);
    }
    SUBCASE("Lobby beitreten")
    {
        Player Client(Service, Network, kClientId, kClientId, Options);
        Client.Run.SetTimeouts(Short);
        Service->NoAnswer = true;
        REQUIRE(Client.Run.Join(4242));
        REQUIRE(TickUntil({&Client.Run}, [&] { return Client.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Client.Run.Status().Message == "Zeitlimit: Steam hat den Beitritt nicht bestätigt");
        CHECK(Client.Run.Status().CanJoin);
    }
    SUBCASE("Host antwortet nicht auf die Verbindung")
    {
        Player Host(Service, Network, kHostId, kHostId, Options);
        Player Client(Service, Network, kClientId, kClientId, Options);
        Client.Run.SetTimeouts(Short);
        REQUIRE(Host.Run.Create());
        REQUIRE(TickUntil({&Host.Run}, [&] { return Host.Run.Status().Phase == SessionPhase::Hosting; }));
        const uint64_t Lobby = Host.Run.Status().Lobby;
        Network->SilenceAccount(kHostId);
        REQUIRE(Client.Run.Join(Lobby));
        REQUIRE(TickUntil({&Host.Run, &Client.Run}, [&] { return Client.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Client.Run.Status().Message == "Zeitlimit: keine Verbindung zum Host");
        CHECK(Client.Game.Connects.empty());
        CHECK(Client.Game.Disconnects == 0);
        CHECK(Service->Lobbies[Lobby].Members.size() == 1);
    }
}

TEST_CASE("Client erkennt, wenn das Spiel selbst die Verbindung zum Host verliert")
{
    Pair Game(47920);
    SessionTimeouts Short;
    Short.GameJoin = std::chrono::milliseconds(200);
    Short.GameLost = std::chrono::milliseconds(400);
    Game.Client.Run.SetTimeouts(Short);

    SUBCASE("Spiel verbindet sich nie")
    {
        Game.Client.Game.InGame = false;
        // Das Zeitlimit kann ablaufen, bevor der Host den Mitspieler zählt, darum nur bis zum Fehler des Clients ticken.
        REQUIRE(Game.Start());
        REQUIRE(TickUntil({&Game.Host.Run, &Game.Client.Run},
                          [&] { return Game.Client.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Game.Client.Run.Status().Message == "Das Spiel konnte sich nicht mit dem Host verbinden");
        CHECK(Game.Client.Game.Connects.size() == 1);
        CHECK(Game.Client.Game.Disconnects == 1);
    }
    SUBCASE("Spiel verliert den Host und kommt nicht zurück")
    {
        REQUIRE(Game.Connect());
        Game.Client.Game.InGame = true;
        TickUntil({&Game.Host.Run, &Game.Client.Run}, [] { return false; }, 50);
        Game.Client.Game.InGame = false;
        REQUIRE(TickUntil({&Game.Host.Run, &Game.Client.Run},
                          [&] { return Game.Client.Run.Status().Phase == SessionPhase::Failed; }));
        CHECK(Game.Client.Run.Status().Message == "Das Spiel hat die Verbindung zum Host verloren");
        CHECK(Game.Client.Game.Disconnects == 1);
        // Der Client hat den Tunnel geschlossen, der Host wartet wieder.
        CHECK(TickUntil({&Game.Host.Run}, [&] { return Game.Host.Run.Status().Phase == SessionPhase::Hosting; }));
    }
    SUBCASE("Kurz ohne Host oder unbekannt beim Laden bleibt verbunden")
    {
        REQUIRE(Game.Connect());
        Game.Client.Game.InGame = true;
        TickUntil({&Game.Host.Run, &Game.Client.Run}, [] { return false; }, 50);
        Game.Client.Game.InGame = false;
        TickUntil({&Game.Host.Run, &Game.Client.Run}, [] { return false; }, 60);
        Game.Client.Game.InGame = std::nullopt;
        TickUntil({&Game.Host.Run, &Game.Client.Run}, [] { return false; }, 300);
        Game.Client.Game.InGame = true;
        TickUntil({&Game.Host.Run, &Game.Client.Run}, [] { return false; }, 50);
        Game.Client.Game.InGame = false;
        TickUntil({&Game.Host.Run, &Game.Client.Run}, [] { return false; }, 60);
        Game.Client.Game.InGame = true;
        TickUntil({&Game.Host.Run, &Game.Client.Run}, [] { return false; }, 200);
        CHECK(Game.Client.Run.Status().Phase == SessionPhase::Playing);
        CHECK(Game.Client.Game.Disconnects == 0);
    }
}

TEST_CASE("Lobby-ID aus +connect_lobby")
{
    CHECK(ParseConnectLobby("+connect_lobby 109775240000000001") == 109775240000000001ull);
    CHECK(ParseConnectLobby("\"C:\\Game\\Game.exe\" -x +connect_lobby  42 -y") == 42);
    CHECK(ParseConnectLobby("+connect_lobby") == 0);
    CHECK(ParseConnectLobby("+connect_lobby abc") == 0);
    CHECK(ParseConnectLobby("") == 0);
}
