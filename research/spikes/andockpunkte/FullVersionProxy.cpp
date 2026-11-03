// Vollstaendige Weiterleitung ohne Annahmen zur ABI undokumentierter Exporte.
#include <windows.h>
#ifdef ENTRY_PROBE
void ScheduleEntryProbe();
#endif
namespace
{
INIT_ONCE Once = INIT_ONCE_STATIC_INIT;
HMODULE Original;
BOOL CALLBACK LoadOriginal(PINIT_ONCE, PVOID, PVOID*)
{
    wchar_t Path[MAX_PATH]{};
    const UINT Length = GetSystemDirectoryW(Path, MAX_PATH);
    if (!Length || Length >= MAX_PATH - 12)
        return FALSE;
    wcscat_s(Path, L"\\version.dll");
    Original = LoadLibraryExW(Path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return Original != nullptr;
}
}

extern "C" FARPROC ResolveVersionExport(const char* Name)
{
    const DWORD Error = GetLastError();
    FARPROC Result = nullptr;
    if (InitOnceExecuteOnce(&Once, LoadOriginal, nullptr, nullptr))
        Result = GetProcAddress(Original, Name);
    if (!Result)
    {
        // Ein fehlender Export darf nicht mit einer geratenen Signatur enden.
        RaiseFailFastException(nullptr, nullptr, 0);
        return nullptr;
    }
    SetLastError(Error);
    return Result;
}

BOOL WINAPI DllMain(HINSTANCE Module, DWORD Reason, LPVOID)
{
#ifdef ENTRY_PROBE
    if (Reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(Module);
        ScheduleEntryProbe();
    }
#else
    (void)Module;
    (void)Reason;
#endif
    return TRUE;
}
