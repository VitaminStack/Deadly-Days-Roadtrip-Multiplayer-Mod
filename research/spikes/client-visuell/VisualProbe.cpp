// Kontrollierte Probe in der eigenen Sitzung, Build 26187885. Kein Produktions-Sync.
// 1: Nur beim Host Bewegungsreplikation existierender Projektile einschalten.
// Keine neuen Actors, kein Schaden und keine XP-Vergabe. Kein Hot-Unload.
#include "../a-connect/Spike.h"

namespace
{
using namespace Spike;
WNDPROC Previous;
constexpr UINT Message = WM_APP + 194;

bool IsA(uintptr_t Object, const char* Name)
{
    if (!Object)
        return false;
    uintptr_t Class = Read<uintptr_t>(Object + 0x10);
    for (int Depth = 0; Class && Depth < 64; ++Depth, Class = Read<uintptr_t>(Class + 0x40))
        if (NameOf(Class) == Name)
            return true;
    return false;
}

bool Invoke(uintptr_t Object, const char* Owner, const char* Name, void* Params)
{
    const uintptr_t Function = FindObject(Name, Owner);
    if (!Object || !Function)
        return false;
    Read<ProcessEventFn*>(Object)[kProcessEventIndex](Object, Function, Params);
    return true;
}

void Perform(int Action)
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t State = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    if (Action == 2)
    {
        if (!State || !IsA(State, "DDMissionGameState"))
            return;
        const uintptr_t Level = Read<uintptr_t>(State + 0x3B8);
        int32_t Number = 0;
        float Fraction = 0;
        if (!Invoke(Level, "LevelSystemComponent", "GetCurrentLevel", &Number) ||
            !Invoke(Level, "LevelSystemComponent", "GetExperiencePercentage", &Fraction))
            return;
        Log("shared-progress level=" + std::to_string(Number) + " fraction=" + std::to_string(Fraction));
        const uintptr_t Players = Read<uintptr_t>(State + kGameStatePlayerArray);
        const int32_t Count = Read<int32_t>(State + kGameStatePlayerArray + 8);
        for (int32_t i = 0; Players && i < Count && i < 2; ++i)
        {
            const uintptr_t PS = Read<uintptr_t>(Players + 8 * i);
            uintptr_t PlayerLevel = 0;
            if (Invoke(PS, "DDPlayerState", "GetLevelSystemComponent", &PlayerLevel))
                Log("player=" + std::to_string(i) + " shares-level-system=" + std::to_string(PlayerLevel == Level));
        }
        return;
    }
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    if (!Driver || Read<uintptr_t>(Driver + kDriverServerConnection))
    {
        Log("abgelehnt: nur Listen-Host");
        return;
    }
    if (Action == 3 || Action == 4)
    {
        if (!State || !IsA(State, "DDMissionGameState"))
            return;
        const uintptr_t Players = Read<uintptr_t>(State + kGameStatePlayerArray);
        const int32_t Count = Read<int32_t>(State + kGameStatePlayerArray + 8);
        const int32_t Index = Action - 3;
        if (!Players || Count <= Index || Count > 2)
            return;
        const uintptr_t PS = Read<uintptr_t>(Players + 8 * Index);
        uintptr_t Pawn = 0;
        struct Vector { double X, Y, Z; } Position{};
        if (!Invoke(PS, "PlayerState", "GetPawn", &Pawn) || !IsA(Pawn, "DDCharacter") ||
            !Invoke(Pawn, "Actor", "K2_GetActorLocation", &Position))
            return;
        struct Params { int32_t Amount; uint32_t Padding; Vector Position; } Request{200, 0, Position};
        static_assert(sizeof(Request) == 32);
        const bool Called = Invoke(FindInstance("ExpSubsystem"), "ExpSubsystem", "RequestExpSpawning", &Request);
        Log("controlled-xp-source player=" + std::to_string(Index) + " amount=200 called=" + std::to_string(Called));
        return;
    }
    if (Action == 5)
    {
        // Ein gezielter Testactor, keine Default-Objekte aendern und keine Reward-Oeffnung aufrufen.
        const uintptr_t Class = FindObject("BP_Airdrop_Lootable_C", nullptr);
        const uintptr_t Library = FindObject("Default__GameplayStatics", nullptr);
        const uintptr_t Players = State ? Read<uintptr_t>(State + kGameStatePlayerArray) : 0;
        const int32_t Count = State ? Read<int32_t>(State + kGameStatePlayerArray + 8) : 0;
        if (!Class || !Library || !Players || Count < 1 || Count > 2)
            return;
        uintptr_t Pawn = 0;
        struct Vector { double X, Y, Z; } Position{};
        if (!Invoke(Read<uintptr_t>(Players), "PlayerState", "GetPawn", &Pawn) || !IsA(Pawn, "DDCharacter") ||
            !Invoke(Pawn, "Actor", "K2_GetActorLocation", &Position))
            return;
        struct alignas(16) Transform
        {
            double Rotation[4]{0, 0, 0, 1};
            double Translation[4]{};
            double Scale[4]{1, 1, 1, 0};
        } Pose;
        Pose.Translation[0] = Position.X + 300;
        Pose.Translation[1] = Position.Y;
        Pose.Translation[2] = Position.Z + 50;
        struct alignas(16) SpawnParams
        {
            uintptr_t World, Class;
            Transform Pose;
            uint64_t Collision = 1;
            uintptr_t Owner = 0;
            uint64_t Scale = 0;
            uintptr_t Result = 0;
        } Spawn{World, Class, Pose};
        static_assert(sizeof(SpawnParams) == 0x90);
        if (!Invoke(Library, "GameplayStatics", "BeginDeferredActorSpawnFromClass", &Spawn) || !Spawn.Result)
            return;
        bool Replicate = true;
        Invoke(Spawn.Result, "Actor", "SetReplicates", &Replicate);
        Invoke(Spawn.Result, "Actor", "SetReplicateMovement", &Replicate);
        struct alignas(16) FinishParams
        {
            uintptr_t Actor, Padding = 0;
            Transform Pose;
            uint64_t Scale = 0;
            uintptr_t Result = 0;
        } Finish{Spawn.Result, 0, Pose};
        static_assert(sizeof(FinishParams) == 0x80);
        Invoke(Library, "GameplayStatics", "FinishSpawningActor", &Finish);
        Log("controlled-replicated-airdrop success=" + std::to_string(Finish.Result != 0));
        return;
    }
    if (Action == 6)
    {
        const uintptr_t Level = World ? Read<uintptr_t>(World + 0x30) : 0;
        const uintptr_t Players = State ? Read<uintptr_t>(State + kGameStatePlayerArray) : 0;
        uintptr_t Pawn = 0;
        if (!Level || !Players || !Invoke(Read<uintptr_t>(Players), "PlayerState", "GetPawn", &Pawn) || !IsA(Pawn, "DDCharacter"))
            return;
        const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
        const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
        for (int32_t Index = 0; Index < Count; ++Index)
        {
            const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (Index >> 16));
            const uintptr_t Actor = Chunk ? Read<uintptr_t>(Chunk + (Index & 0xffff) * 0x18 + 8) : 0;
            if (!Actor || Read<uint32_t>(Actor + 8) & 0x30 || Read<uintptr_t>(Actor + 0x20) != Level ||
                !IsA(Actor, "BP_Airdrop_Lootable_C") || Read<uint8_t>(Actor + 0x5D) & 2 || Read<uint8_t>(Actor + 0x2A8))
                continue;
            const bool Called = Invoke(Actor, "Crate", "OpenCrate", &Pawn);
            Log("controlled-host-open called=" + std::to_string(Called) + " opened=" + std::to_string(Read<uint8_t>(Actor + 0x2A8)));
            return;
        }
        Log("controlled-host-open: kein ungeoeffneter Airdrop im aktuellen Level");
        return;
    }
    if (Action != 1)
        return;
    const uintptr_t Function = FindObject("SetReplicateMovement", "Actor");
    if (!Function)
    {
        Log("SetReplicateMovement fehlt");
        return;
    }
    const uintptr_t Level = Read<uintptr_t>(World + 0x30);
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    int Changed = 0;
    for (int32_t Index = 0; Index < Count; ++Index)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (Index >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (Index & 0xffff) * 0x18 + 8) : 0;
        if (!Object || Read<uint32_t>(Object + 8) & 0x30 || Read<uintptr_t>(Object + 0x20) != Level || !IsA(Object, "Projectile"))
            continue;
        if (Read<uint8_t>(Object + 0x58) & 0x10)
            continue;
        bool Replicate = true;
        const auto ProcessEvent = Read<ProcessEventFn*>(Object)[kProcessEventIndex];
        ProcessEvent(Object, Function, &Replicate);
        Log("movement-enabled=" + NameOf(Object));
        ++Changed;
    }
    Log("changed=" + std::to_string(Changed));
}

LRESULT CALLBACK WindowProc(HWND Window, UINT Msg, WPARAM W, LPARAM L)
{
    if (Msg == Message)
    {
        if (W >= 1 && W <= 6)
            Perform(static_cast<int>(W));
        return 0;
    }
    return CallWindowProcW(Previous, Window, Msg, W, L);
}

BOOL CALLBACK FindWindow(HWND Window, LPARAM Result)
{
    DWORD Process = 0;
    GetWindowThreadProcessId(Window, &Process);
    wchar_t Class[64]{};
    GetClassNameW(Window, Class, 64);
    if (Process == GetCurrentProcessId() && wcscmp(Class, L"UnrealWindow") == 0)
    {
        *reinterpret_cast<HWND*>(Result) = Window;
        return FALSE;
    }
    return TRUE;
}

DWORD WINAPI Worker(void*)
{
    Base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    const auto Root = std::wstring(Temp) + L"ddvisual\\";
    CreateDirectoryW(Root.c_str(), nullptr);
    const auto Id = std::to_wstring(GetCurrentProcessId());
    SetLogPath(Root + Id + L".log");
    const auto Command = Root + Id + L".cmd";
    HWND Window = nullptr;
    EnumWindows(FindWindow, reinterpret_cast<LPARAM>(&Window));
    if (!Window)
        return 1;
    Previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProc)));
    if (!Previous)
        return 2;
    Log("bereit; 1=Bewegungsreplikation, 2=XP lesen, 3/4=Host-XP-Quelle am Spieler 0/1, 5=Test-Airdrop, 6=Host-Airdrop oeffnen");
    for (;;)
    {
        Sleep(200);
        FILE* File = nullptr;
        if (_wfopen_s(&File, Command.c_str(), L"r") != 0 || !File)
            continue;
        int Action = -1;
        const int Parsed = fscanf_s(File, "%d", &Action);
        fclose(File);
        DeleteFileW(Command.c_str());
        if (Parsed == 1 && Action >= 1 && Action <= 6)
            SendMessageW(Window, Message, static_cast<WPARAM>(Action), 0);
    }
}
}

BOOL WINAPI DllMain(HINSTANCE Module, DWORD Reason, void*)
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(Module);
        const HANDLE Thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (Thread)
            CloseHandle(Thread);
    }
    return TRUE;
}
