// opt-waffen-sound: Schuss- und Nachladesound beim Client.
//
// Schuss: Beim Host startet AHands::FireIfPossible nach jedem Schuss den FireWeaponSoundEvent der WeaponData direkt
// (PlayEventAtLocation am Ort der Hände). Die Feuerschleife läuft nur beim Host, beim Client sind CurrentRangedWeapons der Hände
// leer, der Sound startet dort nie. Die Waffen-Actors replizieren aber, und der unzuverlässige Multicast
// AWeapon.TriggerWeaponShotFX (Mündungsfeuer, Hülsen) kommt je Schuss beim Client an. Lokal zuerst: Der Client startet bei jedem
// ankommenden Multicast denselben Sound selbst, aus der WeaponData zur Klasse des Waffen-Actors (WeaponActorToSpawn). Keine
// Nachricht, kein Hook, kein Code-Patch.
//
// Nachladen: läuft in derselben Feuerschleife, ohne repliziertes Signal. Der Host meldet jeden Aufruf von AHands.PlayReloadSound
// (0xB00, WeaponSoundAdapter.h), der Client ruft die native Funktion an seinen eigenen Händen auf.
//
// Zielwahl, Schuss, Munition und Schaden bleiben beim Host. Befund: docs/GAME_ANALYSIS.md#waffensounds-beim-client-opt-waffen-sound
#include <windows.h>

#include <array>
#include <atomic>
#include <cstring>
#include <unordered_map>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/CodeCave.h"
#include "Game/Memory.h"
#include "Game/NetworkObjects.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"
#include "Game/WeaponSoundAdapter.h"

#ifdef DDCOOP_WITH_SDK
#pragma warning(push, 0)
#include <SDK/Engine_classes.hpp>
#include <SDK/DDSurvivors_classes.hpp>
#include <SDK/DDSurvivors_parameters.hpp>
#include <SDK/FMODStudio_parameters.hpp>
#pragma warning(pop)
static_assert(offsetof(SDK::UWeaponData, WeaponActorToSpawn) == 0xB0);
static_assert(offsetof(SDK::UWeaponData, FireWeaponSoundEvent) == 0xB8);
static_assert(offsetof(SDK::UWeaponData, ReloadWeaponSoundEvent) == 0xC0);
static_assert(offsetof(SDK::AActor, RootComponent) == 0x1B8);
static_assert(offsetof(SDK::Params::FMODBlueprintStatics_PlayEventAtLocation, Location) == 0x10);
static_assert(offsetof(SDK::Params::FMODBlueprintStatics_PlayEventAtLocation, bAutoPlay) == 0x70);
static_assert(sizeof(SDK::Params::FMODBlueprintStatics_PlayEventAtLocation) == 0x80);
static_assert(sizeof(SDK::Params::Hands_PlayReloadSound) == 0x8);
#endif

namespace Game
{
namespace
{
// Build 26495302, SDK.
constexpr uintptr_t DataWeaponActor = 0xB0;     // UWeaponData.WeaponActorToSpawn (TSubclassOf<AActor>)
constexpr uintptr_t DataFireSound = 0xB8;       // UWeaponData.FireWeaponSoundEvent (UFMODEvent*)
constexpr uintptr_t DataReloadSound = 0xC0;     // UWeaponData.ReloadWeaponSoundEvent (UFMODEvent*)
constexpr uintptr_t ActorRootComponent = 0x1B8; // AActor.RootComponent
constexpr uintptr_t ComponentToWorld = 0x1F0;   // USceneComponent.ComponentToWorld (FTransform, 0x60)
constexpr size_t TransformSize = 0x60;
constexpr uint32_t GarbageOrUnreachable = 0x30;
constexpr size_t MaxWeaponData = 64;
constexpr size_t MaxRecentWeapons = 16;

// UFMODBlueprintStatics::PlayEventAtLocation(WorldContextObject, Event, const FTransform& Location, bAutoPlay)
struct alignas(16) PlayEventParams
{
    uintptr_t WorldContextObject = 0;
    uintptr_t Event = 0;
    uint8_t Location[TransformSize]{};
    bool AutoPlay = true;
    uint8_t Pad[7]{};
    uintptr_t ReturnValue = 0;
};
static_assert(sizeof(PlayEventParams) == 0x80);
static_assert(offsetof(PlayEventParams, Location) == 0x10);
static_assert(offsetof(PlayEventParams, AutoPlay) == 0x70);

struct WeaponSound
{
    uintptr_t ActorClass = 0;
    Objects::Ref Data; // WeaponData; die Sounds werden je Ereignis frisch daraus gelesen
};

struct RecentShot
{
    uintptr_t Weapon = 0;
    ULONGLONG At = 0;
};

struct PendingReload
{
    Objects::Ref Hands;
    std::string Override;
};

// AHands::PlayReloadSound, der native Event-Wrapper (Hands, Override). Ohne Blueprint-Überschreibung springt er direkt in die
// _Implementation und läuft nicht über ProcessEvent (gemessen: 0 Aufrufe im Beobachter bei 18 Nachladesounds des Hosts). Aufrufer
// ist die Nachladelogik der Waffeninstanz (0x5A45F5C) aus der Feuerschleife der Hände.
constexpr uintptr_t ReloadSoundEvent = 0x5707870;
using ReloadSoundFn = void(__fastcall*)(uintptr_t Hands, uintptr_t Override);
ReloadSoundFn OriginalReloadSound = nullptr;

// Alles nur auf dem Game Thread: ProcessEvent, Patches::Tick, der Sync-Takt und die Feuerschleife laufen dort.
std::atomic<uintptr_t> ShotFunction = 0;     // Client
std::atomic<bool> ClientActive = false, HostActive = false;
uintptr_t ReloadFunction = 0, PlayFunction = 0, PlayDefaults = 0;
std::array<WeaponSound, MaxWeaponData> Sounds{};
size_t SoundCount = 0;
std::array<RecentShot, MaxRecentWeapons> Recent{};
size_t RecentNext = 0;
std::array<PendingReload, MaxReloadSounds> Pending{};
size_t PendingCount = 0;
uintptr_t LastWorld = 0;
ULONGLONG NextResolve = 0, NextScan = 0, NextSummary = 0;
bool Observing = false, ObserverMissing = false;
int Reported = 0, Unknown = 0, ReloadReported = 0;
// Zähler für den Beleg im Log, je Minute bei Änderung.
struct
{
    uint32_t Shots = 0, Played = 0, Repeated = 0, Melee = 0, Silent = 0;
    uint32_t ReloadSeen = 0, ReloadSent = 0, ReloadNoGuid = 0, ReloadOverflow = 0;
    uint32_t ReloadReceived = 0, ReloadPlayed = 0, ReloadNoHands = 0, ReloadNoSound = 0;
    uint32_t Sum() const
    {
        return Shots + ReloadSeen + ReloadReceived;
    }
} Count;
uint32_t Logged = 0;

void ForgetAll()
{
    ShotFunction.store(0, std::memory_order_relaxed);
    ReloadFunction = PlayFunction = PlayDefaults = 0;
    SoundCount = PendingCount = 0;
    Recent = {};
    NextResolve = NextScan = 0;
}

bool Alive(uintptr_t Object)
{
    const int32_t Index = Memory::Read<int32_t>(Object + Offsets::UObject::InternalIndex).value_or(-1);
    if (Objects::At(Index) != Object)
        return false;
    return !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable);
}

// Alle WeaponData mit Waffen-Actor-Klasse, einmal über das Objekt-Array. Verglichen wird nur der Klassenzeiger, kein Name je Objekt.
void ScanWeaponData()
{
    SoundCount = 0;
    const uintptr_t DataClass = Objects::FindObject("WeaponData", "/Script/DDSurvivors");
    if (!DataClass)
        return;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total && SoundCount < MaxWeaponData; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object || Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0) != DataClass)
            continue;
        const uintptr_t ActorClass = Memory::ReadPointer(Object + DataWeaponActor).value_or(0);
        if (ActorClass && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable))
            Sounds[SoundCount++] = WeaponSound{ActorClass, Objects::RefTo(Object)};
    }
    Core::Log::Info("Waffensound: {} WeaponData mit Waffen-Actor gefunden", SoundCount);
}

// Sucht höchstens alle 5 s neu, Daten können nachgeladen sein.
bool RescanWeaponData(ULONGLONG Now)
{
    if (Now < NextScan)
        return false;
    NextScan = Now + 5000;
    ScanWeaponData();
    return true;
}

// FireWeaponSoundEvent zur Klasse des Waffen-Actors, 0 ohne Datensatz oder ohne Sound (etwa Flammenwerfer).
uintptr_t FireSoundFor(uintptr_t Weapon, ULONGLONG Now)
{
    const uintptr_t Class = Memory::ReadPointer(Weapon + Offsets::UObject::Class).value_or(0);
    if (!Class)
        return 0;
    for (int Pass = 0; Pass < 2; ++Pass)
    {
        for (size_t i = 0; i < SoundCount; ++i)
            if (Sounds[i].ActorClass == Class)
                return Objects::IsLive(Sounds[i].Data) ? Memory::ReadPointer(Sounds[i].Data.Object + DataFireSound).value_or(0) : 0;
        if (Pass || !RescanWeaponData(Now))
            break;
    }
    if (Unknown < 3 && ++Unknown)
        Core::Log::Info("Waffensound: keine WeaponData zur Waffenklasse {}", Memory::NameOf(Class));
    return 0;
}

// ReloadWeaponSoundEvent mit diesem Objektnamen, 0 wenn keine WeaponData ihn führt.
uintptr_t ReloadSoundNamed(const std::string& Name, ULONGLONG Now)
{
    for (int Pass = 0; Pass < 2; ++Pass)
    {
        for (size_t i = 0; i < SoundCount; ++i)
        {
            const uintptr_t Event = Objects::IsLive(Sounds[i].Data) ? Memory::ReadPointer(Sounds[i].Data.Object + DataReloadSound).value_or(0) : 0;
            if (Event && Memory::NameOf(Event) == Name)
                return Event;
        }
        if (Pass || !RescanWeaponData(Now))
            break;
    }
    return 0;
}

// Zeit seit dem letzten Schuss derselben Waffe in ms, sehr groß beim ersten.
ULONGLONG SinceLastShot(uintptr_t Weapon, ULONGLONG Now)
{
    for (RecentShot& Entry : Recent)
        if (Entry.Weapon == Weapon)
        {
            const ULONGLONG Since = Now - Entry.At;
            Entry.At = Now;
            return Since;
        }
    Recent[RecentNext] = RecentShot{Weapon, Now};
    RecentNext = (RecentNext + 1) % MaxRecentWeapons;
    return ~0ull;
}

void PlayShot(uintptr_t Weapon)
{
    const ULONGLONG Now = GetTickCount64();
    if (!Alive(Weapon) || !Objects::IsA(Weapon, "Weapon"))
        return;
    ++Count.Shots;
    if (Objects::IsA(Weapon, "MeleeWeapon"))
    {
        ++Count.Melee; // Nahkampf feuert nicht über FireIfPossible, sein Klang ist nicht untersucht.
        return;
    }
    if (Patches::WeaponShotRepeated(SinceLastShot(Weapon, Now)))
    {
        ++Count.Repeated;
        return;
    }
    const uintptr_t Event = FireSoundFor(Weapon, Now);
    const uintptr_t Root = Memory::ReadPointer(Weapon + ActorRootComponent).value_or(0);
    PlayEventParams Params{Weapon, Event};
    if (!Event || !Root || !PlayFunction || !PlayDefaults || !Memory::ReadBytes(Root + ComponentToWorld, Params.Location, TransformSize))
    {
        ++Count.Silent;
        return;
    }
    Objects::ProcessEvent(PlayDefaults, PlayFunction, &Params);
    ++Count.Played;
    if (Reported < 3 && ++Reported)
        Core::Log::Info("Waffensound: Client startet den Schusssound lokal ({}, {})",
                        Memory::NameOf(Memory::ReadPointer(Weapon + Offsets::UObject::Class).value_or(0)), Memory::NameOf(Event));
}

// Host: Aufruf von PlayReloadSound(Override) an Händen merken, gesendet wird im Sync-Takt.
void NoteReload(uintptr_t Hands, uintptr_t Override)
{
    if (!Alive(Hands) || !Objects::IsA(Hands, "Hands"))
        return;
    ++Count.ReloadSeen;
    if (PendingCount == MaxReloadSounds)
    {
        ++Count.ReloadOverflow;
        return;
    }
    std::string Name = Override && Alive(Override) ? Memory::NameOf(Override) : std::string();
    if (Name.size() > MaxReloadSoundName)
        Name.clear();
    if (ReloadReported < 3 && ++ReloadReported)
        Core::Log::Info("Waffensound: Host meldet Nachladesound ({})", Name.empty() ? "Standard" : Name);
    Pending[PendingCount++] = PendingReload{Objects::RefTo(Hands), std::move(Name)};
}

void __fastcall ReloadSoundHook(uintptr_t Hands, uintptr_t Override)
{
    if (HostActive.load(std::memory_order_relaxed))
        NoteReload(Hands, Override);
    OriginalReloadSound(Hands, Override);
}

// Nach dem ursprünglichen ProcessEvent: Mündungsfeuer und Hülsen des Spiels sind dann schon gestartet.
void Observe(uintptr_t Object, uintptr_t Function, void*)
{
    if (Function && ClientActive.load(std::memory_order_relaxed) && Function == ShotFunction.load(std::memory_order_relaxed))
        PlayShot(Object);
}

bool CheckReloadSound()
{
    return Code::Expect(Patches::At(ReloadSoundEvent),
                        {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xFA},
                        "Hands.PlayReloadSound");
}

bool ApplyReloadSound(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(Patches::At(ReloadSoundEvent)), reinterpret_cast<void*>(&ReloadSoundHook),
                                reinterpret_cast<void**>(&OriginalReloadSound));
}

constexpr uintptr_t ReloadSoundSites[] = {ReloadSoundEvent};
const Patches::Patch List[] = {
    {"Nachladesound des Hosts für den Client melden", &CheckReloadSound, &ApplyReloadSound, ReloadSoundSites},
};

void Summary(ULONGLONG Now, NetRole Role)
{
    if (Now < NextSummary)
        return;
    NextSummary = Now + 60000;
    if (Count.Sum() == Logged)
        return;
    Logged = Count.Sum();
    if (Role == NetRole::Client)
        Core::Log::Info("Waffensound: {} Schüsse beim Client, {} mit Sound, {} doppelt, {} Nahkampf, {} ohne Sound; Nachladen {} "
                        "empfangen, {} gespielt, {} ohne Hände, {} ohne Sound",
                        Count.Shots, Count.Played, Count.Repeated, Count.Melee, Count.Silent, Count.ReloadReceived, Count.ReloadPlayed,
                        Count.ReloadNoHands, Count.ReloadNoSound);
    else
        Core::Log::Info("Waffensound: Nachladen beim Host {} gesehen, {} gesendet, {} ohne NetGUID, {} über der Obergrenze",
                        Count.ReloadSeen, Count.ReloadSent, Count.ReloadNoGuid, Count.ReloadOverflow);
}
}

std::vector<ReloadSound> TakeReloadSounds()
{
    std::vector<ReloadSound> Result;
    if (!PendingCount || Patches::CurrentRole() != NetRole::Host)
    {
        PendingCount = 0;
        return Result;
    }
    // Die NetGUID-Tabelle nur in Takten mit Nachladen lesen, nie im Beobachter.
    std::unordered_map<uintptr_t, uint64_t> Guids;
    if (const auto Entries = ReadNetworkObjects())
        for (const auto& Entry : *Entries)
            Guids.emplace(Entry.Object, Entry.Guid);
    for (size_t i = 0; i < PendingCount; ++i)
    {
        const auto Guid = Objects::IsLive(Pending[i].Hands) ? Guids.find(Pending[i].Hands.Object) : Guids.end();
        if (Guid == Guids.end())
            ++Count.ReloadNoGuid;
        else
            Result.push_back({Guid->second, std::move(Pending[i].Override)});
    }
    PendingCount = 0;
    Count.ReloadSent += static_cast<uint32_t>(Result.size());
    return Result;
}

void PlayReloadSounds(std::span<const ReloadSound> Reloads)
{
    if (Reloads.empty() || Patches::CurrentRole() != NetRole::Client || !ClientActive.load(std::memory_order_relaxed) || !ReloadFunction)
        return;
    Count.ReloadReceived += static_cast<uint32_t>(Reloads.size());
    const ULONGLONG Now = GetTickCount64();
    std::unordered_map<uint64_t, uintptr_t> Known;
    if (const auto Entries = ReadNetworkObjects())
        for (const auto& Entry : *Entries)
            Known.emplace(Entry.Guid, Entry.Object);
    for (const ReloadSound& Reload : Reloads)
    {
        const auto Found = Known.find(Reload.Hands);
        if (Found == Known.end() || !Alive(Found->second) || !Objects::IsA(Found->second, "Hands"))
        {
            ++Count.ReloadNoHands;
            continue;
        }
        // Unbekannter Name: Standardsound der Hände statt Stille.
        uintptr_t Override = Reload.Override.empty() ? 0 : ReloadSoundNamed(Reload.Override, Now);
        if (!Reload.Override.empty() && !Override)
            ++Count.ReloadNoSound;
        Objects::ProcessEvent(Found->second, ReloadFunction, &Override);
        ++Count.ReloadPlayed;
        if (ReloadReported < 3 && ++ReloadReported)
            Core::Log::Info("Waffensound: Client spielt den Nachladesound an den eigenen Händen ({})",
                            Reload.Override.empty() ? "Standard" : Reload.Override);
    }
}

namespace Patches
{
std::span<const Patch> WeaponSoundPatches()
{
    return List;
}

void TickWeaponSounds(NetRole Role)
{
    if (!Observing)
    {
        Observing = AddEventObserver(&Observe);
        if (!Observing && !ObserverMissing)
        {
            ObserverMissing = true;
            Core::Log::Warn("Waffensound: kein freier ProcessEvent-Beobachter, Waffensounds beim Client bleiben aus");
        }
    }
    const uintptr_t World = CurrentWorld();
    if (Role == NetRole::Standalone || World != LastWorld)
    {
        ClientActive.store(false, std::memory_order_relaxed);
        HostActive.store(false, std::memory_order_relaxed);
        if (ReloadFunction || SoundCount || PendingCount)
            ForgetAll();
        LastWorld = World;
        if (Role == NetRole::Standalone)
            return;
    }
    const ULONGLONG Now = GetTickCount64();
    Summary(Now, Role);
    if (!World)
        return;
    if (!ReloadFunction)
    {
        if (Now < NextResolve)
            return;
        NextResolve = Now + 1000;
        const uintptr_t Reload = Objects::FindObject("PlayReloadSound", "Hands");
        const uintptr_t Shot = Objects::FindObject("TriggerWeaponShotFX", "Weapon");
        PlayFunction = Objects::FindObject("PlayEventAtLocation", "FMODBlueprintStatics");
        PlayDefaults = Objects::FindNamed("Default__FMODBlueprintStatics");
        if (!Reload || !Shot || !PlayFunction || !PlayDefaults)
            return;
        ShotFunction.store(Shot, std::memory_order_relaxed);
        ReloadFunction = Reload;
    }
    ClientActive.store(Role == NetRole::Client, std::memory_order_relaxed);
    HostActive.store(Role == NetRole::Host, std::memory_order_relaxed);
}
}
}
