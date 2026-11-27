// opt-powerups-client: Power-up-Slots im Coop, siehe PowerUpAdapter.h und PowerUpPolicy.h.
//
// UPowerupSlot::TryActivateSlot ist der eine Weg, auf dem ein Power-up wirkt: die Taste und das automatische Benutzen lösen einen
// Slot aus, die Sofortwirkung beim Aufsammeln (Dosenessen, Level-Up) baut dafür einen temporären Slot. Er wendet den Effekt der
// Definition an (ApplyGameplayEffectSpecToSelf) oder gibt ihre Fähigkeit und aktiviert sie einmal (GiveAbilityAndActivateOnce),
// spielt den Klang, leert einen verbrauchbaren Slot und meldet die Änderung. Ohne RPC und ohne Rollenprüfung.
//
// Host: Vor dem Auslösen stellt er eine LocalPredicted- oder LocalOnly-Fähigkeit der Definition auf ServerOnly, sonst bricht die
// Engine die Aktivierung für die Figur eines Netz-Clients ab. Für seine eigene Figur ändert das nichts, sie hat ohnehin Autorität.
// Client: Er lässt das native Auslösen laufen (Klang, Slot leeren, Anzeige); Effekt und Fähigkeit hätten ohne Autorität keine
// Wirkung. Hat es einen verbrauchbaren Slot der eigenen Figur geleert, meldet er dessen Definition, der Host löst denselben Slot
// der Client-Figur aus.
// Mit DDCOOP_POWERUPS_NATIVE=1 bleibt alles wie im Spiel, zum Vergleich vorher/nachher.
#include "Game/PowerUpAdapter.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Dispatcher.h"
#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

namespace Game
{
namespace
{
using Patches::At;
using Patches::CurrentRole;

constexpr uintptr_t TryActivateSlot = 0x5A288B0; // UPowerupSlot::TryActivateSlot(bool FailIfUsingGamepad), Build 26786272
constexpr uintptr_t ClassDefaultObject = 0x118;  // UClass::ClassDefaultObject, wie ItemAbilityPatches.cpp
constexpr uintptr_t ActorRole = 0x168;           // AActor.Role, docs/OFFSETS.md
constexpr uint8_t RoleAutonomousProxy = 2;
constexpr int32_t MaxSlotsRead = 16, MaxPlayers = 16;

constexpr std::string_view Package = "/Script/DDSurvivors";
constexpr Reflection::FieldSpec SlotDefinition{Package, "PowerupSlot", "EquippedPowerupDefinition", "ObjectProperty", 8};
constexpr Reflection::FieldSpec SlotConsumable{Package, "PowerupSlot", "bConsumable", "BoolProperty", 1};
constexpr Reflection::FieldSpec SlotLevel{Package, "PowerupSlot", "PowerLevel", "IntProperty", 4};
constexpr Reflection::FieldSpec InventorySlots{Package, "ActivePowerupInventory", "PowerupSlots", "ArrayProperty", 16};
constexpr Reflection::FieldSpec InventoryOwner{Package, "ActivePowerupInventory", "OwnerCharacter", "ObjectProperty", 8};
constexpr Reflection::FieldSpec StateInventory{Package, "DDPlayerState", "ActivePowerupInventory", "ObjectProperty", 8};
constexpr Reflection::FieldSpec DefinitionAbility{Package, "PowerupDefinition", "PowerupAbility", "ClassProperty", 8};
constexpr Reflection::FieldSpec ControllerState{"/Script/Engine", "Controller", "PlayerState", "ObjectProperty", 8};
constexpr Reflection::FieldSpec AbilityPolicy{"/Script/GameplayAbilities", "GameplayAbility", "NetExecutionPolicy", "ByteProperty", 1};

using SlotFn = void(__fastcall*)(uintptr_t, bool);
SlotFn OriginalTryActivateSlot = nullptr;

// Nur Game Thread.
std::vector<std::string> ClientActivations;
std::vector<uintptr_t> ChangedAbilities;
std::unordered_map<std::string, Objects::Ref> Definitions;

bool NativeOnly()
{
    static const bool Native = []
    {
        char Value[4]{};
        return GetEnvironmentVariableA("DDCOOP_POWERUPS_NATIVE", Value, sizeof(Value)) == 1 && Value[0] == '1';
    }();
    return Native;
}

uintptr_t PointerField(uintptr_t Object, const Reflection::FieldSpec& Spec)
{
    const auto Field = Object ? Reflection::Property(Spec) : std::nullopt;
    return Field ? Memory::ReadPointer(Object + Field->Offset).value_or(0) : 0;
}

struct SlotView
{
    uintptr_t Definition = 0;
    bool Consumable = false;
    int32_t Level = 1;
};

std::optional<SlotView> ReadSlot(uintptr_t Slot)
{
    const auto Definition = Reflection::Property(SlotDefinition);
    const auto Consumable = Reflection::Property(SlotConsumable);
    const auto Level = Reflection::Property(SlotLevel);
    if (!Slot || !Definition || !Consumable || !Level)
        return std::nullopt;
    SlotView View;
    View.Definition = Memory::ReadPointer(Slot + Definition->Offset).value_or(0);
    View.Consumable = Memory::Read<uint8_t>(Slot + Consumable->Offset).value_or(0) != 0;
    View.Level = Memory::Read<int32_t>(Slot + Level->Offset).value_or(1);
    return View;
}

std::vector<uintptr_t> SlotsOf(uintptr_t Inventory)
{
    std::vector<uintptr_t> Slots;
    const auto Field = Inventory ? Reflection::Property(InventorySlots) : std::nullopt;
    if (!Field)
        return Slots;
    const uintptr_t Data = Memory::ReadPointer(Inventory + Field->Offset).value_or(0);
    const int32_t Count = Memory::Read<int32_t>(Inventory + Field->Offset + 8).value_or(0);
    for (int32_t i = 0; Data && i < Count && i < MaxSlotsRead; ++i)
        if (const uintptr_t Slot = Memory::ReadPointer(Data + i * sizeof(uintptr_t)).value_or(0))
            Slots.push_back(Slot);
    return Slots;
}

uintptr_t InventoryOfController(uintptr_t Controller)
{
    const uintptr_t State = PointerField(Controller, ControllerState);
    const uintptr_t Inventory = State && Patches::IsA(State, "DDPlayerState") ? PointerField(State, StateInventory) : 0;
    return Inventory && Patches::IsA(Inventory, "ActivePowerupInventory") ? Inventory : 0;
}

// Client: Inventar der eigenen Figur, das einzige, dessen Figur hier AutonomousProxy ist.
uintptr_t OwnInventory()
{
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t GameState = World ? Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0) : 0;
    const uintptr_t Players = GameState ? Memory::ReadPointer(GameState + Offsets::AGameStateBase::PlayerArray).value_or(0) : 0;
    const int32_t Count = GameState ? Memory::Read<int32_t>(GameState + Offsets::AGameStateBase::PlayerArray + 8).value_or(0) : 0;
    for (int32_t i = 0; Players && i < Count && i < MaxPlayers; ++i)
    {
        const uintptr_t State = Memory::ReadPointer(Players + i * sizeof(uintptr_t)).value_or(0);
        const uintptr_t Inventory = State && Patches::IsA(State, "DDPlayerState") ? PointerField(State, StateInventory) : 0;
        const uintptr_t Owner = PointerField(Inventory, InventoryOwner);
        if (Owner && Memory::Read<uint8_t>(Owner + ActorRole).value_or(0) == RoleAutonomousProxy)
            return Inventory;
    }
    return 0;
}

// Host: Fähigkeit der Definition vor dem Auslösen auf ServerOnly stellen (PowerUp::AbilityMovesToHost).
void PrepareAbility(uintptr_t Slot)
{
    const auto View = ReadSlot(Slot);
    const uintptr_t Class = View ? PointerField(View->Definition, DefinitionAbility) : 0;
    if (!Class || std::find(ChangedAbilities.begin(), ChangedAbilities.end(), Class) != ChangedAbilities.end())
        return;
    const uintptr_t Default = Memory::ReadPointer(Class + ClassDefaultObject).value_or(0);
    const auto Policy = Reflection::Property(AbilityPolicy);
    if (!Default || !Policy || !Patches::IsA(Default, "GameplayAbility"))
        return;
    const auto Current = Memory::Read<uint8_t>(Default + Policy->Offset);
    ChangedAbilities.push_back(Class);
    if (!Current || !PowerUp::AbilityMovesToHost(*Current))
        return;
    const uint8_t Next = PowerUp::ServerOnly;
    if (Memory::WriteBytes(Default + Policy->Offset, &Next, sizeof(Next)))
        Core::Log::Info("Power-up: {} von {} läuft jetzt beim Host für alle Figuren (Richtlinie {} -> ServerOnly)", Memory::NameOf(Class),
                        Memory::NameOf(View->Definition), *Current);
}

void __fastcall TryActivateSlotHook(uintptr_t Slot, bool FailIfUsingGamepad)
{
    const NetRole Role = CurrentRole();
    if (NativeOnly() || !Core::GameThread().IsDrainThread() || Role == NetRole::Standalone)
        return OriginalTryActivateSlot(Slot, FailIfUsingGamepad);
    if (Role == NetRole::Host)
    {
        PrepareAbility(Slot);
        return OriginalTryActivateSlot(Slot, FailIfUsingGamepad);
    }
    const auto Before = ReadSlot(Slot);
    OriginalTryActivateSlot(Slot, FailIfUsingGamepad);
    if (!Before || !Before->Definition || !Before->Consumable)
        return;
    const auto After = ReadSlot(Slot);
    // Abklingzeit, Gamepad oder Sperr-Tag: nicht ausgelöst, der Slot ist noch voll.
    if (!After || After->Definition)
        return;
    const uintptr_t Inventory = Memory::ReadPointer(Slot + Offsets::UObject::Outer).value_or(0);
    if (!Inventory || Inventory != OwnInventory())
        return;
    const std::string Name = Memory::NameOf(Before->Definition);
    if (!PowerUp::ValidName(Name) || ClientActivations.size() >= PowerUp::MaxSlots * 4)
        return;
    ClientActivations.push_back(Name);
    Core::Log::Info("Power-up: {} beim Client ausgelöst, der Host bucht die Wirkung", Name);
}

uintptr_t FindDefinition(const std::string& Name)
{
    if (const auto Hit = Definitions.find(Name); Hit != Definitions.end() && Objects::IsLive(Hit->second))
        return Hit->second.Object;
    const uintptr_t Found = Objects::FindNamed(Name);
    if (!Found || !Patches::IsA(Found, "PowerupDefinition"))
        return 0;
    Definitions[Name] = Objects::RefTo(Found);
    return Found;
}

bool CheckTryActivateSlot()
{
    return Code::Expect(At(TryActivateSlot), {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20, 0x55},
                        "PowerupSlot.TryActivateSlot");
}

bool ApplyTryActivateSlot(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(TryActivateSlot)), reinterpret_cast<void*>(&TryActivateSlotHook),
                                reinterpret_cast<void**>(&OriginalTryActivateSlot));
}

constexpr uintptr_t TryActivateSlotSites[] = {TryActivateSlot};
const Patches::Patch List[] = {
    {"Power-up-Slots löst der Host aus, für alle Figuren", &CheckTryActivateSlot, &ApplyTryActivateSlot, TryActivateSlotSites},
};
}

namespace Patches
{
Patch PowerUpSlotPatch()
{
    return List[0];
}
}

std::optional<std::vector<PowerUp::Slot>> ReadPowerUps(uintptr_t Controller)
{
    if (NativeOnly() || !OriginalTryActivateSlot || CurrentRole() != NetRole::Host)
        return std::nullopt;
    const uintptr_t Inventory = InventoryOfController(Controller);
    if (!Inventory)
        return std::nullopt;
    std::vector<PowerUp::Slot> Result;
    for (const uintptr_t Slot : SlotsOf(Inventory))
    {
        const auto View = ReadSlot(Slot);
        if (!View || !View->Definition || !View->Consumable || Result.size() >= PowerUp::MaxSlots)
            continue;
        PowerUp::Slot Entry{Memory::NameOf(View->Definition), std::clamp(View->Level, 0, PowerUp::MaxLevel)};
        if (PowerUp::ValidName(Entry.Definition))
            Result.push_back(std::move(Entry));
    }
    return Result;
}

bool ActivatePowerUp(uintptr_t Controller, std::string_view Definition)
{
    if (NativeOnly() || !OriginalTryActivateSlot || CurrentRole() != NetRole::Host)
        return false;
    for (const uintptr_t Slot : SlotsOf(InventoryOfController(Controller)))
    {
        const auto View = ReadSlot(Slot);
        if (!View || !View->Definition || !View->Consumable || Memory::NameOf(View->Definition) != Definition)
            continue;
        TryActivateSlotHook(Slot, false);
        const auto After = ReadSlot(Slot);
        const bool Used = After && !After->Definition;
        Core::Log::Info("Power-up: {} der Client-Figur beim Host {}", Definition, Used ? "ausgelöst" : "nicht ausgelöst (Abklingzeit)");
        return Used;
    }
    Core::Log::Info("Power-up: {} gemeldet, die Client-Figur hat beim Host keinen solchen Slot", Definition);
    return false;
}

std::vector<std::string> TakePowerUpActivations()
{
    std::vector<std::string> Result;
    Result.swap(ClientActivations);
    return Result;
}

bool ApplyHostPowerUps(const std::vector<PowerUp::Slot>& Host, const std::vector<std::string>& Pending)
{
    if (NativeOnly() || CurrentRole() != NetRole::Client)
        return false;
    const uintptr_t Inventory = OwnInventory();
    if (!Inventory)
        return false;
    std::vector<std::string> Local;
    for (const uintptr_t Slot : SlotsOf(Inventory))
        if (const auto View = ReadSlot(Slot); View && View->Definition && View->Consumable)
            Local.push_back(Memory::NameOf(View->Definition));
    const PowerUp::Plan Plan = PowerUp::Reconcile(Local, Host, Pending);
    if (Plan.Empty())
        return true;
    const uintptr_t Remove = Reflection::Function(Package, "ActivePowerupInventory", "TryRemovePowerup");
    const uintptr_t Add = Reflection::Function(Package, "ActivePowerupInventory", "TryAddPowerup");
    for (const std::string& Name : Plan.Remove)
    {
        struct
        {
            uintptr_t PowerupType = 0, OptionalRequestor = 0;
            bool ReturnValue = false;
            uint8_t Pad[7]{};
        } Params{FindDefinition(Name)};
        static_assert(sizeof(Params) == 0x18);
        if (!Remove || !Params.PowerupType)
            continue;
        Objects::ProcessEvent(Inventory, Remove, &Params);
        Core::Log::Info("Power-up: {} beim Client aus dem Slot genommen, der Host hat es nicht ({})", Name, Params.ReturnValue);
    }
    for (const PowerUp::Slot& Entry : Plan.Add)
    {
        struct
        {
            uintptr_t PowerupType = 0;
            bool IsConsumable = true;
            uint8_t Pad[7]{};
            uintptr_t Requestor = 0;
            int32_t PowerLevel = 1;
            bool ReturnValue = false;
            uint8_t Pad2[3]{};
        } Params{FindDefinition(Entry.Definition)};
        static_assert(sizeof(Params) == 0x20);
        Params.PowerLevel = Entry.Level;
        if (!Add || !Params.PowerupType)
        {
            Core::Log::Warn("Power-up: {} des Hosts ist beim Client nicht geladen", Entry.Definition);
            continue;
        }
        Objects::ProcessEvent(Inventory, Add, &Params);
        Core::Log::Info("Power-up: {} vom Host in den Slot des Clients gelegt ({})", Entry.Definition, Params.ReturnValue);
    }
    return true;
}

uintptr_t PowerUpWorld()
{
    return Patches::CurrentWorld();
}
}
