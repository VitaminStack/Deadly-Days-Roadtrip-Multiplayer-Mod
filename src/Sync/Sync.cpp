#include "Sync/Sync.h"
#include "Sync/ControlDispatch.h"
#include "Game/GameThread.h"
#include "Net/Net.h"
#include <chrono>

namespace Sync
{
void ConfigureDesync(const Core::Config& Config, DiagnosticSink Diagnostics);
void Start(const Core::Config& Config, DiagnosticSink Diagnostics)
{
    ConfigureDesync(Config, std::move(Diagnostics));
    Game::GameThread::AddTick([]
    {
        using Clock = std::chrono::steady_clock;
        static auto Next = Clock::now();
        if (Clock::now() < Next) return;
        Next = Clock::now() + std::chrono::milliseconds(250);
        const auto Transport = Net::CurrentTransport();
        const auto Messages = Transport ? Transport->TakeControlMessages() : std::vector<Net::ControlMessage>{};
        DispatchControls(Messages, TickProgress, TickDesync, TickReward, TickMap);
    });
}
}
