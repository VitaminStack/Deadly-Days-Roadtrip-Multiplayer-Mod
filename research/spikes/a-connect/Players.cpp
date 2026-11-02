// Host: Remote-Spieler spawnen und in SpawnedPlayers halten, Positionen vom Client annehmen.
#include "Spike.h"

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
    return PatchRemoteSpawn(Code) && PatchPlayerIndex(Code) && PatchLootSpawner(Code) && PatchEnemySpawner(Code);
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

// Host: Der Seamless Travel ersetzt die Controller. In SpawnedPlayers bleibt der alte Remote-Controller stehen, nach der
// naechsten Garbage Collection als null. Der Eintrag wird direkt ersetzt, der Hash passt dann nicht mehr. Das Spiel
// durchlaeuft die Menge nur.
void RepairSpawnedPlayers(uintptr_t World)
{
    const uintptr_t GameMode = Read<uintptr_t>(World + kWorldGameMode);
    const uintptr_t GameState = Read<uintptr_t>(World + kWorldGameState);
    if (!GameMode || !GameState)
        return;
    const uintptr_t States = Read<uintptr_t>(GameState + kGameStatePlayerArray);
    const int32_t StateCount = Read<int32_t>(GameState + kGameStatePlayerArray + 8);
    const uintptr_t Entries = Read<uintptr_t>(GameMode + kGameModeSpawnedPlayers);
    const int32_t EntryCount = Read<int32_t>(GameMode + kGameModeSpawnedPlayers + 8);
    const auto Entry = [&](int32_t Index) { return reinterpret_cast<uintptr_t*>(Entries + Index * 0x10); };
    const auto IsCurrent = [&](uintptr_t Controller) {
        for (int32_t i = 0; Controller && i < StateCount; ++i)
            if (Read<uintptr_t>(Read<uintptr_t>(States + 8 * i) + kActorOwner) == Controller)
                return true;
        return false;
    };
    for (int32_t i = 0; i < StateCount; ++i)
    {
        const uintptr_t Controller = Read<uintptr_t>(Read<uintptr_t>(States + 8 * i) + kActorOwner);
        bool Found = !Controller;
        for (int32_t j = 0; j < EntryCount && !Found; ++j)
            Found = *Entry(j) == Controller;
        for (int32_t j = 0; j < EntryCount && !Found; ++j)
        {
            const bool IsFreeSlot = (*Entry(j) >> 47) != 0;
            if (IsFreeSlot || IsCurrent(*Entry(j)))
                continue;
            *Entry(j) = Controller;
            Found = true;
            Log("SpawnedPlayers: Eintrag ersetzt");
        }
    }
}
}
