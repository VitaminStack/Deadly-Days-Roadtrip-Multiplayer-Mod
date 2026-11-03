// Layout observed in build 26187885. No allocator ownership in this adapter.
#pragma once
#include <cstdint>
#include <cstddef>

namespace Spike
{
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

    // Check every allocated slot and every hash chain before changing either.
    // In particular, GC-cleared null keys still occupy a slot and keep their old bucket.
    bool Valid() const
    {
        if (Num < 0 || Num > 4096 || Max < Num || NumBits != Num || MaxBits < NumBits ||
            NumFree < 0 || NumFree > Num || (Num && !Data) || (!SecondaryBits && NumBits > 128) ||
            HashSize < 0 || HashSize > 8192 || (HashSize && (HashSize & (HashSize - 1))) ||
            (!SecondaryHash && HashSize > 2))
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

    // Unlink by stored bucket, not by the pointer hash: GC can have zeroed the key.
    // Native TSparseArray::RemoveAt performs free-list/allocation-bit bookkeeping.
    using RemoveSparseFn = void(*)(ControllerSet*, int32_t, int32_t);
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
}
