// opt-sensenmann-ansage-client, siehe ReaperAnnouncementAdapter.h. Kein Code-Patch, kein Hook, keine Nachricht: ein Takt je Sekunde
// beim Client. Spawn, Bewegung und Schaden des Sensenmanns entscheidet weiter der Host.
// Befund: docs/faelle/opt-sensenmann-ansage-client.md
#include "Game/ReaperAnnouncementAdapter.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Core/Log.h"
#include "Game/FmodSound.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/ReaperAnnouncementPolicy.h"
#include "Game/Reflection.h"

#ifdef DDCOOP_WITH_SDK
#pragma warning(push, 0)
#include <SDK/DDSurvivors_parameters.hpp>
#pragma warning(pop)
#endif

namespace Game
{
namespace
{
constexpr ULONGLONG CheckIntervalMs = 1000;
constexpr std::string_view Package = "/Script/DDSurvivors";
constexpr Reflection::FieldSpec OnReaperSpawned{Package, "ReaperSpawner", "OnReaperSpawned", "MulticastInlineDelegateProperty", 16};
constexpr Reflection::FieldSpec SpawnedSpeech{Package, "ReaperSpawner", "ReaperSpawnedSpeech", "ObjectProperty", 8};
constexpr Reflection::FieldSpec SpawnedSound{Package, "ReaperSpawner", "ReaperSpawnedSoundEvent", "ObjectProperty", 8};
constexpr Reflection::FieldSpec BubbleWidget{Package, "SpeechBubbleWidgetComponent", "SpeechBubbleWidget", "ObjectProperty", 8};
constexpr Reflection::FieldSpec BubbleSpeaker{Package, "CharacterSpeechBubble", "SpeakerDefinition", "ObjectProperty", 8};
constexpr std::string_view ReaperEnemyClass = "BP_ReaperEnemy_C";
constexpr std::string_view SpawnerClass = "ReaperSpawner";
constexpr std::string_view SpeechSubsystemClass = "CharacterSpeechSubSystem";
// UGameInstance.LocalPlayers, UPlayer.PlayerController, AController.Pawn wie in RewardPatches.cpp.
constexpr uintptr_t GameInstanceLocalPlayers = 0x38;
constexpr uintptr_t PlayerController = 0x30;
constexpr uintptr_t ControllerPawn = 0x2E8;
constexpr uintptr_t ActorDestroyFlags = 0x5D; // Bit 1: bActorIsBeingDestroyed
constexpr uint32_t NonInstanceFlags = 0x30;   // Klassen-Default- und Archetyp-Objekt
constexpr int32_t MaxReceivers = 32;

// CharacterSpeechSubSystem.TriggerSpeechDefinition(Def, FAdditionalSpeechData), Layout wie im SDK.
struct SpeechParams
{
    uintptr_t Def = 0;
    uintptr_t OverrideSpeaker = 0;
    uintptr_t StartingCharacter = 0;
    uintptr_t ScreenAnchor = 0;
    bool UseAbsolutePosition = false;
    uint8_t Pad19[7]{};
    double ScreenLocation[2]{};
    double ScreenPercentages[2]{};
    bool TweenBubbleToNewPosition = false;
    uint8_t Pad49[3]{};
    float TweenTime = 0.0f;
    uint8_t TweenType = 0;
    bool ShowBubbleDuringTween = false;
    uint8_t Pad52[6]{};
};
static_assert(sizeof(SpeechParams) == 0x58);
static_assert(offsetof(SpeechParams, UseAbsolutePosition) == 0x8 + 0x18);
static_assert(offsetof(SpeechParams, ScreenLocation) == 0x8 + 0x20);
static_assert(offsetof(SpeechParams, TweenTime) == 0x8 + 0x44);
static_assert(offsetof(SpeechParams, TweenType) == 0x8 + 0x48);
#ifdef DDCOOP_WITH_SDK
static_assert(sizeof(SDK::Params::CharacterSpeechSubSystem_TriggerSpeechDefinition) == sizeof(SpeechParams));
static_assert(offsetof(SDK::Params::CharacterSpeechSubSystem_TriggerSpeechDefinition, AdditionalData) == offsetof(SpeechParams, OverrideSpeaker));
static_assert(offsetof(SDK::FAdditionalSpeechData, ScreenLocation) == offsetof(SpeechParams, ScreenLocation) - 8);
static_assert(offsetof(SDK::FAdditionalSpeechData, ScreenPercentages) == offsetof(SpeechParams, ScreenPercentages) - 8);
static_assert(offsetof(SDK::FAdditionalSpeechData, TweenTime) == offsetof(SpeechParams, TweenTime) - 8);
static_assert(offsetof(SDK::FAdditionalSpeechData, TweenType) == offsetof(SpeechParams, TweenType) - 8);
static_assert(offsetof(SDK::FAdditionalSpeechData, ShowBubbleDuringTween) == offsetof(SpeechParams, ShowBubbleDuringTween) - 8);
#endif

// FScriptDelegate: schwacher Zeiger (Index, Seriennummer), dann der FName der Funktion.
struct ScriptDelegate
{
    int32_t ObjectIndex;
    int32_t ObjectSerial;
    uint64_t FunctionName;
};
static_assert(sizeof(ScriptDelegate) == 16);

enum class ActorKind : uint8_t
{
    Other,
    Spawner,
    Reaper
};

// Nur Game Thread.
std::unordered_map<uintptr_t, ActorKind> ActorKinds;

uintptr_t Pointer(uintptr_t Address)
{
    return Address ? Memory::ReadPointer(Address).value_or(0) : 0;
}

uintptr_t ResolveWeak(int32_t Index, int32_t Serial)
{
    if (Index <= 0 || Index >= Objects::Count())
        return 0;
    const Objects::Ref Held{Objects::At(Index), Index, static_cast<uint32_t>(Serial)};
    return Held.Object && Objects::IsLive(Held) ? Held.Object : 0;
}

// UFunction mit diesem FName in der Klassenkette des Objekts.
uintptr_t FindFunction(uintptr_t Object, uint64_t Name)
{
    uintptr_t Class = Pointer(Object + Offsets::UObject::Class);
    for (int Depth = 0; Class && Depth < 64; ++Depth, Class = Pointer(Class + Offsets::UStruct::SuperStruct))
    {
        uintptr_t Field = Pointer(Class + Offsets::UStruct::Children);
        for (int Count = 0; Field && Count < 4096; ++Count, Field = Pointer(Field + Offsets::UField::Next))
            if (Memory::Read<uint64_t>(Field + Offsets::UObject::Name) == Name)
                return Field;
    }
    return 0;
}

std::vector<std::string> ClassChain(uintptr_t Object)
{
    std::vector<std::string> Chain;
    uintptr_t Class = Pointer(Object + Offsets::UObject::Class);
    for (int Depth = 0; Class && Depth < 64; ++Depth, Class = Pointer(Class + Offsets::UStruct::SuperStruct))
        Chain.push_back(Memory::NameOf(Class));
    return Chain;
}

// Wie der Broadcast der Spawn-Funktion, je Empfänger über ProcessEvent, ohne die Empfänger, die nur der Host ausführt. Die Liste wird
// vorher kopiert, ein Empfänger darf sie ändern.
void Broadcast(uintptr_t Spawner, uintptr_t DelegateOffset, std::string& Called, std::string& Skipped)
{
    const uintptr_t Data = Pointer(Spawner + DelegateOffset);
    const int32_t Num = Memory::Read<int32_t>(Spawner + DelegateOffset + 8).value_or(0);
    if (!Data || Num <= 0 || Num > MaxReceivers)
        return;
    std::vector<ScriptDelegate> Bindings(static_cast<size_t>(Num));
    if (!Memory::ReadBytes(Data, Bindings.data(), Bindings.size() * sizeof(ScriptDelegate)))
        return;
    for (const ScriptDelegate& Binding : Bindings)
    {
        const uintptr_t Object = ResolveWeak(Binding.ObjectIndex, Binding.ObjectSerial);
        const uintptr_t Function = Object ? FindFunction(Object, Binding.FunctionName) : 0;
        if (!Function)
            continue;
        const std::vector<std::string> Chain = ClassChain(Object);
        std::string& List = ReaperReceiverRunsOnClient(Chain) ? Called : Skipped;
        List += (List.empty() ? "" : ", ") + (Chain.empty() ? std::string("?") : Chain.front()) + "." + Memory::NameOf(Function);
        if (&List == &Skipped)
            continue;
        alignas(16) uint8_t NoParams[16]{};
        Objects::ProcessEvent(Object, Function, NoParams);
    }
}

// Sprecher der Blase: die eigene Figur, über ihre Sprechblasen-Komponente wie beim Host. 0, wenn es keine gibt; die Blase kommt
// dann ohne Sprecher.
uintptr_t LocalSpeaker(uintptr_t World)
{
    const auto WidgetField = Reflection::Property(BubbleWidget);
    const auto SpeakerField = Reflection::Property(BubbleSpeaker);
    const uintptr_t Instance = Pointer(World + Offsets::UWorld::OwningGameInstance);
    const uintptr_t Controller = Pointer(Pointer(Pointer(Instance ? Instance + GameInstanceLocalPlayers : 0)) + PlayerController);
    const uintptr_t Pawn = Pointer(Controller ? Controller + ControllerPawn : 0);
    if (!WidgetField || !SpeakerField || !Pawn)
        return 0;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object || Pointer(Object + Offsets::UObject::Outer) != Pawn ||
            (Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(NonInstanceFlags) & NonInstanceFlags) ||
            !Objects::IsA(Object, "SpeechBubbleWidgetComponent"))
            continue;
        const uintptr_t Widget = Pointer(Object + WidgetField->Offset);
        return Widget ? Pointer(Widget + SpeakerField->Offset) : 0;
    }
    return 0;
}

bool Speak(uintptr_t World, uintptr_t Speech)
{
    const uintptr_t Function = Reflection::Function(Package, SpeechSubsystemClass, "TriggerSpeechDefinition");
    const uintptr_t Subsystem = Objects::FindInstance(SpeechSubsystemClass);
    if (!Speech || !Function || !Subsystem)
        return false;
    SpeechParams Params;
    Params.Def = Speech;
    Params.OverrideSpeaker = LocalSpeaker(World);
    Params.UseAbsolutePosition = ReaperSpeech.UseAbsolutePosition;
    Params.ScreenLocation[0] = ReaperSpeech.ScreenLocation[0];
    Params.ScreenLocation[1] = ReaperSpeech.ScreenLocation[1];
    Params.ScreenPercentages[0] = ReaperSpeech.ScreenPercentages[0];
    Params.ScreenPercentages[1] = ReaperSpeech.ScreenPercentages[1];
    Params.TweenBubbleToNewPosition = ReaperSpeech.TweenBubbleToNewPosition;
    Params.TweenTime = ReaperSpeech.TweenTime;
    Params.TweenType = ReaperSpeech.TweenType;
    Params.ShowBubbleDuringTween = ReaperSpeech.ShowBubbleDuringTween;
    Objects::ProcessEvent(Subsystem, Function, &Params);
    return true;
}

// Was die Spawn-Funktion des Hosts nach dem Erzeugen tut (0x58CA20E bis 0x58CA3D7), ohne den Sensenmann selbst.
void Announce(uintptr_t World, uintptr_t Spawner)
{
    // Zuerst die Sperre, damit ein Fehler unten keine zweite Ansage auslöst.
    const uint8_t Spawned = 1;
    if (!Memory::WriteBytes(Spawner + Offsets::AReaperSpawner::WasReaperSpawned, &Spawned, 1))
        return;
    std::string Called, Skipped;
    if (const auto Delegate = Reflection::Property(OnReaperSpawned))
        Broadcast(Spawner, static_cast<uintptr_t>(Delegate->Offset), Called, Skipped);
    const auto SoundField = Reflection::Property(SpawnedSound);
    const bool Sound = SoundField && FmodSound::Play2D(Spawner, Pointer(Spawner + SoundField->Offset));
    const auto SpeechField = Reflection::Property(SpawnedSpeech);
    const bool Speech = SpeechField && Speak(World, Pointer(Spawner + SpeechField->Offset));
    Core::Log::Info("Sensenmann: beim Client angekommen, Ansage lokal: Empfänger [{}], ausgelassen [{}], Ton {}, Sprechblase {}", Called,
                    Skipped, Sound ? "ja" : "nein", Speech ? "ja" : "nein");
}
}

void TickReaperAnnouncement()
{
    static ULONGLONG LastCheck = 0;
    const ULONGLONG Now = GetTickCount64();
    if (Now - LastCheck < CheckIntervalMs)
        return;
    LastCheck = Now;
    if (Patches::CurrentRole() != NetRole::Client)
        return;
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t Level = Pointer(World ? World + Offsets::UWorld::PersistentLevel : 0);
    if (!Level)
        return;
    uintptr_t Spawner = 0;
    bool Reaper = false;
    Objects::ForEachLevelActor(Level, [&](uintptr_t Actor)
    {
        const uintptr_t Class = Pointer(Actor + Offsets::UObject::Class);
        if (!Class || (Memory::Read<uint8_t>(Actor + ActorDestroyFlags).value_or(0x02) & 0x02))
            return;
        auto Known = ActorKinds.find(Class);
        if (Known == ActorKinds.end())
            Known = ActorKinds.emplace(Class, Objects::IsA(Actor, SpawnerClass)       ? ActorKind::Spawner
                                              : Objects::IsA(Actor, ReaperEnemyClass) ? ActorKind::Reaper
                                                                                      : ActorKind::Other).first;
        if (Known->second == ActorKind::Spawner && !Spawner)
            Spawner = Actor;
        Reaper = Reaper || Known->second == ActorKind::Reaper;
    });
    const bool Was = Spawner && Memory::Read<uint8_t>(Spawner + Offsets::AReaperSpawner::WasReaperSpawned).value_or(1) != 0;
    if (ReaperAnnouncementDue(true, Spawner != 0, Was, Reaper))
        Announce(World, Spawner);
}
}
