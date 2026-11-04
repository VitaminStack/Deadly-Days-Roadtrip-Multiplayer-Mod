#include "../research/spikes/a-connect/BlockadeAuthority.h"
#include "../research/spikes/a-connect/NetGuidPolicy.h"
#include "../research/spikes/a-connect/NetGuidExpiryGuard.h"
#include "../research/spikes/a-connect/TestGodmodePolicy.h"
#include "../research/spikes/a-connect/SneakTargetReadiness.h"
#include <windows.h>
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <iostream>

class FixtureCode
{
public:
    uint8_t* Out;
    void Emit(std::initializer_list<uint8_t> Bytes) { for (uint8_t Byte : Bytes) *Out++ = Byte; }
    void EmitRel(uintptr_t Target)
    {
        const auto Displacement = static_cast<int32_t>(Target - reinterpret_cast<uintptr_t>(Out + 4));
        std::memcpy(Out, &Displacement, sizeof(Displacement));
        Out += sizeof(Displacement);
    }
};

void TestNativeExpiryGuard()
{
    // Synthetic memory only: this never loads/injects the game or executes copied game instructions.
    auto* Memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(Memory);
    FixtureCode Code{Memory};
    const auto Original = reinterpret_cast<uintptr_t>(Code.Out);
    Code.Emit({0xB8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x70, 0x5F, 0xC3});
    const auto Next = reinterpret_cast<uintptr_t>(Code.Out);
    Code.Emit({0xB8, 0x01, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x70, 0x5F, 0xC3});
    using GuardFn = int(__fastcall*)(const void*, const void*);
    const auto Guard = reinterpret_cast<GuardFn>(Code.Out);
    Code.Emit({0x57, 0x48, 0x83, 0xEC, 0x70, 0x48, 0x89, 0xCF, 0x48, 0x89, 0x54, 0x24, 0x68});
    alignas(8) uint8_t Driver[0x200]{}, Cache[0x200]{}, Entry[0x40]{};
    *reinterpret_cast<uintptr_t*>(Cache + 0x150) = reinterpret_cast<uintptr_t>(Driver);
    *reinterpret_cast<uintptr_t*>(Driver + 0x100) = 1;
    *reinterpret_cast<uintptr_t*>(Driver + 0x1C8) = reinterpret_cast<uintptr_t>(Cache);
    NetGuidPolicy::EmitExpiryGuard(Code, Original, Next);
    FlushInstructionCache(GetCurrentProcess(), Memory, static_cast<SIZE_T>(Code.Out - Memory));
    for (uint64_t Guid = 0; Guid < 1024; ++Guid)
        for (unsigned Flags = 0; Flags < 256; ++Flags)
        {
            *reinterpret_cast<uint64_t*>(Entry) = Guid;
            *reinterpret_cast<uint32_t*>(Entry + 0x18) = 42;
            Entry[0x30] = static_cast<uint8_t>(Flags);
            const auto Before = *reinterpret_cast<uint64_t*>(Entry + 0x28) = 0x4123456789ABCDEF;
            const bool Expected = NetGuidPolicy::RetainPath(true, Guid, 42, static_cast<uint8_t>(Flags));
            assert(Guard(Entry + 8, Cache + 8) == static_cast<int>(Expected));
            assert(*reinterpret_cast<uint64_t*>(Entry + 0x28) == Before); // Timestamp is read-only.
            assert(Guard(Entry + 8, Cache + 16) == 0); // Other/old cache must remain untouched.
        }
    *reinterpret_cast<uint64_t*>(Entry) = 3;
    Entry[0x30] = 0x10;
    *reinterpret_cast<uint32_t*>(Entry + 0x18) = 0;
    assert(Guard(Entry + 8, Cache + 8) == 0);
    *reinterpret_cast<uint32_t*>(Entry + 0x18) = 42;
    *reinterpret_cast<uintptr_t*>(Driver + 0x100) = 0;
    assert(Guard(Entry + 8, Cache + 8) == 0); // Host.
    *reinterpret_cast<uintptr_t*>(Driver + 0x100) = 1;
    *reinterpret_cast<uintptr_t*>(Driver + 0x1C8) = reinterpret_cast<uintptr_t>(Cache + 16);
    assert(Guard(Entry + 8, Cache + 8) == 0); // Driver now belongs to another cache.
    *reinterpret_cast<uintptr_t*>(Driver + 0x1C8) = 0;
    assert(Guard(Entry + 8, Cache + 8) == 0); // Cache disconnected.
    *reinterpret_cast<uintptr_t*>(Driver + 0x1C8) = reinterpret_cast<uintptr_t>(Cache);
    assert(Guard(Entry + 8, Cache + 8) == 1); // No dependency on a live GWorld during travel GC.
    *reinterpret_cast<uintptr_t*>(Cache + 0x150) = 0;
    assert(Guard(Entry + 8, Cache + 8) == 0); // No driver.
    assert(Guard(Entry + 8, nullptr) == 0); // No lookup.
    VirtualFree(Memory, 0, MEM_RELEASE);
}

void TestSneakReadiness()
{
    auto* Memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(Memory);
    FixtureCode Code{Memory};
    const auto Retry = reinterpret_cast<uintptr_t>(Code.Out);
    Code.Emit({0xB8, 0x00, 0x00, 0x00, 0x00, 0x5F, 0xC3});
    const auto Ready = reinterpret_cast<uintptr_t>(Code.Out);
    Code.Emit({0xB8, 0x01, 0x00, 0x00, 0x00, 0x5F, 0xC3});
    using GuardFn = int(__fastcall*)(const void*, int);
    const auto Guard = reinterpret_cast<GuardFn>(Code.Out);
    Code.Emit({0x57, 0x48, 0x89, 0xCF, 0x83, 0xFA, 0x01}); // rdi=component, ZF=SameTarget
    SneakTargetReadiness::Emit(Code, Retry, Ready);
    FlushInstructionCache(GetCurrentProcess(), Memory, static_cast<SIZE_T>(Code.Out - Memory));
    alignas(8) uint8_t Component[0x138]{};
    for (int Same = 0; Same < 2; ++Same)
        for (int Mesh = 0; Mesh < 2; ++Mesh)
            for (int State = 0; State < 2; ++State)
            {
                *reinterpret_cast<uintptr_t*>(Component + 0xE8) = Mesh;
                *reinterpret_cast<uintptr_t*>(Component + 0xD8) = State;
                assert(Guard(Component, Same) == static_cast<int>(SneakTargetReadiness::Reuse(Same != 0, Mesh != 0, State != 0)));
            }
    VirtualFree(Memory, 0, MEM_RELEASE);
}

int main()
{
    static_assert(TestGodmodePolicy::Delta(true, false, false) == 1);
    static_assert(TestGodmodePolicy::Delta(true, false, true) == 1); // Own contribution beside gameplay effect.
    static_assert(TestGodmodePolicy::Delta(true, true, true) == 0); // Repeated ticks never oscillate/accumulate.
    static_assert(TestGodmodePolicy::Delta(true, true, false) == 1); // Recover if game cleared the tag.
    static_assert(TestGodmodePolicy::Delta(false, true, true) == -1);
    static_assert(TestGodmodePolicy::Delta(false, true, false) == 0);
    static_assert(TestGodmodePolicy::Delta(false, false, true) == 0); // Never remove a gameplay contribution.
    static_assert(TestGodmodePolicy::Delta(false, false, false) == 0);
    // Full input matrix; not a substitute for a real route/endless acceptance test.
    for (int World = 0; World < 2; ++World)
        for (int Driver = 0; Driver < 2; ++Driver)
            for (int Connection = 0; Connection < 2; ++Connection)
                assert(BlockadeAuthority::MayStart(World != 0, Driver != 0, Connection != 0) ==
                    (World != 0 && (Driver == 0 || Connection == 0)));
    static_assert(BlockadeAuthority::MayStart(true, true, false)); // Listen/dedicated host.
    static_assert(BlockadeAuthority::MayStart(true, false, false)); // Standalone.
    static_assert(!BlockadeAuthority::MayStart(true, true, true)); // Network client.
    static_assert(!BlockadeAuthority::MayStart(false, false, false)); // Teardown.
    for (uint64_t Guid = 0; Guid < 1024; ++Guid)
        for (unsigned Flags = 0; Flags < 256; ++Flags)
        {
            const bool Expected = Guid > 1 && (Guid % 2 == 1) && !(Flags & 0x09);
            assert(NetGuidPolicy::RetainPath(true, Guid, 42, static_cast<uint8_t>(Flags)) == Expected);
            assert(!NetGuidPolicy::RetainPath(false, Guid, 42, static_cast<uint8_t>(Flags)));
            assert(!NetGuidPolicy::RetainPath(true, Guid, 0, static_cast<uint8_t>(Flags)));
        }
    TestNativeExpiryGuard();
    TestSneakReadiness();
    std::cout << "Blockade authority, Godmode policy and native NetGUID/Sneak readiness tests passed\n";
}
