// Log, Objektsuche und Werkzeuge fuer Code-Patches.
#include "Spike.h"

namespace Spike
{
uintptr_t Base;

namespace
{
std::wstring LogPath;

struct FString
{
    const wchar_t* Data;
    int32_t Num;
    int32_t Max;
};

struct ExecuteConsoleCommandParams
{
    void* WorldContext;
    FString Command;
    void* SpecificPlayer;
};
}

void SetLogPath(const std::wstring& Path) { LogPath = Path; }

void Log(const std::string& Line)
{
    FILE* File = nullptr;
    if (_wfopen_s(&File, LogPath.c_str(), L"a") == 0 && File)
    {
        fprintf(File, "%s\n", Line.c_str());
        fclose(File);
    }
}

std::string NameFromIndex(int32_t Index)
{
    const uintptr_t Block = Read<uintptr_t>(Base + kNameBlocks + 8 * (Index >> 16));
    const uintptr_t Entry = Block + (Index & 0xFFFF) * 2;
    const uint16_t Header = Read<uint16_t>(Entry);
    if (Header & 1)
        return {};
    return std::string(reinterpret_cast<const char*>(Entry + 2), Header >> 6);
}

std::string NameOf(uintptr_t Object) { return NameFromIndex(Read<int32_t>(Object + 0x18)); }

uintptr_t FindObject(const char* Name, const char* OuterName)
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        if (!Chunk)
            continue;
        const uintptr_t Object = Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8);
        if (!Object || NameOf(Object) != Name)
            continue;
        const uintptr_t Outer = Read<uintptr_t>(Object + 0x20);
        if (!OuterName || (Outer && NameOf(Outer) == OuterName))
            return Object;
    }
    return 0;
}

uintptr_t FindInstance(const char* ClassName)
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        if (!Chunk)
            continue;
        const uintptr_t Object = Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8);
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        const uintptr_t Class = Read<uintptr_t>(Object + 0x10);
        if (Class && NameOf(Class) == ClassName)
            return Object;
    }
    return 0;
}

void RunConsoleCommand(const std::wstring& Command)
{
    const uintptr_t Function = FindObject("ExecuteConsoleCommand", "KismetSystemLibrary");
    const uintptr_t Library = FindObject("Default__KismetSystemLibrary", nullptr);
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    if (!Function || !Library || !World)
    {
        Log("Objekte nicht gefunden");
        return;
    }

    ExecuteConsoleCommandParams Params{};
    Params.WorldContext = reinterpret_cast<void*>(World);
    Params.Command = {Command.c_str(), static_cast<int32_t>(Command.size() + 1), static_cast<int32_t>(Command.size() + 1)};

    const auto ProcessEvent = Read<ProcessEventFn*>(Library)[kProcessEventIndex];
    ProcessEvent(Library, Function, &Params);
    Log("ausgefuehrt");
}

bool CodeWriter::Allocate()
{
    uint8_t* Cave = nullptr;
    for (uintptr_t Try = (Base - 0x10000000) & ~uintptr_t(0xFFFF); !Cave && Try > Base - 0x70000000; Try -= 0x10000)
        Cave = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(Try), 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    Out = Cave;
    return Cave != nullptr;
}

void CodeWriter::Emit(std::initializer_list<uint8_t> Bytes)
{
    for (uint8_t Byte : Bytes)
        *Out++ = Byte;
}

void CodeWriter::EmitRel(uintptr_t Target)
{
    const int32_t Value = static_cast<int32_t>(Target - (reinterpret_cast<uintptr_t>(Out) + 4));
    memcpy(Out, &Value, 4);
    Out += 4;
}

uint8_t* CodeWriter::EmitAbsoluteJump(uintptr_t Target)
{
    uint8_t* Start = Out;
    Emit({0x48, 0xB8});                                     // mov rax,Target
    memcpy(Out, &Target, 8);
    Out += 8;
    Emit({0xFF, 0xE0});                                     // jmp rax
    return Start;
}

bool Expect(uintptr_t Site, std::initializer_list<uint8_t> Bytes, const char* Where)
{
    if (memcmp(reinterpret_cast<void*>(Site), Bytes.begin(), Bytes.size()) == 0)
        return true;
    Log(std::string("Patch: unerwartete Bytes ") + Where);
    return false;
}

void WriteJump(uintptr_t From, uint8_t* To)
{
    DWORD Old = 0;
    VirtualProtect(reinterpret_cast<void*>(From), 5, PAGE_EXECUTE_READWRITE, &Old);
    *reinterpret_cast<uint8_t*>(From) = 0xE9;
    const int32_t Jump = static_cast<int32_t>(reinterpret_cast<uintptr_t>(To) - (From + 5));
    memcpy(reinterpret_cast<void*>(From + 1), &Jump, 4);
    VirtualProtect(reinterpret_cast<void*>(From), 5, Old, &Old);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(From), 5);
}
}
