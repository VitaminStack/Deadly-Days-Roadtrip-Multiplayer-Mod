// winsock2.h muss vor windows.h stehen.
#include <winsock2.h>
#include <ws2tcpip.h>

#include "Net/Transport.h"

#include <cwchar>
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
    uint64_t PacketsToSteam = 0, BytesToSteam = 0, PacketsFromSteam = 0, BytesFromSteam = 0, Dropped = 0;
};

namespace
{
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
    {
        std::lock_guard Guard(StatsLock);
        Current = {};
        Current.IsHost = Role == Mode::Host;
    }
    if (!Open(Role, Remote))
    {
        for (Link& Entry : *Links)
        {
            Sockets->Close(Entry.Connection);
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
    for (Link& Entry : *Links)
    {
        Sockets->Close(Entry.Connection);
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
    Publish(TransportState::Stopped, "Beendet");
    Core::Log::Info("Transport: beendet");
}

TransportStats Transport::Stats() const
{
    std::lock_guard Guard(StatsLock);
    return Current;
}

void Transport::Publish(TransportState State, std::string Message)
{
    std::lock_guard Guard(StatsLock);
    Current.State = State;
    Current.Message = std::move(Message);
}

void Transport::Close(uint32_t Connection)
{
    for (auto It = Links->begin(); It != Links->end(); ++It)
        if (It->Connection == Connection)
        {
            Sockets->Close(Connection);
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
            if (!Sockets->Accept(Change.Connection))
            {
                Core::Log::Warn("Transport: eingehende Verbindung nicht angenommen");
                Sockets->Close(Change.Connection);
                continue;
            }
            Link Incoming;
            Incoming.Connection = Change.Connection;
            Incoming.Udp = OpenUdp(0);
            Incoming.GameAddress = LoopbackUdp(Options.GamePort);
            Incoming.HasGameAddress = true;
            Links->push_back(Incoming);
            Core::Log::Info("Transport: eingehende Verbindung angenommen, {} Client(s)", Links->size());
        }
        else if (Change.State == SteamConnectionState::Connected && Entry)
        {
            Entry->Connected = true;
            const std::string Detail = Sockets->DetailedStatus(Change.Connection);
            const std::string Route = RouteSummary(Detail);
            {
                std::lock_guard Guard(StatsLock);
                Current.Route = Route;
            }
            Core::Log::Info("Transport: verbunden, Route {}", Route);
            Publish(TransportState::Connected, "Verbunden");
        }
        else if (IsEnd(Change.State) && Entry)
        {
            Core::Log::Warn("Transport: Verbindung beendet, Grund {} {}", Change.EndReason, MaskAddresses(Change.EndDebug));
            Close(Change.Connection);
            if (Role != Mode::Host)
                Publish(TransportState::Failed, Change.State == SteamConnectionState::ClosedByPeer
                                                    ? "Der Host hat die Verbindung beendet"
                                                    : "Verbindung zum Host verloren");
            else
            {
                bool AnyConnected = false;
                for (const Link& Other : *Links)
                    AnyConnected |= Other.Connected;
                Publish(AnyConnected ? TransportState::Connected : TransportState::Listening,
                        AnyConnected ? "Verbunden" : "Client gegangen, wartet auf Clients");
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
        if (!Entry.Connected)
        {
            ++Entry.Dropped;
            continue;
        }
        Sockets->Send(Entry.Connection, {reinterpret_cast<const uint8_t*>(Buffer), static_cast<size_t>(Size)});
        ++Entry.PacketsToSteam;
        Entry.BytesToSteam += Size;
    }
    // Steam -> Spiel
    while (Sockets->Receive(Entry.Connection, 64, [&Entry](std::span<const uint8_t> Message) {
        if (Entry.HasGameAddress)
            sendto(Entry.Udp, reinterpret_cast<const char*>(Message.data()), static_cast<int>(Message.size()), 0,
                   reinterpret_cast<const sockaddr*>(&Entry.GameAddress), sizeof(Entry.GameAddress));
        ++Entry.PacketsFromSteam;
        Entry.BytesFromSteam += Message.size();
    }) > 0)
    {
    }
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
        if (Entry.Connected)
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
                Pump(Entry);

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
