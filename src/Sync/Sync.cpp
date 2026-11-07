#include "Sync/Sync.h"
#include "Sync/ControlDispatch.h"
#include "Sync/MovementState.h"
#include "Game/GameThread.h"
#include "Net/Net.h"
#include <chrono>

namespace Sync
{
void ConfigureDesync(const Core::Config& Config, DiagnosticSink Diagnostics);

namespace
{
// Jeder Takt statt alle 250 ms: Die Dodge-Richtung muss beim Host ankommen, solange seine Aktivierung wartet.
void TickMovement(Net::Transport& Transport)
{
    const bool Host = Transport.Stats().IsHost;
    if (Host)
    {
        for (const auto& Message : Transport.TakeControlMessages(Movement::DashInput))
            if (const auto Value = Movement::Decode(Message.Value.Payload))
                Game::ReceiveDashInput(*Value);
        return;
    }
    const auto Inputs = Game::TakeDashInputs();
    const auto Peers = Inputs.empty() ? std::vector<uint64_t>{} : Transport.ControlPeers();
    if (Peers.size() != 1)
        return;
    static uint64_t Revision = 0;
    for (const auto& Value : Inputs)
    {
        const auto Payload = Movement::Encode(Value);
        if (!Payload.empty())
            Transport.SendControl(Peers[0], {1, 1, ++Revision, Movement::DashInput, Payload});
    }
}
}

void Start(const Core::Config& Config, DiagnosticSink Diagnostics)
{
    ConfigureDesync(Config, std::move(Diagnostics));
    Game::GameThread::AddTick([]
    {
        using Clock = std::chrono::steady_clock;
        const auto Transport = Net::CurrentTransport();
        if (Transport)
            TickMovement(*Transport);
        static auto Next = Clock::now();
        if (Clock::now() < Next) return;
        Next = Clock::now() + std::chrono::milliseconds(250);
        const auto Messages = Transport ? Transport->TakeControlMessages() : std::vector<Net::ControlMessage>{};
        DispatchControls(Messages, TickProgress, TickDesync, TickReward, TickMap);
    });
}
}
