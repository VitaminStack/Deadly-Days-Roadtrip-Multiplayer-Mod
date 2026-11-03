// Host: Remote-Spieler spawnen und in SpawnedPlayers halten, Positionen vom Client annehmen.
#include "Spike.h"
#include "SpawnedPlayersSet.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kSpawnNullSite = 0x5901CE8;
constexpr uintptr_t kSpawnSkipTarget = 0x5901E53;
constexpr uintptr_t kPlayerIndexNullSite = 0x5B0A2E5;
constexpr uintptr_t kSpawnerPlayerRead = 0x58CEDC7;
constexpr uintptr_t kSpawnerNoPlayerExit = 0x58CF2B7;
constexpr uintptr_t kLootPawnRead = 0x5994BB0;
constexpr uintptr_t kLootNoRootTarget = 0x5994BD1;
constexpr uintptr_t kGameModeSpawnedPlayers = 0x388;
constexpr uintptr_t kActorOwner = 0x158;
constexpr uintptr_t kActorRemoteRole = 0x60;
constexpr uintptr_t kCharacterMovement = 0x330;
constexpr uintptr_t kMovementFlags = 0x579;
constexpr uintptr_t kControllerSetAdd = 0x57BD5E0;
constexpr uintptr_t kControllerSparseRemove = 0x12D6BA0;
constexpr uintptr_t kRegisterController = 0x58FB020;
bool ControllerSetFunctionsReady = false;
using RegisterControllerFn = void(__fastcall*)(uintptr_t, uintptr_t);
RegisterControllerFn OriginalRegisterController = nullptr;
void ReconcileSpawnedPlayers(uintptr_t World, const char* Trigger);

// The original must run FIRST: inserting its key beforehand would skip its legitimate broadcast.
// Reconcile only after it returns, never call registration again or install another delegate.
void __fastcall RegisterControllerHook(uintptr_t GameMode, uintptr_t Controller)
{
    OriginalRegisterController(GameMode, Controller);
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    if (World && Read<uintptr_t>(World + kWorldGameMode) == GameMode)
        ReconcileSpawnedPlayers(World, "registration");
}

bool PatchControllerRegistration(CodeWriter& Code)
{
    if (OriginalRegisterController)
        return true; // Do not chain a second hook/trampoline on repeated installation.
    const uintptr_t Site = Base + kRegisterController;
    // Complete first instruction (no RIP-relative operand), plus the add/broadcast call sites.
    if (!Expect(Site, {0x48, 0x89, 0x5C, 0x24, 0x18}, "controller registration entry") ||
        !Expect(Base + 0x58FB11D, {0xE8, 0xBE, 0x24, 0xEC, 0xFF}, "registration TSet add") ||
        !Expect(Base + 0x58FB133, {0xE8, 0xD8, 0xE2, 0xBE, 0xFC}, "registration broadcast"))
        return false;
    OriginalRegisterController = reinterpret_cast<RegisterControllerFn>(Code.Here());
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x18}); // mov [rsp+18h],rbx
    Code.Emit({0xE9});
    Code.EmitRel(Site + 5);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&RegisterControllerHook)));
    return true;
}

// Beim Spawnen ruft das Spiel GetLocalPlayer()->...() ohne Nullpruefung auf. Ein Remote-Controller hat keinen
// LocalPlayer, der Host stuerzt also ab. Der Patch springt in dem Fall in den vorhandenen Pfad ohne Coop-Subsystem.
bool PatchRemoteSpawn(CodeWriter& Code)
{
    const uintptr_t Site = Base + kSpawnNullSite;
    if (!Expect(Site, {0x48, 0x8B, 0x08, 0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00}, "beim Spawn"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC0});                               // test rax,rax
    Code.Emit({0x0F, 0x84});                                     // je ohne Coop-Logik
    Code.EmitRel(Base + kSpawnSkipTarget);
    Code.Emit({0x48, 0x8B, 0x08});                               // mov rcx,[rax]
    Code.Emit({0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00});       // mov rdx,[rcx+330h]
    Code.Emit({0x48, 0x8B, 0xC8});                               // mov rcx,rax
    Code.Emit({0xFF, 0xD2});                                     // call rdx
    Code.Emit({0x8B, 0xD0});                                     // mov edx,eax
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 17);
    WriteJump(Site, Cave);
    return true;
}

// Hilfsfunktion PC -> Spielerindex: gleiche Annahme. Ohne LocalPlayer gilt der Remote-Spieler als P2 (Index 1).
bool PatchPlayerIndex(CodeWriter& Code)
{
    const uintptr_t Site = Base + kPlayerIndexNullSite;
    if (!Expect(Site, {0x48, 0x8B, 0x08, 0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00}, "im Index-Helfer"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xC0});                               // test rax,rax
    Code.Emit({0x75, 0x0A});                                     // jne original
    Code.Emit({0xB8, 0x01, 0x00, 0x00, 0x00});                   // mov eax,1
    Code.Emit({0x48, 0x83, 0xC4, 0x28});                         // add rsp,28h
    Code.Emit({0xC3});                                           // ret
    Code.Emit({0x48, 0x8B, 0x08});                               // mov rcx,[rax]
    Code.Emit({0x48, 0x8B, 0x91, 0x30, 0x03, 0x00, 0x00});       // mov rdx,[rcx+330h]
    Code.Emit({0x48, 0x8B, 0xC8});                               // mov rcx,rax
    Code.Emit({0x48, 0x83, 0xC4, 0x28});                         // add rsp,28h
    Code.Emit({0xFF, 0xE2});                                     // jmp rdx
    WriteJump(Site, Cave);
    return true;
}

// Nach der Kartenerzeugung verteilt MapLootSpawner die Beute um die Figur des ersten Spielers. Beim Spawn mit zwei
// Spielern gibt es die noch nicht, dann gilt der vorhandene Pfad ohne RootComponent.
bool PatchLootSpawner(CodeWriter& Code)
{
    const uintptr_t Site = Base + kLootPawnRead;
    if (!Expect(Site, {0x48, 0x8B, 0x83, 0xB8, 0x01, 0x00, 0x00}, "im Loot-Spawner"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x48, 0x85, 0xDB});                               // test rbx,rbx
    Code.Emit({0x0F, 0x84});                                     // je ohne Figur
    Code.EmitRel(Base + kLootNoRootTarget);
    Code.Emit({0x48, 0x8B, 0x83, 0xB8, 0x01, 0x00, 0x00});       // mov rax,[rbx+1B8h]
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 7);
    WriteJump(Site, Cave);
    return true;
}

// Der Spawner waehlt zufaellig einen Eintrag aus SpawnedPlayers, dort kann null stehen. Dann faellt der Spawn aus.
bool PatchEnemySpawner(CodeWriter& Code)
{
    const uintptr_t Site = Base + kSpawnerPlayerRead;
    if (!Expect(Site, {0x49, 0x8B, 0x86, 0xE8, 0x02, 0x00, 0x00}, "im Gegner-Spawner"))
        return false;
    uint8_t* Cave = Code.Here();
    Code.Emit({0x4D, 0x85, 0xF6});                               // test r14,r14
    Code.Emit({0x0F, 0x84});                                     // je Spawn-Ende
    Code.EmitRel(Base + kSpawnerNoPlayerExit);
    Code.Emit({0x49, 0x8B, 0x86, 0xE8, 0x02, 0x00, 0x00});       // mov rax,[r14+2E8h]
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 7);
    WriteJump(Site, Cave);
    return true;
}
}

bool PatchPlayers(CodeWriter& Code)
{
    ControllerSetFunctionsReady =
        Expect(Base + kControllerSetAdd, {0x40, 0x53, 0x56, 0x57, 0x41, 0x55, 0x48, 0x83, 0xEC, 0x38}, "TSet controller add") &&
        Expect(Base + kControllerSparseRemove, {0x40, 0x53, 0x48, 0x8B, 0x19, 0x48, 0x8B, 0xC1}, "TSparseArray controller remove");
    if (!ControllerSetFunctionsReady)
        return false;
    return PatchRemoteSpawn(Code) && PatchPlayerIndex(Code) && PatchLootSpawner(Code) && PatchEnemySpawner(Code) &&
        PatchControllerRegistration(Code);
}

// Host: Nimmt die Position von Remote-Figuren direkt vom Client an (bIgnoreClientMovementErrorChecksAndCorrection
// und bServerAcceptClientAuthoritativePosition). Die eigene Simulation des Hosts bremst diese Figuren sonst aus.
void AcceptClientPositions()
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    int Changed = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        const uintptr_t Class = Read<uintptr_t>(Object + 0x10);
        if (!Class || NameOf(Class) != "BP_DD_Character_C" || Read<uint8_t>(Object + kActorRemoteRole) != 2)
            continue;
        auto& Flags = *reinterpret_cast<uint8_t*>(Read<uintptr_t>(Object + kCharacterMovement) + kMovementFlags);
        Changed += (Flags & 0x0A) != 0x0A;
        Flags |= 0x0A;
    }
    if (Changed)
        Log("Client-Positionen: " + std::to_string(Changed));
}

// Host/Game Thread: Reconcile the current PlayerArray with the real TSet, including its hash and sparse allocation.
// No cached actor pointers survive travel. Do not invoke the full player-registration path (which broadcasts delegates).
namespace
{
void ReconcileSpawnedPlayers(uintptr_t World, const char* Trigger)
{
    if (!ControllerSetFunctionsReady || !World)
        return;
    const uintptr_t Driver = Read<uintptr_t>(World + kWorldNetDriver);
    if (!Driver || Read<uintptr_t>(Driver + kDriverServerConnection))
        return;
    const uintptr_t GameMode = Read<uintptr_t>(World + kWorldGameMode);
    const uintptr_t GameState = Read<uintptr_t>(World + kWorldGameState);
    if (!GameMode || !GameState)
        return;
    bool IsMissionMode = false;
    for (uintptr_t Class = Read<uintptr_t>(GameMode + 0x10); Class && !IsMissionMode; Class = Read<uintptr_t>(Class + 0x40))
        IsMissionMode = NameOf(Class) == "DDMissionGameMode";
    if (!IsMissionMode)
        return; // The menu GameMode does not have the game's controller set at +0x388.
    const uintptr_t States = Read<uintptr_t>(GameState + kGameStatePlayerArray);
    const int32_t StateCount = Read<int32_t>(GameState + kGameStatePlayerArray + 8);
    if (!States || StateCount <= 0 || StateCount > 16)
        return;
    std::vector<uintptr_t> Controllers;
    for (int32_t i = 0; i < StateCount; ++i)
    {
        const uintptr_t State = Read<uintptr_t>(States + 8 * i);
        if (!State || (Read<uint32_t>(State + 8) & 0x30))
            return; // Travel/initial replication is not a complete roster yet.
        const uintptr_t Controller = Read<uintptr_t>(State + kActorOwner);
        if (!Controller || (Read<uint32_t>(Controller + 8) & 0x30))
            return;
        bool IsController = false;
        for (uintptr_t Class = Read<uintptr_t>(Controller + 0x10); Class && !IsController; Class = Read<uintptr_t>(Class + 0x40))
            IsController = NameOf(Class) == "DDPlayerController";
        if (!IsController)
            return;
        bool Duplicate = false;
        for (uintptr_t Existing : Controllers)
            Duplicate = Duplicate || Existing == Controller;
        if (Duplicate)
            return;
        Controllers.push_back(Controller);
    }
    auto& Set = *reinterpret_cast<ControllerSet*>(GameMode + kGameModeSpawnedPlayers);
    if (!Set.Valid())
    {
        Log("SpawnedPlayers: invalid set layout/hash; reconciliation refused");
        return;
    }
    int Removed = 0, Added = 0;
    for (int32_t i = 0; i < Set.Num; ++i)
    {
        if (!Set.Allocated(i))
            continue;
        bool Current = false;
        for (uintptr_t Controller : Controllers)
            Current = Current || Set.Data[i].Controller == Controller;
        // Remove duplicate allocated keys as well; retain the first current entry.
        for (int32_t j = 0; j < i && Current; ++j)
            if (Set.Allocated(j) && Set.Data[j].Controller == Set.Data[i].Controller)
                Current = false;
        if (!Current)
        {
            if (!Set.Remove(i, reinterpret_cast<ControllerSet::RemoveSparseFn>(Base + kControllerSparseRemove)))
                return;
            ++Removed;
        }
    }
    using AddFn = int32_t*(__fastcall*)(ControllerSet*, int32_t*, const uintptr_t*, bool*);
    for (uintptr_t Controller : Controllers)
        if (!Set.Contains(Controller))
        {
            int32_t Index = -1;
            reinterpret_cast<AddFn>(Base + kControllerSetAdd)(&Set, &Index, &Controller, nullptr);
            ++Added;
        }
    if (Removed || Added || std::strcmp(Trigger, "registration") == 0)
        Log(std::string("SpawnedPlayers: source=") + Trigger + " removed=" + std::to_string(Removed) +
            " added=" + std::to_string(Added) + " roster=" + std::to_string(Controllers.size()) +
            " valid=" + std::to_string(Set.Valid()));
}
}

void RepairSpawnedPlayers(uintptr_t World)
{
    ReconcileSpawnedPlayers(World, "timer");
}
}
