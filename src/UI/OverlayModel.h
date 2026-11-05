#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

// Airdrop-Auswahl (airdrop-auswahl). Knöpfe nur bei Selectable, sonst nur der Hinweis.
struct RewardPrompt
{
    uint64_t Session = 0, Revision = 0;
    std::vector<std::string> Choices;
    std::string Status;
    bool Selectable = false;
    bool operator==(const RewardPrompt&) const = default;
};

struct OverlaySnapshot
{
    OverlayStatus Status;
    bool HasBackend = false;
    bool Pending = false;
    std::string Diagnostics = "Desync: wartet auf Kontrollkanal/Missionswelt";
    std::optional<RewardPrompt> Reward;
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
    // nullopt schließt die Auswahl.
    void PublishReward(std::optional<RewardPrompt> Prompt);
    // Läuft im Game Thread (Dispatcher::Drain) mit Sitzung, Revision und Index der gewählten Choice.
    using RewardHandler = std::function<void(uint64_t Session, uint64_t Revision, uint32_t Index)>;
    void BindReward(RewardHandler Handler);
    // Vom Overlay-Thread. Falsch, wenn keine wählbare Auswahl mit diesem Index offen ist.
    bool ChooseReward(uint32_t Index);
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
