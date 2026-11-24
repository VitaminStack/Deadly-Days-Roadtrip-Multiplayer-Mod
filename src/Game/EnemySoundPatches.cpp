// opt-gegner-sound, siehe EnemySoundAdapter.h. Kein Code-Patch, kein Hook und kein ProcessEvent-Beobachter: ein Takt und Aufrufe
// aus DamageNumberPatches.cpp. Schaden, Treffer und Angriff entscheidet weiter der Host.
// Befund: docs/archive/GAME_ANALYSIS.md#gegner-sounds-beim-client-opt-gegner-sound
#include <windows.h>

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
constexpr ULONGLONG ListRefreshMs = 250;

struct Jumper
{
    Objects::Ref Enemy;
    uint8_t Mode = 0;
    bool Seen = false;
    ULONGLONG LastSound = 0;
};

// Nur Game Thread.
int32_t ImpactFlagOffset = -1;
int32_t ImpactEventOffset = -1, ModeOffset = -1;
Objects::Ref ImpactEvent, AttackEvent;
std::vector<Jumper> Jumpers;
enum class ActorKind : uint8_t { Other, Jumper, Projectile };
std::unordered_map<uintptr_t, ActorKind> ActorKinds;
uintptr_t LastWorld = 0;
ULONGLONG NextRefresh = 0, NextResolve = 0, NextLog = 0;
int PlayedThisTick = 0;
struct Counters
{
    uint64_t Impacts = 0, ImpactsAtTarget = 0, ImpactsSilent = 0, Muted = 0, Jumps = 0, Attacks = 0, AttacksSilent = 0;
    bool operator==(const Counters&) const = default;
} Count, Logged;

// Replizierte Projektile des Clients lösen ihren Einschlagsound nur aus, wenn sie den Gegner vor dem Abbau selbst berühren.
// Den Sound spielt der Client für die Treffer des Hosts, der eigene wäre ein Doppel. Der Client hat keine eigenen Projektile
// (kein Pool, alle repliziert), darum ohne Blick auf die Rolle. Abgeschaltet wird beim Abgleich der Actor-Liste alle 250 ms; ein
// Beobachter für ReceiveBeginPlay sah im Spieltest kein einziges Projektil und ist wieder entfernt.
void MuteProjectile(uintptr_t Projectile)
{
    const int32_t Offset = ImpactFlagOffset;
    if (Offset < 0 || !Memory::Read<uint8_t>(Projectile + Offset).value_or(0))
        return;
    const uint8_t Off = 0;
    if (Memory::WriteBytes(Projectile + Offset, &Off, 1))
        ++Count.Muted;
}

bool Resolve(ULONGLONG Now)
{
    if (ImpactFlagOffset >= 0 && ImpactEventOffset >= 0 && ModeOffset >= 0)
        return true;
    if (Now < NextResolve)
        return false;
    NextResolve = Now + 5000;
    const auto Flag = Reflection::Property(UseImpactSounds);
    const auto Event = Reflection::Property(HitImpactSoundEvent);
    const auto Mode = Reflection::Property(ReplicatedMovementMode);
    if (!Flag || Flag->BoolMask != 0xFF || !Event || !Mode)
        return false;
    ImpactFlagOffset = Flag->Offset;
    ImpactEventOffset = Event->Offset;
    ModeOffset = Mode->Offset;
    return true;
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

// Liste der Sprunggegner der Welt neu aufbauen, bekannte behalten ihren beobachteten Zustand. Dabei die Projektile stummschalten.
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
        auto Known = ActorKinds.find(Class);
        if (Known == ActorKinds.end())
            Known = ActorKinds.emplace(Class, Objects::IsA(Actor, JumpingEnemyClass) ? ActorKind::Jumper
                                              : Objects::IsA(Actor, "Projectile")    ? ActorKind::Projectile
                                                                                      : ActorKind::Other).first;
        if (Known->second == ActorKind::Projectile)
            MuteProjectile(Actor);
        if (Known->second != ActorKind::Jumper)
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
    const int32_t Flag = ImpactFlagOffset;
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
    PlayedThisTick = 0;
    const ULONGLONG Now = GetTickCount64();
    const uintptr_t World = CurrentWorld();
    if (World != LastWorld)
    {
        Jumpers.clear();
        ActorKinds.clear();
        ImpactEvent = AttackEvent = {};
        LastWorld = World;
        NextRefresh = 0;
    }
    // Der Host braucht nur die Feld-Offsets (ProjectileUsesImpactSound), der Client zusätzlich den Takt.
    const bool Resolved = World && Role != NetRole::Standalone && Resolve(Now);
    const bool Client = Resolved && Role == NetRole::Client;
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
