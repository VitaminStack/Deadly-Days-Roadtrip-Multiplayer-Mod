#include "Game/Game.h"

#include "Core/Log.h"
#include "Game/GameThread.h"
#include "Game/Patches.h"
#include "Game/TelemetryAdapter.h"
#include "Game/BossBarAdapter.h"
#include "Game/TutorialAdapter.h"
#include "Game/WorldAdapter.h"

namespace Game
{
void Start()
{
#ifdef DDCOOP_WITH_SDK
    Core::Log::Info("Game: Offsets beim Bauen gegen das SDK geprüft");
#else
    Core::Log::Info("Game: ohne SDK gebaut, Offsets ungeprüft");
#endif
    // Vor dem ersten Takt: Die Patches greifen schon beim ersten Spawn und bei der ersten Reise.
    Patches::Install();
    GameThread::AddTick(TickWorld, "Welt");
    GameThread::AddTick(TickTutorials, "Tutorial");
    GameThread::AddTick(TickBossBar, "Boss-Leiste");
    GameThread::AddTick(TickTelemetry, "Telemetrie");
    GameThread::AddTick(Patches::Tick, "Patches");
    GameThread::Start();
}
}
