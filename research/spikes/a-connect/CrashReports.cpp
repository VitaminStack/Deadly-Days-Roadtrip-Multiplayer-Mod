// Beide: Laesst den Start von CrashReportClient.exe scheitern, damit Testabstuerze nicht bei Pixelsplit landen.
// Zweite Schicht neben CrashGuard.ps1, greift erst ab dem Laden des Spikes. Die Mod hat dieselbe Sperre in Core/CrashReports.
#include "Spike.h"

namespace Spike
{
namespace
{
using CreateProcessWFn = BOOL(WINAPI*)(LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR,
                                       LPSTARTUPINFOW, LPPROCESS_INFORMATION);
CreateProcessWFn OriginalCreateProcessW;

bool NamesReporter(std::wstring Text)
{
    for (wchar_t& Char : Text)
        Char = towlower(Char);
    return Text.find(L"crashreportclient") != std::wstring::npos;
}

// Nur das Programm zaehlt, nicht die Argumente. FPlatformProcess::CreateProc setzt es vorne in die Kommandozeile.
std::wstring ProgramOf(const wchar_t* CommandLine)
{
    const std::wstring Line(CommandLine ? CommandLine : L"");
    const size_t End = Line.starts_with(L'"') ? Line.find(L'"', 1) : Line.find(L' ');
    return Line.substr(0, End == std::wstring::npos ? Line.size() : End + 1);
}

BOOL WINAPI CreateProcessWHook(LPCWSTR Application, LPWSTR CommandLine, LPSECURITY_ATTRIBUTES ProcessAttributes,
                               LPSECURITY_ATTRIBUTES ThreadAttributes, BOOL InheritHandles, DWORD Flags, LPVOID Environment,
                               LPCWSTR Directory, LPSTARTUPINFOW StartupInfo, LPPROCESS_INFORMATION ProcessInformation)
{
    if (NamesReporter(Application ? Application : L"") || NamesReporter(ProgramOf(CommandLine)))
    {
        Log("CrashReports: Start des Crash-Reporters verhindert");
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return OriginalCreateProcessW(Application, CommandLine, ProcessAttributes, ThreadAttributes, InheritHandles, Flags, Environment,
                                  Directory, StartupInfo, ProcessInformation);
}
}

bool PatchCrashReports(CodeWriter&)
{
    const HMODULE Kernel32 = GetModuleHandleW(L"kernel32.dll");
    const auto Expected = reinterpret_cast<CreateProcessWFn>(GetProcAddress(Kernel32, "CreateProcessW"));
    // Der Importeintrag wird in der Importtabelle gesucht, damit er nicht vom Build abhaengt.
    const uintptr_t SlotRva = FindImportSlot(reinterpret_cast<const void*>(Expected));
    auto* Slot = reinterpret_cast<CreateProcessWFn*>(Base + SlotRva);
    if (!SlotRva || *Slot != Expected)
    {
        Log("CrashReports: Importtabelle unerwartet, Patch verweigert");
        return false;
    }
    OriginalCreateProcessW = *Slot;
    DWORD Old;
    VirtualProtect(Slot, sizeof(*Slot), PAGE_READWRITE, &Old);
    *Slot = &CreateProcessWHook;
    VirtualProtect(Slot, sizeof(*Slot), Old, &Old);
    Log("CrashReports: Crash-Reporter gesperrt");
    return true;
}
}
