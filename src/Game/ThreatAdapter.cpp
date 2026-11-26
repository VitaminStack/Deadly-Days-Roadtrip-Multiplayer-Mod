// opt-bedrohung-sound, siehe ThreatAdapter.h. Kein Hook und kein Code-Patch: Der Host liest die Stufe über die native Funktion der
// Komponente, der Client ruft das Blueprint-Ereignis seiner eigenen Stoppuhr auf.
#include "Game/ThreatAdapter.h"

#include <windows.h>

#include "Core/Dispatcher.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"
#include "Game/ThreatPolicy.h"

namespace Game
{
namespace
{
constexpr std::string_view Package = "/Script/DDSurvivors";
// Das Ereignis, das der Blueprint der Stoppuhr an ThreatLevel.OnThreatLevelChanged bindet (Old, New, je int32).
constexpr std::string_view StopwatchClass = "WBP_Stopwatch_C";
constexpr std::string_view StopwatchEvent = "CustomEvent";
// opt-routenkarte: Die Zahl unter der Stoppuhr setzt nicht die Stoppuhr selbst, sondern die Haupt-UI über ihr Ereignis am selben
// Delegate (gleiche Parameter). Ohne diesen Aufruf zeigte der Client 0, der Host 7 oder 9.
constexpr std::string_view UiClass = "WP_UI_C";
constexpr std::string_view UiEvent = "OnThreatLevelChanged_Event";
constexpr ULONGLONG ReadEveryMs = 250, SearchEveryMs = 2000;

struct ThreatParams
{
    int32_t Old = 0, New = 0;
};
static_assert(sizeof(ThreatParams) == 8);

bool Ready(NetRole Role)
{
    return Core::GameThread().IsDrainThread() && Patches::CurrentState() == Patches::State::Active && Patches::CurrentRole() == Role;
}

// ---- Host ----
uintptr_t HostWorld = 0;
Objects::Ref Component;
std::optional<int32_t> HostLevel;
ULONGLONG NextRead = 0, NextSearch = 0;

// Die Komponente der aktuellen Welt: Instanz der Klasse ThreatLevel, deren Outer der GameState der Welt ist.
uintptr_t FindComponent(uintptr_t World, uintptr_t Class)
{
    const uintptr_t State = Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0);
    if (!State)
        return 0;
    const int32_t Total = Objects::Count();
    for (int32_t Index = 0; Index < Total; ++Index)
    {
        const uintptr_t Object = Objects::At(Index);
        if (!Object || Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0) != Class)
            continue;
        if (Memory::ReadPointer(Object + Offsets::UObject::Outer).value_or(0) == State)
            return Object;
    }
    return 0;
}

// ---- Client ----
uintptr_t ClientWorld = 0;
ThreatShown Shown;
bool ReportedLayout = false, ReportedUi = false;
unsigned Calls = 0, Raises = 0;
Objects::Ref Ui;       // Haupt-UI, der die Zahl zuletzt gegeben wurde
int32_t UiLevel = -1;  // dort gezeigte Stufe, -1 ohne

bool InViewport(uintptr_t Widget)
{
    static const uintptr_t Function = Reflection::Function("/Script/UMG", "UserWidget", "IsInViewport");
    if (!Function || !Widget)
        return false;
    alignas(16) uint8_t Result[16]{};
    Objects::ProcessEvent(Widget, Function, Result);
    return Result[0] != 0;
}

// Die Haupt-UI der aktuellen Welt: Instanz von WP_UI_C im Viewport. Direkt nach der Reise lebt die der alten Welt noch.
uintptr_t FindUi(uintptr_t Class)
{
    const int32_t Total = Objects::Count();
    for (int32_t Index = 0; Index < Total; ++Index)
    {
        const uintptr_t Object = Objects::At(Index);
        if (Object && Memory::ReadPointer(Object + Offsets::UObject::Class).value_or(0) == Class && InViewport(Object))
            return Object;
    }
    return 0;
}

// opt-routenkarte: Die Zahl unter der Stoppuhr setzt die Haupt-UI über ihr Ereignis am Delegate. Sie bekommt jeden Stand, auch einen
// unveränderten, sobald sie neu ist (Reise, Beitritt) oder ihn noch nicht hat. Gemessen: Ohne das zeigte der Client am Tagesstart 0.
void ShowOnUi(int32_t Level)
{
    const uintptr_t Function = Objects::FindObject(UiEvent, UiClass);
    const uintptr_t Class = Function ? Memory::ReadPointer(Function + Offsets::UObject::Outer).value_or(0) : 0;
    if (!Class || Memory::Read<int32_t>(Function + Offsets::UStruct::PropertiesSize).value_or(0) != sizeof(ThreatParams))
    {
        if (!ReportedUi)
        {
            ReportedUi = true;
            Core::Log::Warn("Bedrohung Client: Ereignis der Haupt-UI fehlt oder hat ein anderes Layout, Zahl unter der Stoppuhr bleibt");
        }
        return;
    }
    const bool Current = Objects::IsLive(Ui) && InViewport(Ui.Object);
    const UiThreatUpdate Update = UiThreatLevelChange(Current, Current ? UiLevel : -1, Level);
    if (!Update.Call)
        return;
    if (Update.Search)
    {
        const uintptr_t Found = FindUi(Class);
        if (!Found)
            return; // noch keine Haupt-UI: Der Host wiederholt den Stand.
        Ui = Objects::RefTo(Found);
        UiLevel = -1;
    }
    ThreatParams Params{Update.Old, Update.New};
    Objects::ProcessEvent(Ui.Object, Function, &Params);
    if (UiLevel < 0)
        Core::Log::Info("Bedrohung Client: Haupt-UI zeigt Stufe {}", Level);
    UiLevel = Level;
}
}

std::optional<int32_t> ReadHostThreatLevel()
{
    if (!Ready(NetRole::Host))
        return std::nullopt;
    const uintptr_t World = Patches::CurrentWorld();
    if (World != HostWorld)
    {
        HostWorld = World;
        Component = {};
        HostLevel.reset();
        NextRead = NextSearch = 0;
    }
    if (!World)
        return std::nullopt;
    const ULONGLONG Now = GetTickCount64();
    if (Now < NextRead)
        return HostLevel;
    NextRead = Now + ReadEveryMs;
    const uintptr_t Get = Reflection::Function(Package, "ThreatLevel", "GetCurrentThreatLevel");
    if (!Get)
        return HostLevel = std::nullopt;
    if (!Objects::IsLive(Component))
    {
        Component = {};
        HostLevel.reset();
        if (Now < NextSearch)
            return std::nullopt;
        NextSearch = Now + SearchEveryMs;
        // Die Klasse ist der Outer ihrer UFunction.
        const uintptr_t Class = Memory::ReadPointer(Get + Offsets::UObject::Outer).value_or(0);
        const uintptr_t Found = Class ? FindComponent(World, Class) : 0;
        if (!Found)
            return std::nullopt;
        Component = Objects::RefTo(Found);
    }
    // GetCurrentThreatLevel hat nur den Rückgabewert (int32); der Puffer ist größer, falls die Funktion mehr schreibt.
    alignas(16) int32_t Result[4]{-1, 0, 0, 0};
    Objects::ProcessEvent(Component.Object, Get, Result);
    if (!ThreatLevelValid(Result[0]))
        return HostLevel = std::nullopt;
    if (!HostLevel || *HostLevel != Result[0])
        Core::Log::Info("Bedrohung Host: Stufe {}", Result[0]);
    return HostLevel = Result[0];
}

void ShowThreatLevel(int32_t Level)
{
    if (!Ready(NetRole::Client))
        return;
    const uintptr_t World = Patches::CurrentWorld();
    if (World != ClientWorld)
    {
        ClientWorld = World;
        Shown = {};
    }
    const ThreatEvent Event = ThreatLevelChange(Shown, Level);
    if (World && ThreatLevelValid(Level))
        ShowOnUi(Level);
    if (!World || !Event.Call)
        return;
    const uintptr_t Widget = Objects::FindInstance(StopwatchClass);
    const uintptr_t Function = Widget ? Objects::FindObject(StopwatchEvent, StopwatchClass) : 0;
    if (!Function)
        return; // Stoppuhr noch nicht da: Der Host wiederholt den Stand.
    if (Memory::Read<int32_t>(Function + Offsets::UStruct::PropertiesSize).value_or(0) != sizeof(ThreatParams))
    {
        if (!ReportedLayout)
        {
            ReportedLayout = true;
            Core::Log::Warn("Bedrohung Client: Ereignis der Stoppuhr hat ein anderes Parameterlayout, Stufe wird nicht gezeigt");
        }
        return;
    }
    ThreatParams Params{Event.Old, Event.New};
    Objects::ProcessEvent(Widget, Function, &Params);
    Shown = {true, Level};
    ++Calls;
    if (Event.New > Event.Old)
        ++Raises;
    Core::Log::Info("Bedrohung Client: Stoppuhr von {} auf {} (Aufrufe {}, davon Anstiege {})", Event.Old, Event.New, Calls, Raises);
}
}
