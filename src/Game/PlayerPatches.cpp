// Host: Remote-Spieler ohne LocalPlayer spawnen und in SpawnedPlayers halten, seine Position annehmen, ihn am Busplatz
// und beim Run-Ende nicht verlieren. Aus Spike Players.cpp, TestEnv.cpp (Busplatz) und RunEnd.cpp.
#include <windows.h>

#include <array>
#include <string>
#include <vector>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t SpawnNullSite = 0x5901CE8;
constexpr uintptr_t SpawnSkipTarget = 0x5901E53;
constexpr uintptr_t PlayerIndexNullSite = 0x5B0A2E5;
constexpr uintptr_t SpawnerPlayerRead = 0x58CEDC7;
constexpr uintptr_t SpawnerNoPlayerExit = 0x58CF2B7;
constexpr uintptr_t LootPawnRead = 0x5994BB0;
constexpr uintptr_t LootNoRootTarget = 0x5994BD1;
constexpr uintptr_t ControllerSetAdd = 0x57BD5E0;
constexpr uintptr_t ControllerSparseRemove = 0x12D6BA0;
constexpr uintptr_t RegisterController = 0x58FB020;
constexpr uintptr_t RegistrationBroadcast = 0x24E9410; // derselbe Broadcast wie DelegateBroadcast in RemoteP2Patches.cpp
constexpr uintptr_t ClearSpawnArea = 0x5991B70;
constexpr uintptr_t RunEndLocalPlayerSite = 0x584CD82;
constexpr uintptr_t RunEndContinue = 0x584CD90;

constexpr uintptr_t GameModeSpawnedPlayers = 0x388;
constexpr uintptr_t ActorOwner = 0x158;
constexpr uintptr_t ActorRemoteRole = 0x60;
constexpr uintptr_t CharacterMovement = 0x330;
constexpr uintptr_t MovementFlags = 0x579;
constexpr uint8_t AcceptClientPositionFlags = 0x0A; // bIgnoreClientMovementErrorChecksAndCorrection, bServerAcceptClientAuthoritativePosition
constexpr uintptr_t ControllerNetConnection = 0x520;
constexpr uintptr_t PlayerStatePawn = 0x320;
constexpr uint32_t GarbageOrUnreachableFlags = 0x30;

using AddFn = int32_t*(__fastcall*)(ControllerSet*, int32_t*, const uintptr_t*, bool*);
using RegisterControllerFn = void(__fastcall*)(uintptr_t, uintptr_t);
using ProcessEventFn = void(__fastcall*)(uintptr_t, uintptr_t, void*);
RegisterControllerFn OriginalRegisterController = nullptr;

struct ActorArray
{
    uintptr_t* Data;
    int32_t Num;
    int32_t Max;
};
using ClearSpawnAreaFn = void(__fastcall*)(const ActorArray*, uintptr_t);
ClearSpawnAreaFn OriginalClearSpawnArea = nullptr;

uint32_t ObjectFlags(uintptr_t Object)
{
    return Memory::Read<uint32_t>(Object + Offsets::UObject::Flags).value_or(GarbageOrUnreachableFlags);
}

bool IsCurrentController(uintptr_t Controller)
{
    return Controller && !(ObjectFlags(Controller) & GarbageOrUnreachableFlags) && IsA(Controller, "DDPlayerController");
}

ControllerSet* SpawnedPlayers(uintptr_t GameMode)
{
    return reinterpret_cast<ControllerSet*>(GameMode + GameModeSpawnedPlayers);
}

void AddController(ControllerSet& Set, uintptr_t Controller)
{
    int32_t Index = -1;
    reinterpret_cast<AddFn>(At(ControllerSetAdd))(&Set, &Index, &Controller, nullptr);
}

const char* Side(uintptr_t Controller)
{
    return Memory::ReadPointer(Controller + ControllerNetConnection) ? "remote" : "lokal";
}

// Gleicht SpawnedPlayers als echte Menge mit der PlayerArray ab, samt Hash und Belegungsbits. Ruft nie den vollen
// Registrierungsweg, der würde Delegates des Spiels erneut auslösen.
void ReconcileSpawnedPlayers(uintptr_t World, const char* Trigger)
{
    if (!World || CurrentRole() != NetRole::Host)
        return;
    const uintptr_t GameMode = Memory::ReadPointer(World + Offsets::UWorld::AuthorityGameMode).value_or(0);
    const uintptr_t GameState = Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0);
    // Das Menü hat einen anderen GameMode ohne die Menge bei +0x388.
    if (!GameMode || !GameState || !IsA(GameMode, "DDMissionGameMode"))
        return;
    const uintptr_t States = Memory::ReadPointer(GameState + Offsets::AGameStateBase::PlayerArray).value_or(0);
    const int32_t StateCount = Memory::Read<int32_t>(GameState + Offsets::AGameStateBase::PlayerArray + 8).value_or(0);
    if (!States || StateCount <= 0 || StateCount > 16)
        return;
    std::vector<uintptr_t> Controllers;
    for (int32_t i = 0; i < StateCount; ++i)
    {
        const uintptr_t State = Memory::ReadPointer(States + 8 * i).value_or(0);
        // Während Reise und erster Replikation ist die Liste noch nicht vollständig.
        if (!State || (ObjectFlags(State) & GarbageOrUnreachableFlags))
            return;
        const uintptr_t Controller = Memory::ReadPointer(State + ActorOwner).value_or(0);
        if (!IsCurrentController(Controller))
            return;
        for (const uintptr_t Existing : Controllers)
            if (Existing == Controller)
                return;
        Controllers.push_back(Controller);
    }
    ControllerSet& Set = *SpawnedPlayers(GameMode);
    if (!Set.Valid())
    {
        Core::Log::Error("SpawnedPlayers: Menge unlesbar, kein Abgleich");
        return;
    }
    int Removed = 0, Added = 0;
    std::string Detail;
    for (int32_t i = 0; i < Set.Num; ++i)
    {
        if (!Set.Allocated(i))
            continue;
        bool Current = false;
        for (const uintptr_t Controller : Controllers)
            Current = Current || Set.Data[i].Controller == Controller;
        for (int32_t j = 0; j < i && Current; ++j)
            if (Set.Allocated(j) && Set.Data[j].Controller == Set.Data[i].Controller)
                Current = false; // doppelter Schlüssel, der erste bleibt
        if (Current)
            continue;
        Detail += Set.Data[i].Controller ? " entfernt:lebt" : " entfernt:null";
        if (!Set.Remove(i, reinterpret_cast<ControllerSet::RemoveSparseFn>(At(ControllerSparseRemove))))
            return;
        ++Removed;
    }
    for (const uintptr_t Controller : Controllers)
        if (!Set.Contains(Controller))
        {
            AddController(Set, Controller);
            Detail += std::string(" dazu:") + Side(Controller);
            ++Added;
        }
    if (Removed || Added || std::string_view(Trigger) == "registration")
        Core::Log::Info("SpawnedPlayers: Quelle={} entfernt={} dazu={} Spieler={} gültig={}{}", Trigger, Removed, Added,
                        Controllers.size(), Set.Valid(), Detail);
}

// Das Original zuerst: Ein vorher eingetragener Schlüssel ließe seinen eigenen Broadcast aus.
void __fastcall RegisterControllerHook(uintptr_t GameMode, uintptr_t Controller)
{
    OriginalRegisterController(GameMode, Controller);
    const uintptr_t World = CurrentWorld();
    if (World && Memory::ReadPointer(World + Offsets::UWorld::AuthorityGameMode).value_or(0) == GameMode)
        ReconcileSpawnedPlayers(World, "registration");
}

// Nach dem Missionswechsel ersetzt der Host die Controller ohne den Registrierungsweg. Die Engine meldet das über
// K2_OnSwapPlayerControllers(OldPC, NewPC) am GameMode, und das läuft immer über ProcessEvent.
ProcessEventFn OriginalGameModeProcessEvent = nullptr;
uintptr_t SwapControllersFunction = 0;

void SwapSpawnedController(uintptr_t GameMode, uintptr_t OldController, uintptr_t NewController)
{
    const uintptr_t World = CurrentWorld();
    if (!World || Memory::ReadPointer(World + Offsets::UWorld::AuthorityGameMode).value_or(0) != GameMode ||
        !IsCurrentController(NewController))
        return;
    ControllerSet& Set = *SpawnedPlayers(GameMode);
    if (!Set.Valid())
    {
        Core::Log::Error("SpawnedPlayers: Menge unlesbar, kein Tausch");
        return;
    }
    int Removed = 0, Added = 0;
    for (int32_t i = 0; i < Set.Num; ++i)
        if (Set.Allocated(i) && Set.Data[i].Controller == OldController)
        {
            if (!Set.Remove(i, reinterpret_cast<ControllerSet::RemoveSparseFn>(At(ControllerSparseRemove))))
                return;
            ++Removed;
        }
    if (!Set.Contains(NewController))
    {
        AddController(Set, NewController);
        ++Added;
    }
    Core::Log::Info("SpawnedPlayers: Quelle=swap entfernt={} dazu={} gültig={} {}", Removed, Added, Set.Valid(), Side(NewController));
}

void __fastcall GameModeProcessEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    OriginalGameModeProcessEvent(Object, Function, Params);
    if (Function == SwapControllersFunction && Params)
    {
        const auto* Controllers = static_cast<const uintptr_t*>(Params); // OldPC, NewPC
        SwapSpawnedController(Object, Controllers[0], Controllers[1]);
    }
}

// Tauscht ProcessEvent in der vtable des nativen Missions-GameModes, die Blueprint-Unterklassen teilen sie. Das
// Klassen-Defaultobjekt gibt es erst nach dem Engine-Start, deshalb aus dem Tick.
void InstallControllerSwap()
{
    if (OriginalGameModeProcessEvent)
        return;
    static ULONGLONG NextTry = 0;
    if (GetTickCount64() < NextTry)
        return;
    NextTry = GetTickCount64() + 2000;
    const uintptr_t Defaults = Objects::FindNamed("Default__DDMissionGameMode");
    const uintptr_t Swap = Objects::FindObject("K2_OnSwapPlayerControllers", "GameModeBase");
    const uintptr_t VTable = Defaults ? Memory::ReadPointer(Defaults).value_or(0) : 0;
    if (!VTable || !Swap)
        return;
    const uintptr_t Slot = VTable + 8 * Offsets::ProcessEventIndex;
    uintptr_t Old = 0;
    SwapControllersFunction = Swap;
    if (!Code::WriteVirtual(Slot, reinterpret_cast<uintptr_t>(&GameModeProcessEvent), &Old) || !Old)
    {
        Core::Log::Error("SpawnedPlayers: Controller-Tausch nicht eingehängt");
        return;
    }
    OriginalGameModeProcessEvent = reinterpret_cast<ProcessEventFn>(Old);
    Core::Log::Info("SpawnedPlayers: Controller-Tausch eingehängt");
}

// Host: Position der Remote-Figuren direkt vom Client annehmen. Die eigene Simulation des Hosts bremst sie sonst aus.
void AcceptClientPositions()
{
    int Changed = 0;
    // Spielerfiguren leben im Level der Welt; dessen Actor-Liste ist viel kürzer als das Objekt-Array (host-lagspike).
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0) : 0;
    Objects::ForEachLevelActor(Level, [&](uintptr_t Object)
    {
        const auto Class = Memory::ReadPointer(Object + Offsets::UObject::Class);
        if (!Class || Memory::NameOf(*Class) != "BP_DD_Character_C" || Memory::Read<uint8_t>(Object + ActorRemoteRole) != uint8_t{2})
            return;
        const auto Movement = Memory::ReadPointer(Object + CharacterMovement);
        const auto Flags = Movement ? Memory::Read<uint8_t>(*Movement + MovementFlags) : std::nullopt;
        if (!Flags || (*Flags & AcceptClientPositionFlags) == AcceptClientPositionFlags)
            return;
        const uint8_t Value = *Flags | AcceptClientPositionFlags;
        Changed += Memory::WriteBytes(*Movement + MovementFlags, &Value, 1);
    });
    if (Changed)
        Core::Log::Info("Client-Positionen angenommen: {}", Changed);
}

// Busplatz: Die Freiräumung schont nur die übergebene Liste. Die Remote-Figur steht auf der Host-Figur und wurde
// zerstört, also kommen alle Spielerfiguren dazu.
void __fastcall ClearSpawnAreaHook(const ActorArray* Keep, uintptr_t Context)
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t GameState = World ? Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0) : 0;
    if (!Keep || Keep->Num <= 0 || Keep->Num > 60 || !Keep->Data || !GameState)
    {
        OriginalClearSpawnArea(Keep, Context);
        return;
    }
    std::array<uintptr_t, 16> Pawns{};
    size_t PawnCount = 0;
    const uintptr_t States = Memory::ReadPointer(GameState + Offsets::AGameStateBase::PlayerArray).value_or(0);
    const int32_t Count = Memory::Read<int32_t>(GameState + Offsets::AGameStateBase::PlayerArray + 8).value_or(0);
    for (int32_t i = 0; States && i < Count && PawnCount < Pawns.size(); ++i)
        if (const auto State = Memory::ReadPointer(States + 8 * i))
            Pawns[PawnCount++] = Memory::ReadPointer(*State + PlayerStatePawn).value_or(0);
    // Nur lesend benutzt, ein eigener Puffer für die Dauer des Aufrufs reicht.
    std::array<uintptr_t, 64> Buffer{};
    size_t Added = 0;
    const size_t Merged = MergeKeepList(Keep->Data, static_cast<size_t>(Keep->Num), Pawns.data(), PawnCount, Buffer.data(), Buffer.size(), Added);
    const ActorArray Extended{Buffer.data(), static_cast<int32_t>(Merged), static_cast<int32_t>(Buffer.size())};
    Core::Log::Info("Busplatz: {} geschont, {} Spielerfiguren ergänzt", Keep->Num, Added);
    OriginalClearSpawnArea(&Extended, Context);
}

bool CheckRemoteSpawn()
{
    return Code::Expect(At(SpawnNullSite), {0x48, 0x8B, 0x08, 0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00}, "beim Spawn");
}

// Beim Spawnen ruft das Spiel GetLocalPlayer()->...() ohne Nullprüfung. Ohne LocalPlayer in den vorhandenen Pfad ohne Coop-Subsystem.
bool ApplyRemoteSpawn(Code::Writer& Code)
{
    const uintptr_t Site = At(SpawnNullSite);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC0});                         // test rax,rax
    Code.Emit({0x0F, 0x84});                               // je ohne Coop-Logik
    Code.EmitRel(At(SpawnSkipTarget));
    Code.Emit({0x48, 0x8B, 0x08});                         // mov rcx,[rax]
    Code.Emit({0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00}); // mov rdx,[rcx+330h]
    Code.Emit({0x48, 0x8B, 0xC8});                         // mov rcx,rax
    Code.Emit({0xFF, 0xD2});                               // call rdx
    Code.Emit({0x8B, 0xD0});                               // mov edx,eax
    Code.Emit({0xE9});                                     // jmp zurück
    Code.EmitRel(Site + 17);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

bool CheckPlayerIndex()
{
    return Code::Expect(At(PlayerIndexNullSite), {0x48, 0x8B, 0x08, 0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00}, "im Index-Helfer");
}

// Controller zu Spielerindex: Ohne LocalPlayer gilt der Remote-Spieler als P2 (Index 1).
bool ApplyPlayerIndex(Code::Writer& Code)
{
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC0});                         // test rax,rax
    Code.Emit({0x75, 0x0A});                               // jne original
    Code.Emit({0xB8, 0x01, 0x00, 0x00, 0x00});             // mov eax,1
    Code.Emit({0x48, 0x83, 0xC4, 0x28});                   // add rsp,28h
    Code.Emit({0xC3});                                     // ret
    Code.Emit({0x48, 0x8B, 0x08});                         // mov rcx,[rax]
    Code.Emit({0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00}); // mov rdx,[rcx+330h]
    Code.Emit({0x48, 0x8B, 0xC8});                         // mov rcx,rax
    Code.Emit({0x48, 0x83, 0xC4, 0x28});                   // add rsp,28h
    Code.Emit({0xFF, 0xE2});                               // jmp rdx
    return !Code.Failed() && Code::WriteJump(At(PlayerIndexNullSite), Cave);
}

bool CheckLootSpawner()
{
    return Code::Expect(At(LootPawnRead), {0x48, 0x8B, 0x83, 0xB8, 0x01, 0x00, 0x00}, "im Loot-Spawner");
}

// MapLootSpawner verteilt die Beute um die Figur des ersten Spielers, die es beim Spawn zu zweit noch nicht gibt.
bool ApplyLootSpawner(Code::Writer& Code)
{
    const uintptr_t Site = At(LootPawnRead);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xDB});                         // test rbx,rbx
    Code.Emit({0x0F, 0x84});                               // je ohne Figur
    Code.EmitRel(At(LootNoRootTarget));
    Code.Emit({0x48, 0x8B, 0x83, 0xB8, 0x01, 0x00, 0x00}); // mov rax,[rbx+1B8h]
    Code.Emit({0xE9});
    Code.EmitRel(Site + 7);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

bool CheckEnemySpawner()
{
    return Code::Expect(At(SpawnerPlayerRead), {0x49, 0x8B, 0x86, 0xE8, 0x02, 0x00, 0x00}, "im Gegner-Spawner");
}

// Der Spawner wählt zufällig einen Eintrag aus SpawnedPlayers, dort kann null stehen. Dann fällt dieser Spawn aus.
bool ApplyEnemySpawner(Code::Writer& Code)
{
    const uintptr_t Site = At(SpawnerPlayerRead);
    uint8_t* Cave = Code.Here();
    Code.Emit({0x4D, 0x85, 0xF6});                         // test r14,r14
    Code.Emit({0x0F, 0x84});                               // je Spawn-Ende
    Code.EmitRel(At(SpawnerNoPlayerExit));
    Code.Emit({0x49, 0x8B, 0x86, 0xE8, 0x02, 0x00, 0x00}); // mov rax,[r14+2E8h]
    Code.Emit({0xE9});
    Code.EmitRel(Site + 7);
    return !Code.Failed() && Code::WriteJump(Site, Cave);
}

bool CheckControllerRegistration()
{
    return Code::Expect(At(ControllerSetAdd), {0x40, 0x53, 0x56, 0x57, 0x41, 0x55, 0x48, 0x83, 0xEC, 0x38}, "TSet-Add der Controller") &&
           Code::Expect(At(ControllerSparseRemove), {0x40, 0x53, 0x48, 0x8B, 0x19, 0x48, 0x8B, 0xC1}, "TSparseArray-RemoveAt der Controller") &&
           Code::Expect(At(RegisterController), {0x48, 0x89, 0x5C, 0x24, 0x18}, "Controller-Registrierung") &&
           ExpectBranch(At(0x58FB11D), CallRel32, ControllerSetAdd, "TSet-Add in der Registrierung") &&
           ExpectBranch(At(0x58FB133), CallRel32, RegistrationBroadcast, "Broadcast in der Registrierung");
}

bool ApplyControllerRegistration(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(RegisterController)), reinterpret_cast<void*>(&RegisterControllerHook),
                                reinterpret_cast<void**>(&OriginalRegisterController));
}

bool CheckBus()
{
    return Code::Expect(At(ClearSpawnArea), {0x48, 0x89, 0x5C, 0x24, 0x18}, "in der Freiräumung am Busplatz");
}

bool ApplyBus(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(ClearSpawnArea)), reinterpret_cast<void*>(&ClearSpawnAreaHook),
                                reinterpret_cast<void**>(&OriginalClearSpawnArea));
}

bool CheckRunEnd()
{
    return Code::Expect(At(RunEndLocalPlayerSite), {0x48, 0x8B, 0xC8, 0x48, 0x8B, 0x00, 0xFF, 0x90, 0x30, 0x03, 0x00, 0x00, 0x8B, 0xE8},
                        "beim Run-Ende");
}

// Nach "Zurück zur Basis" ruft der Handler je Controller GetLocalPlayer()->...() ohne Nullprüfung. Ohne LocalPlayer gilt Index 1.
bool ApplyRunEnd(Code::Writer& Code)
{
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC0});                   // test rax,rax
    Code.Emit({0x75, 0x0A});                         // jne original
    Code.Emit({0xBD, 0x01, 0x00, 0x00, 0x00});       // mov ebp,1
    Code.Emit({0xE9});                               // jmp weiter
    Code.EmitRel(At(RunEndContinue));
    Code.Emit({0x48, 0x8B, 0xC8});                   // mov rcx,rax
    Code.Emit({0x48, 0x8B, 0x00});                   // mov rax,[rax]
    Code.Emit({0xFF, 0x90, 0x30, 0x03, 0x00, 0x00}); // call [rax+330h]
    Code.Emit({0x8B, 0xE8});                         // mov ebp,eax
    Code.Emit({0xE9});
    Code.EmitRel(At(RunEndContinue));
    return !Code.Failed() && Code::WriteJump(At(RunEndLocalPlayerSite), Cave);
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t RemoteSpawnSites[] = {SpawnNullSite, SpawnSkipTarget};
constexpr uintptr_t PlayerIndexSites[] = {PlayerIndexNullSite};
constexpr uintptr_t LootSpawnerSites[] = {LootPawnRead, LootNoRootTarget};
constexpr uintptr_t EnemySpawnerSites[] = {SpawnerPlayerRead, SpawnerNoPlayerExit};
constexpr uintptr_t ControllerRegistrationSites[] = {ControllerSparseRemove, RegistrationBroadcast, ControllerSetAdd, RegisterController, 0x58FB11D, 0x58FB133};
constexpr uintptr_t BusSites[] = {ClearSpawnArea};
constexpr uintptr_t RunEndSites[] = {RunEndLocalPlayerSite, RunEndContinue};
const Patch List[] = {
    {"Spawn des Remote-Spielers", &CheckRemoteSpawn, &ApplyRemoteSpawn, RemoteSpawnSites},
    {"Spielerindex ohne LocalPlayer", &CheckPlayerIndex, &ApplyPlayerIndex, PlayerIndexSites},
    {"Loot-Spawner ohne Figur", &CheckLootSpawner, &ApplyLootSpawner, LootSpawnerSites},
    {"Gegner-Spawner ohne Spieler", &CheckEnemySpawner, &ApplyEnemySpawner, EnemySpawnerSites},
    {"SpawnedPlayers bei der Registrierung", &CheckControllerRegistration, &ApplyControllerRegistration, ControllerRegistrationSites},
    {"Busplatz schont Spielerfiguren", &CheckBus, &ApplyBus, BusSites},
    {"Run-Ende ohne LocalPlayer", &CheckRunEnd, &ApplyRunEnd, RunEndSites},
};
}

std::span<const Patch> PlayerPatches()
{
    return List;
}

void TickPlayers(NetRole Role, bool Second)
{
    InstallControllerSwap();
    if (Role != NetRole::Host || !Second)
        return;
    // Rückfallebene: Registrierung und Tausch decken heute jeden gemessenen Wechsel ab.
    ReconcileSpawnedPlayers(CurrentWorld(), "timer");
    AcceptClientPositions();
}
}
