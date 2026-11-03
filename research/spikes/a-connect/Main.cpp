// Wegwerf-Spike: Fuehrt Konsolenbefehle im Spiel aus (Shipping-Build hat keine Konsole).
// Befehle kommen als Datei %TEMP%\ddspike\<pid>.cmd und laufen auf dem Game Thread.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr UINT kRunCommand = WM_APP + 77;
constexpr UINT_PTR kSessionTimer = 7701;

WNDPROC OriginalWndProc;
std::wstring Pending;
HWND GameWindow = nullptr;

void InstallPatches()
{
    CodeWriter Code;
    if (!Code.Allocate())
    {
        Log("Patch: kein Speicher");
        return;
    }
    if (PatchPlayers(Code) && PatchClient(Code) && PatchCamera(Code) && PatchInventory(Code) && PatchTravel(Code) && PatchWidgets(Code))
        Log("Patch: aktiv");
    if (PatchMapGen(Code))
        Log("Patch: Karte vom Host aktiv");
}

// Laeuft jede Sekunde auf dem Game Thread und ersetzt die Befehle !cam und !authmove nach jedem Kartenwechsel.
void TickSession()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    if (!Driver)
        return;
    if (Read<uintptr_t>(Driver + kDriverServerConnection))
    {
        ReleaseDeferredWidgetEvents(World);
        TickMapGen();
        ViewThroughSharedCamera();
        return;
    }
    AcceptClientPositions();
    RepairSpawnedPlayers(World);
}

void RunPendingCommand()
{
    if (Pending == L"!arm")
        ArmTravelRedirect();
    else if (Pending == L"!cam")
        ViewThroughSharedCamera();
    else if (Pending == L"!trace")
        TraceCharacterRpcs();
    else if (Pending == L"!authmove")
        AcceptClientPositions();
    else if (Pending == L"!wave")
        ForceEnemyWave();
    else if (Pending == L"!flow")
        TraceRunFlow();
    else if (Pending == L"!finish")
        FinishMission();
    else if (Pending.rfind(L"!round ", 0) == 0)
        EndRound(static_cast<uint8_t>(_wtoi(Pending.c_str() + 7)));
    else
        RunConsoleCommand(Pending);
}

LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    if (Message == kRunCommand)
    {
        RunPendingCommand();
        return 0;
    }
    if (Message == WM_TIMER && WParam == kSessionTimer)
    {
        TickSession();
        return 0;
    }
    return CallWindowProcW(OriginalWndProc, Window, Message, WParam, LParam);
}

BOOL CALLBACK FindGameWindow(HWND Window, LPARAM Result)
{
    DWORD Process = 0;
    GetWindowThreadProcessId(Window, &Process);
    wchar_t ClassName[64]{};
    GetClassNameW(Window, ClassName, 64);
    if (Process == GetCurrentProcessId() && wcscmp(ClassName, L"UnrealWindow") == 0)
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
    const std::wstring Directory = std::wstring(Temp) + L"ddspike\\";
    CreateDirectoryW(Directory.c_str(), nullptr);
    const std::wstring Id = std::to_wstring(GetCurrentProcessId());
    const std::wstring CommandPath = Directory + Id + L".cmd";
    SetLogPath(Directory + Id + L".log");

    Base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));

    HWND Window = nullptr;
    while (!Window)
    {
        EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&Window));
        Sleep(200);
    }
    OriginalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    GameWindow = Window;
    SetTimer(Window, kSessionTimer, 1000, nullptr);
    Log("bereit");
    InstallPatches();

    for (;;)
    {
        Sleep(200);
        HANDLE File = CreateFileW(CommandPath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (File == INVALID_HANDLE_VALUE)
            continue;
        char Buffer[512]{};
        DWORD Size = 0;
        ReadFile(File, Buffer, sizeof(Buffer) - 1, &Size, nullptr);
        CloseHandle(File);
        DeleteFileW(CommandPath.c_str());

        Log(std::string("Befehl: ") + Buffer);
        wchar_t Wide[512]{};
        MultiByteToWideChar(CP_UTF8, 0, Buffer, -1, Wide, 512);
        std::wstring Command = Wide;
        while (!Command.empty() && (Command.back() == L'\n' || Command.back() == L'\r'))
            Command.pop_back();
        QueueCommand(Command);
    }
}
}

void QueueCommand(const std::wstring& Command)
{
    Pending = Command;
    PostMessageW(GameWindow, kRunCommand, 0, 0);
}
}

BOOL WINAPI DllMain(HINSTANCE Module, DWORD Reason, void*)
{
    if (Reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(Module);
        CreateThread(nullptr, 0, Spike::Worker, nullptr, 0, nullptr);
    }
    return TRUE;
}
