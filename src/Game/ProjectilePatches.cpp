// Projektile kommen aus einem Pool. Sie replizieren als Actor, aber ohne Bewegung (bReplicateMovement aus).
// Der Client sah sie deshalb nur dort, wo der Kanal sie zuerst zeigte, meist versteckt oder weit weggeflogen.
// Der Host schaltet die Bewegungsreplikation einmal je Projektil-Actor ein; der Pool behaelt den Actor und damit
// das Flag. Schaden, Treffer und Pool bleiben beim Host unveraendert, der Client bekommt Ort, Tempo, Sichtbarkeit.
// OnTakeFromPool_BP laeuft beim Host nicht ueber ProcessEvent (05.11.2026 beobachtet), daher ein Abgleich je Sekunde.
#include <windows.h>

#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t ReplicateMovementByte = 0x58; // AActor::bReplicateMovement, Maske 0x10
constexpr uint8_t ReplicateMovementMask = 0x10;
constexpr uintptr_t DestroyingByte = 0x5D; // AActor::bActorIsBeingDestroyed, Maske 0x02
}

void TickProjectiles(NetRole Role, bool Second)
{
    if (Role != NetRole::Host || !Second)
        return;
    static uintptr_t SetReplicateMovement = 0, ForceNetUpdate = 0;
    if (!SetReplicateMovement)
    {
        SetReplicateMovement = Objects::FindObject("SetReplicateMovement", "Actor");
        ForceNetUpdate = Objects::FindObject("ForceNetUpdate", "Actor");
        if (!SetReplicateMovement)
            return;
    }
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0) : 0;
    if (!Level)
        return;
    unsigned Changed = 0;
    for (int32_t Index = 0, Total = Objects::Count(); Index < Total; ++Index)
    {
        const uintptr_t Actor = Objects::At(Index);
        if (!Actor || Memory::ReadPointer(Actor + Offsets::UObject::Outer).value_or(0) != Level ||
            (Memory::Read<uint32_t>(Actor + Offsets::UObject::Flags).value_or(0x30) & 0x30))
            continue;
        const uint8_t Flags = Memory::Read<uint8_t>(Actor + ReplicateMovementByte).value_or(ReplicateMovementMask);
        if ((Flags & ReplicateMovementMask) || (Memory::Read<uint8_t>(Actor + DestroyingByte).value_or(2) & 2) || !IsA(Actor, "Projectile"))
            continue;
        bool Replicate = true;
        Objects::ProcessEvent(Actor, SetReplicateMovement, &Replicate);
        if (ForceNetUpdate)
            Objects::ProcessEvent(Actor, ForceNetUpdate, nullptr);
        ++Changed;
    }
    static unsigned Logs = 0;
    if (Changed && Logs < 5)
    {
        ++Logs;
        Core::Log::Info("Projektile: Bewegung fuer {} weitere(s) repliziert", Changed);
    }
}
}
