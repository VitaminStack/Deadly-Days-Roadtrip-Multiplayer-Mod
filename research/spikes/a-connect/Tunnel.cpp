// Spike E: Spielverkehr ueber Steam Networking tunneln. Der IpNetDriver spricht weiter UDP auf 127.0.0.1, der Tunnel
// leitet jedes Paket unveraendert ueber ISteamNetworkingSockets weiter. Die Steam-Laufzeit des Spiels (steam_api64.dll,
// Steamworks 1.64) wird ueber ihre flache C-Schnittstelle genutzt, ein SDK ist nicht noetig.
//
//   Client-Spiel -> UDP 127.0.0.1:7778 -> Tunnel -> Steam -> Tunnel -> UDP 127.0.0.1:7777 -> Host-Spiel
//
// Befehle: !tunnel host            Host nimmt Verbindungen ueber P2P (virtueller Port 0) und ueber IP 127.0.0.1:7790 an
//          !tunnel join self       Client verbindet per P2P zur eigenen SteamID (ein PC, ein Konto)
//          !tunnel join <steamid>  Client verbindet per P2P zu einem anderen Konto
//          !tunnel join ip         Client verbindet ueber Steam Networking per IP zu 127.0.0.1:7790, ohne P2P-Vermittlung
// Danach beim Client: open 127.0.0.1:7778
// winsock2.h muss vor windows.h aus Spike.h stehen.
#include <winsock2.h>
#include <ws2tcpip.h>

#include "Spike.h"

#include <atomic>
#include <mutex>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

namespace Spike
{
namespace
{
constexpr uint16_t kGamePort = 7777;
constexpr uint16_t kClientPort = 7778;
constexpr uint16_t kSteamIpPort = 7790;
constexpr int kVirtualPort = 0;

// Typen aus steamnetworkingtypes.h, nur die benutzten Felder. Windows: Packing 8, IP-Adresse gepackt auf 1.
using HSteamNetConnection = uint32_t;
using HSteamListenSocket = uint32_t;

#pragma pack(push, 1)
struct SteamNetworkingIPAddr
{
    uint8_t Ipv6[16];
    uint16_t Port;
};
#pragma pack(pop)
static_assert(sizeof(SteamNetworkingIPAddr) == 18);

struct SteamNetworkingIdentity
{
    int32_t Type;
    int32_t Size;
    union
    {
        uint64_t SteamId64;
        char Raw[128];
    };
};
static_assert(sizeof(SteamNetworkingIdentity) == 136);

struct SteamNetworkingConfigValue
{
    int32_t Value;
    int32_t DataType;
    union
    {
        int32_t Int32;
        int64_t Int64;
        void* Ptr;
    };
};
static_assert(sizeof(SteamNetworkingConfigValue) == 16);

struct SteamNetConnectionInfo
{
    SteamNetworkingIdentity IdentityRemote;
    int64_t UserData;
    HSteamListenSocket ListenSocket;
    SteamNetworkingIPAddr AddrRemote;
    uint16_t Pad;
    uint32_t PopRemote;
    uint32_t PopRelay;
    int32_t State;
    int32_t EndReason;
    char EndDebug[128];
    char Description[128];
    int32_t Flags;
    uint32_t Reserved[63];
};
static_assert(offsetof(SteamNetConnectionInfo, State) == 176 && sizeof(SteamNetConnectionInfo) == 696);

struct SteamNetConnectionStatusChanged
{
    HSteamNetConnection Connection;
    SteamNetConnectionInfo Info;
    int32_t OldState;
};
static_assert(offsetof(SteamNetConnectionStatusChanged, Info) == 8);

struct SteamNetworkingMessage
{
    void* Data;
    int32_t Size;
    HSteamNetConnection Connection;
};

constexpr int32_t kIdentitySteamId = 16;
constexpr int32_t kConfigInt32 = 1;
constexpr int32_t kConfigPtr = 5;
constexpr int32_t kConfigIpAllowWithoutAuth = 23;
constexpr int32_t kConfigStatusChangedCallback = 201;
constexpr int32_t kStateConnecting = 1;
constexpr int32_t kStateConnected = 3;
constexpr int32_t kStateClosedByPeer = 4;
constexpr int32_t kStateProblemDetectedLocally = 5;
constexpr int kSendUnreliableNoNagle = 1;

using StatusCallback = void(__cdecl*)(SteamNetConnectionStatusChanged*);

struct SteamApi
{
    void* (*Sockets)();
    void* (*Utils)();
    void* (*User)();
    uint64_t (*GetSteamId)(void*);
    void (*InitRelay)(void*);
    HSteamListenSocket (*ListenP2P)(void*, int, int, const SteamNetworkingConfigValue*);
    HSteamListenSocket (*ListenIp)(void*, const SteamNetworkingIPAddr&, int, const SteamNetworkingConfigValue*);
    HSteamNetConnection (*ConnectP2P)(void*, const SteamNetworkingIdentity&, int, int, const SteamNetworkingConfigValue*);
    HSteamNetConnection (*ConnectIp)(void*, const SteamNetworkingIPAddr&, int, const SteamNetworkingConfigValue*);
    int (*Accept)(void*, HSteamNetConnection);
    bool (*Close)(void*, HSteamNetConnection, int, const char*, bool);
    int (*Send)(void*, HSteamNetConnection, const void*, uint32_t, int, int64_t*);
    int (*Receive)(void*, HSteamNetConnection, SteamNetworkingMessage**, int);
    void (*RunCallbacks)(void*);
    int (*RealTimeStatus)(void*, HSteamNetConnection, void*, int, void*);
    int (*DetailedStatus)(void*, HSteamNetConnection, char*, int);
    void (*Release)(SteamNetworkingMessage*);
};

SteamApi Api{};
void* Sockets = nullptr;

template <typename T> bool Resolve(HMODULE Module, const char* Name, T& Out)
{
    Out = reinterpret_cast<T>(GetProcAddress(Module, Name));
    if (!Out)
        Log(std::string("Tunnel: Export fehlt ") + Name);
    return Out != nullptr;
}

bool LoadSteamApi()
{
    const HMODULE Module = GetModuleHandleW(L"steam_api64.dll");
    if (!Module)
    {
        Log("Tunnel: steam_api64.dll ist nicht geladen");
        return false;
    }
    bool Ok = Resolve(Module, "SteamAPI_SteamNetworkingSockets_SteamAPI_v012", Api.Sockets);
    Ok &= Resolve(Module, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004", Api.Utils);
    Ok &= Resolve(Module, "SteamAPI_SteamUser_v023", Api.User);
    Ok &= Resolve(Module, "SteamAPI_ISteamUser_GetSteamID", Api.GetSteamId);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess", Api.InitRelay);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P", Api.ListenP2P);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP", Api.ListenIp);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_ConnectP2P", Api.ConnectP2P);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress", Api.ConnectIp);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_AcceptConnection", Api.Accept);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_CloseConnection", Api.Close);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_SendMessageToConnection", Api.Send);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection", Api.Receive);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_RunCallbacks", Api.RunCallbacks);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus", Api.RealTimeStatus);
    Ok &= Resolve(Module, "SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus", Api.DetailedStatus);
    Ok &= Resolve(Module, "SteamAPI_SteamNetworkingMessage_t_Release", Api.Release);
    if (!Ok)
        return false;
    Sockets = Api.Sockets();
    if (!Sockets)
        Log("Tunnel: ISteamNetworkingSockets fehlt, Steam im Spiel nicht initialisiert");
    return Sockets != nullptr;
}

SteamNetworkingIPAddr Loopback(uint16_t Port)
{
    SteamNetworkingIPAddr Address{};
    Address.Ipv6[10] = 0xFF;
    Address.Ipv6[11] = 0xFF;
    Address.Ipv6[12] = 127;
    Address.Ipv6[15] = 1;
    Address.Port = Port;
    return Address;
}

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
        Log("Tunnel: UDP-Port " + std::to_string(Port) + " nicht verfuegbar, Fehler " + std::to_string(WSAGetLastError()));
        if (Socket != INVALID_SOCKET)
            closesocket(Socket);
        return INVALID_SOCKET;
    }
    u_long NonBlocking = 1;
    ioctlsocket(Socket, FIONBIO, &NonBlocking);
    return Socket;
}

struct StatusEvent
{
    HSteamNetConnection Connection;
    HSteamListenSocket ListenSocket;
    int32_t State;
    int32_t OldState;
    int32_t EndReason;
    std::string EndDebug;
    std::string Description;
};

std::mutex EventLock;
std::vector<StatusEvent> Events;

// Kann aus RunCallbacks des Tunnels oder aus SteamAPI_RunCallbacks des Spiels kommen, deshalb nur einreihen.
void __cdecl OnStatusChanged(SteamNetConnectionStatusChanged* Change)
{
    std::lock_guard Guard(EventLock);
    Events.push_back({Change->Connection, Change->Info.ListenSocket, Change->Info.State, Change->OldState,
                      Change->Info.EndReason, Change->Info.EndDebug, Change->Info.Description});
}

// Eine Steam-Verbindung mit ihrem UDP-Gegenstueck zum Spiel.
struct Link
{
    HSteamNetConnection Connection = 0;
    SOCKET Udp = INVALID_SOCKET;
    sockaddr_in GameAddress{};
    bool HasGameAddress = false;
    bool Connected = false;
    uint64_t PacketsToSteam = 0, BytesToSteam = 0, PacketsFromSteam = 0, BytesFromSteam = 0, Dropped = 0;
};

enum class Mode
{
    Host,
    JoinP2P,
    JoinIp
};

std::atomic<bool> Running = false;
std::vector<Link> Links;

Link* FindLink(HSteamNetConnection Connection)
{
    for (Link& Entry : Links)
        if (Entry.Connection == Connection)
            return &Entry;
    return nullptr;
}

void CloseLink(HSteamNetConnection Connection)
{
    for (auto It = Links.begin(); It != Links.end(); ++It)
        if (It->Connection == Connection)
        {
            Api.Close(Sockets, Connection, 0, "Tunnel", false);
            // Beim Client bleibt der Port fuer das Spiel offen, nur der Host gibt seinen Gegenport frei.
            if (It->Udp != INVALID_SOCKET && It->GameAddress.sin_port == htons(kGamePort))
                closesocket(It->Udp);
            Links.erase(It);
            return;
        }
}

void LogRoute(HSteamNetConnection Connection)
{
    std::string Text(2048, '\0');
    if (Api.DetailedStatus(Sockets, Connection, Text.data(), static_cast<int>(Text.size())) < 0)
        return;
    Text.resize(strlen(Text.c_str()));
    // Nur die Zeilen zur Route, der Rest ist Statistik.
    size_t Start = 0;
    while (Start < Text.size())
    {
        const size_t End = Text.find('\n', Start);
        const std::string Line = Text.substr(Start, End == std::string::npos ? std::string::npos : End - Start);
        if (Line.find("route") != std::string::npos || Line.find("Route") != std::string::npos ||
            Line.find("relay") != std::string::npos || Line.find("Relay") != std::string::npos ||
            Line.find("Transport") != std::string::npos || Line.find("Ping") != std::string::npos)
            Log("Tunnel: Route " + Line);
        if (End == std::string::npos)
            break;
        Start = End + 1;
    }
}

void HandleEvents(Mode Role, SOCKET ClientUdp)
{
    std::vector<StatusEvent> Pending;
    {
        std::lock_guard Guard(EventLock);
        Pending.swap(Events);
    }
    for (const StatusEvent& Event : Pending)
    {
        Log("Tunnel: Verbindung " + std::to_string(Event.Connection) + " Zustand " + std::to_string(Event.OldState) +
            " -> " + std::to_string(Event.State) + ", " + Event.Description);
        if (Event.State == kStateConnecting && Event.ListenSocket && Role == Mode::Host && !FindLink(Event.Connection))
        {
            const int Result = Api.Accept(Sockets, Event.Connection);
            Link Entry;
            Entry.Connection = Event.Connection;
            Entry.Udp = OpenUdp(0);
            Entry.GameAddress = LoopbackUdp(kGamePort);
            Entry.HasGameAddress = true;
            Links.push_back(Entry);
            Log("Tunnel: eingehende Verbindung angenommen, Ergebnis " + std::to_string(Result));
        }
        else if (Event.State == kStateConnected)
        {
            if (Link* Entry = FindLink(Event.Connection))
                Entry->Connected = true;
            LogRoute(Event.Connection);
        }
        else if (Event.State == kStateClosedByPeer || Event.State == kStateProblemDetectedLocally)
        {
            Log("Tunnel: Verbindung beendet, Grund " + std::to_string(Event.EndReason) + " " + Event.EndDebug);
            CloseLink(Event.Connection);
        }
    }
    (void)ClientUdp;
}

void Pump(Link& Entry)
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
        if (!Entry.HasGameAddress)
        {
            Entry.GameAddress = From;
            Entry.HasGameAddress = true;
        }
        else if (Entry.GameAddress.sin_port != htons(kGamePort))
            Entry.GameAddress = From;
        if (!Entry.Connected)
        {
            ++Entry.Dropped;
            continue;
        }
        Api.Send(Sockets, Entry.Connection, Buffer, static_cast<uint32_t>(Size), kSendUnreliableNoNagle, nullptr);
        ++Entry.PacketsToSteam;
        Entry.BytesToSteam += Size;
    }
    // Steam -> Spiel
    SteamNetworkingMessage* Messages[64];
    for (;;)
    {
        const int Count = Api.Receive(Sockets, Entry.Connection, Messages, 64);
        if (Count <= 0)
            break;
        for (int Index = 0; Index < Count; ++Index)
        {
            if (Entry.HasGameAddress)
                sendto(Entry.Udp, static_cast<const char*>(Messages[Index]->Data), Messages[Index]->Size, 0,
                       reinterpret_cast<const sockaddr*>(&Entry.GameAddress), sizeof(Entry.GameAddress));
            ++Entry.PacketsFromSteam;
            Entry.BytesFromSteam += Messages[Index]->Size;
            Api.Release(Messages[Index]);
        }
    }
}

void LogStats()
{
    for (Link& Entry : Links)
    {
        uint8_t Status[256]{};
        int32_t Ping = -1;
        float Quality = -1.0f;
        if (Api.RealTimeStatus(Sockets, Entry.Connection, Status, 0, nullptr) == 1)
        {
            memcpy(&Ping, Status + 4, 4);
            memcpy(&Quality, Status + 8, 4);
        }
        Log("Tunnel: Verbindung " + std::to_string(Entry.Connection) + (Entry.Connected ? " verbunden" : " wartet") +
            ", zu Steam " + std::to_string(Entry.PacketsToSteam) + " Pakete/" + std::to_string(Entry.BytesToSteam) +
            " B, von Steam " + std::to_string(Entry.PacketsFromSteam) + " Pakete/" + std::to_string(Entry.BytesFromSteam) +
            " B, verworfen " + std::to_string(Entry.Dropped) + ", Ping " + std::to_string(Ping) + " ms, Qualitaet " +
            std::to_string(Quality));
    }
}

void Run(Mode Role, uint64_t Remote)
{
    WSADATA Data;
    WSAStartup(MAKEWORD(2, 2), &Data);
    Api.InitRelay(Api.Utils());

    SteamNetworkingConfigValue Options[2]{};
    Options[0].Value = kConfigStatusChangedCallback;
    Options[0].DataType = kConfigPtr;
    Options[0].Ptr = reinterpret_cast<void*>(&OnStatusChanged);
    Options[1].Value = kConfigIpAllowWithoutAuth;
    Options[1].DataType = kConfigInt32;
    Options[1].Int32 = 1;

    SOCKET ClientUdp = INVALID_SOCKET;
    if (Role == Mode::Host)
    {
        const HSteamListenSocket P2P = Api.ListenP2P(Sockets, kVirtualPort, 1, Options);
        const HSteamListenSocket Ip = Api.ListenIp(Sockets, Loopback(kSteamIpPort), 2, Options);
        Log("Tunnel: Host lauscht, P2P " + std::to_string(P2P) + ", IP " + std::to_string(Ip) + " auf 127.0.0.1:" +
            std::to_string(kSteamIpPort) + ", leitet an UDP " + std::to_string(kGamePort));
    }
    else
    {
        ClientUdp = OpenUdp(kClientPort);
        if (ClientUdp == INVALID_SOCKET)
        {
            Running = false;
            return;
        }
        HSteamNetConnection Connection = 0;
        if (Role == Mode::JoinIp)
            Connection = Api.ConnectIp(Sockets, Loopback(kSteamIpPort), 2, Options);
        else
        {
            SteamNetworkingIdentity Identity{};
            Identity.Type = kIdentitySteamId;
            Identity.Size = 8;
            Identity.SteamId64 = Remote;
            Connection = Api.ConnectP2P(Sockets, Identity, kVirtualPort, 1, Options);
        }
        // Steam lehnt P2P zur eigenen SteamID mit Handle 0 ab; ein PC mit einem Konto braucht deshalb "join ip".
        if (!Connection)
        {
            Log("Tunnel: Steam hat die Verbindung abgelehnt (Handle 0)");
            closesocket(ClientUdp);
            Running = false;
            return;
        }
        Link Entry;
        Entry.Connection = Connection;
        Entry.Udp = ClientUdp;
        Links.push_back(Entry);
        Log(std::string("Tunnel: Client verbindet ueber ") + (Role == Mode::JoinIp ? "IP" : "P2P") + ", Verbindung " +
            std::to_string(Connection) + ", Spiel auf UDP " + std::to_string(kClientPort));
    }

    ULONGLONG NextStats = GetTickCount64() + 5000;
    while (Running)
    {
        Api.RunCallbacks(Sockets);
        HandleEvents(Role, ClientUdp);

        fd_set Readable;
        FD_ZERO(&Readable);
        for (const Link& Entry : Links)
            if (Entry.Udp != INVALID_SOCKET)
                FD_SET(Entry.Udp, &Readable);
        timeval Wait{0, 1000};
        if (Readable.fd_count)
            select(0, &Readable, nullptr, nullptr, &Wait);
        else
            Sleep(1);
        for (Link& Entry : Links)
            if (Entry.Udp != INVALID_SOCKET)
                Pump(Entry);

        if (GetTickCount64() >= NextStats)
        {
            NextStats += 5000;
            LogStats();
        }
    }
}
}

void TunnelCommand(const std::wstring& Arguments)
{
    if (Running)
    {
        Log("Tunnel: laeuft bereits");
        return;
    }
    if (!LoadSteamApi())
        return;
    const uint64_t Own = Api.GetSteamId(Api.User());
    Mode Role = Mode::Host;
    uint64_t Remote = 0;
    if (Arguments == L"host")
        Role = Mode::Host;
    else if (Arguments == L"join ip")
        Role = Mode::JoinIp;
    else if (Arguments == L"join self")
    {
        Role = Mode::JoinP2P;
        Remote = Own;
    }
    else if (Arguments.rfind(L"join ", 0) == 0)
    {
        Role = Mode::JoinP2P;
        Remote = _wcstoui64(Arguments.c_str() + 5, nullptr, 10);
    }
    else
    {
        Log("Tunnel: unbekannter Befehl");
        return;
    }
    // Die eigene SteamID steht nur im lokalen Log, nie im Repo.
    Log("Tunnel: eigene SteamID " + std::to_string(Own));
    Running = true;
    std::thread(Run, Role, Remote).detach();
}
}
