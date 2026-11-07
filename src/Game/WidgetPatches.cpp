// Client: Die HUD wartet, bis der GameState angekommen ist. Dazu die Beobachter nach jedem ProcessEvent. Aus Spike Widgets.cpp.
#include <windows.h>

#include <array>
#include <atomic>
#include <vector>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
using ProcessEventFn = void(__fastcall*)(uintptr_t, uintptr_t, void*);
ProcessEventFn OriginalProcessEvent = nullptr;

// FName-Index von Construct und PreConstruct, -1 bis der Tick sie gefunden hat. Bis dahin hält der Hook nichts zurück.
std::atomic<int32_t> ConstructName = -1;
std::atomic<int32_t> PreConstructName = -1;

struct DeferredEvent
{
    uintptr_t World;
    uintptr_t Object;
    uintptr_t Function;
    uint64_t Params;
};
// Nur Game Thread.
std::vector<DeferredEvent> Deferred;

std::array<std::atomic<EventObserver>, 4> Observers{};

bool HasGameState()
{
    const uintptr_t World = CurrentWorld();
    return World && Memory::ReadPointer(World + Offsets::UWorld::GameState);
}

// WP_UI wartet in PreConstruct in einer Schleife auf den GameState. Nach dem zweiten Kartenwechsel entsteht die HUD
// vor ihm, und in der Schleife kann er nie ankommen. Beide Ereignisse warten deshalb, bis er da ist.
void __fastcall ProcessEventHook(uintptr_t Object, uintptr_t Function, void* Params)
{
    TraceInventoryEvent(Object, Function, Params);
    if (PrepareRemoteDash(Object, Function))
        return;
    if (FilterRewardEvent(Object, Function, Params))
        return;
    const int32_t Name = Memory::Read<int32_t>(Function + Offsets::UObject::Name).value_or(-1);
    if (Name >= 0 && (Name == ConstructName.load(std::memory_order_relaxed) || Name == PreConstructName.load(std::memory_order_relaxed)) &&
        CurrentRole() == NetRole::Client && !HasGameState() &&
        Memory::NameOf(Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0)) == "WP_UI_C")
    {
        // Construct und PreConstruct haben höchstens ein bool als Parameter, 8 Byte reichen.
        Deferred.push_back({CurrentWorld(), Object, Function, Params ? Memory::Read<uint64_t>(reinterpret_cast<uintptr_t>(Params)).value_or(0) : 0});
        Core::Log::Info("HUD: zurückgehalten, bis der GameState da ist");
        return;
    }
    OriginalProcessEvent(Object, Function, Params);
    NotifyEventObservers(Object, Function, Params);
}

bool CheckProcessEvent()
{
    return Code::Expect(At(Offsets::ProcessEvent), {0x40, 0x55, 0x56, 0x57, 0x41, 0x54}, "in ProcessEvent");
}

bool ApplyProcessEvent(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(Offsets::ProcessEvent)), reinterpret_cast<void*>(&ProcessEventHook),
                                reinterpret_cast<void**>(&OriginalProcessEvent));
}

void ResolveNames()
{
    if (ConstructName >= 0 && PreConstructName >= 0)
        return;
    static ULONGLONG NextTry = 0;
    if (GetTickCount64() < NextTry)
        return;
    NextTry = GetTickCount64() + 2000;
    const uintptr_t Construct = Objects::FindObject("Construct", "UserWidget");
    const uintptr_t PreConstruct = Objects::FindObject("PreConstruct", "UserWidget");
    if (!Construct || !PreConstruct)
        return;
    ConstructName = Memory::Read<int32_t>(Construct + Offsets::UObject::Name).value_or(-1);
    PreConstructName = Memory::Read<int32_t>(PreConstruct + Offsets::UObject::Name).value_or(-1);
}

const Patch List[] = {
    {"HUD wartet auf den GameState", &CheckProcessEvent, &ApplyProcessEvent},
};
}

std::span<const Patch> WidgetPatches()
{
    return List;
}

void ProcessEventLocal(uintptr_t Object, uintptr_t Function, void* Params)
{
    // UFunction.FunctionFlags, FUNC_Net. Ohne das Bit liefert GetFunctionCallspace Local, das native _Implementation
    // läuft direkt. Danach den alten Wert zurück, auch bei verschachtelten Aufrufen derselben Funktion.
    constexpr uintptr_t FunctionFlags = 0xB0;
    constexpr uint32_t FuncNet = 0x40;
    const uint32_t Flags = Memory::Read<uint32_t>(Function + FunctionFlags).value_or(0);
    const uint32_t Local = Flags & ~FuncNet;
    if (Flags != Local)
        Memory::WriteBytes(Function + FunctionFlags, &Local, sizeof(Local));
    OriginalProcessEvent(Object, Function, Params);
    if (Flags != Local)
        Memory::WriteBytes(Function + FunctionFlags, &Flags, sizeof(Flags));
}

void NotifyEventObservers(uintptr_t Object, uintptr_t Function, void* Params)
{
    for (auto& Slot : Observers)
        if (const EventObserver Observer = Slot.load(std::memory_order_acquire))
            Observer(Object, Function, Params);
}

bool AddEventObserver(EventObserver Observer)
{
    if (!Observer)
        return false;
    for (auto& Slot : Observers)
    {
        EventObserver Empty = nullptr;
        if (Slot.load() == Observer || Slot.compare_exchange_strong(Empty, Observer))
            return true;
    }
    return false;
}

void TickWidgets(NetRole Role)
{
    ResolveNames();
    if (Deferred.empty() || Role != NetRole::Client || !HasGameState())
        return;
    const std::vector<DeferredEvent> Events = std::move(Deferred);
    Deferred.clear();
    // Ereignisse einer alten Welt gehören zu einer HUD, die es nicht mehr gibt.
    const uintptr_t World = CurrentWorld();
    size_t Released = 0;
    for (DeferredEvent Event : Events)
        if (Event.World == World)
        {
            OriginalProcessEvent(Event.Object, Event.Function, &Event.Params);
            ++Released;
        }
    Core::Log::Info("HUD: {} Ereignisse nachgeholt, {} einer alten Welt verworfen", Released, Events.size() - Released);
}
}
