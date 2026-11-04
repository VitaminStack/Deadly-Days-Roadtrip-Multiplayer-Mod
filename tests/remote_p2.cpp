#include <windows.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include "../research/spikes/a-connect/RemoteP2Policy.h"

struct Writer
{
    uint8_t* Out;
    void Emit(std::initializer_list<uint8_t> Bytes) { for (auto Byte : Bytes) *Out++ = Byte; }
    void EmitPointer(uintptr_t Pointer) { memcpy(Out, &Pointer, 8); Out += 8; }
    void EmitRel(uintptr_t Target)
    {
        const auto Offset = static_cast<int32_t>(Target - reinterpret_cast<uintptr_t>(Out + 4));
        memcpy(Out, &Offset, 4); Out += 4;
    }
};

bool __fastcall Remote(uintptr_t Value) { return Value == 1; }

void RemoveSparse(Spike::ActivePlayersMap* Map, int32_t Index, int32_t Count)
{
    assert(Count == 1);
    ++Map->NumFree;
    Map->InlineBits[0] &= ~(1u << Index);
    Map->FirstFree = Index;
}

int main()
{
    using namespace Spike;
    ActivePlayerEntry Entries[] = {{0, {10, 20}, -1, 0}, {1, {11, 21}, 0, 0}, {99, {}, -1, 0}};
    ActivePlayersMap Map{};
    Map.Data = Entries; Map.Num = Map.Max = Map.NumBits = Map.MaxBits = 3;
    Map.NumFree = 1; Map.InlineBits[0] = 3;
    Map.HashSize = 1; Map.InlineHash[0] = 1;
    assert(Map.Valid()); // Sparse free slot with unrelated bytes is not a player.
    Entries[1].Key = 0; assert(!Map.Valid()); Entries[1].Key = 1;
    Entries[0].HashNext = 1; assert(!Map.Valid()); Entries[0].HashNext = -1;
    Entries[1].HashIndex = 1; assert(!Map.Valid()); Entries[1].HashIndex = 0;
    Map.InlineHash[0] = 2; assert(!Map.Valid()); Map.InlineHash[0] = 1;
    Map.NumFree = 0; assert(!Map.Valid()); Map.NumFree = 1;
    Map.NumBits = 2; assert(!Map.Valid()); Map.NumBits = 3;
    assert(Map.Valid());
    assert(Map.Remove(1, RemoveSparse));
    assert(Map.Valid() && Map.InlineHash[0] == 0 && Map.NumFree == 2);
    assert(!Map.Remove(1, RemoveSparse));
    assert(Map.Remove(0, RemoveSparse));
    assert(Map.Valid() && Map.InlineHash[0] == -1 && Map.NumFree == 3);
    ActivePlayersMap Empty{}; assert(Empty.Valid());
    assert(SameRemoteNotification(100, {10, 20}, 100, {10, 20}));
    assert(!SameRemoteNotification(101, {10, 20}, 100, {10, 20}));
    assert(!SameRemoteNotification(100, {11, 20}, 100, {10, 20}));
    assert(!SameRemoteNotification(100, {10, 21}, 100, {10, 20}));

    auto* Memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(Memory);
    Writer Code{Memory};
    // Windows x64 wrapper: save nonvolatile RBX/RDI/RBP; match the real site's live registers/stack alignment.
    Code.Emit({0x53, 0x57, 0x55, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xF9, 0x48, 0x8B, 0xDA, 0x41, 0x8A, 0xE8});
    Code.Emit({0x45, 0x84, 0xC9, 0x0F, 0x84}); // native P1-device check BEFORE the replaced branch
    Code.EmitRel(reinterpret_cast<uintptr_t>(Memory + 0x200));
    EmitRemoteDeviceGuard(Code, reinterpret_cast<uintptr_t>(&Remote), reinterpret_cast<uintptr_t>(Memory + 0x100),
        reinterpret_cast<uintptr_t>(Memory + 0x200));
    Code.Out = Memory + 0x100;
    Code.Emit({0x40, 0x84, 0xED, 0x0F, 0x85}); // native remaining solo-controller-loss check
    Code.EmitRel(reinterpret_cast<uintptr_t>(Memory + 0x200));
    Code.Emit({0x31, 0xC0, 0x48, 0x83, 0xC4, 0x20, 0x5D, 0x5F, 0x5B, 0xC3});
    Code.Out = Memory + 0x200;
    Code.Emit({0xB8, 1, 0, 0, 0, 0x48, 0x83, 0xC4, 0x20, 0x5D, 0x5F, 0x5B, 0xC3});
    FlushInstructionCache(GetCurrentProcess(), Memory, 0x1000);
    using Probe = bool(__fastcall*)(uintptr_t, uintptr_t, bool, bool);
    const auto Run = reinterpret_cast<Probe>(Memory);
    for (int RemotePresent = 0; RemotePresent < 2; ++RemotePresent)
        for (int LocalP2Device = 0; LocalP2Device < 2; ++LocalP2Device)
            for (int SoloLoss = 0; SoloLoss < 2; ++SoloLoss)
                for (int P1Device = 0; P1Device < 2; ++P1Device)
                    assert(Run(RemotePresent, LocalP2Device, SoloLoss != 0, P1Device != 0) ==
                        (!P1Device || (!RemotePresent && !LocalP2Device) || SoloLoss));
    VirtualFree(Memory, 0, MEM_RELEASE);
    puts("Remote P2: sparse-map/notification validation and actual device guard passed (16 branch combinations)");
}
