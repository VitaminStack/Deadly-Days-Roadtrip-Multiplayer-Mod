// live-protokoll: Zustandszeile und Testermarken im Log, siehe Sync/SessionLogState.h. Nur lesen, nur Game Thread.
#include "Sync/SessionLogState.h"
#include "Sync/Sync.h"
#include "Core/Log.h"
#include "Game/PauseAdapter.h"
#include "Game/RewardAdapter.h"
#include "Game/SessionProbe.h"
#include "Game/WorldAdapter.h"
#include "Net/Net.h"

#include <windows.h>

namespace Sync
{
namespace
{
std::function<uint32_t()> MarkSource;

SessionLog::State Read(Net::Transport* Transport, const std::optional<Game::WorldInfo>& World)
{
    SessionLog::State S;
    if (World)
    {
        S.Role = std::string(Game::ToString(World->Role));
        S.Map = World->Map;
        S.Players = World->Players;
    }
    if (const auto Time = Game::Pause::ReadWorldTime())
    {
        S.Dilation = Time->Dilation;
        S.Pauser = Time->Pauser;
    }
    S.TimeRequests = Game::Pause::ActiveTimeRequests();
    S.Scrap = Game::Rewards::LocalScrap();
    if (const auto Fuel = Game::Probe::ReadFuel())
    {
        S.FuelCurrent = Fuel->Current;
        S.FuelMax = Fuel->Max;
    }
    if (Transport)
    {
        const auto Stats = Transport->Stats();
        S.PingMs = Stats.PingMs;
        S.Connections = Stats.Connections;
        S.ControlRefused = Stats.ControlRefused;
    }
    return S;
}
}

void BindMarkSource(std::function<uint32_t()> Source)
{
    MarkSource = std::move(Source);
}

void TickSessionLog(Net::Transport* Transport)
{
    static bool Ever = false;
    static uint64_t Last = 0, Marks = 0;
    static std::string Map;
    const uint32_t Pressed = MarkSource ? MarkSource() : 0;
    const auto World = Game::ReadWorld();
    const std::string Now = World ? World->Map : std::string{};
    const bool Changed = Ever && Now != Map;
    const uint64_t Tick = GetTickCount64();
    if (!SessionLog::Due(Ever, Last, Tick, Changed, Pressed != 0))
        return;
    Ever = true;
    Last = Tick;
    Map = Now;
    const std::string Line = SessionLog::Format(Read(Transport, World));
    if (Pressed)
    {
        Marks += Pressed;
        Core::Log::Info("MARKE {}: Tester hat F9 gedrückt, {}", Marks, Line);
    }
    else
        Core::Log::Info("Zustand ({}): {}", Changed ? "Weltwechsel" : "Takt", Line);
}
}
