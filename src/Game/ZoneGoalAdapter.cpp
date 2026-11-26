// opt-zonenziel-anzeige, siehe ZoneGoalAdapter.h. Kein Hook und kein Code-Patch: Der Host liest das laufende Ziel seines
// Subsystems, der Client legt ein Objekt derselben Klasse an und meldet es seiner eigenen Anzeige.
#include "Game/ZoneGoalAdapter.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Core/Dispatcher.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

namespace Game
{
namespace
{
constexpr std::string_view Package = "/Script/DDSurvivors";
constexpr ULONGLONG ReadEveryMs = 250, DisplayEveryMs = 500, ScanEveryMs = 2000;
constexpr uint32_t NonInstanceFlags = 0x30; // Klassen-Default- und Archetyp-Objekt

constexpr Reflection::FieldSpec SubsystemCurrent{Package, "DDObjectiveSubsystem", "CurrentObjective", "ObjectProperty", 8};
constexpr Reflection::FieldSpec ObjectiveCurrent{Package, "DDObjective", "CurrentValue", "Int64Property", 8};
constexpr Reflection::FieldSpec ObjectiveTarget{Package, "DDObjective", "TargetValue", "Int64Property", 8};
constexpr Reflection::FieldSpec ComponentClass{Package, "BlockadeEventComponent", "ObjectiveClass", "ClassProperty", 8};
constexpr Reflection::FieldSpec WidgetManager{Package, "DDObjectiveWidget", "ObjectiveManager", "ObjectProperty", 8};
constexpr Reflection::FieldSpec WidgetCurrent{Package, "DDObjectiveWidget", "CurrentObjective", "ObjectProperty", 8};

bool Ready(NetRole Role)
{
    return Core::GameThread().IsDrainThread() && Patches::CurrentState() == Patches::State::Active && Patches::CurrentRole() == Role;
}

// Das Subsystem gehört zur GameInstance und bleibt über Reisen bestehen.
uintptr_t Subsystem()
{
    static Objects::Ref Held;
    if (!Objects::IsLive(Held))
        Held = Objects::RefTo(Objects::FindInstance("DDObjectiveSubsystem"));
    return Objects::IsLive(Held) ? Held.Object : 0;
}

// ---- Host ----
uintptr_t HostWorld = 0;
std::optional<ZoneGoalView> HostView;
ULONGLONG NextRead = 0;

// ---- Client ----
enum class Kind
{
    Other,
    Component,
    Widget
};

Kind KindOf(uintptr_t Class)
{
    static std::unordered_map<uintptr_t, Kind> Cache;
    if (const auto Hit = Cache.find(Class); Hit != Cache.end())
        return Hit->second;
    Kind Result = Kind::Other;
    uintptr_t Walk = Class;
    for (int Depth = 0; Walk && Depth < 64 && Result == Kind::Other; ++Depth)
    {
        const std::string Name = Memory::NameOf(Walk);
        if (Name == "BlockadeEventComponent")
            Result = Kind::Component;
        else if (Name == "DDObjectiveWidget")
            Result = Kind::Widget;
        Walk = Memory::ReadPointer(Walk + Offsets::UStruct::SuperStruct).value_or(0);
    }
    Cache[Class] = Result;
    return Result;
}

struct Scan
{
    uintptr_t Class = 0;            // Klasse des Ziels aus der Zonen-Komponente der aktuellen Karte
    std::vector<uintptr_t> Widgets; // Anzeigen mit Subsystem
    bool Busy = false;              // eine davon zeigt ein fremdes Ziel
};

struct State
{
    uintptr_t World = 0;
    std::optional<ZoneGoalView> Host;
    Objects::Ref Ours;                 // von uns angelegtes Ziel
    std::vector<Objects::Ref> Widgets; // Anzeigen, denen wir es gemeldet haben
    int64_t ShownCurrent = 0, ShownTarget = 0;
    int SettleTicks = 0;
    bool Failed = false;
    std::string Waiting;
    ULONGLONG NextTick = 0, NextScan = 0;
} S;

Scan ScanObjects(uintptr_t World, uintptr_t Ours)
{
    Scan Result;
    const auto ClassField = Reflection::Property(ComponentClass);
    const auto ManagerField = Reflection::Property(WidgetManager);
    const auto CurrentField = Reflection::Property(WidgetCurrent);
    const uintptr_t Level = Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0);
    if (!ManagerField || !CurrentField)
        return Result;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object)
            continue;
        const auto Flags = Memory::Read<uint32_t>(Object + Offsets::UObject::Flags);
        const auto Class = Memory::ReadPointer(Object + Offsets::UObject::Class);
        if (!Flags || (*Flags & NonInstanceFlags) || !Class)
            continue;
        switch (KindOf(*Class))
        {
        case Kind::Component:
        {
            // Nur die Komponente der aktuellen Karte: ihr Actor liegt im Level dieser Welt.
            const uintptr_t Actor = Memory::ReadPointer(Object + Offsets::UObject::Outer).value_or(0);
            const uintptr_t Outer = Actor ? Memory::ReadPointer(Actor + Offsets::UObject::Outer).value_or(0) : 0;
            if (ClassField && Level && Outer == Level && !Result.Class)
                Result.Class = Memory::ReadPointer(Object + ClassField->Offset).value_or(0);
            break;
        }
        case Kind::Widget:
            // Die zweite Instanz ohne Subsystem stürzt bei OnObjective* ab (wie bei der Boss-Leiste).
            if (Memory::ReadPointer(Object + ManagerField->Offset).value_or(0))
            {
                Result.Widgets.push_back(Object);
                const uintptr_t Current = Memory::ReadPointer(Object + CurrentField->Offset).value_or(0);
                Result.Busy = Result.Busy || (Current && Current != Ours);
            }
            break;
        default:
            break;
        }
    }
    return Result;
}

void NoteWait(std::string_view Reason)
{
    if (Reason == S.Waiting)
        return;
    S.Waiting = std::string(Reason);
    if (!Reason.empty())
        Core::Log::Info("Zonenziel: wartet, {}", Reason);
}

void Notify(uintptr_t Widget, uintptr_t Function, uintptr_t Objective)
{
    struct
    {
        uintptr_t Objective;
    } Params{Objective};
    Objects::ProcessEvent(Widget, Function, &Params);
}

// Ob eine der Anzeigen, denen wir das Ziel gemeldet haben, es noch zeigt. Eine Boss-Leiste kann es verdrängt haben.
bool StillShown()
{
    const auto CurrentField = Reflection::Property(WidgetCurrent);
    if (!CurrentField || !Objects::IsLive(S.Ours))
        return false;
    for (const auto& Widget : S.Widgets)
        if (Objects::IsLive(Widget) && Memory::ReadPointer(Widget.Object + CurrentField->Offset).value_or(0) == S.Ours.Object)
            return true;
    return false;
}

void Forget()
{
    S.Ours = {};
    S.Widgets.clear();
    S.ShownCurrent = S.ShownTarget = 0;
    S.SettleTicks = 0;
}

// Nur Anzeigen, die unser Ziel noch zeigen: OnObjectiveRemoved nähme sonst einer Boss-Leiste ihr Ziel.
void Remove(std::string_view Reason)
{
    const auto CurrentField = Reflection::Property(WidgetCurrent);
    const uintptr_t Removed = Reflection::Function(Package, "DDObjectiveWidget", "OnObjectiveRemoved");
    unsigned Count = 0;
    if (CurrentField && Removed && Objects::IsLive(S.Ours))
        for (const auto& Widget : S.Widgets)
            if (Objects::IsLive(Widget) && Memory::ReadPointer(Widget.Object + CurrentField->Offset).value_or(0) == S.Ours.Object)
            {
                Notify(Widget.Object, Removed, S.Ours.Object);
                ++Count;
            }
    Core::Log::Info("Zonenziel: Leiste entfernt ({}, {} Anzeige(n))", Reason, Count);
    Forget();
}

bool Create(const Scan& Found, const ZoneGoalView& Host)
{
    const uintptr_t Manager = Subsystem();
    const uintptr_t CreateFn = Reflection::Function(Package, "DDObjectiveSubsystem", "CreateAndStartObjective");
    const uintptr_t Started = Reflection::Function(Package, "DDObjectiveWidget", "OnObjectiveStarted");
    const auto CurrentField = Reflection::Property(ObjectiveCurrent);
    const auto TargetField = Reflection::Property(ObjectiveTarget);
    if (!Manager || !CreateFn || !Started || !CurrentField || !TargetField || !Objects::IsA(Found.Class, "Class"))
    {
        if (!S.Failed)
            Core::Log::Error("Zonenziel: nicht moeglich (Subsystem {}, Funktionen {}/{}, Felder {}/{})", Manager != 0, CreateFn != 0,
                             Started != 0, CurrentField.has_value(), TargetField.has_value());
        S.Failed = true;
        return false;
    }
    // Ohne AutoStart und ohne StartTracking: Das Objekt des Clients zählt nicht selbst und wird nie das laufende Ziel seines
    // Subsystems. Es trägt nur Beschreibung, Ist und Soll für die Anzeige.
    struct
    {
        uintptr_t Class;
        bool AutoStart;
        uint8_t Pad[7];
        uintptr_t Result;
    } Params{Found.Class, false, {}, 0};
    Objects::ProcessEvent(Manager, CreateFn, &Params);
    const uintptr_t Objective = Params.Result;
    if (!Objective || !Objects::IsA(Objective, "DDObjective"))
    {
        if (!S.Failed)
            Core::Log::Error("Zonenziel: Ziel der Klasse {} nicht entstanden", Memory::NameOf(Found.Class));
        S.Failed = true;
        return false;
    }
    // Soll und Ist des Hosts vor dem Start: Die Anzeige liest beide, wenn sie das Ziel übernimmt.
    Memory::WriteBytes(Objective + TargetField->Offset, &Host.Target, sizeof(Host.Target));
    Memory::WriteBytes(Objective + CurrentField->Offset, &Host.Current, sizeof(Host.Current));
    S.Ours = Objects::RefTo(Objective);
    S.Widgets.clear();
    for (const uintptr_t Widget : Found.Widgets)
    {
        Notify(Widget, Started, Objective);
        S.Widgets.push_back(Objects::RefTo(Widget));
    }
    S.ShownCurrent = Host.Current;
    S.ShownTarget = Host.Target;
    S.SettleTicks = 0;
    S.Failed = false;
    Core::Log::Info("Zonenziel: {} lokal angelegt, {} von {} ({} Anzeige(n))", Memory::NameOf(Found.Class), Host.Current, Host.Target,
                    Found.Widgets.size());
    return true;
}
}

std::optional<ZoneGoalView> ReadHostZoneGoal()
{
    if (!Ready(NetRole::Host))
        return std::nullopt;
    const uintptr_t World = Patches::CurrentWorld();
    if (World != HostWorld)
    {
        HostWorld = World;
        HostView.reset();
        NextRead = 0;
    }
    if (!World)
        return std::nullopt;
    const ULONGLONG Now = GetTickCount64();
    if (Now < NextRead)
        return HostView;
    NextRead = Now + ReadEveryMs;
    const uintptr_t Manager = Subsystem();
    const auto ManagerCurrent = Reflection::Property(SubsystemCurrent);
    const auto CurrentField = Reflection::Property(ObjectiveCurrent);
    const auto TargetField = Reflection::Property(ObjectiveTarget);
    if (!Manager || !ManagerCurrent || !CurrentField || !TargetField)
        return HostView = std::nullopt;
    const uintptr_t Objective = Memory::ReadPointer(Manager + ManagerCurrent->Offset).value_or(0);
    const bool Live = Objective && Objects::IsA(Objective, "DDObjective");
    const ZoneGoalView View =
        ZoneGoalFromHost(Live, Live && Objects::IsA(Objective, "DDBossObjective"),
                         Live ? Memory::Read<int64_t>(Objective + CurrentField->Offset).value_or(0) : 0,
                         Live ? Memory::Read<int64_t>(Objective + TargetField->Offset).value_or(0) : 0);
    if (!HostView || !(*HostView == View))
    {
        if (View.Active)
            Core::Log::Info("Zonenziel Host: {} {} von {}", Memory::NameOf(Memory::ReadPointer(Objective + Offsets::UObject::Class).value_or(0)),
                            View.Current, View.Target);
        else if (HostView && HostView->Active)
            Core::Log::Info("Zonenziel Host: kein laufendes Ziel mehr");
    }
    return HostView = View;
}

void SetHostZoneGoal(const ZoneGoalView& View)
{
    if (!Ready(NetRole::Client) || !ZoneGoalValid(View))
        return;
    // Ein Stand gilt für die Welt, in der er ankam. Vor dem ersten Takt in einer neuen Welt verwirft TickZoneGoalDisplay ihn.
    if (Patches::CurrentWorld() == S.World)
        S.Host = View;
}

void ResetZoneGoal()
{
    if (!Core::GameThread().IsDrainThread())
        return;
    S.Host.reset();
    if (S.Ours.Object)
    {
        if (Objects::IsLive(S.Ours))
            Remove("keine Verbindung");
        else
            Forget();
    }
}

void TickZoneGoalDisplay()
{
    if (!Ready(NetRole::Client))
    {
        // Als Host oder allein: nichts anzeigen, einen alten Stand nicht in die nächste Sitzung tragen.
        if (S.Ours.Object || S.Host)
            ResetZoneGoal();
        return;
    }
    const ULONGLONG Now = GetTickCount64();
    if (Now < S.NextTick)
        return;
    S.NextTick = Now + DisplayEveryMs;
    const uintptr_t World = Patches::CurrentWorld();
    if (World != S.World)
    {
        // Die Anzeige überlebt die Reise (sie gehört zur GameInstance), die alte Leiste muss darum weg.
        S.Host.reset();
        if (S.Ours.Object)
        {
            if (Objects::IsLive(S.Ours))
                Remove("Weltwechsel");
            else
                Forget();
        }
        S.World = World;
        S.Failed = false;
        S.NextScan = 0;
        NoteWait({});
    }
    if (!World)
        return;

    ZoneGoalClient Input;
    Input.Client = true;
    Input.HostKnown = S.Host.has_value();
    Input.Host = S.Host.value_or(ZoneGoalView{});
    if (S.Ours.Object && !StillShown())
    {
        Core::Log::Info("Zonenziel: Anzeige zeigt das Ziel nicht mehr");
        Forget();
    }
    Input.Shown = S.Ours.Object != 0;
    Input.ShownCurrent = S.ShownCurrent;
    Input.ShownTarget = S.ShownTarget;
    Input.SettleTicks = S.SettleTicks;
    if (Input.Shown && S.SettleTicks < ZoneGoalSettleTicks)
        ++S.SettleTicks;

    Scan Found;
    if (!Input.Shown && ZoneGoalWanted(Input))
    {
        // Die Suche geht über das ganze Objekt-Array, darum nur, solange eine Leiste fehlt, und höchstens alle 2 s.
        if (S.Failed || Now < S.NextScan)
            return;
        S.NextScan = Now + ScanEveryMs;
        Found = ScanObjects(World, 0);
        Input.HasClass = Found.Class != 0;
        Input.WidgetReady = !Found.Widgets.empty();
        Input.WidgetBusy = Found.Busy;
        NoteWait(!Input.HasClass      ? "Host meldet ein Ziel, die Karte des Clients nennt keine Klasse dafuer"
                 : !Input.WidgetReady ? "keine Anzeige mit Subsystem"
                 : Input.WidgetBusy   ? "Anzeige zeigt ein anderes Ziel"
                                      : "");
    }

    switch (DecideZoneGoal(Input))
    {
    case ZoneGoalStep::Create:
        Create(Found, Input.Host);
        break;
    case ZoneGoalStep::Update:
    {
        const uintptr_t SetProgress = Reflection::Function(Package, "DDObjective", "SetCurrentProgressCounter");
        if (!SetProgress)
            break;
        struct
        {
            int64_t Progress;
        } Params{Input.Host.Current};
        Objects::ProcessEvent(S.Ours.Object, SetProgress, &Params);
        Core::Log::Info("Zonenziel: {} von {}", Input.Host.Current, Input.Host.Target);
        S.ShownCurrent = Input.Host.Current;
        break;
    }
    case ZoneGoalStep::Remove:
        Remove(!Input.HostKnown || !Input.Host.Active ? "Host meldet kein Ziel mehr" : "neuer Stand des Hosts");
        S.NextScan = 0;
        break;
    case ZoneGoalStep::Settle:
    case ZoneGoalStep::None:
        break;
    }
}
}
