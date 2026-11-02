// Host: Befehle, die Gegner und Rundenende von aussen ausloesen.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kWorldGameInstance = 0x228;

bool IsClientWorld(uintptr_t World)
{
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    return Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
}
}

// Die direkt geladene Mission startet keine Gegner, fuer Trefferversuche werden sie hier angestossen.
void ForceEnemyWave()
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    const uintptr_t CanSpawn = FindObject("SetCanSpawn", "EnemySpawner");
    const uintptr_t AutoSpawn = FindObject("ToggleAutoSpawning", "EnemySpawner");
    const uintptr_t Wave = FindObject("ForceSpawnWave", "EnemySpawner");
    if (!CanSpawn || !AutoSpawn || !Wave)
    {
        Log("Gegner: Funktionen nicht gefunden");
        return;
    }
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        bool IsSpawner = false;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class && !IsSpawner; Class = Read<uintptr_t>(Class + 0x40))
            IsSpawner = NameOf(Class) == "EnemySpawner";
        if (!IsSpawner)
            continue;
        const auto ProcessEvent = Read<ProcessEventFn*>(Object)[kProcessEventIndex];
        uint32_t Enable = 1;
        ProcessEvent(Object, CanSpawn, &Enable);
        Enable = 1;
        ProcessEvent(Object, AutoSpawn, &Enable);
        int32_t Enemies = 10;
        ProcessEvent(Object, Wave, &Enemies);
        Log("Gegner: Welle angestossen");
    }
}

// Beendet die Mission wie der abfahrende Bus, um den Rueckweg zu pruefen.
void FinishMission()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    // FinishMission ist kein Client-RPC. Ein erneuter lokaler Aufruf waere kein UI-Sync,
    // sondern wuerde den unvollstaendigen Run-/Persistenzpfad des Clients betreten.
    if (IsClientWorld(World))
    {
        Log("Missionsende: Client-Aufruf abgelehnt; Host-Phase muss separat synchronisiert werden");
        return;
    }
    const uintptr_t GameState = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    const uintptr_t Function = FindObject("FinishMission", "DDMissionGameState");
    if (!GameState || !Function)
    {
        Log("Missionsende: nicht gefunden");
        return;
    }
    uint32_t IsLoading = 0;
    Read<ProcessEventFn*>(GameState)[kProcessEventIndex](GameState, Function, &IsLoading);
    Log("Missionsende: ausgeloest");
}

// Beendet den Run ueber die GameInstance, Grund wie ERoundChangeReason.
void EndRound(uint8_t Reason)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    if (IsClientWorld(World))
    {
        Log("Rundenende: Client-Aufruf abgelehnt; keine lokale Run-Persistenz ausfuehren");
        return;
    }
    const uintptr_t GameInstance = World ? Read<uintptr_t>(World + kWorldGameInstance) : 0;
    const uintptr_t Function = FindObject("TriggerEndRound", "DDMissionGameInstance");
    if (!GameInstance || !Function)
    {
        Log("Rundenende: nicht gefunden");
        return;
    }
    uint32_t Params = Reason;
    Read<ProcessEventFn*>(GameInstance)[kProcessEventIndex](GameInstance, Function, &Params);
    Log("Rundenende: ausgeloest");
}
}
