#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "Core/Dispatcher.h"

namespace UI
{
enum class SessionState { Unavailable, Offline, Connecting, Lobby, Playing, Failed };
enum class SessionRole { None, Host, Client };
enum class LobbyAction { Create, Join, Invite, Leave };

struct OverlayStatus
{
    SessionState State = SessionState::Unavailable;
    SessionRole Role = SessionRole::None;
    std::string Message = "Steam-Lobby und Transport sind noch nicht angebunden.";
    uint64_t LobbyId = 0;
    unsigned Players = 0;
    unsigned PingMs = 0;
    bool HasPing = false;
    bool CanCreate = false;
    bool CanJoin = false;
    bool CanInvite = false;
    bool CanLeave = false;
};

struct LobbyRequest
{
    LobbyAction Action;
    uint64_t LobbyId = 0;
};

struct OverlaySnapshot
{
    OverlayStatus Status;
    bool HasBackend = false;
    bool Pending = false;
    std::string Diagnostics = "Desync: wartet auf Kontrollkanal/Missionswelt";
};

// No SDK or engine pointers. Producer publishes copied values; actions only run in Dispatcher::Drain.
class OverlayModel
{
public:
    using ActionHandler = std::function<void(LobbyRequest)>;
    explicit OverlayModel(Core::Dispatcher& Dispatcher = Core::GameThread());
    void Bind(ActionHandler Handler);
    void Publish(OverlayStatus Status);
    void PublishDiagnostics(std::string Text);
    OverlaySnapshot Snapshot() const;
    bool Submit(LobbyRequest Request);
    static bool CanSubmit(const OverlaySnapshot& Snapshot, LobbyRequest Request);
    static bool ParseLobbyId(const std::wstring& Text, uint64_t& Value);

private:
    struct SharedState;
    std::shared_ptr<SharedState> State;
    Core::Dispatcher& Dispatcher;
};
}
