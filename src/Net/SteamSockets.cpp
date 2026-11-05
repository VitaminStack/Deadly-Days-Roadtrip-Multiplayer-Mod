// Echte Umsetzung über die flache C-Schnittstelle der steam_api64.dll, die das Spiel mitbringt (Steamworks 1.64,
// ISteamNetworkingSockets v012). Ein Steamworks-SDK ist nicht nötig, die wenigen Strukturen sind nachgebildet und per
// static_assert auf ihre Größe geprüft. Herkunft: Spike E, research/spikes/a-connect/Tunnel.cpp.
#include "Net/SteamSockets.h"

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <regex>

namespace Net
{
namespace
{
// Typen aus steamnetworkingtypes.h, nur die benutzten Felder. Windows: Packing 8, IP-Adresse gepackt auf 1.
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
    uint32_t ListenSocket;
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
    uint32_t Connection;
    SteamNetConnectionInfo Info;
    int32_t OldState;
};
static_assert(offsetof(SteamNetConnectionStatusChanged, Info) == 8);

struct SteamNetworkingMessage
{
    void* Data;
    int32_t Size;
    uint32_t Connection;
};

constexpr int32_t kIdentitySteamId = 16;
constexpr int32_t kConfigInt32 = 1;
constexpr int32_t kConfigPtr = 5;
constexpr int32_t kConfigIpAllowWithoutAuth = 23;
constexpr int32_t kConfigStatusChangedCallback = 201;
constexpr int kSendUnreliableNoNagle = 1;

struct FlatApi
{
    void* (*Sockets)();
    void* (*Utils)();
    void* (*User)();
    uint64_t (*GetSteamId)(void*);
    void (*InitRelay)(void*);
    uint32_t (*ListenP2P)(void*, int, int, const SteamNetworkingConfigValue*);
    uint32_t (*ListenIp)(void*, const SteamNetworkingIPAddr&, int, const SteamNetworkingConfigValue*);
    uint32_t (*ConnectP2P)(void*, const SteamNetworkingIdentity&, int, int, const SteamNetworkingConfigValue*);
    uint32_t (*ConnectIp)(void*, const SteamNetworkingIPAddr&, int, const SteamNetworkingConfigValue*);
    int (*Accept)(void*, uint32_t);
    bool (*Close)(void*, uint32_t, int, const char*, bool);
    bool (*CloseListen)(void*, uint32_t);
    int (*Send)(void*, uint32_t, const void*, uint32_t, int, int64_t*);
    int (*Receive)(void*, uint32_t, SteamNetworkingMessage**, int);
    void (*RunCallbacks)(void*);
    int (*RealTimeStatus)(void*, uint32_t, void*, int, void*);
    int (*DetailedStatus)(void*, uint32_t, char*, int);
    void (*Release)(SteamNetworkingMessage*);
};

// Der Callback der Steam-Laufzeit ist eine freie Funktion, deshalb liegt die Warteschlange global.
std::mutex ChangeLock;
std::vector<SteamStatusChange> Changes;
std::atomic<bool> Loaded = false;

void __cdecl OnStatusChanged(SteamNetConnectionStatusChanged* Change)
{
    SteamStatusChange Entry;
    Entry.Connection = Change->Connection;
    Entry.ListenSocket = Change->Info.ListenSocket;
    Entry.State = static_cast<SteamConnectionState>(Change->Info.State);
    Entry.OldState = static_cast<SteamConnectionState>(Change->OldState);
    Entry.EndReason = Change->Info.EndReason;
    Entry.EndDebug.assign(Change->Info.EndDebug, strnlen(Change->Info.EndDebug, sizeof(Change->Info.EndDebug)));
    // Steam füllt IdentityRemote bei P2P nach der Authentifizierung. Alles andere (IP ohne Auth, ungültig) bleibt 0.
    const SteamNetworkingIdentity& Remote = Change->Info.IdentityRemote;
    if (Remote.Type == kIdentitySteamId && Remote.Size == 8 && IsIndividualSteamId(Remote.SteamId64))
        Entry.RemoteSteamId = Remote.SteamId64;
    std::lock_guard Guard(ChangeLock);
    Changes.push_back(std::move(Entry));
}

template <typename T> bool Resolve(HMODULE Module, const char* Name, T& Out, std::string& Missing)
{
    Out = reinterpret_cast<T>(GetProcAddress(Module, Name));
    if (!Out && Missing.empty())
        Missing = Name;
    return Out != nullptr;
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

class RealSockets final : public SteamSockets
{
public:
    RealSockets(const FlatApi& Functions, void* Interface) : Api(Functions), Self(Interface)
    {
        Options[0].Value = kConfigStatusChangedCallback;
        Options[0].DataType = kConfigPtr;
        Options[0].Ptr = reinterpret_cast<void*>(&OnStatusChanged);
        // Nur für die Verbindung über 127.0.0.1 auf einem PC, P2P authentifiziert Steam selbst.
        Options[1].Value = kConfigIpAllowWithoutAuth;
        Options[1].DataType = kConfigInt32;
        Options[1].Int32 = 1;
    }

    ~RealSockets() override { Loaded = false; }

    uint64_t OwnSteamId() override
    {
        void* User = Api.User();
        return User ? Api.GetSteamId(User) : 0;
    }

    void InitRelayNetworkAccess() override
    {
        if (void* Utils = Api.Utils())
            Api.InitRelay(Utils);
    }

    SteamListenSocket ListenP2P(int VirtualPort) override { return Api.ListenP2P(Self, VirtualPort, 1, Options); }

    SteamListenSocket ListenLoopback(uint16_t Port) override { return Api.ListenIp(Self, Loopback(Port), 2, Options); }

    SteamConnection ConnectP2P(uint64_t SteamId, int VirtualPort) override
    {
        SteamNetworkingIdentity Identity{};
        Identity.Type = kIdentitySteamId;
        Identity.Size = 8;
        Identity.SteamId64 = SteamId;
        return Api.ConnectP2P(Self, Identity, VirtualPort, 1, Options);
    }

    SteamConnection ConnectLoopback(uint16_t Port) override { return Api.ConnectIp(Self, Loopback(Port), 2, Options); }

    bool Accept(SteamConnection Connection) override { return Api.Accept(Self, Connection) == 1; }

    void Close(SteamConnection Connection, int32_t Reason) override { Api.Close(Self, Connection, Reason, "ddcoop", false); }

    void CloseListen(SteamListenSocket Socket) override { Api.CloseListen(Self, Socket); }

    bool Send(SteamConnection Connection, std::span<const uint8_t> Data) override
    {
        return Api.Send(Self, Connection, Data.data(), static_cast<uint32_t>(Data.size()), kSendUnreliableNoNagle, nullptr) == 1;
    }

    bool SendReliable(SteamConnection Connection, std::span<const uint8_t> Data) override
    {
        // k_nSteamNetworkingSend_Reliable | k_nSteamNetworkingSend_NoNagle.
        return Api.Send(Self, Connection, Data.data(), static_cast<uint32_t>(Data.size()), 8 | 1, nullptr) == 1;
    }

    int Receive(SteamConnection Connection, int Max, const std::function<void(std::span<const uint8_t>)>& Sink) override
    {
        SteamNetworkingMessage* Messages[64];
        const int Count = Api.Receive(Self, Connection, Messages, Max < 64 ? Max : 64);
        for (int Index = 0; Index < Count; ++Index)
        {
            Sink({static_cast<const uint8_t*>(Messages[Index]->Data), static_cast<size_t>(Messages[Index]->Size)});
            Api.Release(Messages[Index]);
        }
        return Count < 0 ? 0 : Count;
    }

    void RunCallbacks() override { Api.RunCallbacks(Self); }

    std::vector<SteamStatusChange> TakeStatusChanges() override
    {
        std::vector<SteamStatusChange> Result;
        std::lock_guard Guard(ChangeLock);
        Result.swap(Changes);
        return Result;
    }

    SteamQuality Quality(SteamConnection Connection) override
    {
        // SteamNetConnectionRealTimeStatus_t: Zustand, Ping, lokale Qualität, ...
        uint8_t Status[256]{};
        SteamQuality Result;
        if (Api.RealTimeStatus(Self, Connection, Status, 0, nullptr) == 1)
        {
            memcpy(&Result.PingMs, Status + 4, 4);
            memcpy(&Result.Quality, Status + 8, 4);
        }
        return Result;
    }

    std::string DetailedStatus(SteamConnection Connection) override
    {
        std::string Text(4096, '\0');
        if (Api.DetailedStatus(Self, Connection, Text.data(), static_cast<int>(Text.size())) < 0)
            return {};
        Text.resize(strnlen(Text.c_str(), Text.size()));
        return Text;
    }

private:
    FlatApi Api;
    void* Self;
    SteamNetworkingConfigValue Options[2]{};
};
}

std::shared_ptr<SteamSockets> LoadSteamSockets(std::string* Reason)
{
    auto Fail = [Reason](std::string Text) -> std::shared_ptr<SteamSockets> {
        if (Reason)
            *Reason = std::move(Text);
        return nullptr;
    };
    const HMODULE Module = GetModuleHandleW(L"steam_api64.dll");
    if (!Module)
        return Fail("steam_api64.dll ist nicht geladen");
    FlatApi Api{};
    std::string Missing;
    Resolve(Module, "SteamAPI_SteamNetworkingSockets_SteamAPI_v012", Api.Sockets, Missing);
    Resolve(Module, "SteamAPI_SteamNetworkingUtils_SteamAPI_v004", Api.Utils, Missing);
    Resolve(Module, "SteamAPI_SteamUser_v023", Api.User, Missing);
    Resolve(Module, "SteamAPI_ISteamUser_GetSteamID", Api.GetSteamId, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingUtils_InitRelayNetworkAccess", Api.InitRelay, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_CreateListenSocketP2P", Api.ListenP2P, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_CreateListenSocketIP", Api.ListenIp, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_ConnectP2P", Api.ConnectP2P, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_ConnectByIPAddress", Api.ConnectIp, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_AcceptConnection", Api.Accept, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_CloseConnection", Api.Close, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_CloseListenSocket", Api.CloseListen, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_SendMessageToConnection", Api.Send, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection", Api.Receive, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_RunCallbacks", Api.RunCallbacks, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_GetConnectionRealTimeStatus", Api.RealTimeStatus, Missing);
    Resolve(Module, "SteamAPI_ISteamNetworkingSockets_GetDetailedConnectionStatus", Api.DetailedStatus, Missing);
    Resolve(Module, "SteamAPI_SteamNetworkingMessage_t_Release", Api.Release, Missing);
    if (!Missing.empty())
        return Fail("Steam-Export fehlt: " + Missing);
    void* Interface = Api.Sockets();
    if (!Interface)
        return Fail("ISteamNetworkingSockets fehlt, Steam ist im Spiel noch nicht initialisiert");
    if (Loaded.exchange(true))
        return Fail("Steam Networking ist schon in Benutzung");
    return std::make_shared<RealSockets>(Api, Interface);
}

bool IsIndividualSteamId(uint64_t SteamId)
{
    return (SteamId >> 56) == 1 && ((SteamId >> 52) & 0xF) == 1 && (SteamId & 0xFFFFFFFFull) != 0;
}

std::string MaskAddresses(const std::string& Text)
{
    static const std::regex Ipv4(R"(\b(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})\b)");
    std::string Result;
    auto Last = Text.cbegin();
    for (std::sregex_iterator It(Text.begin(), Text.end(), Ipv4), End; It != End; ++It)
    {
        Result.append(Last, (*It)[0].first);
        Result += (*It)[0] == "127.0.0.1" ? "127.0.0.1" : "<ip>";
        Last = (*It)[0].second;
    }
    Result.append(Last, Text.cend());
    return Result;
}

std::string RouteSummary(const std::string& DetailedStatus)
{
    // Steam schreibt bei Relays etwa "Primary router: fra#364 (...)". Der Name vor # ist der Standort.
    static const std::regex Router(R"(Primary router:\s*([A-Za-z]+)#)");
    std::smatch Match;
    if (std::regex_search(DetailedStatus, Match, Router))
        return "Relay " + Match[1].str();
    if (DetailedStatus.empty())
        return "unbekannt";
    return "ohne Relay";
}
}
