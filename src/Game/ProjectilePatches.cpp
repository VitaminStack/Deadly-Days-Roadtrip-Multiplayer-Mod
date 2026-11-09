// Projektile kommen aus einem Pool. Sie replizieren als Actor, aber ohne Bewegung (bReplicateMovement aus) und ohne
// Root-Komponente. Der Client sah sie deshalb nur dort, wo der Kanal sie zuerst zeigte, und immer in Grundgroesse:
// SetBulletSize skaliert beim Host die Root-Komponente, BulletSize selbst ist kein Net-Feld.
//
// Lokal zuerst (kampf): Der Client simuliert nichts selbst. Der Host schaltet nur zwei native Replikationen ein:
// - bReplicateMovement: Ort und Tempo des Projektils.
// - bReplicates der Root-Komponente: RelativeScale3D ist ein Net-Feld, damit kommt die Groesse ohne eigene Nachricht an.
//   Ort und Drehung der Root-Komponente laesst die Engine bei bReplicateMovement weg, es gibt keine Doppelung.
// Die Root-Komponente erbt bReplicates von der Vorlage im Klassen-Defaultobjekt. bReplicateMovement am Actor erbt sie
// nicht (05.11.2026 gemessen: Defaultobjekt gesetzt, neue Actors trotzdem ohne). Darum prueft der Host je Tick nur die
// neu ans Level angehaengten Actors und je Sekunde alle als Rueckfall. Schaden, Treffer und Pool bleiben beim Host.
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
constexpr uintptr_t ClassDefaults = 0x118; // UClass::ClassDefaultObject

struct
{
    uintptr_t SetReplicateMovement = 0, SetIsReplicated = 0, ForceNetUpdate = 0;
    uintptr_t Level = 0;
    int32_t SeenActors = 0;
    unsigned Moving = 0, Sized = 0, Defaults = 0, Fresh = 0, Logs = 0;
} P;

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

bool Resolve()
{
    if (P.SetReplicateMovement && P.SetIsReplicated)
        return true;
    P.SetReplicateMovement = Objects::FindObject("SetReplicateMovement", "Actor");
    P.SetIsReplicated = Objects::FindObject("SetIsReplicated", "ActorComponent");
    P.ForceNetUpdate = Objects::FindObject("ForceNetUpdate", "Actor");
    return P.SetReplicateMovement && P.SetIsReplicated;
}

// Klassen-Defaultobjekt: Bits direkt schreiben. Dort laufen keine Replikationsfunktionen.
void PrepareDefault(uintptr_t Default)
{
    bool Changed = false;
    if (!HasBit(Default + ReplicateMovementByte, ReplicateMovementMask))
        Changed |= SetBit(Default + ReplicateMovementByte, ReplicateMovementMask);
    const uintptr_t Root = Memory::ReadPointer(Default + RootComponent).value_or(0);
    if (Root && !HasBit(Root + ComponentReplicatesByte, ComponentReplicatesMask))
        Changed |= SetBit(Root + ComponentReplicatesByte, ComponentReplicatesMask);
    P.Defaults += Changed;
}

// Lebender Projektil-Actor im aktuellen Level: Replikation ueber die Engine-Setter einschalten. Wahr, wenn geaendert.
bool PrepareActor(uintptr_t Actor)
{
    const bool NeedsMovement = !HasBit(Actor + ReplicateMovementByte, ReplicateMovementMask);
    const uintptr_t Root = Memory::ReadPointer(Actor + RootComponent).value_or(0);
    const bool NeedsRoot = Root && !HasBit(Root + ComponentReplicatesByte, ComponentReplicatesMask);
    if ((!NeedsMovement && !NeedsRoot) || HasBit(Actor + DestroyingByte, 2) || !IsA(Actor, "Projectile"))
        return false;
    bool Replicate = true;
    if (NeedsMovement)
    {
        Objects::ProcessEvent(Actor, P.SetReplicateMovement, &Replicate);
        ++P.Moving;
    }
    if (NeedsRoot)
    {
        Replicate = true;
        Objects::ProcessEvent(Root, P.SetIsReplicated, &Replicate);
        ++P.Sized;
    }
    if (P.ForceNetUpdate)
        Objects::ProcessEvent(Actor, P.ForceNetUpdate, nullptr);
    return true;
}

// Je Tick: nur die seit dem letzten Tick angehaengten Eintraege von Level->Actors. Schrumpft das Array oder wechselt
// das Level, beginnt die Zaehlung neu.
void TickFresh(uintptr_t Level)
{
    const uintptr_t Data = Memory::ReadPointer(Level + Offsets::ULevel::Actors).value_or(0);
    const int32_t Count = Memory::Read<int32_t>(Level + Offsets::ULevel::Actors + 8).value_or(-1);
    if (!Data || Count < 0 || Count > Offsets::ULevel::MaxActors)
        return;
    if (Level != P.Level || Count < P.SeenActors)
    {
        P.Level = Level;
        P.SeenActors = Count; // Der Bestand gehoert dem Sekunden-Abgleich.
        return;
    }
    for (int32_t Index = P.SeenActors; Index < Count; ++Index)
        if (const uintptr_t Actor = Memory::ReadPointer(Data + Index * sizeof(uintptr_t)).value_or(0))
            P.Fresh += PrepareActor(Actor);
    P.SeenActors = Count;
}

// Je Sekunde: alle Actors des Levels, nicht das ganze Objekt-Array (das hielt den Frame spuerbar auf, host-lagspike).
// Das Defaultobjekt einer Projektilklasse kommt ueber ihre Actors dazu; neue Actors faengt ohnehin TickFresh.
void TickAll(uintptr_t Level)
{
    Objects::ForEachLevelActor(Level, [](uintptr_t Actor)
    {
        if (!IsA(Actor, "Projectile"))
            return;
        const uintptr_t Class = Memory::ReadPointer(Actor + Offsets::UObject::Class).value_or(0);
        if (const uintptr_t Default = Class ? Memory::ReadPointer(Class + ClassDefaults).value_or(0) : 0)
            PrepareDefault(Default);
        PrepareActor(Actor);
    });
}
}

void TickProjectiles(NetRole Role, bool Second)
{
    if (Role != NetRole::Host || !Resolve())
        return;
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0) : 0;
    if (!Level)
        return;
    TickFresh(Level);
    if (!Second)
        return;
    const unsigned FreshBefore = P.Fresh;
    P.Moving = P.Sized = P.Defaults = 0;
    TickAll(Level);
    // Fresh zaehlt seit der letzten Meldung, die anderen nur den Sekunden-Abgleich (Rueckfall).
    if ((P.Moving || P.Sized || P.Defaults || FreshBefore) && P.Logs < 12)
    {
        ++P.Logs;
        Core::Log::Info("Projektile: {} neu sofort, Abgleich: {} Klassen, Bewegung {}, Groesse {}", FreshBefore,
                        P.Defaults, P.Moving, P.Sized);
    }
    P.Fresh = 0;
}
}
