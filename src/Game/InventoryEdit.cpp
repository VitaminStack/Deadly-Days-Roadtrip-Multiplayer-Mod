// Lokale Inventarbearbeitung des Clients (inventar-lokal), siehe src/Game/README.md#inventar-lokal-bearbeiten.
//
// Solange der Client sein Inventar-Panel bearbeitet, laufen dessen Inventar-RPCs (Ablegen, Feldbelegung, Rucksackausbau,
// Pivot, Belohnungs- und Lagerflags) mit der nativen Implementierung lokal statt als Server-RPC. Beim Bestätigen wird
// daraus ein Edit, den der Host mit denselben Funktionen übernimmt. Bis zur Antwort bleibt der Ausgangsstand gemerkt.
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>

#include "Core/Log.h"
#include "Game/InventoryEditRuntime.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game::InventoryEdit
{
namespace
{
using Patches::CurrentRole;

// UInventoryComponent.Slots/MyItems, UBackpackSlot.Coordinates/Occupier
constexpr uintptr_t InventorySlots = 0x100, InventoryItems = 0x120;
constexpr uintptr_t SlotCoordinates = 0x28, SlotOccupier = 0x30;
// UInventoryItemInstance
constexpr uintptr_t ItemPivot = 0x28, ItemTurns = 0xD8, ItemData = 0xE0;
constexpr uintptr_t ItemInBackpack = 0x180, ItemIsTrash = 0x184;
constexpr uintptr_t FunctionFlags = 0xB0;
constexpr uint32_t FuncNet = 0x40, FuncNetServer = 0x200000, GarbageOrUnreachable = 0x30;
// EBackpackEffectRemovalStyle
constexpr uint8_t ForceRemoveAllEffects = 0, RetainEffects = 2;

// waffe-client-auswahl: wie das native Panel. Bleibt das Item im Rucksack (verschieben), behält es seine Wirkung (RetainEffects);
// verlässt es ihn, endet sie (ForceRemoveAllEffects). Vorher war es vertauscht: Herausgenommene Waffen feuerten weiter, verschobene
// verloren ihre Wirkung und wurden nicht wieder ausgerüstet.
constexpr uint8_t RemovalStyle(bool StaysInBackpack)
{
    return StaysInBackpack ? RetainEffects : ForceRemoveAllEffects;
}

struct Array
{
    void* Data;
    int32_t Count, Max;
};

uintptr_t Pointer(uintptr_t Address)
{
    return Memory::ReadPointer(Address).value_or(0);
}

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable);
}

uintptr_t Function(uintptr_t& Cache, const char* Name, const char* Outer)
{
    if (!Cache)
        Cache = Objects::FindObject(Name, Outer);
    return Cache;
}

// Server-RPCs beim Client lokal, sonst normal.
void Call(uintptr_t Object, uintptr_t Fn, void* Params, bool Local)
{
    if (Local && (Memory::Read<uint32_t>(Fn + FunctionFlags).value_or(0) & FuncNet))
        Patches::ProcessEventLocal(Object, Fn, Params);
    else
        Objects::ProcessEvent(Object, Fn, Params);
}

std::vector<uintptr_t> SlotPointers(uintptr_t Inventory)
{
    const Array Slots = Memory::Read<Array>(Inventory + InventorySlots).value_or(Array{});
    std::vector<uintptr_t> Result;
    if (!Slots.Data || Slots.Count <= 0 || Slots.Count > 4096)
        return Result;
    for (int32_t i = 0; i < Slots.Count; ++i)
        Result.push_back(Pointer(reinterpret_cast<uintptr_t>(Slots.Data) + i * sizeof(uintptr_t)));
    return Result;
}

// Die Feld-Widgets des Panels hängen an einem nativen Delegate, den nur OnRep_Slots auslöst, beim Client also nur die
// Replikation vom Host. Nach lokal angelegten oder entfernten Feldern ruft der Client es selbst mit dem alten Stand auf,
// sonst erscheinen die Felder erst, wenn der Host später etwas repliziert (ausruestung-felder).
void NotifySlots(uintptr_t Inventory, const std::vector<uintptr_t>& Old)
{
    static uintptr_t OnRepSlots = 0;
    if (!Function(OnRepSlots, "OnRep_Slots", "InventoryComponent") || !Alive(Inventory))
        return;
    if (Old == SlotPointers(Inventory))
        return;
    std::vector<uintptr_t> Copy = Old;
    Array Params{Copy.data(), static_cast<int32_t>(Copy.size()), static_cast<int32_t>(Copy.size())};
    Objects::ProcessEvent(Inventory, OnRepSlots, &Params);
    Core::Log::Info("Inventar lokal: Felder neu angezeigt, {} statt {}", SlotPointers(Inventory).size(), Old.size());
}

std::string ItemLabel(uintptr_t Item)
{
    return Item ? Memory::NameOf(Pointer(Item + ItemData)) : std::string("?");
}

// Inventar-RPCs, die während der Bearbeitung lokal laufen. ApplyAddToBackpackEffect merkt sich der Client nur, der Host löst ihn
// beim Übernehmen aus. Alle anderen Server-RPCs, auch die anderer Klassen (Ausrüsten, Waffen), verwirft der Client, bis der Host
// geantwortet hat: Sie nennen oft Objekte, die es nur beim Client gibt, der Host läse Nullzeiger. Nur Bewegung geht weiter.
enum class Kind : uint8_t
{
    Other,
    InventoryRpc,
    RemoveFromInventory,
    ItemRpc,
    AddEffect,
    Foreign,  // Server-RPC einer anderen Klasse
    Movement, // geht immer an den Host
};

struct Known
{
    Kind Type = Kind::Other;
    std::string Name;
};

const Known& Classify(uintptr_t Fn)
{
    static std::unordered_map<uintptr_t, Known> Cache;
    if (const auto It = Cache.find(Fn); It != Cache.end())
        return It->second;
    Known Value;
    // Nur Server-RPCs (FUNC_NetServer): Client-RPCs und Multicasts vom Host laufen beim Client ganz normal.
    if (Memory::Read<uint32_t>(Fn + FunctionFlags).value_or(0) & FuncNetServer)
    {
        const std::string Owner = Memory::NameOf(Pointer(Fn + Offsets::UObject::Outer));
        Value.Name = Memory::NameOf(Fn);
        static const char* const InventoryNames[] = {"AddToBackpack",         "AddToInventory",      "OccupyBackpackSlots",
                                                     "UnoccupyBackpackSlots", "RemoveFromBackpack", "Server_CreateSlotsFromBagItem"};
        static const char* const ItemNames[] = {"SetZeroPivotTile", "UpdateIsInBackpack", "UpdateIsInStash", "UpdateIsReward", "UpdateIsTrash"};
        if (Owner == "InventoryComponent")
        {
            if (Value.Name == "RemoveFromInventory")
                Value.Type = Kind::RemoveFromInventory;
            else if (Value.Name == "ApplyAddToBackpackEffect")
                Value.Type = Kind::AddEffect;
            else if (std::find(std::begin(InventoryNames), std::end(InventoryNames), Value.Name) != std::end(InventoryNames))
                Value.Type = Kind::InventoryRpc;
        }
        else if (Owner == "InventoryItemInstance" && std::find(std::begin(ItemNames), std::end(ItemNames), Value.Name) != std::end(ItemNames))
            Value.Type = Kind::ItemRpc;
        if (Owner != "InventoryComponent" && Owner != "InventoryItemInstance")
            Value.Type = Value.Name.starts_with("ServerMove") || Value.Name == "ServerUpdateCamera" || Value.Name.starts_with("ServerUpdateLevelVisibility") ||
                                 Value.Name == "ServerNotifyLoadedWorld" || Value.Name == "ServerAcknowledgePossession"
                             ? Kind::Movement
                             : Kind::Foreign;
        Value.Name = Owner + "." + Value.Name;
    }
    return Cache.emplace(Fn, std::move(Value)).first->second;
}

enum class Phase : uint8_t
{
    Idle,
    Editing,
    Committed, // Edit gesendet, Antwort des Hosts steht aus
};

struct
{
    Phase State = Phase::Idle;
    uintptr_t Inventory = 0;
    std::vector<uintptr_t> Items;   // Ausgangsstand MyItems
    std::vector<ItemState> States;  // je Item, Source = Index
    std::vector<Tile> Slots;        // Ausgangsfelder
    std::vector<uintptr_t> Choices; // lokale Anzeige-Instanzen
    std::unordered_map<uintptr_t, uint8_t> RemoveContext;
    std::vector<uintptr_t> Effects; // Items mit ApplyAddToBackpackEffect
    std::vector<std::string> Dropped; // schon gemeldete verworfene RPCs
    struct Recorded
    {
        std::vector<uintptr_t> Inputs;
        uintptr_t Output = 0;
        std::string Data, Rarity;
    };
    std::vector<Recorded> Upgrades; // lokal gelaufene Aufwertungen (werkbank-client)
    unsigned Logged = 0;
    uint64_t NextId = 0;
} Session;

bool Contains(const std::vector<uintptr_t>& List, uintptr_t Value)
{
    return Value && std::find(List.begin(), List.end(), Value) != List.end();
}

bool IsUpgradeOutput(uintptr_t Item)
{
    return Item && std::any_of(Session.Upgrades.begin(), Session.Upgrades.end(), [&](const auto& Next) { return Next.Output == Item; });
}

bool BelongsToSession(uintptr_t Item)
{
    return Contains(Session.Items, Item) || Contains(Session.Choices, Item) || IsUpgradeOutput(Item) || Contains(ItemsOf(Session.Inventory), Item);
}

// Endstand eines Items, das während der Bearbeitung entstanden ist oder verbraucht wurde.
ItemState FinalState(uintptr_t Item, const std::vector<uintptr_t>& Now, const std::vector<SlotInfo>& Slots, uint16_t Source)
{
    ItemState State = ReadItem(Item, Slots, Source);
    State.Present = Contains(Now, Item);
    State.AddEffect = State.Present && State.InBackpack && Contains(Session.Effects, Item);
    if (!State.Present)
    {
        State.InBackpack = State.Trash = false;
        State.Tiles.clear();
        const auto It = Session.RemoveContext.find(Item);
        State.RemoveContext = It != Session.RemoveContext.end() ? It->second : 0;
    }
    return State;
}

void LogOp(const std::string& Text)
{
    if (++Session.Logged <= 200)
        Core::Log::Info("Inventar lokal: {}", Text);
}
}

std::vector<uintptr_t> ItemsOf(uintptr_t Inventory)
{
    std::vector<uintptr_t> Items;
    const uintptr_t Data = Alive(Inventory) ? Pointer(Inventory + InventoryItems) : 0;
    const int32_t Count = Data ? Memory::Read<int32_t>(Inventory + InventoryItems + 8).value_or(0) : 0;
    for (int32_t i = 0; i < Count && Count <= 4096; ++i)
        if (const uintptr_t Item = Pointer(Data + 8 * i))
            Items.push_back(Item);
    return Items;
}

std::vector<SlotInfo> SlotsOf(uintptr_t Inventory)
{
    std::vector<SlotInfo> Slots;
    const uintptr_t Data = Alive(Inventory) ? Pointer(Inventory + InventorySlots) : 0;
    const int32_t Count = Data ? Memory::Read<int32_t>(Inventory + InventorySlots + 8).value_or(0) : 0;
    for (int32_t i = 0; i < Count && Count <= 1024; ++i)
    {
        const uintptr_t Slot = Pointer(Data + 8 * i);
        if (!Alive(Slot))
            continue;
        SlotInfo Info;
        Info.Slot = Slot;
        Info.At = {Memory::Read<int32_t>(Slot + SlotCoordinates).value_or(0), Memory::Read<int32_t>(Slot + SlotCoordinates + 4).value_or(0)};
        Info.Occupier = Pointer(Slot + SlotOccupier);
        Slots.push_back(Info);
    }
    return Slots;
}

uint32_t DataHashOf(uintptr_t Item)
{
    const uintptr_t Data = Item ? Pointer(Item + ItemData) : 0;
    return Data ? NameHash(Memory::NameOf(Data)) : 0;
}

ItemState ReadItem(uintptr_t Item, const std::vector<SlotInfo>& Slots, uint16_t Source)
{
    ItemState State;
    State.Source = Source;
    State.DataHash = DataHashOf(Item);
    State.InBackpack = Memory::Read<uint8_t>(Item + ItemInBackpack).value_or(0) != 0;
    State.Trash = Memory::Read<uint8_t>(Item + ItemIsTrash).value_or(0) != 0;
    if (State.InBackpack)
    {
        State.Turns = Memory::Read<int32_t>(Item + ItemTurns).value_or(0);
        State.Pivot = {Memory::Read<int32_t>(Item + ItemPivot).value_or(0), Memory::Read<int32_t>(Item + ItemPivot + 4).value_or(0)};
        for (const SlotInfo& Slot : Slots)
            if (Slot.Occupier == Item)
                State.Tiles.push_back(Slot.At);
        State.Tiles = Sorted(std::move(State.Tiles));
    }
    return State;
}

void RefreshEffects(uintptr_t Inventory)
{
    static uintptr_t GetEffectManager = 0, RefreshItemEffects = 0;
    if (!Function(GetEffectManager, "GetEffectManager", "InventoryComponent") || !Function(RefreshItemEffects, "RefreshItemEffects", "InventoryComponent") ||
        !Alive(Inventory))
    {
        Core::Log::Warn("Inventar lokal: Wirkungen nicht aufgefrischt, Funktion oder Inventar fehlt");
        return;
    }
    uintptr_t Manager = 0;
    Objects::ProcessEvent(Inventory, GetEffectManager, &Manager);
    if (!Alive(Manager))
    {
        Core::Log::Warn("Inventar lokal: Wirkungen nicht aufgefrischt, kein EffectManager");
        return;
    }
    // InventoryComponent_RefreshItemEffects: bool IncludeBackpackEffects (+0), UCharacterEffectManager* (+8).
    struct
    {
        bool IncludeBackpackEffects;
        uint8_t Pad[7];
        uintptr_t EffectManager;
    } Params{false, {}, Manager};
    static_assert(sizeof(Params) == 0x10);
    Objects::ProcessEvent(Inventory, RefreshItemEffects, &Params);
}

bool Apply(uintptr_t Inventory, const std::vector<uintptr_t>& Items, const std::vector<ItemState>& Current, const std::vector<ItemState>& Target,
           const std::vector<Tile>& Create, const std::vector<uintptr_t>& Destroy, bool Local)
{
    static uintptr_t Unoccupy = 0, RemoveFromBackpack = 0, RemoveFromInventory = 0, AddToInventory = 0, DestroySlot = 0, CreateSlots = 0,
                     TryGetSlot = 0, AddToBackpack = 0, Occupy = 0, SetTurns = 0, SetPivot = 0, UpdateTrash = 0;
    if (!Function(Unoccupy, "UnoccupyBackpackSlots", "InventoryComponent") || !Function(RemoveFromBackpack, "RemoveFromBackpack", "InventoryComponent") ||
        !Function(RemoveFromInventory, "RemoveFromInventory", "InventoryComponent") || !Function(AddToInventory, "AddToInventory", "InventoryComponent") ||
        !Function(DestroySlot, "DestroySlotInstance", "InventoryComponent") || !Function(CreateSlots, "Server_CreateSlotsFromBagItem", "InventoryComponent") ||
        !Function(TryGetSlot, "TryGetSlotWithCoordinates", "InventoryComponent") || !Function(AddToBackpack, "AddToBackpack", "InventoryComponent") ||
        !Function(Occupy, "OccupyBackpackSlots", "InventoryComponent") || !Function(SetTurns, "SetTurns", "InventoryItemInstance") ||
        !Function(SetPivot, "SetZeroPivotTile", "InventoryItemInstance") || !Function(UpdateTrash, "UpdateIsTrash", "InventoryItemInstance"))
    {
        Core::Log::Warn("Inventar lokal: native Inventarfunktionen nicht gefunden");
        return false;
    }
    if (!Alive(Inventory) || Items.size() != Current.size() || Items.size() != Target.size())
        return false;
    bool Ok = true;
    const std::vector<uintptr_t> OldSlots = Local ? SlotPointers(Inventory) : std::vector<uintptr_t>{};
    for (const Step& Next : Plan(Current, Target, !Create.empty(), !Destroy.empty()))
    {
        const uintptr_t Item = Next.Item < Items.size() ? Items[Next.Item] : 0;
        if (Next.Kind != StepKind::CreateSlots && Next.Kind != StepKind::DestroySlots && !Alive(Item))
        {
            Ok = false;
            continue;
        }
        switch (Next.Kind)
        {
        case StepKind::Unplace:
        {
            uintptr_t Params = Item;
            Call(Inventory, Unoccupy, &Params, Local);
            struct
            {
                uintptr_t Item;
                uint8_t Style, Pad[7];
            } Remove{Item, RemovalStyle(Target[Next.Item].Present && Target[Next.Item].InBackpack), {}};
            Call(Inventory, RemoveFromBackpack, &Remove, Local);
            break;
        }
        case StepKind::Remove:
        {
            struct
            {
                uintptr_t Item;
                uint8_t Context;
                bool Destroy;
                uint8_t Pad[6];
            } Params{Item, Target[Next.Item].RemoveContext, !Local, {}};
            static_assert(sizeof(Params) == 0x10);
            Call(Inventory, RemoveFromInventory, &Params, Local);
            break;
        }
        case StepKind::Add:
        {
            struct
            {
                uintptr_t Item;
                bool SystemAdd;
                uint8_t Pad[3];
                int32_t Position;
                bool AllowP2Reserve;
                uint8_t Pad2[7];
            } Params{Item, true, {}, -1, false, {}};
            static_assert(sizeof(Params) == 0x18);
            Call(Inventory, AddToInventory, &Params, Local);
            break;
        }
        case StepKind::DestroySlots:
            for (const uintptr_t Slot : Destroy)
            {
                uintptr_t Params = Slot;
                Call(Inventory, DestroySlot, &Params, Local);
            }
            break;
        case StepKind::CreateSlots:
        {
            std::vector<int32_t> Coordinates;
            for (const auto& [X, Y] : Create)
            {
                Coordinates.push_back(X);
                Coordinates.push_back(Y);
            }
            Array Params{Coordinates.data(), static_cast<int32_t>(Create.size()), static_cast<int32_t>(Create.size())};
            Call(Inventory, CreateSlots, &Params, Local);
            break;
        }
        case StepKind::Place:
        {
            const ItemState& Goal = Target[Next.Item];
            std::vector<uintptr_t> Slots;
            for (const auto& [X, Y] : Goal.Tiles)
            {
                struct
                {
                    int32_t X, Y;
                    uintptr_t Slot;
                    bool Found;
                    uint8_t Pad[7];
                } Params{X, Y, 0, false, {}};
                static_assert(sizeof(Params) == 0x18);
                Objects::ProcessEvent(Inventory, TryGetSlot, &Params);
                if (!Params.Found || !Params.Slot)
                    break;
                Slots.push_back(Params.Slot);
            }
            if (Slots.size() != Goal.Tiles.size())
            {
                Core::Log::Warn("Inventar lokal: Feld für {} fehlt, bleibt im Lager", ItemLabel(Item));
                Ok = false;
                break;
            }
            struct
            {
                int32_t Turns;
                bool Instant;
                uint8_t Pad[3];
            } Turns{Goal.Turns, true, {}};
            Objects::ProcessEvent(Item, SetTurns, &Turns);
            int32_t Pivot[2]{Goal.Pivot.first, Goal.Pivot.second};
            Call(Item, SetPivot, Pivot, Local);
            uintptr_t Add = Item;
            Call(Inventory, AddToBackpack, &Add, Local);
            struct
            {
                uintptr_t Item;
                Array Slots;
            } Params{Item, {Slots.data(), static_cast<int32_t>(Slots.size()), static_cast<int32_t>(Slots.size())}};
            static_assert(sizeof(Params) == 0x18);
            // AddToBackpack ruft ApplyAddToBackpackEffect beim Host selbst auf (Trace). Ein zweiter Aufruf
            // würde die Rucksackwirkung doppelt anlegen. Die Itemwirkungen setzt RefreshEffects danach neu.
            Call(Inventory, Occupy, &Params, Local);
            break;
        }
        case StepKind::Trash:
        {
            struct
            {
                bool Trash;
                uint8_t Pad[7];
            } Params{Target[Next.Item].Trash, {}};
            Call(Item, UpdateTrash, &Params, Local);
            break;
        }
        }
    }
    if (Local)
        NotifySlots(Inventory, OldSlots);
    // Ergebnis gegen das Ziel prüfen.
    const std::vector<SlotInfo> Slots = SlotsOf(Inventory);
    const std::vector<uintptr_t> Now = ItemsOf(Inventory);
    for (size_t i = 0; i < Items.size(); ++i)
    {
        ItemState Reached = Contains(Now, Items[i]) && Alive(Items[i]) ? ReadItem(Items[i], Slots, Target[i].Source) : ItemState{};
        if (!Contains(Now, Items[i]) || !Alive(Items[i]))
            Reached.Present = false;
        Reached.DataHash = Target[i].DataHash;
        Reached.RemoveContext = Target[i].RemoveContext;
        Reached.AddEffect = Target[i].AddEffect;
        if (!SameState(Reached, Target[i]))
        {
            Core::Log::Warn("Inventar lokal: {} nach dem Übernehmen abweichend (vorhanden {}, im Rucksack {}, {} Felder statt {})", ItemLabel(Items[i]),
                            Reached.Present, Reached.InBackpack, Reached.Tiles.size(), Target[i].Tiles.size());
            Ok = false;
        }
    }
    return Ok;
}

// waffe-client-auswahl: Hände-Vorschau beim Client. Die Itemwirkungen (auch belegte Hände, UDDCharacterAttributeSet.OccupiedHands)
// setzt nur der Host, beim Client verwirft RouteClientRpc die Wirkungs-RPCs. Ohne Vorschau gibt ein abgelegtes Waffenitem
// während der Bearbeitung keinen Waffenslot frei, und das Panel lässt keine neue Waffe zu. Der Client rechnet darum
// lokal nach: Ausgangswert + (Hände der Rucksackitems jetzt - beim Start). Verbindlich bleibt der Host: Nach dem Übernehmen
// kommt sein Wert über die native Replikation, bei Ablehnung stellt RevertClient den Ausgangswert wieder her.
constexpr uintptr_t AttributeOccupiedHands = 0x110, AttributeBase = 0x8, AttributeCurrent = 0xC;

struct
{
    uintptr_t Character = 0, Set = 0;
    float Base = 0, Current = 0;
    int32_t Start = 0;
} Hands;

// Das Attributset der Figur gehört dem PlayerState, nicht der Figur. AController.PlayerState / .Pawn.
constexpr uintptr_t ControllerPlayerState = 0x2B0, ControllerPawn = 0x2E8;

uintptr_t AttributeSetOf(uintptr_t PlayerState)
{
    if (!PlayerState)
        return 0;
    for (int32_t i = 0, Count = Objects::Count(); i < Count; ++i)
        if (const uintptr_t Object = Objects::At(i);
            Object && Pointer(Object + Offsets::UObject::Outer) == PlayerState && Objects::IsA(Object, "DDCharacterAttributeSet") && Alive(Object))
            return Object;
    return 0;
}

int32_t HandsInBackpack(uintptr_t Inventory, uintptr_t Character)
{
    static uintptr_t Required = 0, Library = 0;
    if (!Function(Required, "GetHandsRequiredForItem", "InventoryEffectHelperLibrary") || !Function(Library, "Default__InventoryEffectHelperLibrary", ""))
        return 0;
    int32_t Total = 0;
    for (const uintptr_t Item : ItemsOf(Inventory))
    {
        if (!Alive(Item) || !Memory::Read<uint8_t>(Item + ItemInBackpack).value_or(0))
            continue;
        // InventoryEffectHelperLibrary_GetHandsRequiredForItem
        struct
        {
            uintptr_t Character, Item, Inventory;
            Array AdditionalSlots;
            bool IgnoreModifier;
            uint8_t Pad[3];
            int32_t ReturnValue;
        } Params{Character, Item, Inventory, {}, false, {}, 0};
        static_assert(sizeof(Params) == 0x30);
        Objects::ProcessEvent(Library, Required, &Params);
        Total += Params.ReturnValue;
    }
    return Total;
}

// Anzeige setzen. Kein OnRep_OccupiedHands: Das ließe das ASC den Wert aus seinen Modifikatoren neu berechnen und die Vorschau
// sofort überschreiben. RefreshHandsBar zeichnet den Balken nicht neu (im Spiel geprüft), darum UpdateCurrentHandsBar mit dem
// Wert, den sonst die Attributänderung liefert. Der Client zeigt den Balken HandBarSinglePlayer mit CachedCharacter = eigene Figur.
void ShowHands(int32_t Occupied)
{
    constexpr uintptr_t HandBarCharacter = 0x348; // UHandBarDisplay.CachedCharacter
    static uintptr_t Update = 0;
    if (!Function(Update, "UpdateCurrentHandsBar", "HandBarDisplay"))
        return;
    for (int32_t i = 0, Count = Objects::Count(); i < Count; ++i)
        if (const uintptr_t Object = Objects::At(i); Object && Pointer(Object + HandBarCharacter) == Hands.Character &&
                                                     Objects::IsA(Object, "HandBarDisplay") && Alive(Object))
        {
            int32_t Params = Occupied;
            Objects::ProcessEvent(Object, Update, &Params);
        }
}

void WriteHands(float Base, float Current)
{
    const float OldBase = Memory::Read<float>(Hands.Set + AttributeOccupiedHands + AttributeBase).value_or(0);
    const float OldCurrent = Memory::Read<float>(Hands.Set + AttributeOccupiedHands + AttributeCurrent).value_or(0);
    if (OldBase == Base && OldCurrent == Current)
        return;
    Memory::WriteBytes(Hands.Set + AttributeOccupiedHands + AttributeBase, &Base, sizeof(Base));
    Memory::WriteBytes(Hands.Set + AttributeOccupiedHands + AttributeCurrent, &Current, sizeof(Current));
    ShowHands(static_cast<int32_t>(Current));
    Core::Log::Info("Inventar lokal: Hände-Vorschau {} statt {}", Current, OldCurrent);
}

void BeginHands(uintptr_t Inventory, uintptr_t Controller)
{
    Hands = {};
    const uintptr_t Character = Pointer(Controller + ControllerPawn), PlayerState = Pointer(Controller + ControllerPlayerState);
    Hands.Set = AttributeSetOf(PlayerState);
    if (!Hands.Set)
    {
        Core::Log::Warn("Inventar lokal: Attributset nicht gefunden (PlayerState {:#x}, Figur {:#x}), keine Hände-Vorschau", PlayerState, Character);
        return;
    }
    Hands.Character = Character;
    Hands.Base = Memory::Read<float>(Hands.Set + AttributeOccupiedHands + AttributeBase).value_or(0);
    Hands.Current = Memory::Read<float>(Hands.Set + AttributeOccupiedHands + AttributeCurrent).value_or(0);
    Hands.Start = HandsInBackpack(Inventory, Character);
    Core::Log::Info("Inventar lokal: Hände belegt {}, Rucksack braucht {}", Hands.Current, Hands.Start);
}

void PreviewHands()
{
    if (!Hands.Set || !Alive(Hands.Set) || !Alive(Hands.Character))
        return;
    const float Delta = static_cast<float>(HandsInBackpack(Session.Inventory, Hands.Character) - Hands.Start);
    WriteHands(Hands.Base, std::max(0.0f, Hands.Current + Delta));
}

// Ablehnung oder Abbruch: Ausgangswert wieder setzen. Nach dem Übernehmen nichts schreiben, dann gilt der replizierte Wert.
void EndHands(bool Restore)
{
    if (Restore && Hands.Set && Alive(Hands.Set))
        WriteHands(Hands.Base, Hands.Current);
    Hands = {};
}

void BeginClient(uintptr_t Inventory, const std::vector<uintptr_t>& Choices, uintptr_t Controller)
{
    if (Session.State == Phase::Editing)
        RevertClient("neue Bearbeitung");
    if (Session.State == Phase::Committed)
        Core::Log::Warn("Inventar lokal: neue Bearbeitung, obwohl die Antwort zur letzten fehlt");
    const uint64_t NextId = Session.NextId;
    Session = {};
    Session.NextId = NextId;
    Session.State = Phase::Editing;
    Session.Inventory = Inventory;
    Session.Items = ItemsOf(Inventory);
    Session.Choices = Choices;
    const std::vector<SlotInfo> Slots = SlotsOf(Inventory);
    for (size_t i = 0; i < Session.Items.size(); ++i)
        Session.States.push_back(ReadItem(Session.Items[i], Slots, static_cast<uint16_t>(i)));
    for (const SlotInfo& Slot : Slots)
        Session.Slots.push_back(Slot.At);
    Core::Log::Info("Inventar lokal: Bearbeitung beginnt, {} Items, {} Felder, {} Belohnungen", Session.Items.size(), Session.Slots.size(), Choices.size());
    BeginHands(Inventory, Controller);
}

bool ClientEditing()
{
    return Session.State == Phase::Editing;
}

bool ClientEditingInventory(uintptr_t Inventory)
{
    return Session.State == Phase::Editing && Inventory && Session.Inventory == Inventory;
}

void RecordUpgrade(const std::vector<uintptr_t>& Inputs, uintptr_t Output, uintptr_t Data, uintptr_t Rarity)
{
    if (Session.State != Phase::Editing || !Output)
        return;
    Session.Upgrades.push_back({Inputs, Output, Data ? Memory::NameOf(Data) : std::string{}, Rarity ? Memory::NameOf(Rarity) : std::string{}});
    Core::Log::Info("Inventar lokal: Aufwertung {} lokal, {} Eingaben, Ausgabe {} {}", Session.Upgrades.size(), Inputs.size(), Session.Upgrades.back().Data,
                    Session.Upgrades.back().Rarity);
}

std::optional<Edit> FinishClient(uint64_t SessionId, uint64_t Revision, std::optional<uint8_t> Choice, uintptr_t ChoiceItem)
{
    if (Session.State != Phase::Editing)
        return std::nullopt;
    const std::vector<uintptr_t> Now = ItemsOf(Session.Inventory);
    const std::vector<SlotInfo> Slots = SlotsOf(Session.Inventory);
    std::vector<ItemState> Final;
    for (size_t i = 0; i < Session.Items.size(); ++i)
    {
        if (Contains(Now, Session.Items[i]))
        {
            Final.push_back(ReadItem(Session.Items[i], Slots, static_cast<uint16_t>(i)));
            Final.back().AddEffect = Final.back().InBackpack && Contains(Session.Effects, Session.Items[i]);
            continue;
        }
        ItemState Gone = Session.States[i];
        Gone.Present = false;
        Gone.InBackpack = Gone.Trash = false;
        Gone.Tiles.clear();
        const auto It = Session.RemoveContext.find(Session.Items[i]);
        Gone.RemoveContext = It != Session.RemoveContext.end() ? It->second : 0;
        Final.push_back(std::move(Gone));
    }
    if (Choice && ChoiceItem)
        Final.push_back(FinalState(ChoiceItem, Now, Slots, ChoiceSource));
    std::vector<Tile> FinalSlots;
    for (const SlotInfo& Slot : Slots)
        FinalSlots.push_back(Slot.At);
    Edit Result = Diff(Session.States, Final, Session.Slots, FinalSlots);
    if (Choice && ChoiceItem)
    {
        Result.Session = SessionId;
        Result.Revision = Revision;
        Result.Choice = Choice;
    }
    // Aufwertungen: Eingaben als Sources (Ausgangs-Item, Wahl oder frühere Ausgabe), Ausgabe mit Endstand.
    std::unordered_map<uintptr_t, uint16_t> SourceOf;
    for (size_t i = 0; i < Session.Items.size(); ++i)
        SourceOf.emplace(Session.Items[i], static_cast<uint16_t>(i));
    if (Choice && ChoiceItem)
        SourceOf.emplace(ChoiceItem, ChoiceSource);
    for (size_t K = 0; K < Session.Upgrades.size(); ++K)
    {
        const auto& Recorded = Session.Upgrades[K];
        Upgrade Next;
        for (const uintptr_t Input : Recorded.Inputs)
        {
            const auto It = SourceOf.find(Input);
            if (It == SourceOf.end())
            {
                Core::Log::Warn("Inventar lokal: Eingabe {} der Aufwertung {} unbekannt, Bearbeitung zurückgenommen", ItemLabel(Input), K + 1);
                RevertClient("Aufwertung nicht zuordenbar");
                return std::nullopt;
            }
            Next.Inputs.push_back(It->second);
        }
        Next.Data = Recorded.Data;
        Next.Rarity = Recorded.Rarity;
        Next.Output = FinalState(Recorded.Output, Now, Slots, UpgradeSource(K));
        Next.Output.DataHash = NameHash(Next.Data);
        SourceOf[Recorded.Output] = UpgradeSource(K);
        Result.Upgrades.push_back(std::move(Next));
    }
    if (!WellFormed(Result))
    {
        Core::Log::Warn("Inventar lokal: Edit mit {} Aufwertungen ungültig, Bearbeitung zurückgenommen", Result.Upgrades.size());
        RevertClient("Edit ungültig");
        return std::nullopt;
    }
    if (Result.Items.empty() && Result.NewSlots.empty())
    {
        Core::Log::Info("Inventar lokal: Bearbeitung ohne Änderung beendet");
        Session.State = Phase::Idle;
        return std::nullopt;
    }
    if (!Session.NextId)
        Session.NextId = (uint64_t(GetTickCount64()) << 16) | 1;
    Result.Id = ++Session.NextId;
    Session.State = Phase::Committed;
    Core::Log::Info("Inventar lokal: Edit {} mit {} Items, {} neuen Feldern, {} Aufwertungen{}", Result.Id, Result.Items.size(), Result.NewSlots.size(),
                    Result.Upgrades.size(), Result.Choice ? ", Belohnung " + std::to_string(*Result.Choice) : std::string{});
    for (const ItemState& Item : Result.Items)
        Core::Log::Info("Inventar lokal:   Quelle {} vorhanden {} Rucksack {} Drehung {} Pivot ({}, {}) {} Felder", Item.Source, Item.Present, Item.InBackpack,
                        Item.Turns, Item.Pivot.first, Item.Pivot.second, Item.Tiles.size());
    return Result;
}

void RevertClient(const char* Why)
{
    if (Session.State == Phase::Idle)
        return;
    Session.State = Phase::Idle; // RPCs aus dem Zurückrollen laufen über Apply, nicht über RouteClientRpc
    EndHands(true);
    const std::vector<uintptr_t> Now = ItemsOf(Session.Inventory);
    const std::vector<SlotInfo> Slots = SlotsOf(Session.Inventory);
    std::vector<uintptr_t> Items;
    std::vector<ItemState> Current, Target;
    for (size_t i = 0; i < Session.Items.size(); ++i)
    {
        if (!Alive(Session.Items[i]))
            continue;
        ItemState State = ReadItem(Session.Items[i], Slots, static_cast<uint16_t>(i));
        State.Present = Contains(Now, Session.Items[i]);
        if (!State.Present)
            State.InBackpack = false;
        Items.push_back(Session.Items[i]);
        Current.push_back(std::move(State));
        Target.push_back(Session.States[i]);
    }
    // Lokale Belohnungen und Ausgaben lokaler Aufwertungen wieder hinaus.
    std::vector<uintptr_t> Local = Session.Choices;
    for (const auto& Next : Session.Upgrades)
        Local.push_back(Next.Output);
    for (const uintptr_t Choice : Local)
        if (Alive(Choice) && Contains(Now, Choice) && std::count(Items.begin(), Items.end(), Choice) == 0)
        {
            Items.push_back(Choice);
            Current.push_back(ReadItem(Choice, Slots, ChoiceSource));
            ItemState Gone;
            Gone.Source = ChoiceSource;
            Gone.Present = false;
            Target.push_back(Gone);
        }
    std::vector<uintptr_t> Destroy;
    for (const SlotInfo& Slot : Slots)
        if (std::find(Session.Slots.begin(), Session.Slots.end(), Slot.At) == Session.Slots.end())
            Destroy.push_back(Slot.Slot);
    const bool Ok = Apply(Session.Inventory, Items, Current, Target, {}, Destroy, true);
    Core::Log::Info("Inventar lokal: Ausgangsstand wiederhergestellt ({}), vollständig={}", Why, Ok);
}

void ForgetClient()
{
    if (Session.State == Phase::Committed)
        Core::Log::Info("Inventar lokal: Host hat übernommen");
    Session.State = Phase::Idle;
    Session.Items.clear();
    Session.Choices.clear();
    Session.Upgrades.clear();
    EndHands(false);
}

bool RouteClientRpc(uintptr_t Object, uintptr_t Fn, void* Params)
{
    if (Session.State == Phase::Idle || !Fn || CurrentRole() != NetRole::Client)
        return false;
    const Known& Info = Classify(Fn);
    if (Info.Name.empty() || Info.Type == Kind::Movement)
        return false;
    // Jede verworfene Funktion einmal je Bearbeitung nennen, damit Ziehen und Drehen die Zeilen nicht verbrauchen.
    const auto Drop = [&]
    {
        if (std::find(Session.Dropped.begin(), Session.Dropped.end(), Info.Name) == Session.Dropped.end())
        {
            Session.Dropped.push_back(Info.Name);
            Core::Log::Info("Inventar lokal: {} während der Bearbeitung verworfen", Info.Name);
        }
        return true;
    };
    if (Info.Type == Kind::Foreign)
        return Drop();
    const uintptr_t Item = Info.Type == Kind::ItemRpc ? Object : Params ? Memory::Read<uintptr_t>(reinterpret_cast<uintptr_t>(Params)).value_or(0) : 0;
    const bool Ours = Info.Type == Kind::ItemRpc ? BelongsToSession(Object) : Object == Session.Inventory;
    if (!Ours || Info.Type == Kind::Other)
        return Drop();
    if (Session.State == Phase::Committed)
    {
        LogOp(Info.Name + " nach dem Bestätigen verworfen");
        return true;
    }
    if (Info.Type == Kind::AddEffect)
    {
        if (Item && !Contains(Session.Effects, Item))
            Session.Effects.push_back(Item);
        LogOp(Info.Name + " vorgemerkt, " + ItemLabel(Item));
        return true;
    }
    if (Info.Type == Kind::RemoveFromInventory && Params)
    {
        // Ausgangs-Items nicht zerstören: Bei Ablehnung kommen sie zurück, beim Übernehmen zerstört sie der Host.
        const uintptr_t At = reinterpret_cast<uintptr_t>(Params);
        Session.RemoveContext[Item] = Memory::Read<uint8_t>(At + 8).value_or(0);
        const bool Keep = false;
        Memory::WriteBytes(At + 9, &Keep, sizeof(Keep));
    }
    const bool Slots = Info.Name.ends_with("CreateSlotsFromBagItem");
    LogOp(Info.Name + " lokal, " +
          (Slots ? std::to_string(Params ? Memory::Read<int32_t>(reinterpret_cast<uintptr_t>(Params) + 8).value_or(0) : 0) + " Felder"
                 : ItemLabel(Info.Type == Kind::ItemRpc ? Object : Item)));
    const std::vector<uintptr_t> OldSlots = Slots ? SlotPointers(Object) : std::vector<uintptr_t>{};
    Patches::ProcessEventLocal(Object, Fn, Params);
    if (Slots)
        NotifySlots(Object, OldSlots);
    if (Info.Type == Kind::InventoryRpc)
        PreviewHands();
    return true;
}
}
