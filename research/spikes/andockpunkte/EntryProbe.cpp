// Eigenes kleines Laufzeitbeispiel. Nur mit ENTRY_PROBE in die Test-Proxy-DLL linken.
// Keine Spiellogik und kein Hot-Unload; Sitzung zum Entfernen beenden.
#include "../a-connect/Spike.h"
#include <cmath>
#include <sstream>

namespace
{
constexpr UINT ProbeMessage = WM_APP + 188;
INIT_ONCE StartOnce = INIT_ONCE_STATIC_INIT;
WNDPROC PreviousWindowProc;
Spike::ProcessEventFn PreviousProcessEvent;
uintptr_t ExpectedObject, ExpectedFunction;
LONG Calls = 0;

bool CheckSignature(const char* Name, uintptr_t ExpectedRva, const char* Text)
{
    std::vector<int> Pattern;
    std::istringstream Input(Text);
    for (std::string Token; Input >> Token;)
        Pattern.push_back(Token == "??" ? -1 : std::stoi(Token, nullptr, 16));
    const auto* Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(Spike::Base);
    const auto* Nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(Spike::Base + Dos->e_lfanew);
    const auto* Sections = IMAGE_FIRST_SECTION(Nt);
    size_t Hits = 0;
    uintptr_t FoundRva = 0;
    for (unsigned i = 0; i < Nt->FileHeader.NumberOfSections; ++i)
    {
        if (!(Sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE))
            continue;
        const auto* Data = reinterpret_cast<const uint8_t*>(Spike::Base + Sections[i].VirtualAddress);
        const size_t Size = Sections[i].Misc.VirtualSize;
        for (size_t Offset = 0; Offset + Pattern.size() <= Size; ++Offset)
        {
            if (Data[Offset] != Pattern[0])
                continue;
            bool Match = true;
            for (size_t j = 1; Match && j < Pattern.size(); ++j)
                Match = Pattern[j] < 0 || Data[Offset + j] == Pattern[j];
            if (Match)
            {
                ++Hits;
                FoundRva = Sections[i].VirtualAddress + Offset;
            }
        }
    }
    const bool Valid = Hits == 1 && FoundRva == ExpectedRva;
    Spike::Log(std::string("Live-Signatur ") + Name + ": hits=" + std::to_string(Hits) +
        " expected=" + (Valid ? "yes" : "no"));
    return Valid;
}

void __fastcall ObserveProcessEvent(uintptr_t Object, uintptr_t Function, void* Params)
{
    if (Object == ExpectedObject && Function == ExpectedFunction)
        InterlockedIncrement(&Calls);
    PreviousProcessEvent(Object, Function, Params);
}

bool ExchangeSlot(Spike::ProcessEventFn* Slot, Spike::ProcessEventFn Value)
{
    DWORD Protection = 0;
    if (!VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &Protection))
        return false;
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(Slot), reinterpret_cast<void*>(Value));
    DWORD Ignored = 0;
    return VirtualProtect(Slot, sizeof(*Slot), Protection, &Ignored) != FALSE;
}

void ProbeEvent()
{
    using namespace Spike;
    const auto Object = FindInstance("BP_DD_Character_C");
    const auto Function = FindObject("K2_GetActorLocation", "Actor");
    if (!Object || !Function)
    {
        Log("ProcessEvent: kein Character/Funktion; Probe nicht ausgefuehrt");
        return;
    }
    struct Location { double X, Y, Z; } Before{}, After{};
    auto* Slot = &Read<ProcessEventFn*>(Object)[kProcessEventIndex];
    PreviousProcessEvent = *Slot;
    PreviousProcessEvent(Object, Function, &Before);
    ExpectedObject = Object;
    ExpectedFunction = Function;
    InterlockedExchange(&Calls, 0);
    if (!ExchangeSlot(Slot, ObserveProcessEvent))
    {
        Log("ProcessEvent: Slot konnte nicht installiert werden");
        return;
    }
    (*Slot)(Object, Function, &After);
    const bool Restored = ExchangeSlot(Slot, PreviousProcessEvent);
    const bool Same = memcmp(&Before, &After, sizeof(Before)) == 0 &&
        std::isfinite(After.X) && std::isfinite(After.Y) && std::isfinite(After.Z);
    Log(std::string("ProcessEvent probe: calls=") + std::to_string(Calls) +
        " same-result=" + (Same ? "yes" : "no") + " restored=" + (Restored ? "yes" : "no"));
}

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM W, LPARAM L)
{
    if (Message == ProbeMessage)
    {
        const DWORD WindowThread = GetWindowThreadProcessId(Window, nullptr);
        Spike::Log(WindowThread == GetCurrentThreadId() ? "Probe: Fensterthread bestaetigt" : "Probe: falscher Thread");
        if (WindowThread == GetCurrentThreadId())
            ProbeEvent();
        return 0;
    }
    return CallWindowProcW(PreviousWindowProc, Window, Message, W, L);
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
    wchar_t Exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, Exe, MAX_PATH);
    const auto Name = wcsrchr(Exe, L'\\');
    if (!Name || _wcsicmp(Name + 1, L"DDSurvivors-Win64-Shipping.exe") != 0)
        return 0; // Der isolierte Proxy-Smoke-Test ist kein Unreal-Prozess.
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    const auto Root = std::wstring(Temp) + L"ddentry\\";
    CreateDirectoryW(Root.c_str(), nullptr);
    const auto Id = std::to_wstring(GetCurrentProcessId());
    Spike::SetLogPath(Root + Id + L".log");
    Spike::Base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    Spike::Log("Proxy bootstrap: Attach-Worker ausserhalb DllMain; unabhaengig von VERSION-Aufrufen");
    if (!CheckSignature("ProcessEvent", 0x160C040,
            "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 00 01 00 00 48 8D 6C 24 30 48 89 9D 28 01 00 00 48 8B 05 ?? ?? ?? ??") ||
        !CheckSignature("SetClientTravel", 0x3262830,
            "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 81 B0 12 00 00 41 8B F9 48 63 89 B8 12 00 00 49 8B F0") ||
        !CheckSignature("SeamlessTravelToLevel", 0x5976AD0,
            "48 89 5C 24 18 48 89 54 24 10 55 56 57 48 8D 6C 24 B9 48 81 EC 90 00 00 00 48 8B FA 48 8B F1 80 79 30 00"))
    {
        Spike::Log("Probe deaktiviert: Signaturpruefung fehlgeschlagen");
        return 3;
    }
    HWND Window = nullptr;
    for (int i = 0; i < 300 && !Window; ++i)
    {
        EnumWindows(FindWindow, reinterpret_cast<LPARAM>(&Window));
        if (!Window)
            Sleep(100);
    }
    if (!Window)
    {
        Spike::Log("kein Spielfenster");
        return 1;
    }
    PreviousWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(Window, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&WindowProc)));
    if (!PreviousWindowProc)
        return 2;
    Spike::Log("bereit");
    const auto Command = Root + Id + L".cmd";
    for (;;)
    {
        Sleep(200);
        const HANDLE File = CreateFileW(Command.c_str(), GENERIC_READ, 0, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (File == INVALID_HANDLE_VALUE)
            continue;
        CloseHandle(File);
        DeleteFileW(Command.c_str());
        SendMessageW(Window, ProbeMessage, 0, 0);
    }
}

BOOL CALLBACK StartWorker(PINIT_ONCE, PVOID, PVOID*)
{
    const HANDLE Thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    if (!Thread)
        return FALSE;
    CloseHandle(Thread);
    return TRUE;
}
}

void ScheduleEntryProbe() { InitOnceExecuteOnce(&StartOnce, StartWorker, nullptr, nullptr); }
