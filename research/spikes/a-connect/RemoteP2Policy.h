#pragma once
#include <cstdint>
#include <cstddef>

namespace Spike
{
struct WeakController
{
    int32_t Index;
    int32_t Serial;
};
struct ActivePlayerEntry
{
    int32_t Key;
    WeakController Controller;
    int32_t HashNext;
    int32_t HashIndex;
};

constexpr bool SameRemoteNotification(uintptr_t World, WeakController Controller,
    uintptr_t PreviousWorld, WeakController Previous)
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
    bool Valid() const
    {
        if (Num < 0 || Num > 16 || Max < Num || NumBits != Num || MaxBits < NumBits ||
            NumFree < 0 || NumFree > Num || (Num && !Data) || (!SecondaryBits && NumBits > 128) ||
            HashSize < 0 || HashSize > 32 || (HashSize && (HashSize & (HashSize - 1))) ||
            (!SecondaryHash && HashSize > 2))
            return false;
        int Live = 0, Keys = 0;
        for (int i = 0; i < Num; ++i)
            if (Allocated(i))
            {
                const auto Key = Data[i].Key;
                if (Key < 0 || Key > 1 || (Keys & (1 << Key)))
                    return false;
                Keys |= 1 << Key;
                ++Live;
            }
        if (Live != Num - NumFree || (Live && !HashSize))
            return false;
        const auto* Hash = SecondaryHash ? SecondaryHash : InlineHash;
        int Linked = 0;
        for (int Bucket = 0; Bucket < HashSize; ++Bucket)
        {
            int Steps = 0;
            for (int i = Hash[Bucket]; i != -1; i = Data[i].HashNext)
            {
                if (!Allocated(i) || ++Steps > Live || Data[i].HashIndex != Bucket ||
                    (Data[i].Key & (HashSize - 1)) != Bucket)
                    return false;
                ++Linked;
            }
        }
        return Linked == Live;
    }

    using RemoveSparseFn = void(*)(ActivePlayersMap*, int32_t, int32_t);
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

// Replace only the missing P2-device test. Native P1/device-loss checks before/after this site still run.
// At this exact site all volatile registers and XMM registers are dead (full function disassembled).
// RDI is the subsystem; nonvolatile registers are preserved by the Windows x64 callee.
template<class Writer>
void EmitRemoteDeviceGuard(Writer& Code, uintptr_t Predicate, uintptr_t TestSoloLoss, uintptr_t NeedReconnect)
{
    Code.Emit({0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xCF}); // shadow space; mov rcx,rdi
    Code.Emit({0x48, 0xB8});
    Code.EmitPointer(Predicate);
    Code.Emit({0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x20, 0x84, 0xC0});
    Code.Emit({0x0F, 0x85});
    Code.EmitRel(TestSoloLoss);
    Code.Emit({0x48, 0x85, 0xDB, 0x0F, 0x84}); // original test rbx,rbx / je reconnect
    Code.EmitRel(NeedReconnect);
    Code.Emit({0xE9});
    Code.EmitRel(TestSoloLoss);
}
}
