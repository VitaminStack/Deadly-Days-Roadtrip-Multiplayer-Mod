// Client: Haelt die HUD zurueck, bis der GameState angekommen ist.
#include "Spike.h"
#include "RoundPhaseAdapter.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kProcessEventNative = 0x160C040;

ProcessEventFn OriginalNativeProcessEvent;
int32_t ConstructName = -1;
int32_t PreConstructName = -1;

struct FDeferredEvent
{
    uintptr_t Object;
    uintptr_t Function;
    uint64_t Params;
};
std::vector<FDeferredEvent> DeferredWidgetEvents;

// WP_UI wartet in PreConstruct in einer Schleife auf den GameState. Beim zweiten Kartenwechsel entsteht die HUD vor
// ihm, und in der Schleife kann er nie ankommen. Beide Ereignisse werden zurueckgehalten, bis er da ist.
void __fastcall ProcessEventHook(uintptr_t Object, uintptr_t Function, void* Params)
{
    const int32_t Name = Read<int32_t>(Function + 0x18);
    if (Name == ConstructName || Name == PreConstructName)
    {
        const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
        const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
        const bool IsClient = Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
        if (IsClient && !Read<uintptr_t>(World + kWorldGameState) && NameOf(Read<uintptr_t>(Object + 0x10)) == "WP_UI_C")
        {
            DeferredWidgetEvents.push_back({Object, Function, Params ? Read<uint64_t>(reinterpret_cast<uintptr_t>(Params)) : 0});
            Log("WP_UI: zurueckgehalten");
            return;
        }
    }
    OriginalNativeProcessEvent(Object, Function, Params);
    ObserveRoundPhaseEvent(Object, Function, Params);
}
}

bool PatchWidgets(CodeWriter& Code)
{
    ConstructName = Read<int32_t>(FindObject("Construct", "UserWidget") + 0x18);
    PreConstructName = Read<int32_t>(FindObject("PreConstruct", "UserWidget") + 0x18);
    const uintptr_t Site = Base + kProcessEventNative;
    if (!Expect(Site, {0x40, 0x55, 0x56, 0x57, 0x41, 0x54}, "in ProcessEvent"))
        return false;
    OriginalNativeProcessEvent = reinterpret_cast<ProcessEventFn>(Code.Here());
    Code.Emit({0x40, 0x55, 0x56, 0x57, 0x41, 0x54});             // push rbp,rsi,rdi,r12
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 6);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&ProcessEventHook)));
    return true;
}

void ReleaseDeferredWidgetEvents(uintptr_t World)
{
    if (DeferredWidgetEvents.empty() || !Read<uintptr_t>(World + kWorldGameState))
        return;
    const std::vector<FDeferredEvent> Events = std::move(DeferredWidgetEvents);
    DeferredWidgetEvents.clear();
    for (FDeferredEvent Event : Events)
        OriginalNativeProcessEvent(Event.Object, Event.Function, &Event.Params);
    Log("WP_UI: nachgeholt");
}
}
