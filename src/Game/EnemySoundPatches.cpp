// opt-gegner-sound, siehe EnemySoundAdapter.h. Kein Code-Patch und kein Hook: ein Beobachter am vorhandenen ProcessEvent-Hook,
// ein Takt und Aufrufe aus DamageNumberPatches.cpp. Schaden, Treffer und Angriff entscheidet weiter der Host.
// Befund: docs/GAME_ANALYSIS.md#gegner-sounds-beim-client-opt-gegner-sound
#include <windows.h>

#include <atomic>
#include <unordered_map>
#include <vector>

#include "Core/Log.h"
#include "Game/EnemySoundAdapter.h"
#include "Game/EnemySoundPolicy.h"
#include "Game/FmodSound.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

namespace Game
{
namespace
{
constexpr Reflection::FieldSpec UseImpactSounds{"/Script/DDSurvivors", "Projectile", "bUseImpactSounds", "BoolProperty", 1};
constexpr Reflection::FieldSpec HitImpactSoundEvent{"/Script/DDSurvivors", "Projectile", "HitImpactSoundEvent", "ObjectProperty", 8};
constexpr Reflection::FieldSpec ReplicatedMovementMode{"/Script/Engine", "Character", "ReplicatedMovementMode", "ByteProperty", 1};
// Die Klasse, deren Sprungfähigkeit (GA_SmallJumpAttack) beim Host `StandardEnemyAttack` spielt. Der Sound steht als Literal im
// Graphen der Fähigkeit, die beim Client nicht geladen ist, darum hier Klasse und Pfad.
constexpr std::string_view JumpingEnemyClass = "BP_StandardEnemy_C";
constexpr std::string_view AttackSoundPath = "/Game/FMOD/Events/SFX/Character_FX/Zombies/StandardEnemyAttack.StandardEnemyAttack";
// Alle geladenen Projektilklassen mit Einschlagsound nutzen dasselbe Ereignis, der Client nimmt es von der Grundklasse.
constexpr std::string_view ProjectileDefaults = "Default__BP_Projectile_C";
constexpr uintptr_t ActorRole = 0x168; // AActor.Role, 3 = Authority (wie AirdropAdapter.cpp)
constexpr ULONGLONG ListRefreshMs = 250;

struct Jumper
{
    Objects::Ref Enemy;
    uint8_t Mode = 0;
    bool Seen = false;
    ULONGLONG LastSound = 0;
};

// Vom Beobachter gelesen (läuft nach jedem ProcessEvent, auch außerhalb des Game Threads).
std::atomic<int32_t> BeginPlayName = -1;
std::atomic<int32_t> ImpactFlagOffset = -1;
std::atomic<bool> ClientActive = false;
std::atomic<DWORD> GameThreadId = 0;

// Ab hier nur Game Thread.
int32_t ImpactEventOffset = -1, ModeOffset = -1;
Objects::Ref ImpactEvent, AttackEvent;
std::vector<Jumper> Jumpers;
std::unordered_map<uintptr_t, bool> JumperClasses;
uintptr_t LastWorld = 0;
ULONGLONG NextRefresh = 0, NextResolve = 0, NextLog = 0;
bool Observing = false, ReportedNoSlot = false;
int PlayedThisTick = 0;
struct Counters
{
    uint64_t Impacts = 0, ImpactsAtTarget = 0, ImpactsSilent = 0, Muted = 0, Jumps = 0, Attacks = 0, AttacksSilent = 0;
    bool operator==(const Counters&) const = default;
} Count, Logged;

// Replizierte Projektile des Clients lösen ihren Einschlagsound nur aus, wenn sie den Gegner vor dem Abbau selbst berühren.
// Den Sound spielt der Client für die Treffer des Hosts, der eigene wäre ein Doppel.
void MuteProjectile(uintptr_t Projectile)
{
    const int32_t Offset = ImpactFlagOffset.load(std::memory_order_relaxed);
    if (Offset < 0 || Memory::Read<uint8_t>(Projectile + ActorRole).value_or(3) == 3)
        return;
    if (!Memory::Read<uint8_t>(Projectile + Offset).value_or(0))
        return;
    const uint8_t Off = 0;
    if (Memory::WriteBytes(Projectile + Offset, &Off, 1))
        ++Count.Muted;
}

void ObserveBeginPlay(uintptr_t Object, uintptr_t Function, void*)
{
    // Schneller Weg: ein Namensvergleich. ReceiveBeginPlay hat je Blueprint-Klasse eine eigene UFunction, aber denselben Namen.
    if (!Function || !ClientActive.load(std::memory_order_relaxed))
        return;
    const int32_t Name = BeginPlayName.load(std::memory_order_relaxed);
    if (Name < 0 || Memory::Read<int32_t>(Function + Offsets::UObject::Name).value_or(-1) != Name)
        return;
    if (GetCurrentThreadId() != GameThreadId.load(std::memory_order_relaxed) || !Object || !Objects::IsA(Object, "Projectile"))
        return;
    MuteProjectile(Object);
}

bool Resolve(ULONGLONG Now)
{
    if (ImpactEventOffset >= 0 && ModeOffset >= 0 && BeginPlayName.load(std::memory_order_relaxed) >= 0)
        return true;
    if (Now < NextResolve)
        return false;
    NextResolve = Now + 5000;
    const auto Flag = Reflection::Property(UseImpactSounds);
    const auto Event = Reflection::Property(HitImpactSoundEvent);
    const auto Mode = Reflection::Property(ReplicatedMovementMode);
    const uintptr_t BeginPlay = Objects::FindObject("ReceiveBeginPlay", "Actor");
    if (!Flag || Flag->BoolMask != 0xFF || !Event || !Mode || !BeginPlay)
        return false;
    ImpactFlagOffset.store(Flag->Offset, std::memory_order_relaxed);
    ImpactEventOffset = Event->Offset;
    ModeOffset = Mode->Offset;
    BeginPlayName.store(Memory::Read<int32_t>(BeginPlay + Offsets::UObject::Name).value_or(-1), std::memory_order_relaxed);
    return BeginPlayName.load(std::memory_order_relaxed) >= 0;
}

uintptr_t ImpactSoundEvent()
{
    if (Objects::IsLive(ImpactEvent))
        return ImpactEvent.Object;
    const uintptr_t Defaults = ImpactEventOffset >= 0 ? Objects::FindNamed(ProjectileDefaults) : 0;
    const uintptr_t Event = Defaults ? Memory::ReadPointer(Defaults + ImpactEventOffset).value_or(0) : 0;
    ImpactEvent = Event ? Objects::RefTo(Event) : Objects::Ref{};
    return ImpactEvent.Object;
}

uintptr_t AttackSoundEvent(ULONGLONG Now)
{
    static ULONGLONG NextLoad = 0;
    if (Objects::IsLive(AttackEvent))
        return AttackEvent.Object;
    if (Now < NextLoad)
        return 0;
    NextLoad = Now + 5000;
    const uintptr_t Event = Objects::LoadAsset(AttackSoundPath);
    AttackEvent = Event ? Objects::RefTo(Event) : Objects::Ref{};
    return AttackEvent.Object;
}

// Liste der Sprunggegner der Welt neu aufbauen, bekannte behalten ihren beobachteten Zustand. Dabei auch Projektile
// stummschalten, die schon vor dem Beobachter da waren (Beitritt).
void RefreshJumpers(uintptr_t World)
{
    const uintptr_t Level = Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0);
    std::vector<Jumper> Next;
    Next.reserve(Jumpers.size() + 16);
    Objects::ForEachLevelActor(Level, [&Next](uintptr_t Actor)
    {
        const uintptr_t Class = Memory::ReadPointer(Actor + Offsets::UObject::Class).value_or(0);
        if (!Class)
            return;
        auto Known = JumperClasses.find(Class);
        if (Known == JumperClasses.end())
        {
            if (Objects::IsA(Actor, "Projectile"))
                MuteProjectile(Actor);
            Known = JumperClasses.emplace(Class, Objects::IsA(Actor, JumpingEnemyClass)).first;
        }
        if (!Known->second)
            return;
        for (const auto& Old : Jumpers)
            if (Old.Enemy.Object == Actor && Objects::IsLive(Old.Enemy))
            {
                Next.push_back(Old);
                return;
            }
        if (const Objects::Ref Enemy = Objects::RefTo(Actor); Enemy.Object)
            Next.push_back({Enemy});
    });
    Jumpers.swap(Next);
}

void LogCounters(ULONGLONG Now)
{
    if (Now < NextLog || Count == Logged)
        return;
    NextLog = Now + 10000;
    Logged = Count;
    Core::Log::Info("Gegnersound Client: Einschlagsounds {} (am Ziel {}, ohne Sound {}), eigene Projektilsounds abgeschaltet {}, "
                    "Sprünge gesehen {}, Angriffssounds {} (ohne Sound {})", Count.Impacts, Count.ImpactsAtTarget,
                    Count.ImpactsSilent, Count.Muted, Count.Jumps, Count.Attacks, Count.AttacksSilent);
}
}

bool ProjectileUsesImpactSound(uintptr_t Causer)
{
    const int32_t Flag = ImpactFlagOffset.load(std::memory_order_relaxed);
    if (!Causer || Flag < 0 || ImpactEventOffset < 0 || !Objects::IsA(Causer, "Projectile"))
        return false;
    return Memory::Read<uint8_t>(Causer + Flag).value_or(0) != 0 && Memory::ReadPointer(Causer + ImpactEventOffset).value_or(0) != 0;
}

void PlayEnemyImpactSound(uintptr_t Target, uintptr_t WorldContext, float X, float Y, float Z)
{
    const uintptr_t Event = ImpactSoundEvent();
    if (PlayedThisTick >= EnemySoundsPerTick || !Event)
    {
        ++Count.ImpactsSilent;
        return;
    }
    if (Target && FmodSound::PlayAtActor(Target, Event))
        ++Count.ImpactsAtTarget;
    else if (!FmodSound::PlayAtLocation(WorldContext, Event, X, Y, Z))
    {
        ++Count.ImpactsSilent;
        return;
    }
    ++PlayedThisTick;
    ++Count.Impacts;
}

namespace Patches
{
void TickEnemySounds(NetRole Role)
{
    GameThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
    PlayedThisTick = 0;
    const ULONGLONG Now = GetTickCount64();
    const uintptr_t World = CurrentWorld();
    if (World != LastWorld)
    {
        Jumpers.clear();
        JumperClasses.clear();
        ImpactEvent = AttackEvent = {};
        LastWorld = World;
        NextRefresh = 0;
    }
    // Der Host braucht nur die Feld-Offsets (ProjectileUsesImpactSound), der Client zusätzlich Beobachter und Takt.
    const bool Resolved = World && Role != NetRole::Standalone && Resolve(Now);
    const bool Client = Resolved && Role == NetRole::Client;
    if (Client && !Observing)
    {
        Observing = AddEventObserver(&ObserveBeginPlay);
        if (!Observing && !ReportedNoSlot)
        {
            ReportedNoSlot = true;
            Core::Log::Warn("Gegnersound: kein freier ProcessEvent-Beobachter, eigene Projektilsounds des Clients bleiben an");
        }
    }
    ClientActive.store(Client, std::memory_order_relaxed);
    if (!Client)
        return;
    if (Now >= NextRefresh)
    {
        NextRefresh = Now + ListRefreshMs;
        RefreshJumpers(World);
    }
    for (auto& Entry : Jumpers)
    {
        if (!Objects::IsLive(Entry.Enemy))
            continue;
        const auto Mode = Memory::Read<uint8_t>(Entry.Enemy.Object + ModeOffset);
        if (!Mode)
            continue;
        const bool Jump = EnemyJumpStarted(Entry.Seen, Entry.Mode, *Mode);
        Entry.Seen = true;
        Entry.Mode = *Mode;
        if (!Jump)
            continue;
        ++Count.Jumps;
        if (!EnemyAttackSoundDue(true, true, Entry.LastSound, Now, PlayedThisTick))
            continue;
        if (!FmodSound::PlayAtActor(Entry.Enemy.Object, AttackSoundEvent(Now)))
        {
            ++Count.AttacksSilent;
            continue;
        }
        Entry.LastSound = Now;
        ++PlayedThisTick;
        ++Count.Attacks;
    }
    LogCounters(Now);
}
}
}
