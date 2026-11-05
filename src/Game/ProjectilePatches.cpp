// Projektile kommen aus einem Pool. Sie replizieren als Actor, aber ohne Bewegung (bReplicateMovement aus) und ohne
// Root-Komponente. Der Client sah sie deshalb nur dort, wo der Kanal sie zuerst zeigte, und immer in Grundgroesse:
// SetBulletSize skaliert beim Host die Root-Komponente, BulletSize selbst ist kein Net-Feld.
//
// Lokal zuerst (kampf): Der Client simuliert nichts selbst. Der Host schaltet nur zwei native Replikationen ein:
// - bReplicateMovement: Ort und Tempo des Projektils.
// - bReplicates der Root-Komponente: RelativeScale3D ist ein Net-Feld, damit kommt die Groesse ohne eigene Nachricht an.
//   Ort und Drehung der Root-Komponente laesst die Engine bei bReplicateMovement weg, es gibt keine Doppelung.
// Beides setzt der Host auf den Klassen-Defaultobjekten, neue Pool-Actors erben es damit ab dem ersten Schuss.
// Ein Abgleich je Sekunde faengt Actors ab, die vorher entstanden sind oder deren Root kein Default-Subobjekt ist.
// Schaden, Treffer und Pool bleiben beim Host unveraendert.
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
constexpr uintptr_t RootComponent = 0x1B8; // AActor::RootComponent
constexpr uintptr_t ComponentReplicatesByte = 0x98; // UActorComponent::bReplicates, Maske 0x80
constexpr uint8_t ComponentReplicatesMask = 0x80;
constexpr uint32_t ClassDefaultObject = 0x10; // RF_ClassDefaultObject
constexpr uint32_t ArchetypeObject = 0x20; // RF_ArchetypeObject

bool HasBit(uintptr_t Address, uint8_t Mask)
{
    // Unlesbar gilt als gesetzt, dann fasst der Patch das Objekt nicht an.
    return (Memory::Read<uint8_t>(Address).value_or(Mask) & Mask) != 0;
}

bool SetBit(uintptr_t Address, uint8_t Mask)
{
    const auto Value = Memory::Read<uint8_t>(Address);
    if (!Value)
        return false;
    const uint8_t Next = *Value | Mask;
    return Memory::WriteBytes(Address, &Next, 1);
}

// Klassen-Defaultobjekt: Bits direkt schreiben. Dort laufen keine Replikationsfunktionen.
bool PrepareDefault(uintptr_t Default)
{
    bool Changed = false;
    if (!HasBit(Default + ReplicateMovementByte, ReplicateMovementMask))
        Changed |= SetBit(Default + ReplicateMovementByte, ReplicateMovementMask);
    const uintptr_t Root = Memory::ReadPointer(Default + RootComponent).value_or(0);
    if (Root && !HasBit(Root + ComponentReplicatesByte, ComponentReplicatesMask))
        Changed |= SetBit(Root + ComponentReplicatesByte, ComponentReplicatesMask);
    return Changed;
}
}

void TickProjectiles(NetRole Role, bool Second)
{
    if (Role != NetRole::Host || !Second)
        return;
    static uintptr_t SetReplicateMovement = 0, SetIsReplicated = 0, ForceNetUpdate = 0;
    if (!SetReplicateMovement || !SetIsReplicated)
    {
        SetReplicateMovement = Objects::FindObject("SetReplicateMovement", "Actor");
        SetIsReplicated = Objects::FindObject("SetIsReplicated", "ActorComponent");
        ForceNetUpdate = Objects::FindObject("ForceNetUpdate", "Actor");
        if (!SetReplicateMovement || !SetIsReplicated)
            return;
    }
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0) : 0;
    if (!Level)
        return;
    unsigned Moving = 0, Sized = 0, Defaults = 0;
    for (int32_t Index = 0, Total = Objects::Count(); Index < Total; ++Index)
    {
        const uintptr_t Actor = Objects::At(Index);
        if (!Actor)
            continue;
        const uint32_t Flags = Memory::Read<uint32_t>(Actor + Offsets::UObject::Flags).value_or(ArchetypeObject);
        if (Flags & ClassDefaultObject)
        {
            if (IsA(Actor, "Projectile") && PrepareDefault(Actor))
                ++Defaults;
            continue;
        }
        if ((Flags & ArchetypeObject) || Memory::ReadPointer(Actor + Offsets::UObject::Outer).value_or(0) != Level)
            continue;
        const bool NeedsMovement = !HasBit(Actor + ReplicateMovementByte, ReplicateMovementMask);
        const uintptr_t Root = Memory::ReadPointer(Actor + RootComponent).value_or(0);
        const bool NeedsRoot = Root && !HasBit(Root + ComponentReplicatesByte, ComponentReplicatesMask);
        if ((!NeedsMovement && !NeedsRoot) || HasBit(Actor + DestroyingByte, 2) || !IsA(Actor, "Projectile"))
            continue;
        if (NeedsMovement)
        {
            bool Replicate = true;
            Objects::ProcessEvent(Actor, SetReplicateMovement, &Replicate);
            ++Moving;
        }
        if (NeedsRoot)
        {
            bool Replicate = true;
            Objects::ProcessEvent(Root, SetIsReplicated, &Replicate);
            ++Sized;
        }
        if (ForceNetUpdate)
            Objects::ProcessEvent(Actor, ForceNetUpdate, nullptr);
    }
    static unsigned Logs = 0;
    if ((Moving || Sized || Defaults) && Logs < 8)
    {
        ++Logs;
        Core::Log::Info("Projektile: {} Klassen vorbereitet, Bewegung fuer {}, Groesse fuer {} weitere(s) repliziert",
                        Defaults, Moving, Sized);
    }
}
}
