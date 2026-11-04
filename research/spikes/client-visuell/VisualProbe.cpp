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
    uintptr_t Class = Read<uintptr_t>(Object + 0x10);
    for (int Depth = 0; Class && Depth < 64; ++Depth, Class = Read<uintptr_t>(Class + 0x40))
        if (NameOf(Class) == Name)
            return true;
    return false;
}

void Perform()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    if (!Driver || Read<uintptr_t>(Driver + kDriverServerConnection))
    {
        Log("abgelehnt: nur Listen-Host");
        return;
    }
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
        if (W == 1)
            Perform();
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
    Log("bereit; 1=Bewegungsreplikation existierender Host-Projektile");
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
        if (Parsed == 1 && Action == 1)
            SendMessageW(Window, Message, 1, 0);
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
