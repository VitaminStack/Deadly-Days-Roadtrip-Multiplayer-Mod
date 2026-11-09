// Client: Unterdrueckt lokale Kopien replizierter Kartenobjekte, die Blueprints beim Client selbst spawnen.
#include "Spike.h"

namespace Spike
{
namespace
{
const uintptr_t kBeginDeferredActorSpawnFromClass = Rva(0x2A67AF0);

using BeginDeferredSpawnFn = uintptr_t(__fastcall*)(uintptr_t, uintptr_t*, const void*, uint8_t, uintptr_t, uint8_t);
BeginDeferredSpawnFn OriginalBeginDeferredSpawn;

bool IsClientWorld()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    return Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
}

// BP_PowerUpCrate_C spawnt in BeginPlay ohne Autoritaetspruefung seine BP_ChaosDestructable_PowerUp_C. Beim Client
// kommt dieselbe vom Host repliziert an, die lokale waere doppelt. Ohne sie bleibt DestructablePowerUp der Kiste leer.
uintptr_t __fastcall BeginDeferredSpawnHook(uintptr_t WorldContext, uintptr_t* ActorClass, const void* Transform,
                                            uint8_t Collision, uintptr_t Owner, uint8_t ScaleMethod)
{
    if (IsClientWorld() && WorldContext && ActorClass && *ActorClass &&
        NameOf(*ActorClass) == "BP_ChaosDestructable_PowerUp_C" &&
        NameOf(Read<uintptr_t>(WorldContext + 0x10)) == "BP_PowerUpCrate_C")
    {
        Log("PowerUp: lokale Kopie unterdrueckt");
        return 0;
    }
    return OriginalBeginDeferredSpawn(WorldContext, ActorClass, Transform, Collision, Owner, ScaleMethod);
}
}

bool PatchMapObjects(CodeWriter& Code)
{
    const uintptr_t Site = Base + kBeginDeferredActorSpawnFromClass;
    if (!Expect(Site, {0x40, 0x53, 0x55, 0x57, 0x41, 0x54}, "in BeginDeferredActorSpawnFromClass"))
        return false;
    OriginalBeginDeferredSpawn = reinterpret_cast<BeginDeferredSpawnFn>(Code.Here());
    Code.Emit({0x40, 0x53, 0x55, 0x57, 0x41, 0x54});             // push rbx,rbp,rdi,r12
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 6);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&BeginDeferredSpawnHook)));
    return true;
}
}
