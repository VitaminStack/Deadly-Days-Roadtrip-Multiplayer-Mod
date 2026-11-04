// Echte Umsetzung über die flache C-Schnittstelle der steam_api64.dll des Spiels (Steamworks 1.64): ISteamMatchmaking
// v009, ISteamFriends v018, ISteamUtils v010, ISteamApps v009. Kein SDK, die Callback-Strukturen sind nachgebildet.
#include "Net/SteamLobbyApi.h"

#include <windows.h>

#include <cstddef>
#include <cstring>
#include <mutex>
#include <regex>

namespace Net
{
namespace
{
// Callback-Strukturen aus isteammatchmaking.h und isteamfriends.h, Windows: Packing 8.
struct LobbyCreated
{
    int32_t Result;
    uint64_t Lobby;
};
static_assert(sizeof(LobbyCreated) == 16);

struct LobbyEnter
{
    uint64_t Lobby;
    uint32_t ChatPermissions;
    bool Locked;
    uint32_t Response;
};
static_assert(offsetof(LobbyEnter, Response) == 16 && sizeof(LobbyEnter) == 24);

struct GameLobbyJoinRequested
{
    uint64_t Lobby;
    uint64_t Friend;
};
static_assert(sizeof(GameLobbyJoinRequested) == 16);

constexpr int kLobbyEnterCallback = 504;
constexpr int kLobbyCreatedCallback = 513;
constexpr int kGameLobbyJoinRequestedCallback = 333;
constexpr int kLobbyTypeFriendsOnly = 1;

// Gleicher Aufbau wie CCallbackBase aus steam_api_common.h. MSVC legt die vtable in derselben Reihenfolge an,
// SteamAPI_RegisterCallback setzt Flags und Callback-Nummer selbst.
class CallbackBase
{
public:
    virtual void Run(void* Param) = 0;
    virtual void Run(void* Param, bool IoFailure, uint64_t Call) = 0;
    virtual int GetCallbackSizeBytes() = 0;

protected:
    uint8_t CallbackFlags = 0;
    int CallbackId = 0;
};

std::mutex RequestLock;
std::vector<uint64_t> Requests;

class JoinRequestedCallback final : public CallbackBase
{
public:
    void Run(void* Param) override
    {
        const auto* Request = static_cast<const GameLobbyJoinRequested*>(Param);
        std::lock_guard Guard(RequestLock);
        Requests.push_back(Request->Lobby);
    }
    void Run(void* Param, bool, uint64_t) override { Run(Param); }
    int GetCallbackSizeBytes() override { return sizeof(GameLobbyJoinRequested); }
};

struct FlatApi
{
    void* (*Matchmaking)();
    void* (*Friends)();
    void* (*Utils)();
    void* (*Apps)();
    void* (*User)();
    uint64_t (*GetSteamId)(void*);
    uint64_t (*CreateLobby)(void*, int, int);
    uint64_t (*JoinLobby)(void*, uint64_t);
    void (*LeaveLobby)(void*, uint64_t);
    uint64_t (*GetLobbyOwner)(void*, uint64_t);
    int (*GetNumLobbyMembers)(void*, uint64_t);
    bool (*SetLobbyData)(void*, uint64_t, const char*, const char*);
    const char* (*GetLobbyData)(void*, uint64_t, const char*);
    void (*InviteDialog)(void*, uint64_t);
    bool (*IsApiCallCompleted)(void*, uint64_t, bool*);
    bool (*GetApiCallResult)(void*, uint64_t, void*, int, int, bool*);
    int (*GetLaunchCommandLine)(void*, char*, int);
    void (*RegisterCallback)(CallbackBase*, int);
    void (*UnregisterCallback)(CallbackBase*);
};

template <typename T> void Resolve(HMODULE Module, const char* Name, T& Out, std::string& Missing)
{
    Out = reinterpret_cast<T>(GetProcAddress(Module, Name));
    if (!Out && Missing.empty())
        Missing = Name;
}

class RealLobbyApi final : public SteamLobbyApi
{
public:
    explicit RealLobbyApi(const FlatApi& Functions) : Api(Functions)
    {
        Matchmaking = Api.Matchmaking();
        Friends = Api.Friends();
        Utils = Api.Utils();
        Apps = Api.Apps();
        Api.RegisterCallback(&JoinRequested, kGameLobbyJoinRequestedCallback);
    }

    ~RealLobbyApi() override { Api.UnregisterCallback(&JoinRequested); }

    bool Ready() const { return Matchmaking && Friends && Utils; }

    uint64_t OwnSteamId() override
    {
        void* User = Api.User();
        return User ? Api.GetSteamId(User) : 0;
    }

    SteamApiCall CreateFriendsLobby(int MaxMembers) override { return Api.CreateLobby(Matchmaking, kLobbyTypeFriendsOnly, MaxMembers); }
    SteamApiCall JoinLobby(uint64_t Lobby) override { return Api.JoinLobby(Matchmaking, Lobby); }
    void LeaveLobby(uint64_t Lobby) override { Api.LeaveLobby(Matchmaking, Lobby); }
    uint64_t LobbyOwner(uint64_t Lobby) override { return Api.GetLobbyOwner(Matchmaking, Lobby); }
    int MemberCount(uint64_t Lobby) override { return Api.GetNumLobbyMembers(Matchmaking, Lobby); }

    bool SetLobbyData(uint64_t Lobby, const std::string& Key, const std::string& Value) override
    {
        return Api.SetLobbyData(Matchmaking, Lobby, Key.c_str(), Value.c_str());
    }

    std::string LobbyData(uint64_t Lobby, const std::string& Key) override
    {
        const char* Value = Api.GetLobbyData(Matchmaking, Lobby, Key.c_str());
        return Value ? Value : "";
    }

    bool OpenInviteDialog(uint64_t Lobby) override
    {
        Api.InviteDialog(Friends, Lobby);
        return true;
    }

    std::optional<LobbyCreatedResult> PollCreated(SteamApiCall Call) override
    {
        LobbyCreated Data{};
        bool Failed = false;
        if (!Poll(Call, &Data, sizeof(Data), kLobbyCreatedCallback, Failed))
            return std::nullopt;
        // EResult 1 heißt OK.
        return LobbyCreatedResult{!Failed && Data.Result == 1, Data.Lobby, Data.Result};
    }

    std::optional<LobbyEnteredResult> PollEntered(SteamApiCall Call) override
    {
        LobbyEnter Data{};
        bool Failed = false;
        if (!Poll(Call, &Data, sizeof(Data), kLobbyEnterCallback, Failed))
            return std::nullopt;
        return LobbyEnteredResult{!Failed && Data.Response == 1, Data.Lobby, Data.Response};
    }

    std::vector<uint64_t> TakeJoinRequests() override
    {
        std::lock_guard Guard(RequestLock);
        std::vector<uint64_t> Result;
        Result.swap(Requests);
        return Result;
    }

    std::string LaunchCommandLine() override
    {
        if (!Apps || !Api.GetLaunchCommandLine)
            return {};
        char Buffer[1024]{};
        const int Length = Api.GetLaunchCommandLine(Apps, Buffer, sizeof(Buffer));
        return Length > 0 ? std::string(Buffer, strnlen(Buffer, sizeof(Buffer))) : std::string();
    }

private:
    // Wahr, sobald Steam fertig ist. Failed meldet einen Fehler von Steam oder falsche Daten.
    bool Poll(SteamApiCall Call, void* Data, int Size, int Expected, bool& Failed)
    {
        Failed = false;
        if (!Call)
        {
            Failed = true;
            return true;
        }
        if (!Api.IsApiCallCompleted(Utils, Call, &Failed))
            return false;
        if (!Api.GetApiCallResult(Utils, Call, Data, Size, Expected, &Failed))
            Failed = true;
        return true;
    }

    FlatApi Api;
    void* Matchmaking = nullptr;
    void* Friends = nullptr;
    void* Utils = nullptr;
    void* Apps = nullptr;
    JoinRequestedCallback JoinRequested;
};
}

std::shared_ptr<SteamLobbyApi> LoadSteamLobbyApi(std::string* Reason)
{
    auto Fail = [Reason](std::string Text) -> std::shared_ptr<SteamLobbyApi> {
        if (Reason)
            *Reason = std::move(Text);
        return nullptr;
    };
    const HMODULE Module = GetModuleHandleW(L"steam_api64.dll");
    if (!Module)
        return Fail("steam_api64.dll ist nicht geladen");
    FlatApi Api{};
    std::string Missing;
    Resolve(Module, "SteamAPI_SteamMatchmaking_v009", Api.Matchmaking, Missing);
    Resolve(Module, "SteamAPI_SteamFriends_v018", Api.Friends, Missing);
    Resolve(Module, "SteamAPI_SteamUtils_v010", Api.Utils, Missing);
    Resolve(Module, "SteamAPI_SteamApps_v009", Api.Apps, Missing);
    Resolve(Module, "SteamAPI_SteamUser_v023", Api.User, Missing);
    Resolve(Module, "SteamAPI_ISteamUser_GetSteamID", Api.GetSteamId, Missing);
    Resolve(Module, "SteamAPI_ISteamMatchmaking_CreateLobby", Api.CreateLobby, Missing);
    Resolve(Module, "SteamAPI_ISteamMatchmaking_JoinLobby", Api.JoinLobby, Missing);
    Resolve(Module, "SteamAPI_ISteamMatchmaking_LeaveLobby", Api.LeaveLobby, Missing);
    Resolve(Module, "SteamAPI_ISteamMatchmaking_GetLobbyOwner", Api.GetLobbyOwner, Missing);
    Resolve(Module, "SteamAPI_ISteamMatchmaking_GetNumLobbyMembers", Api.GetNumLobbyMembers, Missing);
    Resolve(Module, "SteamAPI_ISteamMatchmaking_SetLobbyData", Api.SetLobbyData, Missing);
    Resolve(Module, "SteamAPI_ISteamMatchmaking_GetLobbyData", Api.GetLobbyData, Missing);
    Resolve(Module, "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog", Api.InviteDialog, Missing);
    Resolve(Module, "SteamAPI_ISteamUtils_IsAPICallCompleted", Api.IsApiCallCompleted, Missing);
    Resolve(Module, "SteamAPI_ISteamUtils_GetAPICallResult", Api.GetApiCallResult, Missing);
    Resolve(Module, "SteamAPI_ISteamApps_GetLaunchCommandLine", Api.GetLaunchCommandLine, Missing);
    Resolve(Module, "SteamAPI_RegisterCallback", Api.RegisterCallback, Missing);
    Resolve(Module, "SteamAPI_UnregisterCallback", Api.UnregisterCallback, Missing);
    if (!Missing.empty())
        return Fail("Steam-Export fehlt: " + Missing);
    auto Loaded = std::make_shared<RealLobbyApi>(Api);
    if (!Loaded->Ready())
        return Fail("Steam-Schnittstellen fehlen, Steam ist im Spiel noch nicht initialisiert");
    return Loaded;
}

uint64_t ParseConnectLobby(const std::string& CommandLine)
{
    static const std::regex Pattern(R"(\+connect_lobby\s+(\d{1,20}))");
    std::smatch Match;
    if (!std::regex_search(CommandLine, Match, Pattern))
        return 0;
    try
    {
        return std::stoull(Match[1].str());
    }
    catch (...)
    {
        return 0;
    }
}
}
