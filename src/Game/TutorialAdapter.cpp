#include "Game/TutorialAdapter.h"

#include <windows.h>

#include <cstdint>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"

namespace Game
{
namespace
{
constexpr ULONGLONG CheckIntervalMs = 1000;

// Das Subsystem lebt mit der GameInstance. Solange sein Platz im Objekt-Array dasselbe Objekt hält, gilt der Treffer.
uintptr_t Subsystem = 0;
int32_t SubsystemIndex = -1;

uintptr_t FindSubsystem()
{
    if (Subsystem && Objects::At(SubsystemIndex) == Subsystem)
        return Subsystem;
    Subsystem = Objects::FindInstance("TutorialStateSubsystem", &SubsystemIndex);
    return Subsystem;
}
}

TutorialAction DecideTutorials(NetRole Role, std::optional<int32_t> SkipFlag)
{
    if (Role == NetRole::Standalone || !SkipFlag)
        return TutorialAction::None;
    return *SkipFlag ? TutorialAction::Done : TutorialAction::Skip;
}

void TickTutorials()
{
    static ULONGLONG LastCheck = 0;
    const ULONGLONG Now = GetTickCount64();
    if (Now - LastCheck < CheckIntervalMs)
        return;
    LastCheck = Now;

    const auto World = ReadWorld();
    if (!World || World->Role == NetRole::Standalone)
        return;
    const uintptr_t Tutorials = FindSubsystem();
    const auto Flag = Tutorials ? Memory::Read<int32_t>(Tutorials + Offsets::UTutorialStateSubsystem::SkipTutorialFlag) : std::nullopt;
    if (DecideTutorials(World->Role, Flag) != TutorialAction::Skip)
        return;

    static const uintptr_t Function = Objects::FindObject("SetForceSkipTutorials", "TutorialStateSubsystem");
    if (!Function)
    {
        Core::Log::Error("Tutorial: SetForceSkipTutorials nicht gefunden");
        return;
    }
    struct
    {
        bool bSkip;
    } Params{ true };
    Objects::ProcessEvent(Tutorials, Function, &Params);
    const auto After = Memory::Read<int32_t>(Tutorials + Offsets::UTutorialStateSubsystem::SkipTutorialFlag);
    static bool ReportedFailure = false;
    if (After && *After)
        Core::Log::Info("Tutorial: Hinweise abgeschaltet ({})", ToString(World->Role));
    else if (!ReportedFailure)
        Core::Log::Error("Tutorial: Schalter nach dem Aufruf nicht gesetzt");
    ReportedFailure = !(After && *After);
}
}
