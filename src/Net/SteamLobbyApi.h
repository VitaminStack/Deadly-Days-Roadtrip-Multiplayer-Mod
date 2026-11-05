#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Schmale Sicht auf ISteamMatchmaking, ISteamFriends und ISteamUtils für die Lobby. Die echte Umsetzung nutzt die
// Steam-Laufzeit des Spiels, Tests eine nachgebaute. Nur auf dem Game Thread aufrufen: Dort ruft das Spiel
// SteamAPI_RunCallbacks auf, dort kommen auch die Callbacks an.
namespace Net
{
using SteamApiCall = uint64_t;

struct LobbyCreatedResult
{
    bool Ok = false;
    uint64_t Lobby = 0;
    int Result = 0;
};

struct LobbyEnteredResult
{
    bool Ok = false;
    uint64_t Lobby = 0;
    // EChatRoomEnterResponse, 1 heißt Erfolg.
    uint32_t Response = 0;
};

class SteamLobbyApi
{
public:
    virtual ~SteamLobbyApi() = default;

    virtual uint64_t OwnSteamId() = 0;
    // Nur Freunde sehen und betreten die Lobby. 0 heißt abgelehnt.
    virtual SteamApiCall CreateFriendsLobby(int MaxMembers) = 0;
    virtual SteamApiCall JoinLobby(uint64_t Lobby) = 0;
    virtual void LeaveLobby(uint64_t Lobby) = 0;
    virtual uint64_t LobbyOwner(uint64_t Lobby) = 0;
    virtual int MemberCount(uint64_t Lobby) = 0;
    // SteamIDs aller Mitglieder, eigenes Konto eingeschlossen. Leer, wenn das eigene Konto nicht in der Lobby ist.
    virtual std::vector<uint64_t> Members(uint64_t Lobby) = 0;
    virtual bool SetLobbyData(uint64_t Lobby, const std::string& Key, const std::string& Value) = 0;
    virtual std::string LobbyData(uint64_t Lobby, const std::string& Key) = 0;
    // Steam-Overlay mit der Freundesliste zum Einladen.
    virtual bool OpenInviteDialog(uint64_t Lobby) = 0;
    // nullopt, solange Steam noch arbeitet.
    virtual std::optional<LobbyCreatedResult> PollCreated(SteamApiCall Call) = 0;
    virtual std::optional<LobbyEnteredResult> PollEntered(SteamApiCall Call) = 0;
    // Lobbys, die der Spieler über Steam betreten will (Einladung angenommen, "Spiel beitreten"), seit dem letzten Aufruf.
    virtual std::vector<uint64_t> TakeJoinRequests() = 0;
    // Startparameter von Steam, etwa "+connect_lobby <id>" nach einer angenommenen Einladung bei geschlossenem Spiel.
    virtual std::string LaunchCommandLine() = 0;
};

// Steam-Laufzeit des Spiels. Nur auf dem Game Thread laden. nullptr mit Grund, solange Steam nicht bereit ist.
std::shared_ptr<SteamLobbyApi> LoadSteamLobbyApi(std::string* Reason = nullptr);

// Lobby-ID aus "+connect_lobby <id>" in einer Kommandozeile, 0 ohne.
uint64_t ParseConnectLobby(const std::string& CommandLine);
}
