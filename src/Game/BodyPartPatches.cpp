// opt-koerperteile, Client: Körperteile sterbender Gegner. Beim Host löst UEnemyDyingAbility den GameplayCue
// ChaosDestructionCueTag aus, der native ChaosDestructionProxyGameplayCue holt einen gepoolten EnemyChaosDestructionProxyActor
// und zerlegt den Gegner in Chaos-Bruchstücke. Die Ability läuft nur beim Host, der Cue repliziert nicht. Lokal zuerst: Beim
// Client kommt der zuverlässige Multicast DDCharacterBase.Multi_OnDie an; die Mod führt dann denselben Cue lokal und nicht
// repliziert aus (UGameplayCueManager::ExecuteGameplayCue_NonReplicated). Keine Nachricht, kein neuer Hook, Tod, XP und Drops
// bleiben beim Host. Befund: docs/GAME_ANALYSIS.md#körperteile-beim-client-opt-koerperteile
#include <windows.h>

#include <array>
#include <atomic>
#include <cstring>

#include "Core/Log.h"
#include "Game/CodeCave.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
// UGameplayCueManager::ExecuteGameplayCue_NonReplicated(AActor* Target, FGameplayTag Tag, const FGameplayCueParameters&),
// statisch. UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor ruft es nur für Ziele ohne ASC auf; für Gegner mit ASC
// verlangt es Autorität, beim Client geschieht dort nichts.
constexpr uintptr_t ExecuteCueNonReplicated = 0x4BC58D0;
using ExecuteCueFn = void(__fastcall*)(uintptr_t Target, uint64_t Tag, const void* Parameters);

// Build 26495302, SDK.
constexpr uintptr_t StartupAbilities = 0x708;   // ADDCharacterBase, TArray<TSubclassOf<UGameplayAbility>>
constexpr uintptr_t ChaosDestructionCueTag = 0x3A8; // UEnemyDyingAbility, FGameplayTag (FName)
constexpr uintptr_t ClassDefaultObject = 0x118; // UClass
constexpr uintptr_t RootComponent = 0x1B8;      // AActor
constexpr uintptr_t RelativeLocation = 0x148;   // USceneComponent
constexpr size_t CueParametersSize = 0xD8;      // FGameplayCueParameters
constexpr uintptr_t CueLocation = 0x70;         // FGameplayCueParameters.Location
constexpr uint32_t GarbageOrUnreachable = 0x30;
constexpr int32_t MaxStartupAbilities = 64;
constexpr size_t MaxEnemyClasses = 16;
constexpr size_t RecentDeaths = 32;

struct ClassTag
{
    uintptr_t Class = 0;
    uint64_t Tag = 0; // 0: Klasse hat keine Dying-Ability mit Tag
};

struct Death
{
    uintptr_t Enemy = 0;
    ULONGLONG At = 0;
};

// Alles nur auf dem Game Thread: Multi_OnDie, Patches::Tick und ProcessEvent laufen dort.
std::atomic<uintptr_t> OnDieFunction = 0;
std::atomic<bool> ClientActive = false;
std::array<ClassTag, MaxEnemyClasses> Tags{};
size_t TagCount = 0;
std::array<Death, RecentDeaths> Recent{};
size_t RecentNext = 0;
int32_t ShownThisFrame = 0;
uintptr_t LastWorld = 0;
ULONGLONG NextResolve = 0;
bool Observing = false;
bool ObserverMissing = false;
int Reported = 0;
int Skipped = 0;

void ForgetAll()
{
    OnDieFunction.store(0, std::memory_order_relaxed);
    TagCount = 0;
    Recent = {};
    NextResolve = 0;
}

bool Alive(uintptr_t Object)
{
    const int32_t Index = Memory::Read<int32_t>(Object + Offsets::UObject::InternalIndex).value_or(-1);
    if (Objects::At(Index) != Object)
        return false;
    return !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable);
}

// Tag aus der Dying-Ability der Gegnerklasse (StartupAbilities am Klassen-Defaultobjekt der Instanz), je Klasse gemerkt.
uint64_t CueTagFor(uintptr_t Enemy)
{
    const uintptr_t Class = Memory::ReadPointer(Enemy + Offsets::UObject::Class).value_or(0);
    if (!Class)
        return 0;
    for (size_t i = 0; i < TagCount; ++i)
        if (Tags[i].Class == Class)
            return Tags[i].Tag;
    uint64_t Tag = 0;
    const uintptr_t Data = Memory::ReadPointer(Enemy + StartupAbilities).value_or(0);
    const int32_t Count = Memory::Read<int32_t>(Enemy + StartupAbilities + 8).value_or(0);
    for (int32_t i = 0; Data && i < Count && i < MaxStartupAbilities && !Tag; ++i)
    {
        const uintptr_t AbilityClass = Memory::ReadPointer(Data + 8 * static_cast<uintptr_t>(i)).value_or(0);
        const uintptr_t Default = AbilityClass ? Memory::ReadPointer(AbilityClass + ClassDefaultObject).value_or(0) : 0;
        if (Default && Objects::IsA(Default, "EnemyDyingAbility"))
            Tag = Memory::Read<uint64_t>(Default + ChaosDestructionCueTag).value_or(0);
    }
    if (TagCount < MaxEnemyClasses)
        Tags[TagCount++] = ClassTag{Class, Tag};
    Core::Log::Info("Körperteile: Gegnerklasse {} {}", Memory::NameOf(Class), Tag ? "mit Zerfall-Cue" : "ohne Zerfall-Cue");
    return Tag;
}

bool SeenRecently(uintptr_t Enemy, ULONGLONG Now)
{
    for (const Death& Entry : Recent)
        if (Entry.Enemy == Enemy && BodyPartsRepeated(static_cast<double>(Now - Entry.At) / 1000.0))
            return true;
    Recent[RecentNext] = Death{Enemy, Now};
    RecentNext = (RecentNext + 1) % RecentDeaths;
    return false;
}

void ShowBodyParts(uintptr_t Enemy)
{
    const ULONGLONG Now = GetTickCount64();
    if (!Alive(Enemy) || !Objects::IsA(Enemy, "EnemyBase") || SeenRecently(Enemy, Now))
        return;
    if (!BodyPartsMayShow(ShownThisFrame))
    {
        if (Skipped++ < 3)
            Core::Log::Info("Körperteile: Obergrenze je Frame erreicht, Zerfall ausgelassen");
        return;
    }
    const uint64_t Tag = CueTagFor(Enemy);
    if (!Tag)
        return;
    alignas(16) std::array<uint8_t, CueParametersSize> Parameters{};
    if (const uintptr_t Root = Memory::ReadPointer(Enemy + RootComponent).value_or(0))
        if (const auto Location = Memory::Read<std::array<double, 3>>(Root + RelativeLocation))
            std::memcpy(Parameters.data() + CueLocation, Location->data(), sizeof(*Location));
    reinterpret_cast<ExecuteCueFn>(At(ExecuteCueNonReplicated))(Enemy, Tag, Parameters.data());
    ++ShownThisFrame;
    if (Reported < 3 && ++Reported)
        Core::Log::Info("Körperteile: Client zeigt den Zerfall eines Gegners lokal ({})", Memory::NameOf(Memory::ReadPointer(Enemy + Offsets::UObject::Class).value_or(0)));
}

// Nach dem ursprünglichen ProcessEvent: Der Gegner steht noch an seiner Todesstelle, der Kanal ist noch offen.
void ObserveDeath(uintptr_t Object, uintptr_t Function, void*)
{
    if (Function && Function == OnDieFunction.load(std::memory_order_relaxed) && ClientActive.load(std::memory_order_relaxed))
        ShowBodyParts(Object);
}

bool CheckCue()
{
    return Code::Expect(At(ExecuteCueNonReplicated), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57},
                        "GameplayCueManager.ExecuteGameplayCue_NonReplicated");
}

// Ruft die Funktion nur auf, ändert keinen Code. Passt die Signatur nicht, bleibt die Liste samt Tick aus.
bool ApplyCue(Code::Writer&)
{
    return true;
}

constexpr uintptr_t CueSites[] = {ExecuteCueNonReplicated};
const Patch List[] = {
    {"Körperteile beim Client lokal", CheckCue, ApplyCue, CueSites},
};
}

std::span<const Patch> BodyPartPatches()
{
    return List;
}

void TickBodyParts(NetRole Role)
{
    ShownThisFrame = 0;
    if (!Observing)
    {
        Observing = AddEventObserver(&ObserveDeath);
        if (!Observing && !ObserverMissing)
        {
            ObserverMissing = true;
            Core::Log::Warn("Körperteile: kein freier ProcessEvent-Beobachter, Zerfall beim Client bleibt aus");
        }
    }
    if (Role != NetRole::Client)
    {
        ClientActive.store(false, std::memory_order_relaxed);
        if (OnDieFunction.load(std::memory_order_relaxed) || TagCount)
            ForgetAll();
        return;
    }
    const uintptr_t World = CurrentWorld();
    if (World != LastWorld)
    {
        ClientActive.store(false, std::memory_order_relaxed);
        ForgetAll();
        LastWorld = World;
    }
    if (!World || OnDieFunction.load(std::memory_order_relaxed))
    {
        ClientActive.store(World != 0, std::memory_order_relaxed);
        return;
    }
    const ULONGLONG Now = GetTickCount64();
    if (Now < NextResolve)
        return;
    NextResolve = Now + 1000;
    if (const uintptr_t OnDie = Objects::FindObject("Multi_OnDie", "DDCharacterBase"))
    {
        OnDieFunction.store(OnDie, std::memory_order_relaxed);
        ClientActive.store(true, std::memory_order_relaxed);
    }
}
}
