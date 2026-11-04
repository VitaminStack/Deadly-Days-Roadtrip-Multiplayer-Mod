#include "Game/CodeCave.h"

#include <windows.h>

#include <cstring>
#include <limits>
#include <vector>

#include "Core/Log.h"
#include "Game/Memory.h"

namespace Game::Code
{
std::optional<int32_t> Rel32(uintptr_t Next, uintptr_t Target)
{
    const auto Delta = static_cast<int64_t>(Target - Next);
    if (Delta < std::numeric_limits<int32_t>::min() || Delta > std::numeric_limits<int32_t>::max())
        return std::nullopt;
    return static_cast<int32_t>(Delta);
}

Writer::Writer(uint8_t* Buffer, size_t Size) : Out(Buffer), End(Buffer ? Buffer + Size : nullptr), Broken(!Buffer)
{
}

bool Writer::AllocateNear(uintptr_t Anchor, size_t Size)
{
    // Unterhalb der Exe liegt beim Spiel freier Adressraum, die Grenze hält jeden Sprung in rel32-Weite.
    constexpr uintptr_t Granularity = 0x10000;
    constexpr uintptr_t MinDistance = 0x10000000;
    constexpr uintptr_t MaxDistance = 0x70000000;
    if (Anchor < MaxDistance)
        return false;
    for (uintptr_t Try = (Anchor - MinDistance) & ~(Granularity - 1); Try > Anchor - MaxDistance; Try -= Granularity)
    {
        void* Page = VirtualAlloc(reinterpret_cast<void*>(Try), Size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!Page)
            continue;
        Out = static_cast<uint8_t*>(Page);
        End = Out + Size;
        Broken = false;
        return true;
    }
    return false;
}

bool Writer::Reserve(size_t Size)
{
    if (Broken || !Out || static_cast<size_t>(End - Out) < Size)
        Broken = true;
    return !Broken;
}

void Writer::Emit(std::initializer_list<uint8_t> Bytes)
{
    if (!Reserve(Bytes.size()))
        return;
    for (const uint8_t Byte : Bytes)
        *Out++ = Byte;
}

void Writer::EmitRel(uintptr_t Target)
{
    if (!Reserve(4))
        return;
    const auto Value = Rel32(reinterpret_cast<uintptr_t>(Out) + 4, Target);
    if (!Value)
    {
        Broken = true;
        return;
    }
    std::memcpy(Out, &*Value, 4);
    Out += 4;
}

uint8_t* Writer::EmitAbsoluteJump(uintptr_t Target)
{
    if (!Reserve(12))
        return nullptr;
    uint8_t* Start = Out;
    *Out++ = 0x48; // mov rax,imm64
    *Out++ = 0xB8;
    std::memcpy(Out, &Target, 8);
    Out += 8;
    *Out++ = 0xFF; // jmp rax
    *Out++ = 0xE0;
    return Start;
}

bool Expect(uintptr_t Site, std::initializer_list<uint8_t> Bytes, std::string_view Where)
{
    std::vector<uint8_t> Actual(Bytes.size());
    if (Memory::ReadBytes(Site, Actual.data(), Actual.size()) && std::memcmp(Actual.data(), Bytes.begin(), Bytes.size()) == 0)
        return true;
    Core::Log::Error("Patches: unerwartete Bytes {}", Where);
    return false;
}

bool ExpectVirtual(uintptr_t Entry, uintptr_t Expected, std::string_view Where)
{
    if (Memory::Read<uintptr_t>(Entry) == Expected)
        return true;
    Core::Log::Error("Patches: unerwartete virtuelle Funktion {}", Where);
    return false;
}

bool WriteCode(uintptr_t Site, const void* Bytes, size_t Size)
{
    DWORD Old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(Site), Size, PAGE_EXECUTE_READWRITE, &Old))
        return false;
    std::memcpy(reinterpret_cast<void*>(Site), Bytes, Size);
    DWORD Ignored = 0;
    VirtualProtect(reinterpret_cast<void*>(Site), Size, Old, &Ignored);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(Site), Size);
    return true;
}

bool WriteJump(uintptr_t From, const void* To)
{
    const auto Value = Rel32(From + 5, reinterpret_cast<uintptr_t>(To));
    if (!To || !Value)
        return false;
    uint8_t Jump[5] = {0xE9};
    std::memcpy(Jump + 1, &*Value, 4);
    return WriteCode(From, Jump, sizeof(Jump));
}

bool WriteVirtual(uintptr_t Entry, uintptr_t Replacement, uintptr_t* Old)
{
    DWORD Protection = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(Entry), sizeof(uintptr_t), PAGE_READWRITE, &Protection))
        return false;
    if (Old)
        *Old = *reinterpret_cast<uintptr_t*>(Entry);
    *reinterpret_cast<uintptr_t*>(Entry) = Replacement;
    DWORD Ignored = 0;
    VirtualProtect(reinterpret_cast<void*>(Entry), sizeof(uintptr_t), Protection, &Ignored);
    return true;
}
}
