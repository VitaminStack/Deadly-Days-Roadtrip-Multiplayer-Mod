#include "Core/CrashReports.h"

#include <windows.h>

#include <atomic>
#include <cwctype>

#include "Core/Hooks.h"
#include "Core/Log.h"

namespace Core::CrashReports
{
namespace
{
using CreateProcessWFn = BOOL(WINAPI*)(LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR,
                                       LPSTARTUPINFOW, LPPROCESS_INFORMATION);
CreateProcessWFn Original = nullptr;
std::atomic<int> Blocked = 0;

bool ContainsReporter(std::wstring_view Text)
{
    constexpr std::wstring_view Name = L"crashreportclient";
    if (Text.size() < Name.size())
        return false;
    for (size_t Start = 0; Start + Name.size() <= Text.size(); ++Start)
    {
        size_t Index = 0;
        while (Index < Name.size() && towlower(Text[Start + Index]) == Name[Index])
            ++Index;
        if (Index == Name.size())
            return true;
    }
    return false;
}

// Läuft im Absturz auf dem Melde-Thread des Spiels. Kein Speicher anfordern außer im Log.
BOOL WINAPI Detour(LPCWSTR Application, LPWSTR CommandLine, LPSECURITY_ATTRIBUTES ProcessAttributes,
                   LPSECURITY_ATTRIBUTES ThreadAttributes, BOOL InheritHandles, DWORD Flags, LPVOID Environment, LPCWSTR Directory,
                   LPSTARTUPINFOW StartupInfo, LPPROCESS_INFORMATION ProcessInformation)
{
    if (IsCrashReporterLaunch(Application ? Application : L"", CommandLine ? CommandLine : L""))
    {
        Blocked.fetch_add(1);
        Log::Warn("Absturzbericht: Start des Crash-Reporters verhindert, Bericht bleibt lokal unter Saved/Crashes");
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return Original(Application, CommandLine, ProcessAttributes, ThreadAttributes, InheritHandles, Flags, Environment, Directory,
                    StartupInfo, ProcessInformation);
}
}

bool IsCrashReporterLaunch(std::wstring_view Application, std::wstring_view CommandLine)
{
    // Der Programmname steht je nach Aufruf im ersten Argument oder vorne in der Kommandozeile.
    if (ContainsReporter(Application))
        return true;
    const size_t End = CommandLine.starts_with(L'"') ? CommandLine.find(L'"', 1) : CommandLine.find(L' ');
    return ContainsReporter(CommandLine.substr(0, End == std::wstring_view::npos ? CommandLine.size() : End + 1));
}

bool Block()
{
    // Kernelbase statt der Kernel32-Weiterleitung, so ist auch ein Aufruf an der Importtabelle vorbei abgedeckt.
    const HMODULE KernelBase = GetModuleHandleW(L"kernelbase.dll");
    auto* Target = KernelBase ? reinterpret_cast<CreateProcessWFn>(GetProcAddress(KernelBase, "CreateProcessW")) : nullptr;
    if (!Target || !Hooks::Install(Target, &Detour, &Original))
    {
        Log::Error("Absturzbericht: Sperre des Crash-Reporters nicht aktiv");
        return false;
    }
    Log::Info("Absturzbericht: Crash-Reporter gesperrt");
    return true;
}
}
