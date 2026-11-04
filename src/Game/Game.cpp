#include "Game/Game.h"

#include "Core/Log.h"
#include "Game/GameThread.h"
#include "Game/TelemetryAdapter.h"
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
    GameThread::AddTick(TickWorld);
    GameThread::AddTick(TickTutorials);
    GameThread::AddTick(TickTelemetry);
    GameThread::Start();
}
}
