// opt-lootbalken, Client: Fortschrittsbalken beim Durchsuchen von Lootstellen. Progress und IsLooting von
// BP_EnvironmentObjectLootspot replizieren, aber nur der Host bringt den Wert ins Widget. Lokal zuerst: Der Client reicht den
// replizierten Wert an das native Blueprint „Update Progress Indicator“ weiter. Kein eigener Timer, keine Nachricht, kein neuer Hook,
// Loot-Vergabe und Ende bleiben beim Host. Befund: docs/GAME_ANALYSIS.md#lootbalken-beim-client-opt-lootbalken
#include <windows.h>

#include <array>
#include <atomic>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
// BP_EnvironmentObjectLootspot_C (Build 26495302, SDK).
constexpr uintptr_t SpotProgress = 0x3C0;  // double, Net
constexpr uintptr_t SpotIsLooting = 0x3C8; // bool, Net, RepNotify
constexpr uint32_t GarbageOrUnreachable = 0x30;
constexpr const char* SpotClass = "BP_EnvironmentObjectLootspot_C";
constexpr size_t MaxSpots = 16;

struct Spot
{
    uintptr_t Object = 0;
    uintptr_t Class = 0;
    int32_t Index = -1;
    double Seen = -1.0;
    double Shown = 0.0;
    ULONGLONG ChangedAt = 0;
};

// Beides nur auf dem Game Thread: RepNotify und Patches::Tick laufen dort.
std::array<Spot, MaxSpots> Spots{};
size_t SpotCount = 0;
std::atomic<uintptr_t> OnRepFunction = 0;
uintptr_t UpdateFunction = 0;
uintptr_t LastWorld = 0;
ULONGLONG NextResolve = 0;
bool Observing = false;
int Reported = 0;

void ForgetAll()
{
    OnRepFunction.store(0, std::memory_order_relaxed);
    UpdateFunction = 0;
    SpotCount = 0;
    NextResolve = 0;
}

// Der Platz im Objekt-Array beweist, dass das Objekt noch existiert; freigegebener Speicher wird nie als Lootstelle benutzt.
bool StillThere(const Spot& Entry)
{
    if (Objects::At(Entry.Index) != Entry.Object)
        return false;
    if (Memory::Read<uintptr_t>(Entry.Object + Offsets::UObject::Class).value_or(0) != Entry.Class)
        return false;
    return !(Memory::Read<uint32_t>(Entry.Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable);
}

void Remove(size_t Index)
{
    Spots[Index] = Spots[--SpotCount];
}

void Remember(uintptr_t Object)
{
    for (size_t i = 0; i < SpotCount; ++i)
        if (Spots[i].Object == Object)
            return;
    for (size_t i = SpotCount; i-- > 0;)
        if (!StillThere(Spots[i]))
            Remove(i);
    if (SpotCount == MaxSpots)
    {
        // Eine neue Stelle hat Vorrang vor der am längsten unveränderten. Fertige Stellen melden kein IsLooting 0 und bleiben sonst bis zur Freigabe.
        size_t Oldest = 0;
        for (size_t i = 1; i < SpotCount; ++i)
            if (Spots[i].ChangedAt < Spots[Oldest].ChangedAt)
                Oldest = i;
        Remove(Oldest);
    }
    const int32_t Index = Memory::Read<int32_t>(Object + Offsets::UObject::InternalIndex).value_or(-1);
    const uintptr_t Class = Memory::Read<uintptr_t>(Object + Offsets::UObject::Class).value_or(0);
    if (!Class || Objects::At(Index) != Object)
        return;
    Spots[SpotCount++] = Spot{Object, Class, Index, -1.0, 0.0, GetTickCount64()};
}

void ObserveLootingRep(uintptr_t Object, uintptr_t Function, void*)
{
    if (Function && Function == OnRepFunction.load(std::memory_order_relaxed))
        Remember(Object);
}

// Die Blueprint-Klasse wird erst mit der Karte geladen. Ihre UFunctions gelten nur für diese Welt, ForgetAll vergisst sie beim Wechsel.
bool ResolveFunctions(uintptr_t World, ULONGLONG Now)
{
    if (OnRepFunction.load(std::memory_order_relaxed))
        return true;
    if (Now < NextResolve)
        return false;
    NextResolve = Now + 1000;
    const uintptr_t State = Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0);
    if (!State || !Objects::IsA(State, "DDMissionGameState"))
        return false;
    const uintptr_t OnRep = Objects::FindObject("OnRep_IsLooting", SpotClass);
    const uintptr_t Update = Objects::FindObject("Update Progress Indicator", SpotClass);
    if (!OnRep || !Update)
        return false;
    UpdateFunction = Update;
    OnRepFunction.store(OnRep, std::memory_order_relaxed);
    return true;
}

void Show(const Spot& Entry, double Progress)
{
    struct
    {
        double Progress;
    } Params{Progress};
    Objects::ProcessEvent(Entry.Object, UpdateFunction, &Params);
}
}

void TickLootBar(NetRole Role)
{
    if (!Observing)
        Observing = AddEventObserver(&ObserveLootingRep);
    if (Role != NetRole::Client)
    {
        if (SpotCount || OnRepFunction.load(std::memory_order_relaxed))
            ForgetAll();
        return;
    }
    const uintptr_t World = CurrentWorld();
    if (World != LastWorld)
    {
        ForgetAll();
        LastWorld = World;
    }
    const ULONGLONG Now = GetTickCount64();
    if (!World || !ResolveFunctions(World, Now))
        return;

    for (size_t i = SpotCount; i-- > 0;)
    {
        Spot& Entry = Spots[i];
        if (!StillThere(Entry))
        {
            Remove(i);
            continue;
        }
        const double Progress = Memory::Read<double>(Entry.Object + SpotProgress).value_or(-1.0);
        const bool Looting = Memory::Read<uint8_t>(Entry.Object + SpotIsLooting).value_or(0) != 0;
        if (!LootBarValidProgress(Progress))
            continue;
        if (Progress != Entry.Seen)
        {
            Entry.Seen = Progress;
            Entry.ChangedAt = Now;
        }
        const double StalledFor = static_cast<double>(Now - Entry.ChangedAt) / 1000.0;
        if (LootBarNeedsUpdate(Entry.Shown, Progress))
        {
            Show(Entry, Progress);
            if (Entry.Shown == 0.0 && Reported < 8 && ++Reported)
                Core::Log::Info("Lootbalken: Client zeigt den Fortschritt einer Lootstelle aus dem replizierten Wert");
            Entry.Shown = Progress;
        }
        if (LootBarDone(Looting, Progress, Entry.Shown, StalledFor))
            Remove(i);
    }
}
}
