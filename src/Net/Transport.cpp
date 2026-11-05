// winsock2.h muss vor windows.h stehen.
#include <winsock2.h>
#include <ws2tcpip.h>

#include "Net/Transport.h"

#include <cwchar>
#include <algorithm>
#include <vector>

#include "Core/Log.h"

#pragma comment(lib, "ws2_32.lib")

namespace Net
{
struct Transport::Link
{
    SteamConnection Connection = 0;
    SOCKET Udp = INVALID_SOCKET;
    sockaddr_in GameAddress{};
    bool HasGameAddress = false;
    bool Connected = false;
    bool Ready = false;
    bool Rejected = false;
    uint64_t Peer = 0;
    ULONGLONG HandshakeStarted = 0;
    uint64_t PacketsToSteam = 0, BytesToSteam = 0, PacketsFromSteam = 0, BytesFromSteam = 0, Dropped = 0;
    // Nur mit TransportOptions::Sim, ausgehende Richtung.
    std::shared_ptr<NetSimQueue> Sim;
};

namespace
{
std::atomic<uint64_t> NextPeer = 1;
constexpr size_t MaxControlQueue = 128;
sockaddr_in LoopbackUdp(uint16_t Port)
{
    sockaddr_in Address{};
    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return Address;
}

SOCKET OpenUdp(uint16_t Port)
{
    const SOCKET Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const sockaddr_in Address = LoopbackUdp(Port);
    if (Socket == INVALID_SOCKET || bind(Socket, reinterpret_cast<const sockaddr*>(&Address), sizeof(Address)) != 0)
    {
        Core::Log::Error("Transport: UDP-Port {} nicht verfügbar, Fehler {}", Port, WSAGetLastError());
        if (Socket != INVALID_SOCKET)
            closesocket(Socket);
        return INVALID_SOCKET;
    }
    u_long NonBlocking = 1;
    ioctlsocket(Socket, FIONBIO, &NonBlocking);
    return Socket;
}

bool IsEnd(SteamConnectionState State)
{
    return State == SteamConnectionState::ClosedByPeer || State == SteamConnectionState::ProblemDetectedLocally;
}
}

std::string EndMessage(bool Host, SteamConnectionState State, int32_t Reason, bool WasConnected)
{
    if (State == SteamConnectionState::ClosedByPeer)
    {
        switch (Reason)
        {
        case EndReason::HostLeft:
            return "Der Host hat die Sitzung verlassen";
        case EndReason::ClientLeft:
            return "Der Mitspieler hat die Sitzung verlassen";
        case EndReason::Rejected:
            return Host ? "Der Mitspieler hat die Verbindung abgelehnt"
                        : "Der Host hat die Verbindung abgelehnt (andere Mod-Version oder anderer Spiel-Build)";
        case EndReason::Busy:
            return "Der Host hat schon einen Mitspieler";
        default:
            return Host ? "Der Mitspieler hat die Verbindung beendet" : "Der Host hat die Verbindung beendet";
        }
    }
    // Steam hat selbst aufgegeben, meist nach seinem Zeitlimit ohne Antwort der Gegenseite.
    if (Host)
        return "Verbindung zum Mitspieler verloren";
    return WasConnected ? "Verbindung zum Host verloren" : "Host nicht erreichbar";
}

std::string_view ToString(TransportState State)
{
    switch (State)
    {
    case TransportState::Stopped:
        return "aus";
    case TransportState::Listening:
        return "wartet auf Clients";
    case TransportState::Connecting:
        return "verbindet";
    case TransportState::Connected:
        return "verbunden";
    case TransportState::Failed:
        return "fehlgeschlagen";
    }
    return "unbekannt";
}

Transport::Transport(std::shared_ptr<SteamSockets> SocketsIn, TransportOptions OptionsIn)
    : Sockets(std::move(SocketsIn)), Options(OptionsIn), Links(std::make_unique<std::vector<Link>>())
{
}

Transport::~Transport()
{
    Stop();
}

bool Transport::StartHost()
{
    return Begin(Mode::Host, 0);
}

bool Transport::StartClient(uint64_t HostSteamId)
{
    if (!HostSteamId)
    {
        Publish(TransportState::Failed, "Keine SteamID für den Host");
        return false;
    }
    return Begin(Mode::ClientP2P, HostSteamId);
}

bool Transport::StartLoopbackClient()
{
    return Begin(Mode::ClientLoopback, 0);
}

bool Transport::Begin(Mode RoleIn, uint64_t Remote)
{
    if (Active || !Sockets)
    {
        Core::Log::Warn("Transport: läuft schon oder ohne Steam");
        return false;
    }
    WSADATA Data;
    WSAStartup(MAKEWORD(2, 2), &Data);
    Role = RoleIn;
    Options.Identity.Host = Role == Mode::Host;
    {
        std::lock_guard Guard(StatsLock);
        Current = {};
        Current.IsHost = Role == Mode::Host;
    }
    if (!Open(Role, Remote))
    {
        for (Link& Entry : *Links)
        {
            Sockets->Close(Entry.Connection, EndReason::Generic);
            if (Entry.Udp != INVALID_SOCKET)
                closesocket(Entry.Udp);
        }
        Links->clear();
        if (ListenP2P)
            Sockets->CloseListen(ListenP2P);
        if (ListenLoopback)
            Sockets->CloseListen(ListenLoopback);
        ListenP2P = ListenLoopback = 0;
        WSACleanup();
        return false;
    }
    if (Options.Sim.Enabled())
        Core::Log::Warn("Netzsimulation aktiv, nur für Tests: {}", Describe(Options.Sim));
    Active = true;
    Worker = std::thread(&Transport::Run, this);
    return true;
}

bool Transport::Open(Mode RoleIn, uint64_t Remote)
{
    Sockets->InitRelayNetworkAccess();
    if (RoleIn == Mode::Host)
    {
        ListenP2P = Sockets->ListenP2P(Options.VirtualPort);
        ListenLoopback = Sockets->ListenLoopback(Options.LoopbackPort);
        if (!ListenP2P && !ListenLoopback)
        {
            Publish(TransportState::Failed, "Steam nimmt keine Verbindungen an");
            return false;
        }
        Core::Log::Info("Transport: Host wartet auf Clients (P2P {}, 127.0.0.1:{} {}), leitet an UDP {}",
                        ListenP2P ? "bereit" : "abgelehnt", Options.LoopbackPort, ListenLoopback ? "bereit" : "abgelehnt",
                        Options.GamePort);
        Publish(TransportState::Listening, "Wartet auf Clients");
        return true;
    }

    const SOCKET Udp = OpenUdp(Options.ClientPort);
    if (Udp == INVALID_SOCKET)
    {
        Publish(TransportState::Failed, "UDP-Port " + std::to_string(Options.ClientPort) + " ist belegt");
        return false;
    }
    const SteamConnection Connection = RoleIn == Mode::ClientP2P ? Sockets->ConnectP2P(Remote, Options.VirtualPort)
                                                                 : Sockets->ConnectLoopback(Options.LoopbackPort);
    // Steam lehnt etwa P2P zum eigenen Konto mit Handle 0 ab.
    if (!Connection)
    {
        closesocket(Udp);
        Publish(TransportState::Failed, "Steam hat die Verbindung abgelehnt");
        return false;
    }
    Link Entry;
    Entry.Connection = Connection;
    Entry.Udp = Udp;
    AttachSim(Entry);
    Links->push_back(Entry);
    Core::Log::Info("Transport: Client verbindet über {}, Spiel auf UDP {}", RoleIn == Mode::ClientP2P ? "P2P" : "127.0.0.1",
                    Options.ClientPort);
    Publish(TransportState::Connecting, "Verbindet mit dem Host");
    return true;
}

void Transport::Stop()
{
    if (!Active.exchange(false))
        return;
    if (Worker.joinable())
        Worker.join();
    // Die Gegenseite erfährt so, dass jemand bewusst gegangen ist, und nicht nur die Leitung fehlt.
    const int32_t Reason = Role == Mode::Host ? EndReason::HostLeft : EndReason::ClientLeft;
    for (Link& Entry : *Links)
    {
        Sockets->Close(Entry.Connection, Reason);
        if (Entry.Udp != INVALID_SOCKET)
            closesocket(Entry.Udp);
    }
    Links->clear();
    {
        std::lock_guard Guard(ControlLock);
        Peers.clear();
        Incoming.clear();
        Outgoing.clear();
    }
    if (ListenP2P)
        Sockets->CloseListen(ListenP2P);
    if (ListenLoopback)
        Sockets->CloseListen(ListenLoopback);
    ListenP2P = ListenLoopback = 0;
    WSACleanup();
    Publish(TransportState::Stopped, "Beendet");
    Core::Log::Info("Transport: beendet");
}

TransportStats Transport::Stats() const
{
    std::lock_guard Guard(StatsLock);
    return Current;
}

std::vector<uint64_t> Transport::ControlPeers() const
{
    std::lock_guard Guard(ControlLock);
    return Peers;
}

bool Transport::SendControl(uint64_t Peer, const Protocol::Control& Value)
{
    auto Bytes = Protocol::EncodeControl(Value);
    if (Bytes.empty())
        return false;
    std::lock_guard Guard(ControlLock);
    if (!Active || Outgoing.size() >= MaxControlQueue || std::find(Peers.begin(), Peers.end(), Peer) == Peers.end())
        return false;
    Outgoing.push_back({Peer, std::move(Bytes)});
    return true;
}

std::vector<ControlMessage> Transport::TakeControlMessages()
{
    std::lock_guard Guard(ControlLock);
    std::vector<ControlMessage> Result;
    Result.swap(Incoming);
    return Result;
}

void Transport::Publish(TransportState State, std::string Message)
{
    std::lock_guard Guard(StatsLock);
    Current.State = State;
    Current.Message = std::move(Message);
}

void Transport::Close(uint32_t Connection, int32_t Reason)
{
    for (auto It = Links->begin(); It != Links->end(); ++It)
        if (It->Connection == Connection)
        {
            {
                std::lock_guard Guard(ControlLock);
                const uint64_t Peer = It->Peer;
                std::erase(Peers, Peer);
                std::erase_if(Incoming, [Peer](const auto& Item) { return Item.Peer == Peer; });
                std::erase_if(Outgoing, [Peer](const auto& Item) { return Item.Peer == Peer; });
            }
            Sockets->Close(Connection, Reason);
            if (It->Udp != INVALID_SOCKET)
                closesocket(It->Udp);
            ClosedPacketsTo += It->PacketsToSteam;
            ClosedPacketsFrom += It->PacketsFromSteam;
            ClosedBytesTo += It->BytesToSteam;
            ClosedBytesFrom += It->BytesFromSteam;
            ClosedDropped += It->Dropped;
            Links->erase(It);
            return;
        }
}

void Transport::HandleChanges()
{
    for (const SteamStatusChange& Change : Sockets->TakeStatusChanges())
    {
        Link* Entry = nullptr;
        for (Link& Candidate : *Links)
            if (Candidate.Connection == Change.Connection)
                Entry = &Candidate;

        if (Change.State == SteamConnectionState::Connecting && Change.ListenSocket && Role == Mode::Host && !Entry)
        {
            if (!Links->empty())
            {
                Core::Log::Warn("Transport: weitere Verbindung abgelehnt, es gibt schon einen Mitspieler");
                Sockets->Close(Change.Connection, EndReason::Busy);
                continue; // Genau ein Remote-Spieler. Kein frei behaupteter Spielerindex.
            }
            if (!Sockets->Accept(Change.Connection))
            {
                Core::Log::Warn("Transport: eingehende Verbindung nicht angenommen");
                Sockets->Close(Change.Connection, EndReason::Generic);
                continue;
            }
            Link NewLink;
            NewLink.Connection = Change.Connection;
            NewLink.Udp = OpenUdp(0);
            if (NewLink.Udp == INVALID_SOCKET)
            {
                Sockets->Close(Change.Connection, EndReason::Generic);
                continue;
            }
            NewLink.GameAddress = LoopbackUdp(Options.GamePort);
            NewLink.HasGameAddress = true;
            AttachSim(NewLink);
            Links->push_back(NewLink);
            Core::Log::Info("Transport: eingehende Verbindung angenommen, {} Client(s)", Links->size());
        }
        else if (Change.State == SteamConnectionState::Connected && Entry)
        {
            Entry->Connected = true;
            Entry->HandshakeStarted = GetTickCount64();
            Entry->Peer = NextPeer.fetch_add(1);
            if (!Sockets->SendReliable(Entry->Connection, Protocol::Hello(Options.Identity)))
                Reject(*Entry, "Handshake konnte nicht gesendet werden");
            const std::string Detail = Sockets->DetailedStatus(Change.Connection);
            const std::string Route = RouteSummary(Detail);
            {
                std::lock_guard Guard(StatsLock);
                Current.Route = Route;
            }
            Core::Log::Info("Transport: verbunden, Route {}", Route);
            // Erst nach geprueftem Hello als verbunden veroeffentlichen.
        }
        else if (IsEnd(Change.State) && Entry)
        {
            Core::Log::Warn("Transport: Verbindung beendet, Grund {} {}", Change.EndReason, MaskAddresses(Change.EndDebug));
            const std::string Message = EndMessage(Role == Mode::Host, Change.State, Change.EndReason, Entry->Connected);
            Close(Change.Connection, EndReason::Generic);
            Core::Log::Info("Transport: {}", Message);
            if (Role != Mode::Host)
                Publish(TransportState::Failed, Message);
            else
            {
                bool AnyConnected = false;
                for (const Link& Other : *Links)
                    AnyConnected |= Other.Ready;
                Publish(AnyConnected ? TransportState::Connected : TransportState::Listening,
                        AnyConnected ? "Verbunden" : Message);
            }
        }
    }
}

void Transport::Pump(Link& Entry)
{
    char Buffer[4096];
    // Spiel -> Steam
    for (;;)
    {
        sockaddr_in From{};
        int FromSize = sizeof(From);
        const int Size = recvfrom(Entry.Udp, Buffer, sizeof(Buffer), 0, reinterpret_cast<sockaddr*>(&From), &FromSize);
        if (Size <= 0)
            break;
        // Beim Client lernt der Tunnel die Adresse des Spiels aus dem ersten Paket, sie kann sich ändern.
        if (Role != Mode::Host)
        {
            Entry.GameAddress = From;
            Entry.HasGameAddress = true;
        }
        if (!Entry.Ready || Entry.Rejected)
        {
            ++Entry.Dropped;
            continue;
        }
        const auto Bytes = Protocol::Encode(Protocol::Kind::Game,
            {reinterpret_cast<const uint8_t*>(Buffer), static_cast<size_t>(Size)});
        // Mit Netzsimulation geht das Paket erst in FlushSim an Steam, oder gar nicht.
        if (Entry.Sim)
            Entry.Sim->Push(GetTickCount64(), Bytes, false);
        else
            SendGame(Entry, Bytes);
    }
    // Steam -> Spiel
    if (!Entry.Connected || Entry.Rejected)
        return; // Accept kann vor dem folgenden Connected-Callback schon Nachrichten bereitstellen.
    Sockets->Receive(Entry.Connection, 64, [this, &Entry](std::span<const uint8_t> Message) {
        if (!Entry.Rejected && !ReceiveFrame(Entry, Message))
            Reject(Entry, Protocol::Explain(Message, Options.Identity));
    });
}

void Transport::AttachSim(Link& Entry)
{
    if (Options.Sim.Enabled())
        Entry.Sim = std::make_shared<NetSimQueue>(Options.Sim, static_cast<uint32_t>(GetTickCount64()) ^ Entry.Connection);
}

void Transport::SendGame(Link& Entry, std::span<const uint8_t> Bytes)
{
    if (!Sockets->Send(Entry.Connection, Bytes))
    {
        ++Entry.Dropped;
        return;
    }
    ++Entry.PacketsToSteam;
    // Ohne den Rahmen des Tunnels, wie bisher.
    if (const auto Frame = Protocol::Decode(Bytes))
        Entry.BytesToSteam += Frame->Payload.size();
}

void Transport::FlushSim(Link& Entry, uint64_t NowMs)
{
    if (!Entry.Sim)
        return;
    Entry.Sim->PopReady(NowMs, [this, &Entry](std::span<const uint8_t> Bytes, bool Reliable) {
        if (Entry.Rejected)
            return;
        if (!Reliable)
            SendGame(Entry, Bytes);
        else if (!Sockets->SendReliable(Entry.Connection, Bytes))
            Reject(Entry, "Kontrollnachricht konnte nicht gesendet werden");
    });
}

void Transport::Reject(Link& Entry, const std::string& Reason)
{
    Entry.Rejected = true;
    Core::Log::Warn("Protokoll: {}", Reason);
    Publish(Role == Mode::Host ? TransportState::Listening : TransportState::Failed, Reason);
}

bool Transport::ReceiveFrame(Link& Entry, std::span<const uint8_t> Bytes)
{
    const auto Frame = Protocol::Decode(Bytes);
    if (!Frame)
        return false;
    if (Frame->Type == Protocol::Kind::Hello)
    {
        if (!Entry.Connected || Entry.Ready || !Protocol::Matches(Frame->Payload, Options.Identity))
            return false;
        Entry.Ready = true;
        {
            std::lock_guard Guard(ControlLock);
            Peers.push_back(Entry.Peer);
        }
        Core::Log::Info("Protokoll: Build, Mod-Version und Gegenrolle bestaetigt");
        Publish(TransportState::Connected, "Verbunden");
        return true;
    }
    if (!Entry.Ready)
    {
        // Unreliable-Spielpakete duerfen das reliable Hello ueberholen, aber nie zum Spiel gelangen.
        ++Entry.Dropped;
        return Frame->Type == Protocol::Kind::Game;
    }
    if (Frame->Type == Protocol::Kind::Control)
    {
        auto Value = Protocol::DecodeControl(Frame->Payload);
        if (!Value)
            return false;
        std::lock_guard Guard(ControlLock);
        if (Incoming.size() >= MaxControlQueue)
            return false; // Nicht still einen verlaesslichen Zustand verlieren.
        Incoming.push_back({Entry.Peer, std::move(*Value)});
        return true;
    }
    if (Frame->Payload.empty() || Frame->Payload.size() > 4096)
        return false;
    if (Entry.HasGameAddress)
        sendto(Entry.Udp, reinterpret_cast<const char*>(Frame->Payload.data()), static_cast<int>(Frame->Payload.size()), 0,
               reinterpret_cast<const sockaddr*>(&Entry.GameAddress), sizeof(Entry.GameAddress));
    ++Entry.PacketsFromSteam;
    Entry.BytesFromSteam += Frame->Payload.size();
    return true;
}

void Transport::UpdateTotals()
{
    TransportStats Totals;
    Totals.PacketsToSteam = ClosedPacketsTo;
    Totals.PacketsFromSteam = ClosedPacketsFrom;
    Totals.BytesToSteam = ClosedBytesTo;
    Totals.BytesFromSteam = ClosedBytesFrom;
    Totals.Dropped = ClosedDropped;
    SteamQuality Best;
    unsigned Connected = 0;
    for (const Link& Entry : *Links)
    {
        Totals.PacketsToSteam += Entry.PacketsToSteam;
        Totals.PacketsFromSteam += Entry.PacketsFromSteam;
        Totals.BytesToSteam += Entry.BytesToSteam;
        Totals.BytesFromSteam += Entry.BytesFromSteam;
        Totals.Dropped += Entry.Dropped;
        if (Entry.Ready && !Entry.Rejected)
        {
            ++Connected;
            const SteamQuality Quality = Sockets->Quality(Entry.Connection);
            if (Quality.PingMs > Best.PingMs)
                Best = Quality;
        }
    }
    std::lock_guard Guard(StatsLock);
    Current.Connections = Connected;
    Current.PingMs = Best.PingMs;
    Current.Quality = Best.Quality;
    Current.PacketsToSteam = Totals.PacketsToSteam;
    Current.PacketsFromSteam = Totals.PacketsFromSteam;
    Current.BytesToSteam = Totals.BytesToSteam;
    Current.BytesFromSteam = Totals.BytesFromSteam;
    Current.Dropped = Totals.Dropped;
}

void Transport::Run()
{
    ULONGLONG NextTotals = GetTickCount64() + 500;
    ULONGLONG NextLog = GetTickCount64() + Options.StatsIntervalMs;
    while (Active)
    {
        Sockets->RunCallbacks();
        HandleChanges();

        fd_set Readable;
        FD_ZERO(&Readable);
        for (const Link& Entry : *Links)
            if (Entry.Udp != INVALID_SOCKET && Readable.fd_count < FD_SETSIZE)
                FD_SET(Entry.Udp, &Readable);
        timeval Wait{0, 1000};
        if (Readable.fd_count)
            select(0, &Readable, nullptr, nullptr, &Wait);
        else
            Sleep(1);
        for (Link& Entry : *Links)
            if (Entry.Udp != INVALID_SOCKET)
            {
                Pump(Entry);
                if (Entry.Connected && !Entry.Ready && !Entry.Rejected &&
                    GetTickCount64() - Entry.HandshakeStarted >= Options.HandshakeTimeoutMs)
                    Reject(Entry, "Zeitlimit beim Versions-Handshake");
            }

        std::vector<PendingControl> Pending;
        {
            std::lock_guard Guard(ControlLock);
            Pending.swap(Outgoing);
        }
        const uint64_t SimNow = GetTickCount64();
        for (const auto& Item : Pending)
            for (Link& Entry : *Links)
            {
                if (Entry.Peer != Item.Peer || !Entry.Ready || Entry.Rejected)
                    continue;
                if (Entry.Sim)
                    Entry.Sim->Push(SimNow, Item.Bytes, true);
                else if (!Sockets->SendReliable(Entry.Connection, Item.Bytes))
                    Reject(Entry, "Kontrollnachricht konnte nicht gesendet werden");
            }
        for (Link& Entry : *Links)
            FlushSim(Entry, SimNow);
        std::vector<uint32_t> Rejected;
        for (const Link& Entry : *Links)
            if (Entry.Rejected)
                Rejected.push_back(Entry.Connection);
        for (const uint32_t Connection : Rejected)
            Close(Connection, EndReason::Rejected);

        const ULONGLONG Now = GetTickCount64();
        if (Now >= NextTotals)
        {
            NextTotals = Now + 500;
            UpdateTotals();
        }
        if (Now >= NextLog)
        {
            NextLog = Now + Options.StatsIntervalMs;
            const TransportStats Snapshot = Stats();
            if (Snapshot.Connections)
                Core::Log::Info("Transport: {} Client(s), Ping {} ms, Qualität {:.2f}, zu Steam {} Pakete/{} B, von Steam {} "
                                "Pakete/{} B, verworfen {}",
                                Snapshot.Connections, Snapshot.PingMs, Snapshot.Quality, Snapshot.PacketsToSteam,
                                Snapshot.BytesToSteam, Snapshot.PacketsFromSteam, Snapshot.BytesFromSteam, Snapshot.Dropped);
            // Gemessene Werte der Simulation seit der letzten Zeile, ausgehende Richtung dieser Instanz.
            for (Link& Entry : *Links)
                if (Entry.Sim && Entry.Ready)
                    Core::Log::Info("Netzsimulation: {}, Steam-Ping {} ms", Describe(Entry.Sim->TakeStats()),
                                    Snapshot.PingMs);
        }
    }
}

std::optional<TransportRequest> ParseTransportRequest(std::wstring_view CommandLine)
{
    constexpr std::wstring_view Key = L"-ddcoop_transport=";
    const size_t At = CommandLine.find(Key);
    if (At == std::wstring_view::npos)
        return std::nullopt;
    std::wstring_view Value = CommandLine.substr(At + Key.size());
    Value = Value.substr(0, Value.find_first_of(L" \t\""));
    TransportRequest Request;
    if (Value == L"host")
        Request.Role = TransportRequest::Kind::Host;
    else if (Value == L"loopback")
        Request.Role = TransportRequest::Kind::Loopback;
    else
    {
        if (Value.empty() || Value.size() > 20 || Value.find_first_not_of(L"0123456789") != std::wstring_view::npos)
            return std::nullopt;
        Request.Role = TransportRequest::Kind::Client;
        Request.HostSteamId = std::wcstoull(std::wstring(Value).c_str(), nullptr, 10);
        if (!Request.HostSteamId)
            return std::nullopt;
    }
    return Request;
}
}
