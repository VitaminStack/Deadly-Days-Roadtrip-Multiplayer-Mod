// eingaben: Lokale Geräte bleiben im Netzwerkspiel beim eigenen Spieler. Der native lokale Koop des Spiels würde ein
// weiteres Gerät (Gamepad, zweite Tastatur) an P2 binden und über SpawnLocalPlayer(1) einen zweiten LocalPlayer anlegen.
// Beim Host ist P2 aber der Remote-Spieler (RemoteP2Patches.cpp), beim Client gibt es kein P2. Die drei nativen
// Einstiege lehnen das deshalb im Netzwerkspiel ab, mit dem nativen Fehlerergebnis (false bzw. kein Controller), das die
// Aufrufer schon behandeln: ApplyDeviceAssignmentDraftAndJoin stellt dann den alten Stand wieder her.
// Was lokal bleibt: alles andere, Index 0 und das Lösen einer Zuordnung (-1). Ohne Netzwerk ändert sich nichts.
// Nichts wird übertragen. Regeln in LocalDevicePolicy.h, Befund in docs/GAME_ANALYSIS.md#eingaben-getrennt-eingaben.
#include <atomic>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/LocalDevicePolicy.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t AssignDevice = 0x58D93B0;      // ULocalCoopSubsystem::AssignPhysicalDevice(DeviceId, PlayerIndex)
constexpr uintptr_t DraftAssignDevice = 0x58E5690; // ULocalCoopSubsystem::DraftAssignPhysicalDevice(DeviceId, PlayerIndex)
constexpr uintptr_t SpawnLocal = 0x5901E90;        // ULocalCoopSubsystem::SpawnLocalPlayer(PlayerIndex)
constexpr uintptr_t GetController = 0x58EE0D0;     // ULocalCoopSubsystem::GetPlayerController(PlayerIndex)

using AssignFn = bool(__fastcall*)(uintptr_t, int32_t, int32_t);
using SpawnFn = uintptr_t(__fastcall*)(uintptr_t, int32_t);
using GetControllerFn = uintptr_t(__fastcall*)(uintptr_t, int32_t);
AssignFn OriginalAssign = nullptr, OriginalDraftAssign = nullptr;
SpawnFn OriginalSpawn = nullptr;
std::atomic<unsigned> Refused = 0;

void LogRefused(const char* What, int32_t Device, int32_t PlayerIndex)
{
    if (Refused.fetch_add(1) < 10)
        Core::Log::Info("Eingaben: {} abgelehnt, Gerät {} bleibt lokal, Spieler {} ist im Netzwerkspiel nicht lokal", What, Device, PlayerIndex);
}

bool __fastcall AssignHook(uintptr_t Coop, int32_t Device, int32_t PlayerIndex)
{
    if (!BlocksDeviceAssignment(CurrentRole(), PlayerIndex))
        return OriginalAssign(Coop, Device, PlayerIndex);
    LogRefused("Gerätezuordnung", Device, PlayerIndex);
    return false;
}

bool __fastcall DraftAssignHook(uintptr_t Coop, int32_t Device, int32_t PlayerIndex)
{
    if (!BlocksDeviceAssignment(CurrentRole(), PlayerIndex))
        return OriginalDraftAssign(Coop, Device, PlayerIndex);
    LogRefused("Geräteentwurf", Device, PlayerIndex);
    return false;
}

uintptr_t __fastcall SpawnHook(uintptr_t Coop, int32_t PlayerIndex)
{
    const NetRole Role = CurrentRole();
    const bool HostHasP2 = Role == NetRole::Host && PlayerIndex > 0 && reinterpret_cast<GetControllerFn>(At(GetController))(Coop, PlayerIndex);
    if (!BlocksLocalSpawn(Role, PlayerIndex, HostHasP2))
        return OriginalSpawn(Coop, PlayerIndex);
    LogRefused("Lokaler Spieler", -1, PlayerIndex);
    return 0;
}

bool CheckAssign()
{
    return Code::Expect(At(AssignDevice), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10}, "AssignPhysicalDevice");
}
bool CheckDraftAssign()
{
    return Code::Expect(At(DraftAssignDevice), {0x80, 0xB9, 0x88, 0x01, 0x00, 0x00, 0x00}, "DraftAssignPhysicalDevice");
}
bool CheckSpawn()
{
    return Code::Expect(At(SpawnLocal), {0x89, 0x54, 0x24, 0x10, 0x55, 0x53, 0x56, 0x57}, "SpawnLocalPlayer") &&
           Code::Expect(At(GetController), {0x48, 0x89, 0x5C, 0x24, 0x08}, "GetPlayerController des Coop-Subsystems");
}
bool ApplyAssign(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(AssignDevice)), reinterpret_cast<void*>(&AssignHook),
                                reinterpret_cast<void**>(&OriginalAssign));
}
bool ApplyDraftAssign(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(DraftAssignDevice)), reinterpret_cast<void*>(&DraftAssignHook),
                                reinterpret_cast<void**>(&OriginalDraftAssign));
}
bool ApplySpawn(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(SpawnLocal)), reinterpret_cast<void*>(&SpawnHook),
                                reinterpret_cast<void**>(&OriginalSpawn));
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t AssignSites[] = {AssignDevice};
constexpr uintptr_t DraftAssignSites[] = {DraftAssignDevice};
constexpr uintptr_t SpawnSites[] = {GetController, SpawnLocal};
const Patch List[] = {
    {"Lokales Gerät nicht an P2 im Netzwerkspiel", CheckAssign, ApplyAssign, AssignSites},
    {"Lokaler Geräteentwurf nicht für P2 im Netzwerkspiel", CheckDraftAssign, ApplyDraftAssign, DraftAssignSites},
    {"Kein zweiter LocalPlayer im Netzwerkspiel", CheckSpawn, ApplySpawn, SpawnSites},
};
}

std::span<const Patch> LocalDevicePatches()
{
    return List;
}
}
