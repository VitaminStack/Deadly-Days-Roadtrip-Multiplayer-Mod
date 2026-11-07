// Beide Seiten: Inventar-Unterobjekte über den klassischen Replikationspfad, dazu zwei Lebensdauer-Fixes aus der Reise
// auf Tag 3 (Timer des Achievement-Detektors, Schritt eines alten Kartengenerators). Aus Spike Inventory.cpp.
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <vector>

#include "Core/Log.h"
#include "Core/Hooks.h"
#include "Game/Memory.h"
#include "Game/PatchPolicy.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t InventoryVtable = 0x94BA3F0;
constexpr uintptr_t SlotVtable = 0x939CE08;
constexpr uintptr_t ItemVtable = 0x94B46E8;
constexpr uintptr_t SupportedForNetworkingSlot = 0x200;
constexpr uintptr_t ReadyForReplicationSlot = 0x3D8;
constexpr uintptr_t DefaultNetworkingSupport = 0x1512250;
constexpr uintptr_t InventoryReadyForReplication = 0x5969750;
constexpr uintptr_t ComponentReplicationFlags = 0x98;
constexpr uint8_t UseRegisteredSubobjectList = 0x40;

// AItemLoot::Loot bekommt das ILootableInterface-Teilobjekt, nicht den Actor.
// Auch Blueprint-Aufrufe erreichen diesen virtuellen nativen Pfad.
constexpr uintptr_t ItemLoot = 0x59AB040;
constexpr uintptr_t LootableInterface = 0x2A8;
using ItemLootFn = void(__fastcall*)(uintptr_t, uintptr_t);
ItemLootFn OriginalItemLoot = nullptr;
std::atomic<unsigned> ClientItemLootAttempts = 0;

// UInventoryComponent.MyItems, UInventoryItemInstance.ItemData/Broken/Rarity, UInventoryComponent.OwningPlayerState
constexpr uintptr_t InventoryItems = 0x120;
constexpr uintptr_t ItemDataOffset = 0xE0, ItemBroken = 0x110, ItemRarity = 0x118;
constexpr uintptr_t InventoryPlayerState = 0x228;
constexpr uint32_t GarbageOrUnreachable = 0x30;

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachable) & GarbageOrUnreachable);
}

std::vector<uintptr_t> ItemsOf(uintptr_t Inventory)
{
    std::vector<uintptr_t> Items;
    const uintptr_t Data = Memory::ReadPointer(Inventory + InventoryItems).value_or(0);
    const int32_t Count = Memory::Read<int32_t>(Inventory + InventoryItems + 8).value_or(0);
    for (int32_t i = 0; Data && i < Count && Count <= 4096; ++i)
        if (const uintptr_t Item = Memory::ReadPointer(Data + 8 * i).value_or(0))
            Items.push_back(Item);
    return Items;
}

// Inventare aller Spieler der aktuellen Welt, über die PlayerStates der PlayerArray. Gemerkt, solange sie leben und
// zum selben PlayerState gehören, sonst einmal neu über das Objekt-Array gesucht.
std::vector<uintptr_t> PlayerInventories()
{
    static std::vector<std::pair<uintptr_t, uintptr_t>> Cache; // PlayerState, Inventar
    const uintptr_t World = CurrentWorld();
    const uintptr_t State = World ? Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0) : 0;
    const uintptr_t Array = State ? Memory::ReadPointer(State + Offsets::AGameStateBase::PlayerArray).value_or(0) : 0;
    const int32_t Count = State ? Memory::Read<int32_t>(State + Offsets::AGameStateBase::PlayerArray + 8).value_or(0) : 0;
    std::vector<uintptr_t> States;
    for (int32_t i = 0; Array && i < Count && i < 8; ++i)
        if (const uintptr_t PlayerState = Memory::ReadPointer(Array + 8 * i).value_or(0))
            States.push_back(PlayerState);
    std::erase_if(Cache, [](const std::pair<uintptr_t, uintptr_t>& Entry)
                  { return !Alive(Entry.second) || Memory::ReadPointer(Entry.second + InventoryPlayerState).value_or(0) != Entry.first; });
    const auto Cached = [](uintptr_t PlayerState)
    { return std::any_of(Cache.begin(), Cache.end(), [&](const auto& Entry) { return Entry.first == PlayerState; }); };
    if (!std::all_of(States.begin(), States.end(), Cached))
    {
        const int32_t Total = Objects::Count();
        for (int32_t i = 0; i < Total; ++i)
        {
            const uintptr_t Object = Objects::At(i);
            const uintptr_t Owner = Object ? Memory::ReadPointer(Object + InventoryPlayerState).value_or(0) : 0;
            if (Owner && std::find(States.begin(), States.end(), Owner) != States.end() && !Cached(Owner) && Alive(Object) &&
                IsA(Object, "InventoryComponent"))
                Cache.emplace_back(Owner, Object);
        }
    }
    std::vector<uintptr_t> Result;
    for (const uintptr_t PlayerState : States)
        for (const auto& [Owner, Inventory] : Cache)
            if (Owner == PlayerState)
                Result.push_back(Inventory);
    return Result;
}

// Wie ClaimForClient der Airdrop-Auswahl: UInventoryComponent.CreateAndAddItem beim Host, das Item repliziert.
bool AddCopy(uintptr_t Inventory, uintptr_t Source)
{
    static uintptr_t Create = 0;
    if (!Create)
        Create = Objects::FindObject("CreateAndAddItem", "InventoryComponent");
    const uintptr_t Data = Memory::ReadPointer(Source + ItemDataOffset).value_or(0);
    if (!Create || !Data)
        return false;
    struct
    {
        uintptr_t Data, Rarity;
        bool Broken, FlagAsNew, SystemAdd;
        uint8_t Pad;
        int32_t Position;
        bool AllowP2Reserve;
        uint8_t Pad2[7];
        uintptr_t Result;
    } Add{Data, Memory::ReadPointer(Source + ItemRarity).value_or(0), Memory::Read<uint8_t>(Source + ItemBroken).value_or(0) != 0, true, true, 0, -1, false,
          {}, 0};
    static_assert(sizeof(Add) == 0x28);
    Objects::ProcessEvent(Inventory, Create, &Add);
    return Add.Result != 0;
}

// Host: Nach der nativen Vergabe an den Sammler bekommt jeder andere Spieler dieselben neuen Items.
void ShareLoot(const std::vector<uintptr_t>& Inventories, const std::vector<std::vector<uintptr_t>>& Before)
{
    std::vector<uintptr_t> Gained;
    size_t Looter = 0, GainedInventories = 0;
    for (size_t i = 0; i < Inventories.size(); ++i)
    {
        std::vector<uintptr_t> New;
        for (const uintptr_t Item : ItemsOf(Inventories[i]))
            if (std::find(Before[i].begin(), Before[i].end(), Item) == Before[i].end())
                New.push_back(Item);
        if (!New.empty())
        {
            ++GainedInventories;
            Looter = i;
            Gained = std::move(New);
        }
    }
    if (!ShareItemLoot(CurrentRole(), Inventories.size(), GainedInventories))
        return;
    for (size_t i = 0; i < Inventories.size(); ++i)
        if (i != Looter)
            for (const uintptr_t Item : Gained)
            {
                const bool Added = AddCopy(Inventories[i], Item);
                Core::Log::Info("Itemsack: {} auch an Spieler {} vergeben, angelegt={}",
                                Memory::NameOf(Memory::ReadPointer(Item + ItemDataOffset).value_or(0)), i, Added);
            }
}

void __fastcall ItemLootHook(uintptr_t Interface, uintptr_t Looter)
{
    const uintptr_t Actor = Interface >= LootableInterface ? Interface - LootableInterface : 0;
    const uint8_t Role = Actor ? Memory::Read<uint8_t>(Actor + 0x168).value_or(0) : 0;
    if (!ItemLootMayGrant(Role, CurrentRole()))
    {
        if (ClientItemLootAttempts.fetch_add(1) < 3)
            Core::Log::Info("Itemsack: lokale Item-Vergabe ausgelassen; Vergabe bleibt beim Host");
        return;
    }
    const std::vector<uintptr_t> Inventories = CurrentRole() == NetRole::Host ? PlayerInventories() : std::vector<uintptr_t>{};
    std::vector<std::vector<uintptr_t>> Before;
    for (const uintptr_t Inventory : Inventories)
        Before.push_back(ItemsOf(Inventory));
    OriginalItemLoot(Interface, Looter);
    if (Inventories.size() > 1)
        ShareLoot(Inventories, Before);
}

bool CheckItemLoot()
{
    return Code::Expect(At(ItemLoot), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10}, "ItemLoot.Loot");
}

bool ApplyItemLoot(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(ItemLoot)), reinterpret_cast<void*>(&ItemLootHook),
                                reinterpret_cast<void**>(&OriginalItemLoot));
}

// Der Callback nach der Reise auf Tag 3 gehört nicht zum Inventar: Der Achievement-Detektor legt in BeginPlay einen
// Timer mit rohem this an und löscht ihn in EndPlay nicht.
constexpr uintptr_t DetectorVtable = 0x94C1758;
constexpr uintptr_t DetectorEndPlay = 0x57D0A40;
constexpr uintptr_t EndPlaySlot = 0x3F0;
constexpr uintptr_t ScheduleCall = 0x57CB3C5;
constexpr uintptr_t SetNextTick = 0x314D4D0;
constexpr uintptr_t FindTimer = 0x31471C0;
constexpr uintptr_t ClearTimer = 0x314D0B0;
constexpr uintptr_t DelegateCapturedThis = 0x18;

// Lambda des Kartengenerators: roher Generator bei +18h, &CurrentGenData bei +20h, ruft den Schritt 0x59D9090 ohne
// Prüfung. Nach der Reise auf Tag 3 feuerte er für den Generator der alten Karte.
constexpr uintptr_t MapGenLambdaVtable = 0x9613280;
constexpr uintptr_t MapGenLambdaExecute = 0x59CAE70;
constexpr uintptr_t MapGenLambdaExecuteIfSafe = 0x59CB020;
constexpr uintptr_t LambdaGenerator = 0x18;

// lager-absturz-client: Größe eines Item-Widgets (UItemRepresentation) aus der Itemform, Breite und Höhe je 66 px minus 3.
// Liest ItemInstance (+0xA28), davon ItemData (+0xE0) und dessen Form (+0xE8) ohne Prüfung. Verliert das Widget sein Item, etwa
// beim Verbindungsabbau im Lager, stürzte der Client hier ab (0x5AB513A). Ohne Item bleibt die alte Größe stehen.
constexpr uintptr_t RepresentationSize = 0x5AB5120;
constexpr uintptr_t RepresentationItem = 0xA28;
constexpr uintptr_t ItemInstanceData = 0xE0;
constexpr uintptr_t ItemDataShape = 0xE8;
using RepresentationSizeFn = void(__fastcall*)(uintptr_t);
RepresentationSizeFn OriginalRepresentationSize = nullptr;
std::atomic<unsigned> RepresentationSkips = 0;

void __fastcall RepresentationSizeHook(uintptr_t Representation)
{
    const uintptr_t Item = Memory::Read<uintptr_t>(Representation + RepresentationItem).value_or(0);
    const uintptr_t Data = Item ? Memory::Read<uintptr_t>(Item + ItemInstanceData).value_or(0) : 0;
    const uintptr_t Shape = Data ? Memory::Read<uintptr_t>(Data + ItemDataShape).value_or(0) : 0;
    if (Shape)
    {
        OriginalRepresentationSize(Representation);
        return;
    }
    if (RepresentationSkips.fetch_add(1) < 5)
        Core::Log::Warn("Inventar: Item-Widget ohne {}, Größe nicht neu gesetzt", Item ? (Data ? "Form" : "ItemData") : "Item");
}

struct PendingTimer
{
    uintptr_t Manager;
    uint64_t Handle;
};
// Nur Game Thread, Einträge gehen beim EndPlay.
std::map<uintptr_t, PendingTimer> DetectorTimers;

uint64_t* __fastcall ScheduleDetectorTimer(uintptr_t Manager, uint64_t* Handle, uintptr_t Delegate)
{
    // Diese Stelle baut eine TFunction mit rohem this bei +18h. Gemerkt wird der Handle der Engine, nie ein Objektzeiger,
    // der später schon freigegeben sein kann.
    const uintptr_t Detector = Memory::Read<uintptr_t>(Delegate + DelegateCapturedThis).value_or(0);
    using ScheduleFn = uint64_t*(__fastcall*)(uintptr_t, uint64_t*, uintptr_t);
    uint64_t* Result = reinterpret_cast<ScheduleFn>(At(SetNextTick))(Manager, Handle, Delegate);
    if (Detector && Handle)
        DetectorTimers[Detector] = {Manager, *Handle};
    return Result;
}

void __fastcall DetectorEndPlayHook(uintptr_t Detector, uint8_t Reason)
{
    if (const auto It = DetectorTimers.find(Detector); It != DetectorTimers.end())
    {
        const PendingTimer Timer = It->second;
        DetectorTimers.erase(It);
        // Wie K2_ClearTimerHandle: erst den vollständigen Handle prüfen.
        using FindFn = uintptr_t(__fastcall*)(uintptr_t, const uint64_t*);
        if (reinterpret_cast<FindFn>(At(FindTimer))(Timer.Manager, &Timer.Handle))
            reinterpret_cast<void(__fastcall*)(uintptr_t, uint64_t)>(At(ClearTimer))(Timer.Manager, Timer.Handle);
    }
    reinterpret_cast<void(__fastcall*)(uintptr_t, uint8_t)>(At(DetectorEndPlay))(Detector, Reason);
}

bool __fastcall SupportsInventoryNetworking(uintptr_t)
{
    return true;
}

std::atomic<int> InventoryReady = 0;

void __fastcall InventoryReadyForReplicationHook(uintptr_t Inventory)
{
    // Auch beim Kopieren über Kartenwechsel, dort laufen die Factories für Items und Slots nicht. ReplicateSubobjects
    // der Spielklasse sendet beide Arrays schon, mit der registrierten Liste wird es aber ausgelassen.
    if (const auto Flags = Memory::Read<uint8_t>(Inventory + ComponentReplicationFlags))
    {
        const uint8_t Value = *Flags & static_cast<uint8_t>(~UseRegisteredSubobjectList);
        Memory::WriteBytes(Inventory + ComponentReplicationFlags, &Value, 1);
    }
    reinterpret_cast<void(__fastcall*)(uintptr_t)>(At(InventoryReadyForReplication))(Inventory);
    if (InventoryReady.fetch_add(1) == 0)
        Core::Log::Info("Inventar: klassische Replikation der Unterobjekte bereit");
}

// Wahr, solange der Generator im Objekt-Array steht und zur aktuellen Welt gehört (Outer = Level, dessen Outer = Welt).
bool IsGeneratorInCurrentWorld(uintptr_t Generator)
{
    const auto Index = Memory::Read<int32_t>(Generator + 0xC);
    const int32_t Count = Memory::Read<int32_t>(At(Offsets::GObjects) + Offsets::ObjectArray::NumElements).value_or(0);
    if (!Generator || !Index || *Index < 0 || *Index >= Count)
        return false;
    const auto Chunks = Memory::ReadPointer(At(Offsets::GObjects));
    const auto Chunk = Chunks ? Memory::ReadPointer(*Chunks + 8 * (*Index / Offsets::ObjectArray::ElementsPerChunk)) : std::nullopt;
    const uintptr_t Item = Chunk ? *Chunk + (*Index % Offsets::ObjectArray::ElementsPerChunk) * Offsets::ObjectArray::ItemSize : 0;
    if (!Item || Memory::Read<uintptr_t>(Item + Offsets::ObjectArray::ItemObject) != Generator)
        return false;
    const auto Level = Memory::ReadPointer(Generator + Offsets::UObject::Outer);
    return Level && Memory::ReadPointer(*Level + Offsets::UObject::Outer).value_or(0) == CurrentWorld();
}

std::atomic<int> StaleSteps = 0;

bool SkipStaleMapGenStep(uintptr_t Lambda)
{
    if (IsGeneratorInCurrentWorld(Memory::Read<uintptr_t>(Lambda + LambdaGenerator).value_or(0)))
        return false;
    if (StaleSteps.fetch_add(1) < 3)
        Core::Log::Info("Kartengenerator: Schritt für eine alte Welt ausgelassen");
    return true;
}

void __fastcall MapGenLambdaExecuteHook(uintptr_t Lambda)
{
    if (!SkipStaleMapGenStep(Lambda))
        reinterpret_cast<void(__fastcall*)(uintptr_t)>(At(MapGenLambdaExecute))(Lambda);
}

bool __fastcall MapGenLambdaExecuteIfSafeHook(uintptr_t Lambda)
{
    return !SkipStaleMapGenStep(Lambda) && reinterpret_cast<bool(__fastcall*)(uintptr_t)>(At(MapGenLambdaExecuteIfSafe))(Lambda);
}

bool CheckInventory()
{
    return Code::ExpectVirtual(At(SlotVtable, SupportedForNetworkingSlot), At(DefaultNetworkingSupport), "Slot IsSupportedForNetworking") &&
           Code::ExpectVirtual(At(ItemVtable, SupportedForNetworkingSlot), At(DefaultNetworkingSupport), "Item IsSupportedForNetworking") &&
           Code::ExpectVirtual(At(InventoryVtable, ReadyForReplicationSlot), At(InventoryReadyForReplication), "Inventar ReadyForReplication");
}

// Nur diese drei Klassen, kein globaler UObject- oder ActorComponent-Patch.
bool ApplyInventory(Code::Writer&)
{
    return Code::WriteVirtual(At(SlotVtable, SupportedForNetworkingSlot), reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
           Code::WriteVirtual(At(ItemVtable, SupportedForNetworkingSlot), reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
           Code::WriteVirtual(At(InventoryVtable, ReadyForReplicationSlot), reinterpret_cast<uintptr_t>(&InventoryReadyForReplicationHook));
}

bool CheckDetector()
{
    return Code::ExpectVirtual(At(DetectorVtable, EndPlaySlot), At(DetectorEndPlay), "EndPlay des Achievement-Detektors") &&
           Code::Expect(At(ScheduleCall), {0xE8, 0x06, 0x21, 0x98, 0xFD}, "Timer des Achievement-Detektors") &&
           Code::Expect(At(SetNextTick), {0x40, 0x55, 0x53, 0x48, 0x8D, 0x6C, 0x24, 0xB1}, "SetTimerForNextTick") &&
           Code::Expect(At(FindTimer), {0x4C, 0x8B, 0x0A, 0x4C, 0x8B, 0xD1}, "FindTimer") &&
           Code::Expect(At(ClearTimer), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x54, 0x24, 0x10}, "ClearTimer");
}

bool ApplyDetector(Code::Writer& Code)
{
    // Der CALL bleibt, nur sein Ziel ändert sich. So stimmt die Rücksprungadresse.
    uint8_t* Stub = Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&ScheduleDetectorTimer));
    const auto Delta = Code::Rel32(At(ScheduleCall) + 5, reinterpret_cast<uintptr_t>(Stub));
    if (Code.Failed() || !Delta)
        return false;
    return Code::WriteCode(At(ScheduleCall) + 1, &*Delta, sizeof(*Delta)) &&
           Code::WriteVirtual(At(DetectorVtable, EndPlaySlot), reinterpret_cast<uintptr_t>(&DetectorEndPlayHook));
}

bool CheckMapGenLambda()
{
    return Code::ExpectVirtual(At(MapGenLambdaVtable), At(MapGenLambdaExecute), "Lambda des Kartengenerators, Execute") &&
           Code::ExpectVirtual(At(MapGenLambdaVtable, 8), At(MapGenLambdaExecuteIfSafe), "Lambda des Kartengenerators, ExecuteIfSafe") &&
           Code::Expect(At(MapGenLambdaExecute), {0x48, 0x8B, 0x51, 0x20, 0x48, 0x8B, 0x49, 0x18, 0xE9}, "Lambda des Kartengenerators");
}

bool ApplyMapGenLambda(Code::Writer&)
{
    return Code::WriteVirtual(At(MapGenLambdaVtable), reinterpret_cast<uintptr_t>(&MapGenLambdaExecuteHook)) &&
           Code::WriteVirtual(At(MapGenLambdaVtable, 8), reinterpret_cast<uintptr_t>(&MapGenLambdaExecuteIfSafeHook));
}

bool CheckRepresentationSize()
{
    return Code::Expect(At(RepresentationSize), {0x40, 0x53, 0x48, 0x83, 0xEC, 0x50, 0x48, 0x8B, 0x81, 0x28, 0x0A, 0x00, 0x00},
                        "Größe des Item-Widgets");
}

bool ApplyRepresentationSize(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(RepresentationSize)), reinterpret_cast<void*>(&RepresentationSizeHook),
                                reinterpret_cast<void**>(&OriginalRepresentationSize));
}

const Patch List[] = {
    {"Itemsack-Vergabe nur beim Host, für alle Spieler", &CheckItemLoot, &ApplyItemLoot},
    {"Item-Widget ohne Item: Größe nicht setzen", &CheckRepresentationSize, &ApplyRepresentationSize},
    {"Inventar-Unterobjekte replizieren", &CheckInventory, &ApplyInventory},
    {"Timer des Achievement-Detektors beim EndPlay löschen", &CheckDetector, &ApplyDetector},
    {"Kartengenerator-Schritt nur für die aktuelle Welt", &CheckMapGenLambda, &ApplyMapGenLambda},
};
}

std::span<const Patch> InventoryPatches()
{
    return List;
}
}
