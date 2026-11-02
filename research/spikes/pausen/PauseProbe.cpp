// Eigener Testcode, kein generierter SDK-Code. Nur in geschuetzter Sitzung laden.
// Methodentests ersetzen keinen natuerlichen Level-up-/Inventar-Ablauf.
#include <windows.h>
#include <cstdio>
#include <string>
#include "SDK/DDSurvivors_classes.hpp"

namespace
{
constexpr UINT Message = WM_APP + 175;
WNDPROC Original;
std::wstring LogPath;

void Log(const char* Text)
{
    FILE* File = nullptr;
    if (_wfopen_s(&File, LogPath.c_str(), L"a") == 0 && File)
    {
        fprintf(File, "%llu %s\n", GetTickCount64(), Text);
        fclose(File);
    }
}

template <typename T> T* Instance()
{
    for (int i = 0; i < SDK::UObject::GObjects->Num(); ++i)
    {
        auto* Object = SDK::UObject::GObjects->GetByIndex(i);
        if (Object && !(static_cast<unsigned>(Object->Flags) & 0x30) && Object->IsA(T::StaticClass()))
            return static_cast<T*>(Object);
    }
    return nullptr;
}

void Perform(int Action)
{
    auto* World = SDK::UWorld::GetWorld();
    if (!World || !World->GameState || !World->GameState->IsA(SDK::ADDMissionGameState::StaticClass()))
    {
        Log("kein passender MissionGameState");
        return;
    }
    auto* State = static_cast<SDK::ADDMissionGameState*>(World->GameState);
    if (Action == 1)
    {
        if (auto* Level = State->GetLevelSystem())
        {
            Level->RequestLevelUp();
            Log("RequestLevelUp abgeschlossen");
        }
        else
            Log("kein LevelSystem");
    }
    else if (Action == 2 || Action == 3)
    {
        auto* Panel = Instance<SDK::UInventoryPanel>();
        auto* Animation = Panel ? Panel->GetAnimationContainer() : nullptr;
        if (!Animation)
        {
            Log("kein Inventar-Panel/Animationscontainer");
            return;
        }
        if (Action == 2)
            Animation->OpenPanel();
        else
            Animation->ClosePanel();
        Log(Action == 2 ? "Inventar OpenPanel abgeschlossen" : "Inventar ClosePanel abgeschlossen");
    }
}

LRESULT CALLBACK WindowProc(HWND Window, UINT Msg, WPARAM W, LPARAM L)
{
    if (Msg == Message)
    {
        Perform(static_cast<int>(W));
        return 0;
    }
    return CallWindowProcW(Original, Window, Msg, W, L);
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
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    const auto Root = std::wstring(Temp) + L"ddpause\\";
    CreateDirectoryW(Root.c_str(), nullptr);
    const auto Id = std::to_wstring(GetCurrentProcessId());
    const auto Command = Root + Id + L".cmd";
    LogPath = Root + Id + L".log";
    HWND Window = nullptr;
    EnumWindows(FindWindow, reinterpret_cast<LPARAM>(&Window));
    if (!Window)
        return 1;
    Original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(Window, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&WindowProc)));
    if (!Original)
        return 2;
    Log("bereit; 1=levelup, 2=inventar-auf, 3=inventar-zu; kein Mapwechsel im Test");
    for (;;)
    {
        Sleep(200);
        FILE* File = nullptr;
        if (_wfopen_s(&File, Command.c_str(), L"r") != 0 || !File)
            continue;
        int Action = 0;
        const int Read = fscanf_s(File, "%d", &Action);
        fclose(File);
        DeleteFileW(Command.c_str());
        if (Read == 1 && Action >= 1 && Action <= 3)
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
