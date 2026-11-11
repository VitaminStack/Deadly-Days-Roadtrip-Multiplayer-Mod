#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Game/WorldAdapter.h"

// Reine Logik der Patches, ohne Spielzugriff, damit tests/GameTests sie prüfen kann. Herkunft der Regeln: Spike-Dateien
// unter research/spikes/a-connect, Belege in docs/GAME_ANALYSIS.md.
namespace Game::Patches
{
// Client-Proxies haben keine ItemData; auch lokale Authority-Kopien dürfen keine Items vergeben.
constexpr bool ItemLootMayGrant(uint8_t ActorRole, NetRole WorldRole)
{
    return ActorRole == 3 && WorldRole != NetRole::Client;
}

// Nutzervorgabe 06.11.2026: Ein Itemsack zählt wie ein Airdrop für alle. Der Host gibt jedem anderen Spieler eine Kopie
// jedes neuen Items, aber nur, wenn genau ein Inventar beim Aufsammeln etwas bekommen hat.
constexpr bool ShareItemLoot(NetRole WorldRole, size_t Inventories, size_t GainedInventories)
{
    return WorldRole == NetRole::Host && Inventories > 1 && GainedInventories == 1;
}

// ADDMissionGameMode::SpawnedPlayers, ein TSet<APlayerController*>. Layout aus Build 26187885, der Speicher gehört dem Spiel.
struct ControllerSetEntry
{
    uintptr_t Controller;
    int32_t HashNext;
    int32_t HashIndex;
};

struct ControllerSet
{
    ControllerSetEntry* Data;
    int32_t Num;
    int32_t Max;
    uint32_t InlineBits[4];
    uint32_t* SecondaryBits;
    int32_t NumBits;
    int32_t MaxBits;
    int32_t FirstFree;
    int32_t NumFree;
    int32_t InlineHash[2];
    int32_t* SecondaryHash;
    int32_t HashSize;

    bool Allocated(int32_t Index) const
    {
        const auto* Bits = SecondaryBits ? SecondaryBits : InlineBits;
        return Index >= 0 && Index < Num && (Bits[Index / 32] & (1u << (Index % 32)));
    }

    // Prüft alle belegten Plätze und alle Hash-Ketten, bevor etwas geändert wird. Ein vom GC genullter Schlüssel belegt
    // seinen Platz weiter und behält seinen alten Bucket.
    bool Valid() const
    {
        if (Num < 0 || Num > 4096 || Max < Num || NumBits != Num || MaxBits < NumBits || NumFree < 0 || NumFree > Num ||
            (Num && !Data) || (!SecondaryBits && NumBits > 128) || HashSize < 0 || HashSize > 8192 ||
            (HashSize && (HashSize & (HashSize - 1))) || (!SecondaryHash && HashSize > 2))
            return false;
        int32_t Live = 0;
        for (int32_t i = 0; i < Num; ++i)
            Live += Allocated(i);
        if (Live != Num - NumFree || (Live && !HashSize))
            return false;
        const auto* Hash = SecondaryHash ? SecondaryHash : InlineHash;
        int32_t Linked = 0;
        for (int32_t Bucket = 0; Bucket < HashSize; ++Bucket)
        {
            int32_t Steps = 0;
            for (int32_t i = Hash[Bucket]; i != -1; i = Data[i].HashNext)
            {
                if (!Allocated(i) || ++Steps > Live || Data[i].HashIndex != Bucket)
                    return false;
                ++Linked;
            }
        }
        return Linked == Live;
    }

    bool Contains(uintptr_t Controller) const
    {
        for (int32_t i = 0; i < Num; ++i)
            if (Allocated(i) && Data[i].Controller == Controller)
                return true;
        return false;
    }

    // Hängt den Eintrag über den gespeicherten Bucket aus, nicht über den Hash des Zeigers: Der GC kann den Schlüssel
    // genullt haben. Belegungsbits und Freiliste pflegt das native TSparseArray::RemoveAt.
    using RemoveSparseFn = void (*)(ControllerSet*, int32_t, int32_t);
    bool Remove(int32_t Index, RemoveSparseFn RemoveSparse)
    {
        if (!Valid() || !Allocated(Index))
            return false;
        auto* Hash = SecondaryHash ? SecondaryHash : InlineHash;
        auto* Link = &Hash[Data[Index].HashIndex];
        while (*Link != Index)
            Link = &Data[*Link].HashNext;
        *Link = Data[Index].HashNext;
        RemoveSparse(this, Index, 1);
        return true;
    }
};
static_assert(offsetof(ControllerSet, SecondaryBits) == 0x20);
static_assert(offsetof(ControllerSet, NumFree) == 0x34);
static_assert(offsetof(ControllerSet, SecondaryHash) == 0x40);
static_assert(offsetof(ControllerSet, HashSize) == 0x48);
static_assert(sizeof(ControllerSet) == 0x50);
static_assert(sizeof(ControllerSetEntry) == 0x10);

// Ein abgeleiteter Wert über alle Figuren gilt erst, wenn jede beteiligte Figur ihr AttributeSet hat. Null-Figuren
// ignoriert schon das Original.
template <class CharacterAt, class HasAttributes>
bool AttributesComplete(int32_t Count, int32_t Capacity, CharacterAt At, HasAttributes Ready)
{
    if (Count < 0 || Capacity < Count || Count > 4096)
        return false;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Character = At(i);
        if (Character && !Ready(Character))
            return false;
    }
    return true;
}

// Wert, solange noch eine Figur fehlt: der letzte vollständige, sonst 0 wie bei leerer Liste. -1 ist der
// Startwert des Handlers.
inline float PendingAttributeValue(float LastComplete)
{
    return std::isfinite(LastComplete) && LastComplete >= 0.0f ? LastComplete : 0.0f;
}

// Ein Netzwerk-Client darf Ziele und Gegner einer Blockade nicht selbst planen, allein spielt man sie wie bisher.
constexpr bool BlockadeMayStart(bool HasWorld, bool HasDriver, bool HasServerConnection)
{
    return HasWorld && (!HasDriver || !HasServerConnection);
}

// Die Busabfahrt entscheidet nur der Host. Dem Client fehlt der GameMode, seine Prüfung „alle Spieler am Bus“ schlug
// mit nur einer Figur an und beendete die Runde allein beim Client (bus-ende).
constexpr bool BusTakeOffMayDecide(bool IsNetClient)
{
    return !IsNetClient;
}

// opt-busbalken: Der Client zeigt die Abfahrt, wenn der Tank voll ist und alle Figuren im Busbereich stehen, wie die Bedingungen
// des Hosts. Ohne Figuren oder Tank gibt es keine Abfahrt.
constexpr bool BusBarActive(bool FuelFull, int32_t PlayersInRange, int32_t Players)
{
    return FuelFull && Players > 0 && PlayersInRange >= Players;
}

// Neue verstrichene Zeit des Balkens: läuft bis zur Dauer, ohne Abfahrt zurück auf 0.
constexpr double BusBarElapsed(bool Active, double Elapsed, double Required, double Delta)
{
    if (!Active || !(Required > 0))
        return 0.0;
    const double Next = Elapsed + (Delta > 0 ? Delta : 0.0);
    return Next < Required ? Next : Required;
}

// opt-lootbalken: Replizierter Fortschritt einer Lootstelle ist brauchbar (schließt auch NaN aus).
constexpr bool LootBarValidProgress(double Progress)
{
    return Progress >= 0.0 && Progress <= 1.5;
}

// Das Widget nur bei einer sichtbaren Änderung neu setzen.
constexpr bool LootBarNeedsUpdate(double Shown, double Target)
{
    return Target - Shown > 0.001 || Shown - Target > 0.001;
}

// Eine Lootstelle ist fertig verfolgt, wenn sie nicht mehr durchsucht wird, der Wert angezeigt ist und eine Sekunde stillsteht.
constexpr bool LootBarDone(bool Looting, double Target, double Shown, double StalledSeconds)
{
    return !Looting && !LootBarNeedsUpdate(Shown, Target) && StalledSeconds >= 1.0;
}

// FinishMission beendet die Runde. Beim Client nur der Aufruf beim Laden, das Rundenende selbst kommt vom Host.
constexpr bool MissionFinishAllowed(bool IsNetClient, bool IsLoading)
{
    return !IsNetClient || IsLoading;
}

// rundenende-client: Der Client beendet seine Runde lokal, wenn der Host sie in derselben Mission beendet hat (gleiche
// Karten-Id, nie 0), und nur einmal je Rundenende. Ohne passende Karte bleibt er wie bisher in der Mission und folgt der Reise.
constexpr bool ClientMayFollowRoundEnd(uint64_t HostMapId, uint64_t ClientMapId, bool AlreadyFollowed)
{
    return HostMapId != 0 && HostMapId == ClientMapId && !AlreadyFollowed;
}

// run-ende: Werte von ERoundChangeReason, Argument von DDMissionGameInstance.TriggerEndRound und LastRoundChangeReason.
inline constexpr uint8_t RoundChangeWin = 2, RoundChangeLose = 3;

// run-ende: Eine Niederlage entscheidet der Host. Beim Client läuft sie nie selbst, sie würde seinen Meta-Stand ändern. Der Client
// zeigt das Ende nur an, siehe FollowRunLoss. Die anderen Gründe laufen unverändert, das Tagesende sperrt schon MissionFinishAllowed.
constexpr bool RunLossAllowed(bool IsNetClient, uint8_t Reason)
{
    return !IsNetClient || Reason != RoundChangeLose;
}

// run-ende, Client: Den Bildschirm der Niederlage nur für eine Weile nach dem Ende versuchen, er kommt sonst nie.
constexpr bool ShouldRetryLossPanel(int Attempts, int MaxAttempts)
{
    return Attempts < MaxAttempts;
}

// Der Client startet keine eigene Reise in eine Mission oder die Basis, etwa über seine Routenkarte. Das Ziel wählt der Host,
// der Client folgt über servertravel. Ins Hauptmenü darf er weiter.
constexpr bool ClientMayStartTravel(bool IsNetClient, std::string_view Target)
{
    return !IsNetClient || Target.find("MainMenu") != std::string_view::npos;
}

// bestaetigen-rundenende: Bestätigen im Lager am Rundenende. Ids sind Game::Map::RoundEnd::Id, 0 heißt keins.
// Host: Sein Bestätigen wartet nur, wenn ein Client mit der Mod verbunden ist, dieses Rundenende selbst geöffnet hat und
// noch nicht bereit ist. Hat der Client die Runde nicht nachgezogen (andere Karte, kein GameState), wartet niemand.
constexpr bool HostHoldsRoundEndConfirm(uint64_t End, uint64_t Done, bool RemoteConnected, uint64_t ClientIn, uint64_t ClientReady)
{
    return End != 0 && End != Done && RemoteConnected && ClientIn == End && ClientReady != End;
}

// Client: Sein Bestätigen wartet nur, wenn der Host gerade dasselbe Rundenende meldet. Ohne frischen Stand des Hosts läuft
// es nativ wie bisher.
constexpr bool ClientHoldsRoundEndConfirm(uint64_t End, uint64_t Done, uint64_t HostStatusId)
{
    return End != 0 && End != Done && HostStatusId == End;
}

// Client: zurückgehaltenes Bestätigen auslösen, wenn beide bereit sind oder der Stand des Hosts fehlt oder nicht mehr zu
// diesem Rundenende passt; so bleibt das Lager nie hängen.
constexpr bool ClientMayReleaseRoundEnd(uint64_t End, uint64_t HostStatusId, bool HostReady, bool ClientReadyEcho)
{
    return HostStatusId != End || (HostReady && ClientReadyEcho);
}

// Beim Client bleiben ablaufende statische Pfad-GUIDs erhalten, keine dynamischen, NoLoad- oder kaputten Einträge.
constexpr bool RetainNetGuidPath(bool CurrentClientCache, uint64_t Guid, uint32_t PathName, uint8_t Flags)
{
    constexpr uint8_t NoLoadOrBroken = 0x01 | 0x08;
    return CurrentClientCache && Guid > 1 && (Guid & 1) && PathName && !(Flags & NoLoadOrBroken);
}

// Code an der geprüften Ablaufstelle in FNetGUIDCache::CleanReferences: rdi = Wert im Cache (Schlüssel bei rdi-8),
// [rsp+68h] = durchlaufene ObjectLookup. Nur rax wird benutzt, keine Aufrufe, keine Schreibzugriffe. Original läuft
// den normalen Ablauf, Next überspringt das Löschen. Entspricht RetainNetGuidPath.
template <typename WriterT> void EmitNetGuidExpiryGuard(WriterT& Code, uintptr_t Original, uintptr_t Next)
{
    Code.Emit({0x48, 0x83, 0x7F, 0xF8, 0x01, 0x0F, 0x86}); // Schlüssel <= 1
    Code.EmitRel(Original);
    Code.Emit({0xF6, 0x47, 0xF8, 0x01, 0x0F, 0x84}); // dynamische GUID
    Code.EmitRel(Original);
    Code.Emit({0xF6, 0x47, 0x28, 0x09, 0x0F, 0x85}); // NoLoad oder Broken
    Code.EmitRel(Original);
    Code.Emit({0x83, 0x7F, 0x10, 0x00, 0x0F, 0x84}); // kein Pfad
    Code.EmitRel(Original);
    // GWorld ist beim GC während der Reise null, deshalb der NetDriver des Caches selbst, dann der Rückverweis.
    Code.Emit({0x48, 0x8B, 0x44, 0x24, 0x68}); // ObjectLookup = Cache+8
    Code.Emit({0x48, 0x85, 0xC0, 0x0F, 0x84});
    Code.EmitRel(Original);
    Code.Emit({0x48, 0x8B, 0x80, 0x48, 0x01, 0x00, 0x00, 0x48, 0x85, 0xC0, 0x0F, 0x84}); // Cache+150h NetDriver
    Code.EmitRel(Original);
    Code.Emit({0x48, 0x83, 0xB8, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0F, 0x84}); // ServerConnection
    Code.EmitRel(Original);
    Code.Emit({0x48, 0x8B, 0x80, 0xC8, 0x01, 0x00, 0x00, 0x48, 0x85, 0xC0, 0x0F, 0x84}); // GUID-Cache des Treibers
    Code.EmitRel(Original);
    Code.Emit({0x48, 0x83, 0xC0, 0x08, 0x48, 0x3B, 0x44, 0x24, 0x68, 0x0F, 0x85}); // dieselbe ObjectLookup
    Code.EmitRel(Original);
    Code.Emit({0xE9});
    Code.EmitRel(Next);
}

// SneakApproach nutzt ein schon gewähltes Ziel erst wieder, wenn dessen Mesh und PlayerState da sind.
constexpr bool ReuseSneakTarget(bool SameTarget, bool HasMesh, bool HasPlayerState)
{
    return SameTarget && HasMesh && HasPlayerState;
}

// Code an der Vergleichsstelle: Flags des Originalvergleichs und rdi = Komponente sind gültig. Keine Schreibzugriffe.
// Entspricht ReuseSneakTarget: Ready bei Wiederverwendung, sonst Retry (normale Initialisierung).
template <typename WriterT> void EmitSneakTargetGuard(WriterT& Code, uintptr_t Retry, uintptr_t Ready)
{
    Code.Emit({0x0F, 0x85}); // anderes Ziel
    Code.EmitRel(Retry);
    Code.Emit({0x48, 0x83, 0xBF, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x84}); // kein Mesh
    Code.EmitRel(Retry);
    Code.Emit({0x48, 0x83, 0xBF, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x84}); // kein PlayerState
    Code.EmitRel(Retry);
    Code.Emit({0xE9});
    Code.EmitRel(Ready);
}

// Der Host leitet die lokale Reise des Spiels in servertravel um, damit der Client mitkommt. Allein und ins Hauptmenü
// reist das Spiel wie gewohnt, ebenso bei einer URL, die schon einen Listen-Server öffnet.
bool ShouldRedirectTravel(NetRole Role, std::string_view Target);

// Busplatz: Liste der geschonten Actors um die Spielerfiguren ergänzen. Schreibt nach Out (Platz Capacity) und liefert
// die neue Länge; Added zählt die ergänzten Figuren. Doppelte und Null-Figuren kommen nicht dazu.
size_t MergeKeepList(const uintptr_t* Keep, size_t KeepCount, const uintptr_t* Pawns, size_t PawnCount, uintptr_t* Out,
                     size_t Capacity, size_t& Added);
// UInViewportMovementRestrictor::PreProcessMovementInput liest PlayerArray.Num über MissionGameState (RAX) ohne
// Null-Prüfung. Ohne GameState gilt dasselbe wie bei einem Spieler: keine Bremse (Skip). Sonst das Original
// cmp dword [rax+2C8h],1; jle Skip, danach weiter bei Continue.
template <typename WriterT> void EmitRestrictorGuard(WriterT& Code, uintptr_t Skip, uintptr_t Continue)
{
    Code.Emit({0x48, 0x85, 0xC0}); // test rax,rax
    Code.Emit({0x0F, 0x84});       // je Skip
    Code.EmitRel(Skip);
    Code.Emit({0x83, 0xB8, 0xC8, 0x02, 0x00, 0x00, 0x01}); // cmp dword [rax+2C8h],1
    Code.Emit({0x0F, 0x8E});                               // jle Skip
    Code.EmitRel(Skip);
    Code.Emit({0xE9}); // jmp Continue
    Code.EmitRel(Continue);
}
}
