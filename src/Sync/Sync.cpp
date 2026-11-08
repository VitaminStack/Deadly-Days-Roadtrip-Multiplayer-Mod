#include "Sync/Sync.h"
#include "Sync/ControlDispatch.h"
#include "Sync/DevState.h"
#include "Sync/MovementState.h"
#include "Game/GameThread.h"
#include "Net/Net.h"
#include <chrono>

namespace Sync
{
void ConfigureDesync(const Core::Config& Config, DiagnosticSink Diagnostics);

namespace
{
std::function<uint32_t()> DevSource;

// Dev-Tab: Der Host fuehrt eigene Knoepfe direkt aus, Anfragen des Clients nach Eingang fuer dessen Figur.
void TickDev(Net::Transport& Transport)
{
    const uint32_t Pressed = DevSource ? DevSource() : 0;
    const bool Host = Transport.Stats().IsHost;
    if (Host)
    {
        for (const auto& Message : Transport.TakeControlMessages(Dev::Request))
            if (const auto Action = Dev::Decode(Message.Value.Payload))
                Game::RunDevAction(*Action, 1);
    }
    for (const uint8_t Id : {uint8_t(1), uint8_t(2), uint8_t(3)})
    {
        const auto Action = static_cast<Game::DevAction>(Id);
        if (!(Pressed & (1u << Id)))
            continue;
        if (Host)
        {
            Game::RunDevAction(Action, 0);
            continue;
        }
        const auto Peers = Transport.ControlPeers();
        static uint64_t Revision = 0;
        if (Peers.size() == 1)
            Transport.SendControl(Peers[0], {1, 1, ++Revision, Dev::Request, Dev::Encode(Action)});
    }
}

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

void BindDevSource(std::function<uint32_t()> Source)
{
    DevSource = std::move(Source);
}

void Start(const Core::Config& Config, DiagnosticSink Diagnostics)
{
    ConfigureDesync(Config, std::move(Diagnostics));
    Game::GameThread::AddTick([]
    {
        using Clock = std::chrono::steady_clock;
        const auto Transport = Net::CurrentTransport();
        if (Transport)
        {
            TickMovement(*Transport);
            TickDev(*Transport);
        }
        static auto Next = Clock::now();
        if (Clock::now() < Next) return;
        Next = Clock::now() + std::chrono::milliseconds(250);
        const auto Messages = Transport ? Transport->TakeControlMessages() : std::vector<Net::ControlMessage>{};
        DispatchControls(Messages, TickProgress, TickDesync, TickReward, TickMap);
    });
}
}
