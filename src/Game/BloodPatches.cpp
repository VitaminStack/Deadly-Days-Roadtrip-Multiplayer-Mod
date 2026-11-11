// opt-blut, Client: Blut und Fleisch beim Tod eines Gegners. Der Host spielt sie in UEnemyDyingAbility über
// AEnemyBase::ActivateDeathVFX, die Ability läuft nur bei ihm. Der Host sendet aber für jeden Tod den nativen zuverlässigen
// Multicast DDCharacterBase.Multi_OnDie, der beim Client heute in einem leeren Blueprint-Ereignis endet. Lokal zuerst: Kommt er
// für einen Gegner an, ruft der Client dessen natives ActivateDeathVFX selbst auf. Keine Nachricht, kein neuer Hook, Schaden, Tod
// und Drops bleiben beim Host. Befund: docs/GAME_ANALYSIS.md#blut-beim-client-opt-blut
#include <windows.h>

#include <array>
#include <atomic>

#include "Core/Log.h"
#include "Game/BloodPolicy.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uint32_t GarbageOrUnreachable = 0x30;
constexpr size_t RecentSize = 64;

struct Recent
{
    uintptr_t Object = 0;
    ULONGLONG Ms = 0;
};

// Beide UFunctions gehören nativen Klassen und bleiben bis zum Spielende gültig, darum nur einmal auflösen.
std::atomic<uintptr_t> DieFunction = 0;   // DDCharacterBase.Multi_OnDie
std::atomic<uintptr_t> VfxFunction = 0;   // EnemyBase.ActivateDeathVFX
std::atomic<bool> ClientActive = false;
std::atomic<DWORD> GameThreadId = 0;
std::array<std::atomic<EnemyDeathListener>, 2> Listeners{};

// Ab hier nur Game Thread: der Beobachter prüft die Thread-Id, bevor er etwas davon anfasst.
std::array<Recent, RecentSize> RecentDeaths{};
size_t RecentNext = 0;
int PlayedThisTick = 0;
uintptr_t LastWorld = 0;
ULONGLONG NextResolve = 0;
bool Observing = false;
bool ReportedNoSlot = false;
bool ReportedFirst = false;
// Zähler für den Spieltest, alle 10 Sekunden ins Log, wenn sich etwas geändert hat.
uint64_t Seen = 0, Played = 0, Repeated = 0, Skipped = 0, Logged = 0;
ULONGLONG NextLog = 0;

bool Alive(uintptr_t Object)
{
    const int32_t Index = Memory::Read<int32_t>(Object + Offsets::UObject::InternalIndex).value_or(-1);
    if (Objects::At(Index) != Object)
        return false;
    return !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable);
}

// Letzte Effekte für dieses Objekt; trägt es mit Now neu ein. Wiederholt zählt, ob der Gegner in dieser Welt schon einmal starb
// (Pool), solange er im kleinen Ringpuffer steht.
ULONGLONG Remember(uintptr_t Object, ULONGLONG Now, bool& SeenBefore)
{
    SeenBefore = false;
    ULONGLONG Last = 0;
    for (auto& Entry : RecentDeaths)
        if (Entry.Object == Object)
        {
            SeenBefore = true;
            Last = Entry.Ms;
            Entry.Object = 0;
        }
    RecentDeaths[RecentNext] = Recent{Object, Now};
    RecentNext = (RecentNext + 1) % RecentSize;
    return Last;
}

void ObserveDeath(uintptr_t Object, uintptr_t Function, void*)
{
    // Läuft nach jedem ProcessEvent: der schnelle Weg ist ein Vergleich.
    if (!Function || Function != DieFunction.load(std::memory_order_relaxed))
        return;
    if (!ClientActive.load(std::memory_order_relaxed) || GetCurrentThreadId() != GameThreadId.load(std::memory_order_relaxed))
        return;
    if (!Object || !Objects::IsA(Object, "EnemyBase"))
        return;
    ++Seen;
    const ULONGLONG Now = GetTickCount64();
    bool SeenBefore = false;
    const ULONGLONG Last = Remember(Object, Now, SeenBefore);
    if (SeenBefore)
        ++Repeated;
    const uintptr_t Vfx = VfxFunction.load(std::memory_order_relaxed);
    if (!Vfx || !BloodShouldPlay(true, true, Alive(Object), BloodRecent(Last, Now), PlayedThisTick))
    {
        ++Skipped;
        return;
    }
    ++PlayedThisTick;
    ++Played;
    // ActivateDeathVFX hat keine Parameter; ein genullter Puffer statt nullptr, falls ProcessEvent ihn trotzdem liest.
    alignas(16) uint8_t NoParams[16]{};
    Objects::ProcessEvent(Object, Vfx, NoParams);
    for (auto& Slot : Listeners)
        if (const EnemyDeathListener Listener = Slot.load(std::memory_order_acquire))
            Listener(Object);
    if (!ReportedFirst)
    {
        ReportedFirst = true;
        Core::Log::Info("Blut: Client spielt die nativen Tod-Effekte eines Gegners aus Multi_OnDie");
    }
}

bool ResolveFunctions(ULONGLONG Now)
{
    if (DieFunction.load(std::memory_order_relaxed))
        return true;
    if (Now < NextResolve)
        return false;
    NextResolve = Now + 5000;
    const uintptr_t Vfx = Objects::FindObject("ActivateDeathVFX", "EnemyBase");
    const uintptr_t Die = Objects::FindObject("Multi_OnDie", "DDCharacterBase");
    if (!Vfx || !Die)
        return false;
    VfxFunction.store(Vfx, std::memory_order_relaxed);
    DieFunction.store(Die, std::memory_order_relaxed);
    return true;
}

void LogCounters(ULONGLONG Now)
{
    if (Now < NextLog || Seen == Logged)
        return;
    NextLog = Now + 10000;
    Logged = Seen;
    Core::Log::Info("Blut: Client sah {} Gegnertode (Multi_OnDie), spielte {} Tod-Effekte, {} uebersprungen, {} erneut gestorben",
                    Seen, Played, Skipped, Repeated);
}
}

bool AddEnemyDeathListener(EnemyDeathListener Listener)
{
    if (!Listener)
        return false;
    for (auto& Slot : Listeners)
    {
        EnemyDeathListener Empty = nullptr;
        if (Slot.load() == Listener || Slot.compare_exchange_strong(Empty, Listener))
            return true;
    }
    return false;
}

void TickBlood(NetRole Role)
{
    GameThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
    PlayedThisTick = 0;
    if (!Observing)
    {
        Observing = AddEventObserver(&ObserveDeath);
        if (!Observing && !ReportedNoSlot)
        {
            ReportedNoSlot = true;
            Core::Log::Warn("Blut: kein freier ProcessEvent-Beobachter, Tod-Effekte beim Client bleiben aus");
        }
    }
    const bool Client = Role == NetRole::Client;
    const ULONGLONG Now = GetTickCount64();
    if (Client && !ResolveFunctions(Now))
    {
        ClientActive.store(false, std::memory_order_relaxed);
        return;
    }
    ClientActive.store(Client, std::memory_order_relaxed);
    const uintptr_t World = CurrentWorld();
    if (World != LastWorld)
    {
        RecentDeaths.fill(Recent{});
        LastWorld = World;
    }
    if (Client)
        LogCounters(Now);
}
}
