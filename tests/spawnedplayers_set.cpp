// Standalone synthetic layout tests; no game/SDK required.
#include "../research/spikes/a-connect/SpawnedPlayersSet.h"
#include <cassert>
#include <iostream>

void RemoveSparse(Spike::ControllerSet* Set, int32_t Index, int32_t Count)
{
    assert(Count == 1);
    Set->InlineBits[Index / 32] &= ~(1u << (Index % 32));
    ++Set->NumFree;
}

int main()
{
    Spike::ControllerSetEntry Entries[] = {{0x1234, 1, 0}, {0, -1, 0}, {0x1234, -1, 1}, {~uintptr_t(0), 99, 99}};
    Spike::ControllerSet Set{};
    Set.Data = Entries;
    Set.Num = Set.Max = Set.NumBits = 4;
    Set.MaxBits = 128;
    Set.InlineBits[0] = 7; // Free slot contains garbage, not a controller.
    Set.NumFree = 1;
    Set.HashSize = 2;
    Set.InlineHash[0] = 0;
    Set.InlineHash[1] = 2;
    assert(Set.Valid());
    assert(Set.Contains(0x1234));
    assert(!Set.Contains(~uintptr_t(0)));
    assert(!Set.Remove(3, RemoveSparse));
    assert(Set.Remove(1, RemoveSparse)); // Null key, stored bucket, middle/tail of chain.
    assert(Entries[0].HashNext == -1 && Set.Valid());
    assert(Set.Remove(0, RemoveSparse)); // Head, duplicate remains in the other bucket.
    assert(Set.InlineHash[0] == -1 && Set.Contains(0x1234) && Set.Valid());
    Entries[2].HashNext = 2;
    assert(!Set.Valid()); // Cycle rejected before mutation.
    Entries[2].HashNext = 99;
    assert(!Set.Valid()); // Invalid index rejected.
    Entries[2].HashNext = -1;
    Entries[2].HashIndex = 0;
    assert(!Set.Valid()); // Wrong bucket rejected.
    Entries[2].HashIndex = 1;
    assert(Set.Remove(2, RemoveSparse));
    assert(Set.Valid() && !Set.Contains(0x1234));
    Set.NumBits = 3;
    assert(!Set.Valid());
    std::cout << "SpawnedPlayers set tests passed\n";
}
