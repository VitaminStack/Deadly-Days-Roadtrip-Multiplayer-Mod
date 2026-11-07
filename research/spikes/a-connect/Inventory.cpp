// Inventar-Unterobjekte ueber den vorhandenen klassischen Replikationspfad senden.
// Build 26187885. Keine Schutzspruenge mehr in den Inventar-Schleifen.
#include "Spike.h"

namespace Spike
{
namespace
{
const uintptr_t kInventoryVtable = Rva(0x94BA3F0);
const uintptr_t kSlotVtable = Rva(0x939CE08);
const uintptr_t kItemVtable = Rva(0x94B46E8);
constexpr uintptr_t kSupportedForNetworking = 0x200;
constexpr uintptr_t kReadyForReplication = 0x3D8;
const uintptr_t kDefaultNetworkingSupport = Rva(0x1512250);
const uintptr_t kInventoryReadyForReplication = Rva(0x5969750);
constexpr uintptr_t kComponentReplicationFlags = 0x98;
constexpr uint8_t kUseRegisteredSubobjectList = 0x40;

// The day-3 callback is not an inventory callback. This achievement detector captures
// raw this in a next-tick timer from BeginPlay and does not cancel that timer in EndPlay.
const uintptr_t kDetectorVtable = Rva(0x94C1758);
const uintptr_t kDetectorEndPlay = Rva(0x57D0A40);
constexpr uintptr_t kEndPlaySlot = 0x3F0;
const uintptr_t kScheduleCall = Rva(0x57CB3C5);
const uintptr_t kSetNextTick = Rva(0x314D4D0);
const uintptr_t kFindTimer = Rva(0x31471C0);
const uintptr_t kClearTimer = Rva(0x314D0B0);
struct PendingDetectorTimer
{
    uintptr_t Manager;
    uint64_t Handle;
};
std::map<uintptr_t, PendingDetectorTimer> DetectorTimers; // Game Thread only; erased at EndPlay.

uint64_t* __fastcall ScheduleDetectorTimer(uintptr_t Manager, uint64_t* Handle, uintptr_t Delegate)
{
    // This call site constructs a TFunction with inline raw-this capture at +0x18.
    // Keep the returned engine handle, rather than guarding/dereferencing freed UObject memory later.
    const uintptr_t Detector = Read<uintptr_t>(Delegate + 0x18);
    using ScheduleFn = uint64_t*(__fastcall*)(uintptr_t, uint64_t*, uintptr_t);
    uint64_t* Result = reinterpret_cast<ScheduleFn>(Base + kSetNextTick)(Manager, Handle, Delegate);
    DetectorTimers[Detector] = {Manager, *Handle};
    Log("Detector timer: scheduled " + NameOf(Detector));
    return Result;
}

void __fastcall DetectorEndPlay(uintptr_t Detector, uint8_t Reason)
{
    const auto It = DetectorTimers.find(Detector);
    if (It != DetectorTimers.end())
    {
        auto Timer = It->second;
        DetectorTimers.erase(It);
        // Match the native K2_ClearTimerHandle path: validate the complete handle first.
        using FindFn = uintptr_t(__fastcall*)(uintptr_t, const uint64_t*);
        if (reinterpret_cast<FindFn>(Base + kFindTimer)(Timer.Manager, &Timer.Handle))
        {
            using ClearFn = void(__fastcall*)(uintptr_t, uint64_t);
            reinterpret_cast<ClearFn>(Base + kClearTimer)(Timer.Manager, Timer.Handle);
            Log("Detector timer: cancelled before EndPlay");
        }
    }
    // Preserve normal cleanup, delegates and superclass EndPlay.
    reinterpret_cast<void(__fastcall*)(uintptr_t, uint8_t)>(Base + kDetectorEndPlay)(Detector, Reason);
}

bool __fastcall SupportsInventoryNetworking(uintptr_t) { return true; }

void __fastcall InventoryReadyForReplication(uintptr_t Inventory)
{
    // Auch beim Kopieren ueber Kartenwechsel: dort laufen die Item-/Slot-Factories nicht.
    // ReplicateSubobjects der Spielklasse sendet bereits beide Arrays, wird mit der Liste aber ausgelassen.
    *reinterpret_cast<uint8_t*>(Inventory + kComponentReplicationFlags) &= ~kUseRegisteredSubobjectList;
    reinterpret_cast<void(__fastcall*)(uintptr_t)>(Base + kInventoryReadyForReplication)(Inventory);
    Log("Inventory: classical subobject replication ready");
}

// Lambda delegate of AMapGenerator: captures a raw generator at +18h and &CurrentGenData at +20h, then calls
// the generation step 0x59D9090 unchecked. After the travel to day 3 it fires for the generator of the old map.
const uintptr_t kMapGenLambdaVtable = Rva(0x9613280);
const uintptr_t kMapGenLambdaExecute = Rva(0x59CAE70);
const uintptr_t kMapGenLambdaExecuteIfSafe = Rva(0x59CB020);
using LambdaExecuteFn = void(__fastcall*)(uintptr_t);
using LambdaExecuteIfSafeFn = bool(__fastcall*)(uintptr_t);

// True while the object is still registered in GUObjectArray and belongs to the current world (Outer = Level, its Outer = World).
bool IsGeneratorInCurrentWorldUnsafe(uintptr_t Generator)
{
    if (!Generator) return false;
    const int32_t Index = Read<int32_t>(Generator + 0xC);
    if (Index < 0 || Index >= Read<int32_t>(Base + kGObjects + 8)) return false;
    const uintptr_t Chunk = Read<uintptr_t>(Read<uintptr_t>(Base + kGObjects) + 8 * (Index >> 16));
    if (!Chunk || Read<uintptr_t>(Chunk + (Index & 0xFFFF) * 0x18 + 8) != Generator) return false;
    const uintptr_t Level = Read<uintptr_t>(Generator + 0x20);
    return Level && Read<uintptr_t>(Level + 0x20) == Read<uintptr_t>(Base + kGWorld);
}

// The old generator may already be returned to the OS; an access violation also means stale.
bool IsGeneratorInCurrentWorld(uintptr_t Generator)
{
    __try { return IsGeneratorInCurrentWorldUnsafe(Generator); }
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { return false; }
}

bool SkipStaleMapGenStep(uintptr_t Lambda)
{
    if (IsGeneratorInCurrentWorld(Read<uintptr_t>(Lambda + 0x18))) return false;
    static unsigned Skipped = 0;
    if (++Skipped <= 16) Log("MapGen step: generator of an old world, call skipped, count=" + std::to_string(Skipped));
    return true;
}

void __fastcall MapGenLambdaExecute(uintptr_t Lambda)
{
    if (!SkipStaleMapGenStep(Lambda)) reinterpret_cast<LambdaExecuteFn>(Base + kMapGenLambdaExecute)(Lambda);
}

bool __fastcall MapGenLambdaExecuteIfSafe(uintptr_t Lambda)
{
    return !SkipStaleMapGenStep(Lambda) && reinterpret_cast<LambdaExecuteIfSafeFn>(Base + kMapGenLambdaExecuteIfSafe)(Lambda);
}

bool HasVirtual(uintptr_t Vtable, uintptr_t Offset, uintptr_t Expected)
{
    if (Read<uintptr_t>(Base + Vtable + Offset) == Base + Expected) return true;
    Log("Inventory: unexpected virtual function, patch refused");
    return false;
}

bool SetVirtual(uintptr_t Vtable, uintptr_t Offset, uintptr_t Function)
{
    const uintptr_t Entry = Base + Vtable + Offset;
    DWORD Old;
    if (!VirtualProtect(reinterpret_cast<void*>(Entry), sizeof(uintptr_t), PAGE_READWRITE, &Old)) return false;
    *reinterpret_cast<uintptr_t*>(Entry) = Function;
    DWORD Ignored;
    return VirtualProtect(reinterpret_cast<void*>(Entry), sizeof(uintptr_t), Old, &Ignored) != FALSE;
}
}

bool PatchInventory(CodeWriter& Code)
{
    // Nur diese drei Klassen, kein globaler UObject- oder ActorComponent-Patch.
    if (!HasVirtual(kSlotVtable, kSupportedForNetworking, kDefaultNetworkingSupport) ||
        !HasVirtual(kItemVtable, kSupportedForNetworking, kDefaultNetworkingSupport) ||
        !HasVirtual(kInventoryVtable, kReadyForReplication, kInventoryReadyForReplication) ||
        !HasVirtual(kDetectorVtable, kEndPlaySlot, kDetectorEndPlay) ||
        !HasVirtual(kMapGenLambdaVtable, 0, kMapGenLambdaExecute) ||
        !HasVirtual(kMapGenLambdaVtable, 8, kMapGenLambdaExecuteIfSafe) ||
        !Expect(Base + kMapGenLambdaExecute, {0x48, 0x8B, 0x51, 0x20, 0x48, 0x8B, 0x49, 0x18, 0xE9}, "map generator lambda") ||
        !ExpectBranch(Base + kScheduleCall, 0xE8, 0x314D4D0, "detector next-tick timer") ||
        !Expect(Base + kSetNextTick, {0x40, 0x55, 0x53, 0x48, 0x8D, 0x6C, 0x24, 0xB1}, "set next tick") ||
        !Expect(Base + kFindTimer, {0x4C, 0x8B, 0x0A, 0x4C, 0x8B, 0xD1}, "find timer") ||
        !Expect(Base + kClearTimer, {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x54, 0x24, 0x10}, "clear timer")) return false;
    uint8_t* Stub = Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&ScheduleDetectorTimer));
    const uintptr_t Site = Base + kScheduleCall;
    DWORD Old;
    if (!VirtualProtect(reinterpret_cast<void*>(Site), 5, PAGE_EXECUTE_READWRITE, &Old)) return false;
    const int32_t Delta = static_cast<int32_t>(reinterpret_cast<uintptr_t>(Stub) - (Site + 5));
    memcpy(reinterpret_cast<void*>(Site + 1), &Delta, sizeof(Delta)); // Keep CALL and its return address.
    DWORD Ignored;
    VirtualProtect(reinterpret_cast<void*>(Site), 5, Old, &Ignored);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(Site), 5);
    if (!SetVirtual(kDetectorVtable, kEndPlaySlot, reinterpret_cast<uintptr_t>(&DetectorEndPlay)) ||
        !SetVirtual(kMapGenLambdaVtable, 0, reinterpret_cast<uintptr_t>(&MapGenLambdaExecute)) ||
        !SetVirtual(kMapGenLambdaVtable, 8, reinterpret_cast<uintptr_t>(&MapGenLambdaExecuteIfSafe))) return false;
    return SetVirtual(kSlotVtable, kSupportedForNetworking, reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
        SetVirtual(kItemVtable, kSupportedForNetworking, reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
        SetVirtual(kInventoryVtable, kReadyForReplication, reinterpret_cast<uintptr_t>(&InventoryReadyForReplication));
}
}
