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

bool PatchInventory(CodeWriter&)
{
    // Nur diese drei Klassen, kein globaler UObject- oder ActorComponent-Patch.
    if (!HasVirtual(kSlotVtable, kSupportedForNetworking, kDefaultNetworkingSupport) ||
        !HasVirtual(kItemVtable, kSupportedForNetworking, kDefaultNetworkingSupport) ||
        !HasVirtual(kInventoryVtable, kReadyForReplication, kInventoryReadyForReplication)) return false;
    return SetVirtual(kSlotVtable, kSupportedForNetworking, reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
        SetVirtual(kItemVtable, kSupportedForNetworking, reinterpret_cast<uintptr_t>(&SupportsInventoryNetworking)) &&
        SetVirtual(kInventoryVtable, kReadyForReplication, reinterpret_cast<uintptr_t>(&InventoryReadyForReplication));
}
}
