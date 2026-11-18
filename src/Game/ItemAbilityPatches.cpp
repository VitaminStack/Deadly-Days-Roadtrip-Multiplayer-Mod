// client-items-wirkung: Item-Fähigkeiten der Client-Figur laufen beim Host.
//
// Items geben ihre Wirkung als Gameplay-Ability (FItemEffectWrapper.GameplayAbility), etwa GA_GrenadeThrower_C (LocalOnly) oder
// GA_DelayedHeal_C (LocalPredicted). Das Spiel kennt nur lokalen Coop: Dort ist jede Figur beim Host lokal gesteuert, und die
// Fähigkeit läuft mit Autorität. Für die Figur eines Netz-Clients reicht die Engine eine solche Aktivierung dagegen nur an den
// Client weiter (ClientTryActivateAbility). Dort läuft sie ohne Autorität: kein Projektil, kein Schaden, keine Heilung.
//
// Lokal zuerst: Der Host entscheidet HP, Schaden und Projektile. Er stellt darum die Ausführungsrichtlinie dieser Klassen auf
// ServerOnly, bevor das Spiel die Fähigkeit vergibt. Dann läuft sie beim Host für jede Figur, auch seine eigene (für die ändert
// sich nichts, sie hat dort ohnehin Autorität). Projektile, HP und Gameplay-Cues kommen über die vorhandene Replikation zum
// Client. Keine eigene Nachricht, kein zweiter Auslöseweg.
//
// Umgestellt werden nur Fähigkeiten, die das Item selbst auslöst (beim Vergeben oder über seinen Auslöse-Effekt). Fähigkeiten,
// die jemand anderes startet (etwa GA_SlideAbility_C) oder die einen Eingabe-Tag tragen, bleiben wie der Dodge (GA_Dash_C)
// unberührt: Dort sagt der Client mit seiner Eingabe voraus. Auch LocalPredicted stellt der Host um, obwohl die Engine solche
// Aktivierungen über den Client zum Host zurückreicht: Sonst liefe der Ablauf zusätzlich beim Client ohne Autorität, und nur
// der Host soll Schaden, Heilung und erzeugte Actors buchen. Nebenwirkung: Ton, den die Fähigkeit selbst startet (etwa
// player_Healed), spielt für die Client-Figur nur beim Host.
//
// Zweite Lücke, Fähigkeiten mit Zustand (UDDGameplayAbilityWithState, etwa GA_DelayedHeal_C des Heil-Kits): Das Spiel ruft ihr
// SetupAbility nur dort auf, wo die Figur lokal gesteuert ist. Beim Host fehlen der Instanz der Client-Figur damit Figur,
// AbilitySystem und Attribute; sie ist aktiv, hört aber nie auf Schaden und heilt nie. Der Host holt genau diesen nativen
// Aufruf nach, direkt vor der ersten Aktivierung (K2_ActivateAbility), also in der Reihenfolge des Spiels. Parameter sind die
// ActorInfo der Instanz und ihr Spec aus dem AbilitySystem. Alles Weitere (Lebens-Ereignis, Pulse, Heilung) läuft nativ.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

#include "Core/Log.h"
#include "Game/ItemAbilityPolicy.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t ContextItem = 0x08;        // FItemEffectContext.Item
constexpr uintptr_t ItemData = 0xE0;           // UInventoryItemInstance, Zeiger auf UInventoryItemData
constexpr uintptr_t DataEffectWrappers = 0x118; // UInventoryItemData.ItemEffectWrappers, TArray<FItemEffectWrapper>
constexpr uintptr_t WrapperSize = 0xB0;
constexpr uintptr_t WrapperAbility = 0x48;     // FItemEffectWrapperBase.GameplayAbility, TSubclassOf<UGameplayAbility>
constexpr uintptr_t WrapperActivateOnGive = 0x50, WrapperTriggerable = 0x51; // bool
constexpr uintptr_t AbilityStartupInputTag = 0x3A0; // UDDGameplayAbility.StartupInputTag (FGameplayTag, FName-Index)
constexpr uintptr_t ClassDefaults = 0x118;     // UClass::ClassDefaultObject
constexpr uintptr_t AbilityNetExecutionPolicy = 0x198; // UGameplayAbility.NetExecutionPolicy
constexpr int32_t MaxWrappers = 32;
// UGameplayAbility.CurrentActorInfo (const FGameplayAbilityActorInfo*, kein Reflexionsfeld). Gemessen auf 26495302; vor jedem
// Gebrauch gegen den Besitzer der Instanz geprüft, siehe ActorInfoOf.
constexpr uintptr_t AbilityCurrentActorInfo = 0x328;
constexpr uintptr_t ActorInfoSize = 0x48, ActorInfoOwner = 0x08, ActorInfoAvatar = 0x10, ActorInfoAbilitySystem = 0x20;
constexpr uintptr_t AbilitySystemSpecs = 0x520; // UAbilitySystemComponent.ActivatableAbilities.Items, TArray<FGameplayAbilitySpec>
constexpr uintptr_t SpecSize = 0x108, SpecHandle = 0x0C, SpecNonReplicatedInstances = 0x88, SpecReplicatedInstances = 0x98;
constexpr int32_t MaxSpecs = 512, MaxInstances = 8;
constexpr uintptr_t ActorRemoteRole = 0x60;
constexpr uint8_t RoleAutonomousProxy = 2;
constexpr size_t MaxPrepared = 128;

std::atomic<uintptr_t> ApplyEffectsFromItem = 0, ApplyItemToBackpack = 0;
// FName-Index von K2_ActivateAbility, -1 bis der Tick ihn gefunden hat.
std::atomic<int32_t> ActivateName = -1;
// Nur Game Thread.
std::vector<uintptr_t> Checked;
unsigned Changed = 0;
struct Prepared
{
    uintptr_t Ability;
    int32_t Handle;
};
std::vector<Prepared> PreparedAbilities;
unsigned SetupCalls = 0;
bool InSetup = false;

bool NativeOnly()
{
    // Diagnose: Mit DDCOOP_ITEM_ABILITIES_NATIVE=1 bleibt das Verhalten des Spiels, zum Vergleich vorher/nachher.
    static const bool Native = []
    {
        char Value[4]{};
        return GetEnvironmentVariableA("DDCOOP_ITEM_ABILITIES_NATIVE", Value, sizeof(Value)) == 1 && Value[0] == '1';
    }();
    return Native;
}

// ActorInfo der Instanz, 0, wenn das Feld nicht auf eine ActorInfo dieses Besitzers zeigt (anderer Build, Instanz ohne Vergabe).
uintptr_t ActorInfoOf(uintptr_t Ability)
{
    const uintptr_t Info = Memory::ReadPointer(Ability + AbilityCurrentActorInfo).value_or(0);
    const uintptr_t Outer = Memory::ReadPointer(Ability + Offsets::UObject::Outer).value_or(0);
    if (!Info || !Outer)
        return 0;
    const int32_t Owner = Memory::Read<int32_t>(Info + ActorInfoOwner).value_or(-1);
    const int32_t Avatar = Memory::Read<int32_t>(Info + ActorInfoAvatar).value_or(-1);
    const int32_t OuterIndex = Memory::Read<int32_t>(Outer + Offsets::UObject::InternalIndex).value_or(-2);
    // Die Instanz entsteht mit dem Besitzer ihres AbilitySystems als Outer: PlayerState oder Figur.
    return OuterIndex > 0 && (Owner == OuterIndex || Avatar == OuterIndex) ? Info : 0;
}

bool HoldsInstance(uintptr_t Array, uintptr_t Ability)
{
    const uintptr_t Data = Memory::ReadPointer(Array).value_or(0);
    const int32_t Count = Memory::Read<int32_t>(Array + 8).value_or(0);
    for (int32_t i = 0; Data && i < Count && i < MaxInstances; ++i)
        if (Memory::ReadPointer(Data + i * sizeof(uintptr_t)).value_or(0) == Ability)
            return true;
    return false;
}

// Spec im AbilitySystem, dem die Instanz gehört. 0, wenn keiner sie führt.
uintptr_t SpecOf(uintptr_t AbilitySystem, uintptr_t Ability)
{
    const uintptr_t Items = Memory::ReadPointer(AbilitySystem + AbilitySystemSpecs).value_or(0);
    const int32_t Count = Memory::Read<int32_t>(AbilitySystem + AbilitySystemSpecs + 8).value_or(0);
    if (!Items || Count <= 0 || Count > MaxSpecs)
        return 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Spec = Items + i * SpecSize;
        if (HoldsInstance(Spec + SpecNonReplicatedInstances, Ability) || HoldsInstance(Spec + SpecReplicatedInstances, Ability))
            return Spec;
    }
    return 0;
}

// SetupAbility in der Klasse der Instanz oder der nächsten Oberklasse, die es überschreibt.
uintptr_t SetupFunctionOf(uintptr_t Ability)
{
    uintptr_t Class = Memory::ReadPointer(Ability + Offsets::UObject::Class).value_or(0);
    for (int Depth = 0; Class && Depth < 16; ++Depth)
    {
        const std::string Name = Memory::NameOf(Class);
        if (const uintptr_t Found = Objects::FindObject("SetupAbility", Name))
            return Found;
        if (Name == "DDGameplayAbilityWithState")
            break;
        Class = Memory::ReadPointer(Class + Offsets::UStruct::SuperStruct).value_or(0);
    }
    return 0;
}

void PrepareItem(uintptr_t Item)
{
    const uintptr_t Data = Item ? Memory::ReadPointer(Item + ItemData).value_or(0) : 0;
    if (!Data || std::find(Checked.begin(), Checked.end(), Data) != Checked.end())
        return;
    Checked.push_back(Data);
    const uintptr_t Wrappers = Memory::ReadPointer(Data + DataEffectWrappers).value_or(0);
    const int32_t Count = Memory::Read<int32_t>(Data + DataEffectWrappers + 8).value_or(0);
    if (!Wrappers || Count <= 0 || Count > MaxWrappers)
        return;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Class = Memory::ReadPointer(Wrappers + i * WrapperSize + WrapperAbility).value_or(0);
        if (!Class || !IsA(Class, "Class"))
            continue;
        const uintptr_t Default = Memory::ReadPointer(Class + ClassDefaults).value_or(0);
        if (!Default || !IsA(Default, "GameplayAbility"))
            continue;
        const auto Policy = Memory::Read<uint8_t>(Default + AbilityNetExecutionPolicy);
        const bool OnGive = Memory::Read<uint8_t>(Wrappers + i * WrapperSize + WrapperActivateOnGive).value_or(0) != 0;
        const bool Triggerable = Memory::Read<uint8_t>(Wrappers + i * WrapperSize + WrapperTriggerable).value_or(0) != 0;
        // Unlesbarer Tag gilt als gesetzt, dann bleibt die Klasse unberührt.
        const bool InputTag = Memory::Read<int32_t>(Default + AbilityStartupInputTag).value_or(1) != 0;
        if (!Policy || !ItemAbility::MovesToHost(*Policy, OnGive, Triggerable, InputTag))
            continue;
        const uint8_t Next = ItemAbility::ServerOnly;
        if (!Memory::WriteBytes(Default + AbilityNetExecutionPolicy, &Next, sizeof(Next)))
            continue;
        ++Changed;
        Core::Log::Info("Item-Fähigkeit: {} von {} läuft jetzt beim Host für alle Figuren (Richtlinie {} -> ServerOnly, {}. Klasse)",
                        Memory::NameOf(Class), Memory::NameOf(Data), *Policy, Changed);
    }
}
}

void PrepareItemAbilities(uintptr_t Function, void* Params)
{
    if (!Function || !Params)
        return;
    if (Function != ApplyEffectsFromItem.load(std::memory_order_relaxed) && Function != ApplyItemToBackpack.load(std::memory_order_relaxed))
        return;
    // Beim Client entscheidet der Host; dort bleibt die Klasse, wie sie ist.
    if (CurrentRole() == NetRole::Client || NativeOnly())
        return;
    PrepareItem(Memory::ReadPointer(reinterpret_cast<uintptr_t>(Params) + ContextItem).value_or(0));
}

void PrepareRemoteAbilityState(uintptr_t Object, uintptr_t Function)
{
    if (!Object || !Function || InSetup)
        return;
    const int32_t Wanted = ActivateName.load(std::memory_order_relaxed);
    if (Wanted < 0 || Memory::Read<int32_t>(Function + Offsets::UObject::Name).value_or(-1) != Wanted)
        return;
    if (CurrentRole() != NetRole::Host || NativeOnly() || !IsA(Object, "DDGameplayAbilityWithState"))
        return;
    const uintptr_t Info = ActorInfoOf(Object);
    if (!Info)
        return;
    const uintptr_t Avatar = Objects::At(Memory::Read<int32_t>(Info + ActorInfoAvatar).value_or(-1));
    const uintptr_t AbilitySystem = Objects::At(Memory::Read<int32_t>(Info + ActorInfoAbilitySystem).value_or(-1));
    // Nur Figuren eines Netz-Clients: Für lokal gesteuerte hat das Spiel SetupAbility schon selbst gerufen.
    if (!Avatar || !AbilitySystem || !IsA(Avatar, "DDCharacter") || !IsA(AbilitySystem, "AbilitySystemComponent") ||
        Memory::Read<uint8_t>(Avatar + ActorRemoteRole).value_or(0) != RoleAutonomousProxy)
        return;
    const uintptr_t Spec = SpecOf(AbilitySystem, Object);
    if (!Spec)
        return;
    const int32_t Handle = Memory::Read<int32_t>(Spec + SpecHandle).value_or(-1);
    for (const Prepared& Entry : PreparedAbilities)
        if (Entry.Ability == Object && Entry.Handle == Handle)
            return;
    const uintptr_t Setup = SetupFunctionOf(Object);
    if (!Setup)
        return;
    // DDGameplayAbilityWithState_SetupAbility: FGameplayAbilityActorInfo (+0, 0x48), FGameplayAbilitySpec (+0x48, 0x108).
    // Flache Kopien wie bei jedem Aufruf mit Parametern per Wert; die Funktion liest sie nur und kopiert, was sie behält.
    alignas(8) uint8_t Params[ActorInfoSize + SpecSize]{};
    static_assert(sizeof(Params) == 0x150);
    if (!Memory::ReadBytes(Info, Params, ActorInfoSize) || !Memory::ReadBytes(Spec, Params + ActorInfoSize, SpecSize))
        return;
    if (PreparedAbilities.size() >= MaxPrepared)
        PreparedAbilities.erase(PreparedAbilities.begin(), PreparedAbilities.begin() + MaxPrepared / 2);
    PreparedAbilities.push_back({Object, Handle});
    InSetup = true;
    Objects::ProcessEvent(Object, Setup, Params);
    InSetup = false;
    ++SetupCalls;
    Core::Log::Info("Item-Fähigkeit: {} der Client-Figur beim Host eingerichtet (SetupAbility, Spec {}, {}. Mal)",
                    Memory::NameOf(Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0)), Handle, SetupCalls);
}

void TickItemAbilities(NetRole, bool Second)
{
    if (!Second)
        return;
    if (ActivateName.load(std::memory_order_relaxed) < 0)
        if (const uintptr_t Activate = Objects::FindObject("K2_ActivateAbility", "GameplayAbility"))
            ActivateName.store(Memory::Read<int32_t>(Activate + Offsets::UObject::Name).value_or(-1), std::memory_order_relaxed);
    if (ApplyEffectsFromItem.load(std::memory_order_relaxed) && ApplyItemToBackpack.load(std::memory_order_relaxed))
        return;
    ApplyEffectsFromItem.store(Objects::FindObject("ApplyEffectsFromItem", "CharacterEffectManager"), std::memory_order_relaxed);
    ApplyItemToBackpack.store(Objects::FindObject("ApplyItemToBackpack", "CharacterEffectManager"), std::memory_order_relaxed);
}
}
