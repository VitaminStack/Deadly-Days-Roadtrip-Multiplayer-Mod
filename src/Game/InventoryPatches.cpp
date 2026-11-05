// Beide Seiten: Inventar-Unterobjekte über den klassischen Replikationspfad, dazu zwei Lebensdauer-Fixes aus der Reise
// auf Tag 3 (Timer des Achievement-Detektors, Schritt eines alten Kartengenerators). Aus Spike Inventory.cpp.
#include <windows.h>

#include <atomic>
#include <map>

#include "Core/Log.h"
#include "Core/Hooks.h"
#include "Game/Memory.h"
#include "Game/PatchPolicy.h"
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

void __fastcall ItemLootHook(uintptr_t Interface, uintptr_t Looter)
{
    const uintptr_t Actor = Interface >= LootableInterface ? Interface - LootableInterface : 0;
    const uint8_t Role = Actor ? Memory::Read<uint8_t>(Actor + 0x168).value_or(0) : 0;
    if (ItemLootMayGrant(Role, CurrentRole()))
        OriginalItemLoot(Interface, Looter);
    else if (ClientItemLootAttempts.fetch_add(1) < 3)
        Core::Log::Info("Itemsack: lokale Item-Vergabe ausgelassen; Vergabe bleibt beim Host");
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
    return Code::ExpectVirtual(At(SlotVtable + SupportedForNetworkingSlot), At(DefaultNetworkingSupport), "Slot IsSupportedForNetworking") &&
           Code::ExpectVirtual(At(ItemVtable + SupportedForNetworkingSlot), At(DefaultNetworkingSupport), "Item IsSupportedForNetworking") &&
           Code::ExpectVirtual(At(InventoryVtable + ReadyForReplicationSlot), At(InventoryReadyForReplication), "Inventar ReadyForReplication");
}

// Nur diese drei Klassen, kein globaler UObject- oder ActorComponent-Patch.
bool ApplyInventory(Code::Writer&)
{
    return Code::WriteVirtual(At(SlotVtable + SupportedForNetworkingSlot), reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
           Code::WriteVirtual(At(ItemVtable + SupportedForNetworkingSlot), reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
           Code::WriteVirtual(At(InventoryVtable + ReadyForReplicationSlot), reinterpret_cast<uintptr_t>(&InventoryReadyForReplicationHook));
}

bool CheckDetector()
{
    return Code::ExpectVirtual(At(DetectorVtable + EndPlaySlot), At(DetectorEndPlay), "EndPlay des Achievement-Detektors") &&
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
           Code::WriteVirtual(At(DetectorVtable + EndPlaySlot), reinterpret_cast<uintptr_t>(&DetectorEndPlayHook));
}

bool CheckMapGenLambda()
{
    return Code::ExpectVirtual(At(MapGenLambdaVtable), At(MapGenLambdaExecute), "Lambda des Kartengenerators, Execute") &&
           Code::ExpectVirtual(At(MapGenLambdaVtable + 8), At(MapGenLambdaExecuteIfSafe), "Lambda des Kartengenerators, ExecuteIfSafe") &&
           Code::Expect(At(MapGenLambdaExecute), {0x48, 0x8B, 0x51, 0x20, 0x48, 0x8B, 0x49, 0x18, 0xE9}, "Lambda des Kartengenerators");
}

bool ApplyMapGenLambda(Code::Writer&)
{
    return Code::WriteVirtual(At(MapGenLambdaVtable), reinterpret_cast<uintptr_t>(&MapGenLambdaExecuteHook)) &&
           Code::WriteVirtual(At(MapGenLambdaVtable + 8), reinterpret_cast<uintptr_t>(&MapGenLambdaExecuteIfSafeHook));
}

const Patch List[] = {
    {"Itemsack-Vergabe nur beim Host", &CheckItemLoot, &ApplyItemLoot},
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
