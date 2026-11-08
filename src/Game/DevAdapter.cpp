#include "Game/DevAdapter.h"

#include <cmath>
#include <map>
#include <string_view>

#include "Core/Dispatcher.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game
{
namespace
{
struct Vector { double X, Y, Z; };

bool Call(uintptr_t Object, const char* Owner, const char* Name, void* Params)
{
    if (!Object)
        return false;
    static std::map<std::pair<std::string_view, std::string_view>, uintptr_t> Functions;
    auto& Function = Functions[{Owner, Name}];
    if (!Function)
        Function = Objects::FindObject(Name, Owner);
    if (!Function)
        return false;
    Objects::ProcessEvent(Object, Function, Params);
    return true;
}

uintptr_t MissionState()
{
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t State = World ? Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0) : 0;
    return State && Objects::IsA(State, "DDMissionGameState") ? State : 0;
}

bool SpawnXp(int Player)
{
    const uintptr_t State = MissionState();
    const uintptr_t Players = State ? Memory::ReadPointer(State + Offsets::AGameStateBase::PlayerArray).value_or(0) : 0;
    const int32_t Count = State ? Memory::Read<int32_t>(State + Offsets::AGameStateBase::PlayerArray + 8).value_or(0) : 0;
    if (!Players || Player < 0 || Player >= Count || Count > 2)
        return false;
    uintptr_t Pawn = 0;
    Vector Position{};
    if (!Call(Memory::ReadPointer(Players + 8 * Player).value_or(0), "PlayerState", "GetPawn", &Pawn) ||
        !Objects::IsA(Pawn, "DDCharacter") || !Call(Pawn, "Actor", "K2_GetActorLocation", &Position))
        return false;
    // Parameterlayout wie im Spike (research/spikes/client-visuell/VisualProbe.cpp).
    struct { int32_t Amount; uint32_t Padding; Vector Position; } Request{200, 0, Position};
    static_assert(sizeof(Request) == 32);
    return Call(Objects::FindInstance("ExpSubsystem"), "ExpSubsystem", "RequestExpSpawning", &Request);
}

bool FillTank()
{
    const uintptr_t State = MissionState();
    if (!State)
        return false;
    // UFuelMeter (MaxRequiredFuel +0xF4, CurrentFuel +0xF8, docs/OFFSETS.md) mit dem GameState als Outer: der Gruppenwert.
    // Der Spike schreibt den Wert ebenso (tools/spike/fuel.py); die Replikation zum Client ist dort belegt.
    bool Done = false;
    for (int32_t Index = 0, Total = Objects::Count(); Index < Total; ++Index)
    {
        const uintptr_t Meter = Objects::At(Index);
        if (!Meter || Memory::ReadPointer(Meter + Offsets::UObject::Outer).value_or(0) != State ||
            !Objects::IsA(Meter, "FuelMeter"))
            continue;
        const float Max = Memory::Read<float>(Meter + 0xF4).value_or(0);
        if (!(std::isfinite(Max) && Max > 0 && Max < 100000))
            continue;
        Done |= Memory::WriteBytes(Meter + 0xF8, &Max, sizeof(Max));
    }
    return Done;
}
}

bool RunDevAction(DevAction Action, int Player)
{
    if (!Core::GameThread().IsDrainThread() || Patches::CurrentRole() != NetRole::Host)
        return false;
    bool Done = false;
    switch (Action)
    {
    case DevAction::SpawnXp: Done = SpawnXp(Player); break;
    case DevAction::FullTank: Done = FillTank(); break;
    }
    Core::Log::Info("Dev: Aktion {} fuer Spieler {} {}", static_cast<int>(Action), Player, Done ? "ausgefuehrt" : "fehlgeschlagen");
    return Done;
}
}
