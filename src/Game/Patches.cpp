#include "Game/Patches.h"

#include <windows.h>

#include <atomic>
#include <vector>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/RewardAdapter.h"

namespace Game::Patches
{
namespace
{
std::atomic<State> Current = State::Pending;
Code::Writer Caves;
}

uintptr_t At(uintptr_t Rva)
{
    return Memory::ImageBase() + Rva;
}

uintptr_t CurrentWorld()
{
    return Memory::ReadPointer(At(Offsets::GWorld)).value_or(0);
}

NetRole CurrentRole()
{
    const uintptr_t World = CurrentWorld();
    const auto Driver = World ? Memory::ReadPointer(World + Offsets::UWorld::NetDriver) : std::nullopt;
    const bool HasConnection = Driver && Memory::ReadPointer(*Driver + Offsets::UNetDriver::ServerConnection);
    return RoleFrom(Driver.has_value(), HasConnection);
}

bool IsA(uintptr_t Object, const char* ClassName)
{
    auto Class = Memory::ReadPointer(Object + Offsets::UObject::Class);
    for (int Depth = 0; Class && Depth < 64; ++Depth, Class = Memory::ReadPointer(*Class + Offsets::UStruct::SuperStruct))
        if (Memory::NameOf(*Class) == ClassName)
            return true;
    return false;
}

State CurrentState()
{
    return Current.load();
}

InstallResult InstallAll(std::span<const Patch> List, Code::Writer& Code)
{
    bool Ready = true;
    for (const Patch& Entry : List)
        if (!Entry.Check || !Entry.Check())
        {
            Core::Log::Error("Patches: Prüfung fehlgeschlagen: {}", Entry.Name ? Entry.Name : "?");
            Ready = false;
        }
    if (!Ready)
        return InstallResult::CheckFailed;
    for (const Patch& Entry : List)
        if (!Entry.Apply(Code) || Code.Failed())
        {
            Core::Log::Error("Patches: Setzen fehlgeschlagen: {}", Entry.Name);
            return InstallResult::ApplyFailed;
        }
    return InstallResult::Installed;
}

bool Install()
{
    std::vector<Patch> All;
    for (const auto List : {PlayerPatches(), ClientPatches(), CameraPatches(), InventoryPatches(), TravelPatches(), WidgetPatches(),
                            HealthBarPatches(), AirdropPatches(), ExpPatches(), RemoteP2Patches(), RewardPatches()})
        All.insert(All.end(), List.begin(), List.end());

    if (!Caves.AllocateNear(Memory::ImageBase()))
    {
        Core::Log::Error("Patches: kein Speicher für Code-Höhlen in Sprungweite der Exe");
        Current = State::Inactive;
        return false;
    }
    switch (InstallAll(All, Caves))
    {
    case InstallResult::Installed:
        Current = State::Active;
        Core::Log::Info("Patches: {} gesetzt", All.size());
        return true;
    case InstallResult::CheckFailed:
        Current = State::Inactive;
        Core::Log::Error("Patches: keiner gesetzt, eine Mission zu zweit läuft so nicht");
        return false;
    default:
        Current = State::Partial;
        Core::Log::Error("Patches: nur teilweise gesetzt, das Spiel kann instabil sein");
        return false;
    }
}

void Tick()
{
    if (Current != State::Active)
        return;
    static ULONGLONG LastSecond = 0;
    const ULONGLONG Now = GetTickCount64();
    const bool Second = Now - LastSecond >= 1000;
    if (Second)
        LastSecond = Now;
    const NetRole Role = CurrentRole();
    TickWidgets(Role);
    TickPlayers(Role, Second);
    TickRemoteP2(Role);
    Rewards::Tick();
    TickCamera(Role, Second);
    TickProjectiles(Role, Second);
    TickMovement(Role, Second);
}
}
