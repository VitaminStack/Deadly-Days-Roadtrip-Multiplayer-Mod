// Isolierter Lade-Spike, keine Mod und kein Deployment ins Spiel.
#include <windows.h>

namespace
{
INIT_ONCE Once = INIT_ONCE_STATIC_INIT;
HMODULE SystemVersion = nullptr;

BOOL CALLBACK LoadSystemVersion(PINIT_ONCE, PVOID, PVOID*)
{
    // Ein blosser Basisname kann erneut den bereits geladenen Proxy liefern.
    wchar_t Path[MAX_PATH]{};
    const UINT Length = GetSystemDirectoryW(Path, MAX_PATH);
    if (!Length || Length >= MAX_PATH - 12)
        return FALSE;
    wcscat_s(Path, L"\\version.dll");
    SystemVersion = LoadLibraryExW(Path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return SystemVersion != nullptr;
}

template <typename T> T Resolve(const char* Name)
{
    if (!InitOnceExecuteOnce(&Once, LoadSystemVersion, nullptr, nullptr))
        return nullptr;
    return reinterpret_cast<T>(GetProcAddress(SystemVersion, Name));
}
}

extern "C" DWORD WINAPI ProxyGetFileVersionInfoSizeW(LPCWSTR File, LPDWORD Handle)
{
    using Fn = DWORD(WINAPI*)(LPCWSTR, LPDWORD);
    const auto Original = Resolve<Fn>("GetFileVersionInfoSizeW");
    if (Original)
        return Original(File, Handle);
    SetLastError(ERROR_PROC_NOT_FOUND);
    return 0;
}

extern "C" BOOL WINAPI ProxyGetFileVersionInfoW(LPCWSTR File, DWORD Handle, DWORD Size, LPVOID Data)
{
    using Fn = BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID);
    const auto Original = Resolve<Fn>("GetFileVersionInfoW");
    if (Original)
        return Original(File, Handle, Size, Data);
    SetLastError(ERROR_PROC_NOT_FOUND);
    return FALSE;
}

extern "C" BOOL WINAPI ProxyVerQueryValueW(LPCVOID Block, LPCWSTR SubBlock, LPVOID* Buffer, PUINT Size)
{
    using Fn = BOOL(WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
    const auto Original = Resolve<Fn>("VerQueryValueW");
    if (Original)
        return Original(Block, SubBlock, Buffer, Size);
    SetLastError(ERROR_PROC_NOT_FOUND);
    return FALSE;
}

// Kein LoadLibrary, Thread oder Spielzugriff unter dem Loader-Lock.
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
