// Inventar-Unterobjekte ueber den vorhandenen klassischen Replikationspfad senden.
// Build 26187885. Keine Schutzspruenge mehr in den Inventar-Schleifen.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kInventoryVtable = 0x94BA3F0;
constexpr uintptr_t kSlotVtable = 0x939CE08;
constexpr uintptr_t kItemVtable = 0x94B46E8;
constexpr uintptr_t kSupportedForNetworking = 0x200;
constexpr uintptr_t kReadyForReplication = 0x3D8;
constexpr uintptr_t kDefaultNetworkingSupport = 0x1512250;
constexpr uintptr_t kInventoryReadyForReplication = 0x5969750;
constexpr uintptr_t kComponentReplicationFlags = 0x98;
constexpr uint8_t kUseRegisteredSubobjectList = 0x40;

// The day-3 callback is not an inventory callback. This achievement detector captures
// raw this in a next-tick timer from BeginPlay and does not cancel that timer in EndPlay.
constexpr uintptr_t kDetectorVtable = 0x94C1758;
constexpr uintptr_t kDetectorEndPlay = 0x57D0A40;
constexpr uintptr_t kEndPlaySlot = 0x3F0;
constexpr uintptr_t kScheduleCall = 0x57CB3C5;
constexpr uintptr_t kSetNextTick = 0x314D4D0;
constexpr uintptr_t kFindTimer = 0x31471C0;
constexpr uintptr_t kClearTimer = 0x314D0B0;
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
        !Expect(Base + kScheduleCall, {0xE8, 0x06, 0x21, 0x98, 0xFD}, "detector next-tick timer") ||
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
    if (!SetVirtual(kDetectorVtable, kEndPlaySlot, reinterpret_cast<uintptr_t>(&DetectorEndPlay))) return false;
    return SetVirtual(kSlotVtable, kSupportedForNetworking, reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
        SetVirtual(kItemVtable, kSupportedForNetworking, reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
        SetVirtual(kInventoryVtable, kReadyForReplication, reinterpret_cast<uintptr_t>(&InventoryReadyForReplication));
}
}
