// Proxy für version.dll. Leitet alle Exporte an das System-Original weiter und startet die Mod.
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace
{
using Forwarded = uintptr_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

INIT_ONCE Once = INIT_ONCE_STATIC_INIT;
HMODULE SystemVersion = nullptr;

BOOL CALLBACK LoadSystemVersion(PINIT_ONCE, PVOID, PVOID*)
{
    // Ein blosser Basisname liefert den bereits geladenen Proxy, das endet im Stacküberlauf.
    wchar_t Path[MAX_PATH]{};
    const UINT Length = GetSystemDirectoryW(Path, MAX_PATH);
    if (!Length || Length >= MAX_PATH - 12)
        return FALSE;
    wcscat_s(Path, L"\\version.dll");
    SystemVersion = LoadLibraryExW(Path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return SystemVersion != nullptr;
}

Forwarded Resolve(const char* Name)
{
    if (!InitOnceExecuteOnce(&Once, LoadSystemVersion, nullptr, nullptr))
        return nullptr;
    return reinterpret_cast<Forwarded>(GetProcAddress(SystemVersion, Name));
}

std::filesystem::path OwnDirectory()
{
    HMODULE Self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&OwnDirectory), &Self);
    std::wstring Path(MAX_PATH, L'\0');
    const DWORD Length = GetModuleFileNameW(Self, Path.data(), static_cast<DWORD>(Path.size()));
    Path.resize(Length < Path.size() ? Length : 0);
    return std::filesystem::path(Path).parent_path();
}

DWORD WINAPI StartMod(LPVOID)
{
    // Ohne Mod-Ordner läuft das Spiel unverändert weiter.
    const auto ModDll = OwnDirectory() / L"DDCoop" / L"DDCoop.dll";
    const HMODULE Mod = LoadLibraryExW(ModDll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!Mod)
        return 0;
    if (const auto Start = reinterpret_cast<void (*)()>(GetProcAddress(Mod, "ModStart")))
        Start();
    return 0;
}
}

// Alle Exporte sind Ganzzahlen und Zeiger, höchstens acht Stück, Rückgabe in RAX.
// Zu viel gelesene Argumente liegen im Stack des Aufrufers und werden ignoriert.
#define FORWARD(Name)                                                                                                            \
    extern "C" uintptr_t Proxy##Name(uintptr_t A, uintptr_t B, uintptr_t C, uintptr_t D, uintptr_t E, uintptr_t F, uintptr_t G, uintptr_t H) \
    {                                                                                                                            \
        static const Forwarded Original = Resolve(#Name);                                                                        \
        if (Original)                                                                                                            \
            return Original(A, B, C, D, E, F, G, H);                                                                             \
        SetLastError(ERROR_PROC_NOT_FOUND);                                                                                      \
        return 0;                                                                                                                \
    }

FORWARD(GetFileVersionInfoA)
FORWARD(GetFileVersionInfoByHandle)
FORWARD(GetFileVersionInfoExA)
FORWARD(GetFileVersionInfoExW)
FORWARD(GetFileVersionInfoSizeA)
FORWARD(GetFileVersionInfoSizeExA)
FORWARD(GetFileVersionInfoSizeExW)
FORWARD(GetFileVersionInfoSizeW)
FORWARD(GetFileVersionInfoW)
FORWARD(VerFindFileA)
FORWARD(VerFindFileW)
FORWARD(VerInstallFileA)
FORWARD(VerInstallFileW)
FORWARD(VerLanguageNameA)
FORWARD(VerLanguageNameW)
FORWARD(VerQueryValueA)
FORWARD(VerQueryValueW)

// Unter dem Loader-Lock kein LoadLibrary und kein Spielzugriff. Der Thread läuft erst los, wenn der Lock frei ist.
BOOL WINAPI DllMain(HINSTANCE Module, DWORD Reason, LPVOID)
{
    if (Reason != DLL_PROCESS_ATTACH)
        return TRUE;
    DisableThreadLibraryCalls(Module);
    if (const HANDLE Thread = CreateThread(nullptr, 0, StartMod, nullptr, 0, nullptr))
        CloseHandle(Thread);
    return TRUE;
}
