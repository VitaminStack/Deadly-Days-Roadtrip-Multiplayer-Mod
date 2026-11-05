// Airdrop-Auswahl für Host und Client ohne Koop-Modus, siehe src/Game/README.md#airdrop-auswahl.
//
// Host: Die Kiste läuft wie im Solo-Spiel, öffnet sie der Remote-Spieler, bekommt die Figur des Hosts die Auswahl.
// UInventoryComponent::SetRewardItems (0x5979B30) zeigt die Choices im nativen Panel; die Mod merkt sie sich als
// Sitzung und schickt sie über den Kontrollkanal an den Client. Das Bestätigen des Hosts (InventoryPanel.
// HandleConfirmButton, 0x5A6CB20) hält die Mod zurück, bis der Client gewählt hat; so lange bleibt das Spiel pausiert.
// Die Wahl des Clients legt der Host neu in dessen repliziertes Inventar.
//
// Client: Die Mod legt lokale Instanzen derselben Items an und zeigt sie über SetRewardItems im nativen Panel. Nach dem
// Bestätigen ist die gewählte Instanz keine Belohnung mehr; ihren Index schickt der Client an den Host und entfernt die
// lokalen Instanzen wieder.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <optional>
#include <string>
#include <utility>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/RewardAdapter.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t SetRewardItems = 0x5979B30;
constexpr uintptr_t PanelConfirm = 0x5A6CB20;
constexpr uintptr_t PanelSetupInventory = 0x5A8BAB0;
// UInventoryComponent::CreateItemInstance, nativ. Die UFunction ist BlueprintAuthorityOnly, ProcessEvent überspringt
// sie beim Client.
constexpr uintptr_t CreateItemInstance = 0x5959700;
constexpr uintptr_t MemoryFree = 0x12F8400;

// UInventoryItemInstance, UInventoryItemData, URarity, UInventoryComponent
constexpr uintptr_t ItemData = 0xE0;
constexpr uintptr_t ItemBroken = 0x110;
constexpr uintptr_t ItemRarity = 0x118;
constexpr uintptr_t ItemInBackpack = 0x180;
constexpr uintptr_t ItemIsReward = 0x182;
constexpr uintptr_t DataName = 0x40;
constexpr uintptr_t RarityName = 0x28;
constexpr uintptr_t InventoryPlayerState = 0x228;
constexpr uintptr_t InventoryItems = 0x120;
// UGameInstance.LocalPlayers, UPlayer.PlayerController, AController.PlayerState, AController.Pawn
constexpr uintptr_t GameInstanceLocalPlayers = 0x38;
constexpr uintptr_t PlayerController = 0x30;
constexpr uintptr_t ControllerPlayerState = 0x2B0;
constexpr uintptr_t ControllerPawn = 0x2E8;
// BP_DD_Character_C.RewardQueue, URewardQueueComponent: Byte +0x108 = Kiste offen (OpenNextReward setzt/löscht)
constexpr uintptr_t CharacterRewardQueue = 0x980;
constexpr uintptr_t QueueBusy = 0x108;
constexpr uint32_t GarbageOrUnreachableFlags = 0x30;

struct ItemArray
{
    uintptr_t* Data;
    int32_t Count, Max;
};
using SetRewardsFn = void(__fastcall*)(uintptr_t, ItemArray*, int32_t);
using ConfirmFn = void(__fastcall*)(uintptr_t);
using CreateFn = uintptr_t(__fastcall*)(uintptr_t, uintptr_t, bool, uintptr_t);
using FreeFn = void(__fastcall*)(void*);
SetRewardsFn OriginalSetRewards = nullptr;
ConfirmFn OriginalConfirm = nullptr;
ConfirmFn OriginalSetupInventory = nullptr;

std::atomic<bool> RemoteReady = false;

// Host: aktuelle Auswahl.
struct
{
    uint64_t Session = 0; // Zähler, 0 vor der ersten Auswahl
    bool Active = false;
    uintptr_t Inventory = 0;
    std::vector<uintptr_t> Items;
    std::vector<Game::Rewards::ChoiceItem> Choices;
    std::vector<std::pair<uintptr_t, uintptr_t>> Data; // ItemData, Rarity je Choice
    int32_t Level = 0;
    std::optional<uint32_t> ClientPick;
    uintptr_t HeldPanel = 0;
    int ReplayCount = 0;        // Versuche, das zurückgehaltene Bestätigen nachzuholen
    ULONGLONG NextReplay = 0;
} Host;
// Host: zuletzt für den Client angelegte Wahl, bis seine Ablage im Rucksack kommt (auch nach Ende der Sitzung).
struct
{
    uint64_t Session = 0;
    uintptr_t Item = 0, Inventory = 0;
} ClientItem;

// Client: Anzeige der Host-Choices im nativen Panel, nur lokale Instanzen. Das Inventar arbeitet über Server-RPCs;
// mit einer lokalen Instanz bekäme der Host einen Null-Zeiger (Absturz in AddToBackpack). Die Mod hält solche RPCs
// zurück und nimmt das erste Ablegen einer Choice als Wahl.
struct
{
    uint64_t Session = 0, Revision = 0;
    uintptr_t Inventory = 0;
    std::vector<uintptr_t> Items;
    std::optional<uint32_t> Taken; // per RPC abgelegt, noch nicht gemeldet
    std::optional<uint32_t> Pick;  // gemeldet an die Sync-Schicht
} Shown;
std::atomic<bool> ClientShowing = false;
// Nach dem Schließen schickt das Panel noch RPCs für die gewählte Instanz (Ablegen im Rucksack). Die alten Instanzen
// bleiben deshalb noch eine Weile gefiltert.
std::vector<uintptr_t> Stale;
ULONGLONG StaleUntil = 0;
// Client: gewählte lokale Instanz und ihre Ablage aus OccupyBackpackSlots/ForceEquipItemInBackpack.
uintptr_t PickedItem = 0, PlacedItem = 0;
uint64_t PickedSession = 0;
std::optional<Game::Rewards::Placement> PickedPlacement;
constexpr uintptr_t ItemPivotTile = 0x28, ItemTurns = 0xD8, SlotCoordinates = 0x28;
constexpr size_t MaxSlots = 32;
constexpr uint32_t FuncNet = 0x40;
constexpr uintptr_t FunctionFlags = 0xB0, FunctionParmsSize = 0xB6;
constexpr uintptr_t PanelIsDragging = 0x790;

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachableFlags) & GarbageOrUnreachableFlags);
}

uintptr_t Pointer(uintptr_t Address)
{
    return Memory::ReadPointer(Address).value_or(0);
}

uint64_t Revision(const std::vector<uintptr_t>& Items)
{
    uint64_t Hash = 1469598103934665603ull;
    for (const uintptr_t Item : Items)
        Hash = (Hash ^ Item) * 1099511628211ull;
    return Hash ? Hash : 1;
}

// Nur einmal suchen, auch wenn nicht gefunden (1 = fehlt): läuft in jedem ProcessEvent.
uintptr_t Function_(uintptr_t& Cache, const char* Name)
{
    if (!Cache)
        if (!(Cache = Objects::FindObject(Name, "InventoryComponent")))
            Cache = 1;
    return Cache;
}

uintptr_t Function(uintptr_t& Cache, const char* Name, const char* Outer)
{
    if (!Cache)
        Cache = Objects::FindObject(Name, Outer);
    return Cache;
}

// FText über KismetTextLibrary.Conv_TextToString. Das FString-Ergebnis gibt FMemory::Free frei.
std::string TextAt(uintptr_t Address)
{
    static uintptr_t Library = 0, Convert = 0;
    if (!Library)
        Library = Objects::FindNamed("Default__KismetTextLibrary");
    if (!Library || !Function(Convert, "Conv_TextToString", "KismetTextLibrary"))
        return {};
    struct
    {
        uint8_t Text[0x10];
        wchar_t* Data;
        int32_t Count, Max;
    } Params{};
    static_assert(sizeof(Params) == 0x20);
    if (!Memory::ReadBytes(Address, Params.Text, sizeof(Params.Text)))
        return {};
    Objects::ProcessEvent(Library, Convert, &Params);
    std::string Result;
    if (Params.Data && Params.Count > 1 && Params.Count < 256)
    {
        const int Size = WideCharToMultiByte(CP_UTF8, 0, Params.Data, Params.Count - 1, nullptr, 0, nullptr, nullptr);
        Result.resize(static_cast<size_t>(std::max(Size, 0)));
        WideCharToMultiByte(CP_UTF8, 0, Params.Data, Params.Count - 1, Result.data(), Size, nullptr, nullptr);
    }
    if (Params.Data)
        reinterpret_cast<FreeFn>(At(MemoryFree))(Params.Data);
    return Result;
}

std::string ItemName(uintptr_t Data, uintptr_t Rarity)
{
    std::string Name = Data ? TextAt(Data + DataName) : std::string{};
    if (Name.empty())
        Name = Data ? Memory::NameOf(Data) : "?";
    if (Rarity)
        if (const std::string Text = TextAt(Rarity + RarityName); !Text.empty())
            Name += " (" + Text + ")";
    return Name;
}

uintptr_t InventoryOf(uintptr_t PlayerState)
{
    if (!PlayerState)
        return 0;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (Object && (Pointer(Object + InventoryPlayerState) == PlayerState || Pointer(Object + Offsets::UObject::Outer) == PlayerState) &&
            Alive(Object) && Objects::IsA(Object, "InventoryComponent"))
            return Object;
    }
    return 0;
}

uintptr_t InventoryOfController(uintptr_t Controller)
{
    return InventoryOf(Controller ? Pointer(Controller + ControllerPlayerState) : 0);
}

uintptr_t LocalController()
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t Instance = World ? Pointer(World + Offsets::UWorld::OwningGameInstance) : 0;
    const uintptr_t Players = Instance ? Pointer(Instance + GameInstanceLocalPlayers) : 0;
    const uintptr_t Local = Players ? Pointer(Players) : 0;
    return Local ? Pointer(Local + PlayerController) : 0;
}

bool InInventory(uintptr_t Inventory, uintptr_t Item)
{
    static uintptr_t Fn = 0;
    if (!Inventory || !Function(Fn, "IsInInventory", "InventoryComponent"))
        return false;
    struct
    {
        uintptr_t Item;
        bool Result;
    } Params{Item, false};
    Objects::ProcessEvent(Inventory, Fn, &Params);
    return Params.Result;
}

bool IsReward(uintptr_t Item)
{
    return Alive(Item) && Memory::Read<uint8_t>(Item + ItemIsReward).value_or(0);
}

// Datenobjekte der Choices über ihre Objektnamen, ein Durchlauf über das Objekt-Array.
std::vector<std::pair<uintptr_t, uintptr_t>> ResolveChoices(const std::vector<Game::Rewards::ChoiceItem>& Choices)
{
    std::vector<std::pair<uintptr_t, uintptr_t>> Result(Choices.size());
    size_t Missing = 0;
    for (const auto& Choice : Choices)
        Missing += 1 + !Choice.Rarity.empty();
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total && Missing; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object)
            continue;
        const std::string Name = Memory::NameOf(Object);
        for (size_t c = 0; c < Choices.size(); ++c)
        {
            if (!Result[c].first && Name == Choices[c].Data && Objects::IsA(Object, "InventoryItemData"))
            {
                Result[c].first = Object;
                --Missing;
            }
            if (!Result[c].second && !Choices[c].Rarity.empty() && Name == Choices[c].Rarity && Objects::IsA(Object, "Rarity"))
            {
                Result[c].second = Object;
                --Missing;
            }
        }
    }
    return Result;
}

void EndHostSession(const char* Why)
{
    if (!Host.Active)
        return;
    if (!Host.ClientPick && RemoteReady.load())
        Core::Log::Warn("Airdrop-Auswahl: Sitzung {} endet ohne Wahl des Clients ({})", Host.Session, Why);
    else
        Core::Log::Info("Airdrop-Auswahl: Sitzung {} beendet ({})", Host.Session, Why);
    const uint64_t Last = Host.Session;
    Host = {};
    Host.Session = Last;
}

void __fastcall SetRewardsHook(uintptr_t Inventory, ItemArray* Items, int32_t Level)
{
    if (CurrentRole() == NetRole::Host && Items && Items->Data && Items->Count > 0 && Items->Count <= 8)
    {
        EndHostSession("neue Auswahl");
        Host.Active = true;
        ++Host.Session;
        Host.Inventory = Inventory;
        Host.Level = Level;
        for (int32_t i = 0; i < Items->Count; ++i)
        {
            const uintptr_t Item = Items->Data[i];
            const uintptr_t Data = Item ? Pointer(Item + ItemData) : 0, Rarity = Item ? Pointer(Item + ItemRarity) : 0;
            Host.Items.push_back(Item);
            Host.Data.emplace_back(Data, Rarity);
            Host.Choices.push_back({ItemName(Data, Rarity), Data ? Memory::NameOf(Data) : std::string{}, Rarity ? Memory::NameOf(Rarity) : std::string{},
                                    Item && Memory::Read<uint8_t>(Item + ItemBroken).value_or(0) != 0});
        }
        Core::Log::Info("Airdrop-Auswahl: Sitzung {} mit {} Choices, Stufe {}", Host.Session, Host.Items.size(), Level);
    }
    OriginalSetRewards(Inventory, Items, Level);
}

// Lokale Instanzen direkt aus MyItems nehmen, ohne RemoveFromInventory (Server-RPC).
void DropLocal()
{
    const uintptr_t Array = Shown.Inventory + InventoryItems;
    const uintptr_t Data = Alive(Shown.Inventory) ? Pointer(Array) : 0;
    const int32_t Count = Data ? Memory::Read<int32_t>(Array + 8).value_or(0) : 0;
    int32_t Kept = 0;
    for (int32_t i = 0; i < Count && Count <= 4096; ++i)
    {
        const uintptr_t Item = Pointer(Data + 8 * i);
        if (std::find(Shown.Items.begin(), Shown.Items.end(), Item) != Shown.Items.end())
            continue;
        if (Kept != i)
            Memory::WriteBytes(Data + 8 * Kept, &Item, sizeof(Item));
        ++Kept;
    }
    if (Kept != Count)
        Memory::WriteBytes(Array + 8, &Kept, sizeof(Kept));
    if (Shown.Pick && *Shown.Pick < Shown.Items.size())
    {
        PickedItem = Shown.Items[*Shown.Pick];
        PickedSession = Shown.Session;
        Core::Log::Info("Airdrop-Auswahl: gewählte lokale Instanz {:#x}", PickedItem);
        if (PlacedItem != PickedItem)
            PickedPlacement.reset();
    }
    if (!Shown.Items.empty())
    {
        Stale = Shown.Items;
        StaleUntil = GetTickCount64() + 10000;
    }
    Shown.Items.clear();
    ClientShowing = false;
}

std::string FunctionName(uintptr_t Function)
{
    return Memory::NameOf(Function);
}

bool ClientConfirm(uintptr_t Panel)
{
    if (Shown.Items.empty())
        return false;
    // HandleConfirmButton selbst ignoriert Bestätigen während eines Drags. In diesem
    // Fall darf auch die Mod noch keine Instanz entfernen oder Wahl veröffentlichen.
    if (Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        return true;
    if (Shown.Taken)
    {
        OriginalConfirm(Panel); // RPCs mit lokalen Instanzen hält FilterRewardEvent zurück
        Shown.Pick = std::exchange(Shown.Taken, std::nullopt);
        Core::Log::Info("Airdrop-Auswahl: Client bestätigt im Panel, Wahl={}", *Shown.Pick);
        DropLocal();
        return true;
    }
    if (Shown.Items.empty())
        return false;
    auto Flags = [&]
    {
        std::string Text;
        for (const uintptr_t Item : Shown.Items)
            Text += std::to_string(InInventory(Shown.Inventory, Item)) + "/" + std::to_string(Memory::Read<uint8_t>(Item + ItemIsReward).value_or(9)) +
                    "/" + std::to_string(Memory::Read<uint8_t>(Item + ItemInBackpack).value_or(9)) + " ";
        return Text;
    };
    const std::string Before = Flags();
    OriginalConfirm(Panel);
    // Gewählt ist die Instanz, die nach dem Bestätigen im Inventar liegt und keine Belohnung mehr ist.
    std::optional<uint32_t> Pick;
    for (uint32_t i = 0; i < Shown.Items.size() && !Pick; ++i)
        if (Alive(Shown.Items[i]) && InInventory(Shown.Inventory, Shown.Items[i]) && !IsReward(Shown.Items[i]))
            Pick = i;
    Core::Log::Info("Airdrop-Auswahl: Client bestätigt im Panel, Wahl={} (im Inventar/Belohnung/Rucksack vorher {}nachher {})",
                    Pick ? std::to_string(*Pick) : std::string("keine"), Before, Flags());
    if (!Pick)
        return true;
    Shown.Pick = Pick;
    DropLocal();
    return true;
}

void __fastcall ConfirmHook(uintptr_t Panel)
{
    if (CurrentRole() == NetRole::Client && ClientConfirm(Panel))
        return;
    if (CurrentRole() == NetRole::Host && Host.Active && !Host.ClientPick && RemoteReady.load())
    {
        if (Host.HeldPanel != Panel)
            Core::Log::Info("Airdrop-Auswahl: Host hat bestätigt, wartet auf die Wahl des Clients");
        Host.HeldPanel = Panel;
        return;
    }
    OriginalConfirm(Panel);
}

// CachedBackpackLocalPosition (+0x150) ist NICHT repliziert. Die echte Instanz
// aus dem Host-Inventar kommt daher mit (0, 0), obwohl ZeroPivotTile und Slots
// korrekt replizieren. SetupInventory zeichnet sonst Revolver und Pistole
// übereinander. Den nativen UI-Snap nutzen, nicht Inventar oder Pixelraster erfinden.
void RestoreBackpackVisuals(uintptr_t Panel)
{
    if (CurrentRole() != NetRole::Client || !Alive(Panel) || Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        return;
    constexpr uintptr_t PanelInventory = 0x370;
    const uintptr_t Inventory = Pointer(Panel + PanelInventory);
    const uintptr_t Controller = LocalController();
    if (!Alive(Inventory) || !Controller || Pointer(Inventory + InventoryPlayerState) != Pointer(Controller + ControllerPlayerState))
        return;
    static uintptr_t GetContainer = 0, GetRepresentation = 0, Snap = 0;
    if (!Function(GetContainer, "GetBackpackContainer", "InventoryPanel") ||
        !Function(GetRepresentation, "TryGetItemRepresentation", "InventoryPanel") ||
        !Function(Snap, "SnapRepresentationToTile", "InventoryPanel"))
        return;
    uintptr_t Container = 0;
    Objects::ProcessEvent(Panel, GetContainer, &Container);
    if (!Alive(Container))
        return;
    const uintptr_t Data = Pointer(Inventory + InventoryItems);
    const int32_t Count = Memory::Read<int32_t>(Inventory + InventoryItems + 8).value_or(-1);
    if (!Data || Count <= 0 || Count > 4096)
        return;
    unsigned Restored = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Item = Pointer(Data + 8 * i);
        if (!Alive(Item) || IsReward(Item) || !Memory::Read<uint8_t>(Item + ItemInBackpack).value_or(0))
            continue;
        struct
        {
            uintptr_t Item, Representation;
            bool Found;
            uint8_t Pad[7];
        } Lookup{Item, 0, false, {}};
        static_assert(sizeof(Lookup) == 0x18);
        Objects::ProcessEvent(Panel, GetRepresentation, &Lookup);
        if (!Lookup.Found || !Alive(Lookup.Representation))
            continue;
        struct
        {
            uintptr_t Representation;
            int32_t X, Y;
            uintptr_t Container;
        } Params{Lookup.Representation, 0, 0, Container};
        static_assert(sizeof(Params) == 0x18);
        if (!Memory::ReadBytes(Item + ItemPivotTile, &Params.X, 8))
            continue;
        Objects::ProcessEvent(Panel, Snap, &Params);
        ++Restored;
    }
    if (Restored)
        Core::Log::Info("Airdrop-Auswahl: {} Rucksack-Anzeige(n) aus replizierten Pivot-Feldern positioniert", Restored);
}

void __fastcall SetupInventoryHook(uintptr_t Panel)
{
    OriginalSetupInventory(Panel);
    RestoreBackpackVisuals(Panel);
}

bool CheckSetRewards()
{
    return Code::Expect(At(SetRewardItems), {0x48, 0x89, 0x5C, 0x24, 0x18, 0x48, 0x89, 0x54, 0x24, 0x10}, "InventoryComponent.SetRewardItems");
}
bool CheckConfirm()
{
    if (!Code::Expect(At(CreateItemInstance), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x55, 0x56, 0x57, 0x48, 0x81, 0xEC}, "InventoryComponent::CreateItemInstance"))
        return false;
    return Code::Expect(At(PanelConfirm), {0x40, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48, 0x8B, 0xF9}, "InventoryPanel.HandleConfirmButton") &&
           Code::Expect(At(MemoryFree), {0x48, 0x85, 0xC9, 0x74, 0x2E}, "FMemory::Free");
}
bool ApplySetRewards(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(SetRewardItems)), reinterpret_cast<void*>(&SetRewardsHook),
                                reinterpret_cast<void**>(&OriginalSetRewards));
}
bool ApplyConfirm(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(PanelConfirm)), reinterpret_cast<void*>(&ConfirmHook),
                                reinterpret_cast<void**>(&OriginalConfirm));
}
bool CheckSetupInventory()
{
    return Code::Expect(At(PanelSetupInventory), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18}, "InventoryPanel.SetupInventory");
}
bool ApplySetupInventory(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(PanelSetupInventory)), reinterpret_cast<void*>(&SetupInventoryHook),
                                reinterpret_cast<void**>(&OriginalSetupInventory));
}
const Patch List[] = {
    {"Airdrop-Auswahl: Choices des Hosts mitlesen", CheckSetRewards, ApplySetRewards},
    {"Airdrop-Auswahl: Bestätigen bis zur Wahl des Partners halten", CheckConfirm, ApplyConfirm},
    {"Airdrop-Auswahl: Rucksack-Anzeige aus replizierten Feldern aufbauen", CheckSetupInventory, ApplySetupInventory},
};
}

std::span<const Patch> RewardPatches()
{
    return List;
}

// Aus RewardChest.OpenAirdropChoices beim Host: Öffnet die Figur des Remote-Spielers die Kiste, bekommt die Figur des
// Hosts die native Auswahl, der Client seine über den Kontrollkanal.
// Host: Inventar-RPCs eines Clients mit einer Instanz, die es beim Host nicht gibt, kommen mit Null an. Die nativen
// Implementierungen lesen sie ungeprüft (Absturz in AddToBackpack). Solche Aufrufe verwirft die Mod.
bool NullItemRpc(uintptr_t Function, void* Params)
{
    static uintptr_t Cache[5]{};
    static const char* const Names[5]{"AddToBackpack", "AddToInventory", "RemoveFromInventory", "OccupyBackpackSlots", "ForceEquipItemInBackpack"};
    for (size_t i = 0; i < 5; ++i)
        if (Function && Function == Function_(Cache[i], Names[i]))
        {
            if (Params && !Memory::Read<uintptr_t>(reinterpret_cast<uintptr_t>(Params)).value_or(1))
            {
                Core::Log::Warn("Airdrop-Auswahl: {} ohne Item vom Client verworfen", Names[i]);
                return true;
            }
            return false;
        }
    return false;
}

// Client: Ablage einer Choice im Rucksack aus OccupyBackpackSlots (Item, Slots) oder ForceEquipItemInBackpack
// (Item, CenterPosition, Slots) merken. Drehung und Pivot stehen da schon in der lokalen Instanz.
void RememberPlacement(const std::string& Name, uintptr_t Params)
{
    const uintptr_t SlotsAt = Name == "OccupyBackpackSlots" ? 0x8 : Name == "ForceEquipItemInBackpack" ? 0x18 : 0;
    const uintptr_t Item = SlotsAt ? Pointer(Params) : 0;
    if (!Item)
        return;
    const uintptr_t Data = Pointer(Params + SlotsAt);
    const int32_t Count = Memory::Read<int32_t>(Params + SlotsAt + 8).value_or(0);
    if (!Data || Count <= 0 || Count > static_cast<int32_t>(MaxSlots))
        return;
    Game::Rewards::Placement Value;
    Value.Turns = Memory::Read<int32_t>(Item + ItemTurns).value_or(0);
    Value.PivotX = Memory::Read<int32_t>(Item + ItemPivotTile).value_or(0);
    Value.PivotY = Memory::Read<int32_t>(Item + ItemPivotTile + 4).value_or(0);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Slot = Pointer(Data + 8 * i);
        if (!Slot)
            return;
        Value.Slots.emplace_back(Memory::Read<int32_t>(Slot + SlotCoordinates).value_or(0), Memory::Read<int32_t>(Slot + SlotCoordinates + 4).value_or(0));
    }
    Core::Log::Info("Airdrop-Auswahl: Ablage im Rucksack gemerkt, {} Felder, Drehung {}, Instanz {:#x}, gewählt {:#x}", Value.Slots.size(),
                    Value.Turns, Item, PickedItem);
    PlacedItem = Item;
    PickedPlacement = std::move(Value);
}

bool FilterRewardEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    if (CurrentRole() == NetRole::Host)
        return RemoteReady.load(std::memory_order_relaxed) && NullItemRpc(Function, Params);
    const bool Recent = !Stale.empty() && GetTickCount64() < StaleUntil;
    if ((!ClientShowing.load(std::memory_order_relaxed) && !Recent) || !Params || !(Memory::Read<uint32_t>(Function + FunctionFlags).value_or(0) & FuncNet))
        return false;
    const uint16_t Size = Memory::Read<uint16_t>(Function + FunctionParmsSize).value_or(0);
    std::optional<uint32_t> Index;
    for (uint16_t At = 0; At + 8 <= Size && At < 0x100 && !Index; At += 8)
    {
        const uintptr_t Value = Memory::Read<uintptr_t>(reinterpret_cast<uintptr_t>(Params) + At).value_or(0);
        for (uint32_t i = 0; i < Shown.Items.size(); ++i)
            if (Value && Shown.Items[i] == Value)
                Index = i;
        if (!Index && Recent && Value && std::find(Stale.begin(), Stale.end(), Value) != Stale.end())
        {
            const std::string Name = FunctionName(Function);
            Core::Log::Info("Airdrop-Auswahl: RPC {} mit geschlossener lokaler Choice zurückgehalten", Name);
            RememberPlacement(Name, reinterpret_cast<uintptr_t>(Params));
            return true;
        }
    }
    if (!Index)
        return false;
    const std::string Name = FunctionName(Function);
    const bool Placed = Name == "AddToBackpack" || Name == "AddToInventory" || Name == "OccupyBackpackSlots" || Name == "ForceEquipItemInBackpack";
    Core::Log::Info("Airdrop-Auswahl: RPC {} mit lokaler Choice {} zurückgehalten{}", Name, *Index, Placed && !Shown.Taken ? ", gilt als Wahl" : "");
    if (Placed && !Shown.Taken)
        Shown.Taken = Index;
    RememberPlacement(Name, reinterpret_cast<uintptr_t>(Params));
    (void)Object;
    return true;
}

uintptr_t RewardCharacter(uintptr_t Character)
{
    uintptr_t Coop = 0, HostController = 0, Remote = 0;
    if (CurrentRole() != NetRole::Host || !RemoteReady.load() || !BoundRemoteP2(Coop, HostController, Remote))
        return Character;
    const uintptr_t RemotePawn = Pointer(Remote + ControllerPawn), HostPawn = Pointer(HostController + ControllerPawn);
    if (Character != RemotePawn || !HostPawn)
        return Character;
    Core::Log::Info("Airdrop-Auswahl: Kiste vom Remote-Spieler geöffnet, Auswahl über die Figur des Hosts");
    return HostPawn;
}
}

namespace Game::Rewards
{
using namespace Game::Patches;

void SetRemoteReady(bool Ready)
{
    if (RemoteReady.exchange(Ready) != Ready)
        Core::Log::Info("Airdrop-Auswahl: Client mit Mod {}", Ready ? "verbunden, Airdrop-Auswahl für beide" : "fehlt, Auswahl nur beim Host");
}

std::optional<Offer> ReadOffer()
{
    if (CurrentRole() != NetRole::Host || !Host.Active)
        return std::nullopt;
    Offer Result;
    Result.Session = Host.Session;
    Result.Revision = Revision(Host.Items);
    Result.Level = Host.Level;
    Result.Choices = Host.Choices;
    Result.ClientClaimed = Host.ClientPick.has_value();
    Result.HostClaimed = false;
    Result.HostWaiting = Host.HeldPanel != 0;
    return Result;
}

bool ClaimForClient(uint64_t SessionId, uint64_t Expected, uint32_t Index)
{
    if (CurrentRole() != NetRole::Host || !Host.Active || SessionId != Host.Session || Host.ClientPick || Revision(Host.Items) != Expected ||
        Index >= Host.Data.size() || !Host.Data[Index].first)
        return false;
    uintptr_t Coop = 0, HostController = 0, Remote = 0;
    const uintptr_t Inventory = BoundRemoteP2(Coop, HostController, Remote) ? InventoryOfController(Remote) : 0;
    static uintptr_t Create = 0;
    if (!Inventory || !Function(Create, "CreateAndAddItem", "InventoryComponent"))
    {
        Core::Log::Warn("Airdrop-Auswahl: Inventar des Clients nicht gefunden, Wahl {} verworfen", Index);
        return false;
    }
    struct
    {
        uintptr_t Data, Rarity;
        bool Broken, FlagAsNew, SystemAdd;
        uint8_t Pad;
        int32_t Position;
        bool AllowP2Reserve;
        uint8_t Pad2[7];
        uintptr_t Result;
    } Add{Host.Data[Index].first, Host.Data[Index].second, Host.Choices[Index].Broken, true, true, 0, -1, false, {}, 0};
    static_assert(sizeof(Add) == 0x28);
    Objects::ProcessEvent(Inventory, Create, &Add);
    Core::Log::Info("Airdrop-Auswahl: Client wählt {} ({}), ins Inventar des Clients gelegt={}", Index, Host.Choices[Index].Name, Add.Result != 0);
    if (!Add.Result)
        return false;
    Host.ClientPick = Index;
    ClientItem = {Host.Session, Add.Result, Inventory};
    return true;
}

bool PlaceForClient(uint64_t SessionId, const Placement& Value)
{
    if (CurrentRole() != NetRole::Host || !SessionId || SessionId != ClientItem.Session || !Alive(ClientItem.Item) ||
        !Alive(ClientItem.Inventory) || Value.Slots.empty() || Value.Slots.size() > MaxSlots)
        return false;
    static uintptr_t TryGet = 0, AddToBackpack = 0, Occupy = 0, SetTurns = 0, SetPivot = 0;
    if (!Function(TryGet, "TryGetSlotWithCoordinates", "InventoryComponent") || !Function(AddToBackpack, "AddToBackpack", "InventoryComponent") ||
        !Function(Occupy, "OccupyBackpackSlots", "InventoryComponent") ||
        !Function(SetTurns, "SetTurns", "InventoryItemInstance") || !Function(SetPivot, "SetZeroPivotTile", "InventoryItemInstance"))
        return false;
    std::vector<uintptr_t> Slots;
    for (const auto& [X, Y] : Value.Slots)
    {
        struct
        {
            int32_t X, Y;
            uintptr_t Slot;
            bool Found;
            uint8_t Pad[7];
        } Params{X, Y, 0, false, {}};
        static_assert(sizeof(Params) == 0x18);
        Objects::ProcessEvent(ClientItem.Inventory, TryGet, &Params);
        if (!Params.Found || !Params.Slot || Pointer(Params.Slot + 0x30))
        {
            Core::Log::Warn("Airdrop-Auswahl: Feld ({}, {}) im Rucksack des Clients {}, Wahl bleibt im Lager", X, Y, Params.Slot ? "belegt" : "fehlt");
            return false;
        }
        Slots.push_back(Params.Slot);
    }
    const auto [Session, Item, Inventory] = std::exchange(ClientItem, {});
    (void)Session;
    // Wie das Panel beim Client: Drehung und Pivot an der Instanz, dann AddToBackpack und OccupyBackpackSlots. Beim
    // Host laufen die Server-RPCs direkt, Turns und ZeroPivotTile replizieren.
    // Replizierte Werte über die nativen Setter. Der nicht replizierte
    // Pixelpositions-Cache wird separat beim Client-Panelaufbau wiederhergestellt.
    struct { int32_t Turns; bool Instant; uint8_t Pad[3]; } TurnsParams{Value.Turns, true, {}};
    Objects::ProcessEvent(Item, SetTurns, &TurnsParams);
    int32_t Pivot[2]{Value.PivotX, Value.PivotY};
    Objects::ProcessEvent(Item, SetPivot, Pivot);
    uintptr_t AddParams = Item;
    Objects::ProcessEvent(Inventory, AddToBackpack, &AddParams);
    struct
    {
        uintptr_t Item;
        ItemArray Slots;
    } OccupyParams{Item, {Slots.data(), static_cast<int32_t>(Slots.size()), static_cast<int32_t>(Slots.size())}};
    static_assert(sizeof(OccupyParams) == 0x18);
    Objects::ProcessEvent(Inventory, Occupy, &OccupyParams);
    const bool InBackpack = Memory::Read<uint8_t>(Item + ItemInBackpack).value_or(0) != 0;
    Core::Log::Info("Airdrop-Auswahl: Wahl des Clients in seinen Rucksack gelegt, {} Felder, im Rucksack={}", Slots.size(), InBackpack);
    return InBackpack;
}

// Kisten laufen über die RewardQueue der öffnenden Figur. Das Panel bestätigt nur in der Queue des Hosts
// (InventoryPanel.HandleConfirmButton ruft OpenNextReward der eigenen Queue). Die Queue von P2 bliebe nach einer
// vom Client geöffneten Kiste belegt (+0x108 gesetzt, CurrentRewardObject zerstört), und RegisterReward gibt keine
// weitere Kiste mehr frei. OpenNextReward ändert nichts, solange die aktuelle Kiste lebt und nicht fertig ist.
void AdvanceRemoteQueue()
{
    static ULONGLONG Next = 0;
    const ULONGLONG Now = GetTickCount64();
    if (Now < Next)
        return;
    Next = Now + 1000;
    uintptr_t Coop = 0, HostController = 0, Remote = 0;
    if (!BoundRemoteP2(Coop, HostController, Remote))
        return;
    const uintptr_t Pawn = Pointer(Remote + ControllerPawn);
    const uintptr_t Queue = Alive(Pawn) ? Pointer(Pawn + CharacterRewardQueue) : 0;
    static uintptr_t Open = 0;
    if (!Alive(Queue) || !Memory::Read<uint8_t>(Queue + QueueBusy).value_or(0) ||
        !Function(Open, "OpenNextReward", "RewardQueueComponent"))
        return;
    struct
    {
        bool Opened;
        uint8_t Pad[7];
    } Params{};
    Objects::ProcessEvent(Queue, Open, &Params);
    if (!Memory::Read<uint8_t>(Queue + QueueBusy).value_or(1) || Params.Opened)
        Core::Log::Info("Airdrop-Auswahl: RewardQueue des Remote-Spielers weitergeschaltet, nächste Kiste {}",
                        Params.Opened ? "freigegeben" : "keine");
}

void Tick()
{
    if (CurrentRole() != NetRole::Host)
        return;
    if (!Host.Active)
    {
        AdvanceRemoteQueue();
        return;
    }
    if (!Alive(Host.Inventory))
    {
        EndHostSession("Inventar weg");
        return;
    }
    // Zurückgehaltenes Bestätigen nachholen, sobald der Client gewählt hat oder nicht mehr verbunden ist.
    // HandleConfirmButton ignoriert den Aufruf während eines Drags (+0x790) oder ohne Koop-Autorität. Dann wäre der
    // Klick verloren und der Host müsste erneut bestätigen. Deshalb nicht im Drag nachholen und, solange die Sitzung
    // offen bleibt, alle 500 ms erneut, höchstens zehnmal.
    const auto SessionOpen = [] { return std::any_of(Host.Items.begin(), Host.Items.end(), [](uintptr_t Item) { return IsReward(Item) && InInventory(Host.Inventory, Item); }); };
    if (Host.HeldPanel && (Host.ClientPick || !RemoteReady.load()))
    {
        const uintptr_t Panel = Host.HeldPanel;
        const ULONGLONG Now = GetTickCount64();
        if (!Alive(Panel) || !Objects::IsA(Panel, "InventoryPanel") || Host.ReplayCount >= 10)
        {
            if (Host.ReplayCount >= 10)
                Core::Log::Warn("Airdrop-Auswahl: Bestätigen des Hosts nach 10 Versuchen nicht übernommen, Host muss selbst bestätigen");
            Host.HeldPanel = 0;
        }
        else if (Now >= Host.NextReplay && !Memory::Read<uint8_t>(Panel + PanelIsDragging).value_or(1))
        {
            ++Host.ReplayCount;
            Host.NextReplay = Now + 500;
            Core::Log::Info("Airdrop-Auswahl: Bestätigen des Hosts nachgeholt (Versuch {})", Host.ReplayCount);
            OriginalConfirm(Panel);
            if (!SessionOpen())
                Host.HeldPanel = 0;
        }
    }
    // Ende: keine Choice mehr als Belohnung im Inventar des Hosts, das Panel hat bestätigt.
    if (!SessionOpen())
        EndHostSession("Host hat bestätigt");
}

bool ShowForClient(uint64_t SessionId, uint64_t Expected, int32_t Level, const std::vector<ChoiceItem>& Items)
{
    if (CurrentRole() != NetRole::Client)
        return false;
    if (Shown.Session == SessionId && Shown.Revision == Expected && (!Shown.Items.empty() || Shown.Pick))
        return true;
    CloseForClient();
    static uintptr_t SetRewards = 0;
    const uintptr_t Inventory = InventoryOfController(LocalController());
    if (!Inventory || Items.empty() || !Function(SetRewards, "SetRewardItems", "InventoryComponent"))
    {
        Core::Log::Warn("Airdrop-Auswahl: Panel beim Client nicht möglich (Inventar {})", Inventory != 0);
        return false;
    }
    const auto Resolved = ResolveChoices(Items);
    std::vector<uintptr_t> Instances;
    for (size_t i = 0; i < Items.size(); ++i)
    {
        const uintptr_t Instance =
            Resolved[i].first ? reinterpret_cast<CreateFn>(At(CreateItemInstance))(Inventory, Resolved[i].first, false, Resolved[i].second) : 0;
        if (!Instance)
        {
            Core::Log::Warn("Airdrop-Auswahl: Choice {} ({}) beim Client nicht anlegbar, Daten={}@{:#x} Seltenheit={}@{:#x}", i, Items[i].Name, Items[i].Data,
                            Resolved[i].first, Items[i].Rarity, Resolved[i].second);
            return false;
        }
        const uint8_t Broken = Items[i].Broken;
        Memory::WriteBytes(Instance + ItemBroken, &Broken, 1);
        Instances.push_back(Instance);
    }
    ItemArray Array{Instances.data(), static_cast<int32_t>(Instances.size()), static_cast<int32_t>(Instances.size())};
    struct
    {
        ItemArray Items;
        int32_t Level;
        uint8_t Pad[4];
    } Params{Array, Level, {}};
    static_assert(sizeof(Params) == 0x18);
    Objects::ProcessEvent(Inventory, SetRewards, &Params);
    Shown = {SessionId, Expected, Inventory, Instances, std::nullopt, std::nullopt};
    ClientShowing = true;
    Core::Log::Info("Airdrop-Auswahl: Panel beim Client mit {} Choices geöffnet (Sitzung {})", Instances.size(), SessionId);
    return true;
}

void CloseForClient()
{
    if (!Shown.Items.empty())
        Core::Log::Info("Airdrop-Auswahl: Panel beim Client geschlossen, Sitzung {}", Shown.Session);
    DropLocal();
    Shown = {};
}

std::optional<uint32_t> TakeClientPick()
{
    // AddToInventory läuft bereits beim Aufnehmen mit der Maus, vor dem Ablegen.
    // Nicht automatisch bestätigen: Sonst ignoriert das native Panel den Aufruf
    // (IsDragging), während die Mod seine noch verwendeten Instanzen schon entfernt.
    // Erst der echte Bestätigen-Klick in ClientConfirm veröffentlicht die Wahl.
    return std::exchange(Shown.Pick, std::nullopt);
}

std::optional<Placement> TakeClientPlacement(uint64_t Session)
{
    if (!Session || Session != PickedSession || !PickedPlacement)
        return std::nullopt;
    if (!PickedItem || PlacedItem != PickedItem)
    {
        static uintptr_t Reported = 0;
        if (std::exchange(Reported, PlacedItem) != PlacedItem)
            Core::Log::Warn("Airdrop-Auswahl: Ablage gehört nicht zur Wahl (Instanz {:#x}, gewählt {:#x})", PlacedItem, PickedItem);
        return std::nullopt;
    }
    PickedItem = PlacedItem = 0;
    PickedSession = 0;
    return std::exchange(PickedPlacement, std::nullopt);
}
}
