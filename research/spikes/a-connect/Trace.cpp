// Protokolliert RPCs der Spielfigur und die ersten Aufrufe der Run-Funktionen.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kFunctionFlags = 0xB0;
constexpr uintptr_t kFunctionExec = 0xD8;

ProcessEventFn OriginalProcessEvent;
std::map<std::string, int> RpcCounts;

// RPCs laufen auf beiden Seiten durch ProcessEvent. Zaehlt alles, was nach Client- oder Server-RPC aussieht.
void __fastcall TraceProcessEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    const std::string Name = NameOf(Function);
    if (Name.find("Server") != std::string::npos || Name.find("Client") != std::string::npos)
    {
        const int Count = ++RpcCounts[Name];
        if (Count == 1 || Count % 100 == 0)
            Log("RPC " + Name + " #" + std::to_string(Count));
    }
    OriginalProcessEvent(Object, Function, Params);
}

using ExecFn = void(__fastcall*)(void*, void*, void*);

struct TracedFunction
{
    ExecFn Original;
    std::string Name;
    int Calls;
};

std::vector<TracedFunction> TracedFunctions;

void __fastcall TraceExec(void* Context, void* Stack, void* Result, size_t Index)
{
    TracedFunction& Function = TracedFunctions[Index];
    if (++Function.Calls <= 5)
        Log(std::to_string(GetTickCount64()) + " " + Function.Name);
    Function.Original(Context, Stack, Result);
}
}

// Tauscht den ProcessEvent-Eintrag in der VTable der Spielfigur.
void TraceCharacterRpcs()
{
    const uintptr_t Character = FindInstance("BP_DD_Character_C");
    if (!Character || OriginalProcessEvent)
        return;
    auto* Slot = &Read<ProcessEventFn*>(Character)[kProcessEventIndex];
    DWORD Old = 0;
    VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &Old);
    OriginalProcessEvent = *Slot;
    *Slot = TraceProcessEvent;
    VirtualProtect(Slot, sizeof(*Slot), Old, &Old);
    Log("RPC-Zaehler: aktiv");
}

// Blueprint-Aufrufe nativer Funktionen laufen ueber UFunction::Func. Pro Funktion leitet ein Stub durch TraceExec.
void TraceRunFlow()
{
    const char* const Owners[] = {
        "DDMissionGameInstance", "LoadingSubSystem", "RunPersistencyManager", "DDMissionGameMode", "DDMissionGameState",
        "DDBlueprintHelperFunctions", "Bus", "BusPanel", "ExtractionPointBase", "MissionGenerationSubsystem",
        "CharacterSelectionManager", "CharacterSelectPanel", "EndOfDayUpgradeManager", "RoundEndPanel", "MapGenerator",
        "LocalCoopSubsystem", "PauseMenuWidget"};
    if (!TracedFunctions.empty())
        return;
    TracedFunctions.reserve(4096);

    auto* Stub = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096 * 24, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count && TracedFunctions.size() < 4096; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        const uintptr_t Class = Object ? Read<uintptr_t>(Object + 0x10) : 0;
        if (!Class || NameOf(Class) != "Function" || !(Read<uint32_t>(Object + kFunctionFlags) & 0x400))
            continue;
        const uintptr_t Outer = Read<uintptr_t>(Object + 0x20);
        const std::string OuterName = Outer ? NameOf(Outer) : std::string();
        bool Wanted = false;
        for (const char* Owner : Owners)
            Wanted = Wanted || OuterName == Owner;
        if (!Wanted)
            continue;

        auto& Func = *reinterpret_cast<ExecFn*>(Object + kFunctionExec);
        char Address[32]{};
        sprintf_s(Address, "%llX", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(Func) - Base));
        if (NameOf(Object).find("Load") != std::string::npos || NameOf(Object).find("Travel") != std::string::npos)
            Log("Exec " + NameOf(Object) + " " + Address);
        const uint64_t Index = TracedFunctions.size();
        const uint64_t Hook = reinterpret_cast<uint64_t>(&TraceExec);
        TracedFunctions.push_back({Func, OuterName + "." + NameOf(Object), 0});
        Stub[0] = 0x49; Stub[1] = 0xB9;                     // mov r9,Index
        memcpy(Stub + 2, &Index, 8);
        Stub[10] = 0x48; Stub[11] = 0xB8;                   // mov rax,TraceExec
        memcpy(Stub + 12, &Hook, 8);
        Stub[20] = 0xFF; Stub[21] = 0xE0;                   // jmp rax
        Func = reinterpret_cast<ExecFn>(Stub);
        Stub += 24;
    }
    Log("Ablauf-Trace: " + std::to_string(TracedFunctions.size()) + " Funktionen");
}
}
