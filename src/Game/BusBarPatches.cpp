// opt-busbalken, Client: Ladebalken der Busabfahrt. Das Blueprint BP_SpawnAndLeaveBus zählt ihn nur dort, wo die Abfahrt läuft
// (Host); ElapsedTime, tankFull und der Fortschritt des Widgets replizieren nicht, und CanBusTakeOff ist beim Client gesperrt
// (ClientPatches.cpp). Lokal zuerst: Der Client leitet Start und Abbruch aus replizierten Daten ab, Tank voll und alle Figuren im
// Busbereich (PlayersInBusRange), und treibt den nativen Balken (SetVisibility des CountDownWidget, UpdateBusProgress). Die
// Abfahrt selbst und das Rundenende entscheidet weiter der Host. Befund: docs/GAME_ANALYSIS.md#busbalken-beim-client-opt-busbalken
#include <windows.h>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

namespace Game::Patches
{
namespace
{
// ABP_SpawnAndLeaveBus_C (Build 26495302, SDK).
constexpr uintptr_t BusPlayersInRange = 0x2D8; // AExtractionPointBase.PlayersInBusRange, TArray
constexpr uintptr_t BusCountDownWidget = 0x3B8; // UWidgetComponent*
constexpr uintptr_t BusRequiredTime = 0x420;    // double, Sekunden
// UFuelMeter, siehe docs/OFFSETS.md.
constexpr uintptr_t FuelMax = 0xF4;
constexpr uintptr_t FuelCurrent = 0xF8;
constexpr uint32_t GarbageOrUnreachableFlags = 0x30;

struct
{
    uintptr_t Bus = 0, Meter = 0, State = 0, Widget = 0;
    double Elapsed = 0;
    bool Shown = false;
    ULONGLONG LastTick = 0;
    uint64_t Reported = 0;
} B;

uintptr_t Pointer(uintptr_t Address)
{
    return Memory::ReadPointer(Address).value_or(0);
}

bool Alive(uintptr_t Object)
{
    return Object && !(Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachableFlags) & GarbageOrUnreachableFlags);
}

void Reset()
{
    B = {};
}

// Der echte Bus ist der Kindaktor mit CountDownWidget; die gleichnamige Vorlage (_CAT) hat keines.
uintptr_t FindBus()
{
    if (Alive(B.Bus))
        return B.Bus;
    B.Bus = 0;
    for (int32_t i = 0, Count = Objects::Count(); i < Count; ++i)
        if (const uintptr_t Object = Objects::At(i);
            Object && Objects::IsA(Object, "BP_SpawnAndLeaveBus_C") && Alive(Object) && Pointer(Object + BusCountDownWidget))
            return B.Bus = Object;
    return 0;
}

// Tank des Missions-GameStates (Gruppenwert, repliziert). Gesucht wird nur, wenn der Zeiger fehlt oder der GameState wechselt.
uintptr_t FindMeter(uintptr_t State)
{
    if (B.State == State && Alive(B.Meter))
        return B.Meter;
    B.State = State;
    B.Meter = 0;
    for (int32_t i = 0, Count = Objects::Count(); i < Count; ++i)
        if (const uintptr_t Object = Objects::At(i);
            Object && Pointer(Object + Offsets::UObject::Outer) == State && Objects::IsA(Object, "FuelMeter"))
            return B.Meter = Object;
    return 0;
}

void SetShown(uintptr_t Component, bool Shown)
{
    static uintptr_t Function = 0;
    if (!Function)
        Function = Reflection::Function("/Script/Engine", "SceneComponent", "SetVisibility");
    if (!Function)
        return;
    struct
    {
        bool NewVisibility;
        bool PropagateToChildren;
    } Params{Shown, true};
    Objects::ProcessEvent(Component, Function, &Params);
    B.Shown = Shown;
}

void SetProgress(uintptr_t Widget, double Progress)
{
    static uintptr_t Function = 0;
    if (!Function)
        Function = Objects::FindObject("UpdateBusProgress", "WP_Bus_Countdown_Display_C");
    if (!Function)
        return;
    struct
    {
        double Progress;
    } Params{Progress};
    Objects::ProcessEvent(Widget, Function, &Params);
}
}

void TickBusBar(NetRole Role)
{
    if (Role != NetRole::Client)
        return;
    const uintptr_t World = CurrentWorld();
    const uintptr_t State = World ? Pointer(World + Offsets::UWorld::GameState) : 0;
    if (!State || !Objects::IsA(State, "DDMissionGameState"))
    {
        if (B.Bus || B.Meter)
            Reset();
        return;
    }
    // Die Suche über alle Objekte läuft höchstens einmal je Sekunde, solange kein Bus da ist.
    static ULONGLONG NextSearch = 0;
    if (!Alive(B.Bus) && GetTickCount64() < NextSearch)
        return;
    const uintptr_t Bus = FindBus();
    if (!Bus)
    {
        NextSearch = GetTickCount64() + 1000;
        return;
    }
    const uintptr_t Meter = FindMeter(State);
    const uintptr_t Component = Pointer(Bus + BusCountDownWidget);
    // Das Widget des Busses ist die einzige Laufzeit-Instanz der Klasse (der Aufzug nutzt eine Unterklasse).
    if (!Alive(B.Widget) || !Objects::IsA(B.Widget, "WP_Bus_Countdown_Display_C"))
        B.Widget = Objects::FindInstance("WP_Bus_Countdown_Display_C");
    const uintptr_t Widget = B.Widget;
    if (!Meter || !Widget)
        return;

    const ULONGLONG Now = GetTickCount64();
    const double Delta = B.LastTick ? static_cast<double>(Now - B.LastTick) / 1000.0 : 0.0;
    B.LastTick = Now;

    const float Max = Memory::Read<float>(Meter + FuelMax).value_or(0.0f);
    const float Current = Memory::Read<float>(Meter + FuelCurrent).value_or(0.0f);
    const int32_t InRange = Memory::Read<int32_t>(Bus + BusPlayersInRange + 8).value_or(0);
    const int32_t Players = Memory::Read<int32_t>(State + Offsets::AGameStateBase::PlayerArray + 8).value_or(0);
    const double Required = Memory::Read<double>(Bus + BusRequiredTime).value_or(0.0);
    const bool Active = BusBarActive(Max > 0 && Current >= Max, InRange, Players);
    const double Elapsed = BusBarElapsed(Active, B.Elapsed, Required, Delta);
    if (Active && !B.Shown)
    {
        SetShown(Component, true);
        if (++B.Reported <= 5)
            Core::Log::Info("Busbalken: Abfahrt beim Client abgeleitet (Tank voll, {} von {} Figuren im Bereich)", InRange, Players);
    }
    else if (!Active && B.Shown && B.Elapsed < Required)
        SetShown(Component, false);
    // Nach vollem Balken bleibt er stehen, bis der Host die Runde beendet.
    if (Active || B.Shown)
        SetProgress(Widget, Required > 0 ? Elapsed / Required : 0.0);
    B.Elapsed = Elapsed;
}
}
