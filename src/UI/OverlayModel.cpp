#include "UI/OverlayModel.h"

#include <limits>
#include <mutex>
#include <utility>

namespace UI
{
struct OverlayModel::SharedState
{
    std::mutex Mutex;
    OverlayStatus Status;
    ActionHandler Handler;
    uint64_t Revision = 0;
    uint64_t Ticket = 0;
    bool Pending = false;
    uint32_t DevMask = 0;
    std::string Diagnostics = "Desync: wartet auf Kontrollkanal/Missionswelt";
};

OverlayModel::OverlayModel(Core::Dispatcher& InDispatcher)
    : State(std::make_shared<SharedState>()), Dispatcher(InDispatcher) {}

void OverlayModel::Bind(ActionHandler Handler)
{
    const std::lock_guard Lock(State->Mutex);
    State->Handler = std::move(Handler);
    State->Pending = false;
    ++State->Revision;
    ++State->Ticket;
}

void OverlayModel::Publish(OverlayStatus Status)
{
    const std::lock_guard Lock(State->Mutex);
    // Statistics/message refreshes must not starve a queued action. Session changes cancel it at Drain.
    if (State->Status.State != Status.State || State->Status.Role != Status.Role ||
        State->Status.LobbyId != Status.LobbyId)
        ++State->Revision;
    State->Status = std::move(Status);
}

OverlaySnapshot OverlayModel::Snapshot() const
{
    const std::lock_guard Lock(State->Mutex);
    return { State->Status, static_cast<bool>(State->Handler), State->Pending, State->Diagnostics };
}

void OverlayModel::PublishDiagnostics(std::string Text)
{
    const std::lock_guard Lock(State->Mutex);
    State->Diagnostics = Text.substr(0, 512);
}

bool OverlayModel::CanSubmit(const OverlaySnapshot& View, LobbyRequest Request)
{
    if (!View.HasBackend || View.Pending)
        return false;
    const auto& Status = View.Status;
    const bool Idle = Status.State == SessionState::Offline || Status.State == SessionState::Failed;
    switch (Request.Action)
    {
    case LobbyAction::Create: return Idle && Status.CanCreate;
    case LobbyAction::Join: return Idle && Status.CanJoin && Request.LobbyId != 0;
    case LobbyAction::Invite:
        return Status.State == SessionState::Lobby && Status.Role == SessionRole::Host &&
            Status.LobbyId != 0 && Status.CanInvite;
    case LobbyAction::Leave:
        return (Status.State == SessionState::Lobby || Status.State == SessionState::Playing ||
            Status.State == SessionState::Connecting) && Status.CanLeave;
    }
    return false;
}

bool OverlayModel::CanDev(const OverlaySnapshot& View)
{
    return View.Status.State == SessionState::Playing &&
        (View.Status.Role == SessionRole::Host || View.Status.Role == SessionRole::Client);
}

bool OverlayModel::RequestDev(DevButton Button)
{
    const std::lock_guard Lock(State->Mutex);
    if (!CanDev({ State->Status, static_cast<bool>(State->Handler), State->Pending }))
        return false;
    State->DevMask |= 1u << static_cast<uint8_t>(Button);
    return true;
}

uint32_t OverlayModel::TakeDevRequests()
{
    const std::lock_guard Lock(State->Mutex);
    return std::exchange(State->DevMask, 0u);
}

bool OverlayModel::Submit(LobbyRequest Request)
{
    uint64_t Revision;
    uint64_t Ticket;
    {
        const std::lock_guard Lock(State->Mutex);
        if (!CanSubmit({ State->Status, static_cast<bool>(State->Handler), State->Pending }, Request))
            return false;
        State->Pending = true;
        Revision = State->Revision;
        Ticket = ++State->Ticket;
    }
    const auto Shared = State;
    try
    {
        Dispatcher.Post([Shared, Revision, Ticket, Request]
        {
            ActionHandler Handler;
            {
                const std::lock_guard Lock(Shared->Mutex);
                if (Shared->Ticket != Ticket || !Shared->Pending)
                    return;
                if (Shared->Revision != Revision ||
                    !CanSubmit({ Shared->Status, static_cast<bool>(Shared->Handler), false }, Request))
                {
                    Shared->Pending = false;
                    return;
                }
                try { Handler = Shared->Handler; }
                catch (...)
                {
                    Shared->Pending = false;
                    throw;
                }
            }
            // Never hold the model mutex while the backend runs: it may publish its new state.
            const auto Complete = [&]
            {
                const std::lock_guard Lock(Shared->Mutex);
                if (Shared->Ticket == Ticket)
                    Shared->Pending = false;
            };
            try { Handler(Request); }
            catch (...)
            {
                Complete();
                throw;
            }
            Complete();
        });
    }
    catch (...)
    {
        const std::lock_guard Lock(State->Mutex);
        if (State->Ticket == Ticket)
            State->Pending = false;
        throw;
    }
    return true;
}

bool OverlayModel::ParseLobbyId(const std::wstring& Text, uint64_t& Value)
{
    Value = 0;
    if (Text.empty() || Text.size() > 20)
        return false;
    uint64_t Parsed = 0;
    for (const wchar_t Digit : Text)
    {
        if (Digit < L'0' || Digit > L'9')
            return false;
        const auto Number = static_cast<uint64_t>(Digit - L'0');
        if (Parsed > (std::numeric_limits<uint64_t>::max() - Number) / 10)
            return false;
        Parsed = Parsed * 10 + Number;
    }
    if (Parsed == 0)
        return false;
    Value = Parsed;
    return true;
}
}
