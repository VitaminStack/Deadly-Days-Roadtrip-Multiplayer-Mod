#pragma once

#include <cstddef>
#include <cstdint>

// Reine Logik für den Remote-Spieler als P2, ohne Spielzugriff, damit tests/GameTests sie prüfen kann. Herkunft:
// Spike-Probe research/spikes/a-connect/RemoteP2.cpp, Belege in research/spikes/a-connect/RemoteP2.md.
namespace Game::Patches
{
// Schwacher UObject-Verweis (FWeakObjectPtr): Platz im Objekt-Array und dessen Seriennummer.
struct WeakController
{
    int32_t Index;
    int32_t Serial;
};

// ULocalCoopSubsystem::ActivePlayers, eine TMap<int32, TWeakObjectPtr<APlayerController>>. Layout aus Build 26187885.
struct ActivePlayerEntry
{
    int32_t Key;
    WeakController Controller;
    int32_t HashNext;
    int32_t HashIndex;
};

// Dieselbe Beitrittsmeldung nur einmal je Welt und Controller. Ein neuer Controller nach dem Weltwechsel meldet neu.
constexpr bool SameRemoteNotification(uintptr_t World, WeakController Controller, uintptr_t PreviousWorld, WeakController Previous)
{
    return World == PreviousWorld && Controller.Index == Previous.Index && Controller.Serial == Previous.Serial;
}

struct ActivePlayersMap
{
    ActivePlayerEntry* Data;
    int32_t Num, Max;
    uint32_t InlineBits[4];
    uint32_t* SecondaryBits;
    int32_t NumBits, MaxBits, FirstFree, NumFree;
    int32_t InlineHash[2];
    int32_t* SecondaryHash;
    int32_t HashSize;

    bool Allocated(int32_t Index) const
    {
        const auto* Bits = SecondaryBits ? SecondaryBits : InlineBits;
        return Index >= 0 && Index < Num && (Bits[Index / 32] & (1u << (Index % 32)));
    }

    // Nur die Schlüssel 0 und 1, jeder höchstens einmal, alle Hash-Ketten vollständig. Sonst ändert die Mod nichts.
    bool Valid() const
    {
        if (Num < 0 || Num > 16 || Max < Num || NumBits != Num || MaxBits < NumBits || NumFree < 0 || NumFree > Num ||
            (Num && !Data) || (!SecondaryBits && NumBits > 128) || HashSize < 0 || HashSize > 32 ||
            (HashSize && (HashSize & (HashSize - 1))) || (!SecondaryHash && HashSize > 2))
            return false;
        int32_t Live = 0, Keys = 0;
        for (int32_t i = 0; i < Num; ++i)
            if (Allocated(i))
            {
                const int32_t Key = Data[i].Key;
                if (Key < 0 || Key > 1 || (Keys & (1 << Key)))
                    return false;
                Keys |= 1 << Key;
                ++Live;
            }
        if (Live != Num - NumFree || (Live && !HashSize))
            return false;
        const auto* Hash = SecondaryHash ? SecondaryHash : InlineHash;
        int32_t Linked = 0;
        for (int32_t Bucket = 0; Bucket < HashSize; ++Bucket)
        {
            int32_t Steps = 0;
            for (int32_t i = Hash[Bucket]; i != -1; i = Data[i].HashNext)
            {
                if (!Allocated(i) || ++Steps > Live || Data[i].HashIndex != Bucket || (Data[i].Key & (HashSize - 1)) != Bucket)
                    return false;
                ++Linked;
            }
        }
        return Linked == Live;
    }

    // Platz des Schlüssels, -1 wenn keiner.
    int32_t Find(int32_t Key) const
    {
        for (int32_t i = 0; i < Num; ++i)
            if (Allocated(i) && Data[i].Key == Key)
                return i;
        return -1;
    }

    // Hängt den Eintrag aus seiner Hash-Kette, Belegungsbits und Freiliste pflegt das native TSparseArray::RemoveAt.
    using RemoveSparseFn = void (*)(ActivePlayersMap*, int32_t, int32_t);
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
        return Valid();
    }
};
static_assert(sizeof(WeakController) == 8);
static_assert(sizeof(ActivePlayerEntry) == 0x14);
static_assert(sizeof(ActivePlayersMap) == 0x50);
static_assert(offsetof(ActivePlayersMap, NumFree) == 0x34);
static_assert(offsetof(ActivePlayersMap, HashSize) == 0x48);

// Ersetzt im Reconnect-Update nur die Prüfung auf ein fehlendes lokales P2-Gerät. Die P1-Geräteprüfung davor und die
// Prüfung auf Verlust des Solo-Controllers danach laufen weiter. An der Stelle sind alle flüchtigen Register und XMM-Register
// tot, RDI ist das Subsystem. Predicate ist bool __fastcall(uintptr_t Subsystem).
template <class WriterT> void EmitRemoteDeviceGuard(WriterT& Code, uintptr_t Predicate, uintptr_t TestSoloLoss, uintptr_t NeedReconnect)
{
    Code.Emit({0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xCF}); // sub rsp,20h; mov rcx,rdi
    Code.Emit({0x48, 0xB8});                               // mov rax,Predicate
    for (int i = 0; i < 8; ++i)
        Code.Emit({static_cast<uint8_t>(Predicate >> (i * 8))});
    Code.Emit({0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x20, 0x84, 0xC0}); // call rax; add rsp,20h; test al,al
    Code.Emit({0x0F, 0x85});                                     // jne Solo-Prüfung
    Code.EmitRel(TestSoloLoss);
    Code.Emit({0x48, 0x85, 0xDB, 0x0F, 0x84}); // Original: test rbx,rbx; je Reconnect nötig
    Code.EmitRel(NeedReconnect);
    Code.Emit({0xE9});
    Code.EmitRel(TestSoloLoss);
}
}
