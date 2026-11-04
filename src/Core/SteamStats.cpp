#include "Core/SteamStats.h"

#include <windows.h>

#include <atomic>
#include <cstring>
#include <cwctype>
#include <iterator>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

#include "Core/Hooks.h"
#include "Core/Log.h"

namespace Core::SteamStats
{
namespace
{
// Die Methodenplätze stammen aus den flachen Exporten von Steamworks 1.64 und gelten für diese Version der Schnittstelle.
constexpr std::string_view StatsInterface = "STEAMUSERSTATS_INTERFACE_VERSION";
constexpr std::string_view SupportedVersion = "STEAMUSERSTATS_INTERFACE_VERSION013";

std::atomic<int> Blocked = 0;

// Jede Art nur einmal loggen, Statistiken schreibt das Spiel oft.
void Note(std::atomic<bool>& Reported, const char* What)
{
    Blocked.fetch_add(1);
    if (!Reported.exchange(true))
        Log::Info("Online: {} abgewiesen, Mod-Run zählt nicht für Steam", What);
}

std::atomic<bool> ReportedAchievement, ReportedProgress, ReportedStat, ReportedStore, ReportedLeaderboard;

// x64: Die Argumente liegen in Registern, der Aufgerufene räumt nichts ab. Die Originale werden nie gerufen.
bool __fastcall SetAchievement(void*, const char*)
{
    Note(ReportedAchievement, "Erfolg");
    return false;
}

bool __fastcall IndicateAchievementProgress(void*, const char*, uint32_t, uint32_t)
{
    Note(ReportedProgress, "Erfolgsfortschritt");
    return false;
}

bool __fastcall SetStatInt32(void*, const char*, int32_t)
{
    Note(ReportedStat, "Statistik");
    return false;
}

bool __fastcall SetStatFloat(void*, const char*, float)
{
    Note(ReportedStat, "Statistik");
    return false;
}

bool __fastcall UpdateAvgRateStat(void*, const char*, float, double)
{
    Note(ReportedStat, "Statistik");
    return false;
}

bool __fastcall StoreStats(void*)
{
    Note(ReportedStore, "Speichern der Statistiken");
    return false;
}

// SteamAPICall_t 0 ist k_uAPICallInvalid, es kommt kein Ergebnis-Callback.
uint64_t __fastcall UploadLeaderboardScore(void*, uint64_t, int32_t, int32_t, const int32_t*, int32_t)
{
    Note(ReportedLeaderboard, "Bestenlisten-Eintrag");
    return 0;
}

uint64_t __fastcall AttachLeaderboardUGC(void*, uint64_t, uint64_t)
{
    Note(ReportedLeaderboard, "Bestenlisten-Eintrag");
    return 0;
}

struct Method
{
    const char* Export;
    void* Detour;
};

const Method Methods[] = {
    {"SteamAPI_ISteamUserStats_SetAchievement", reinterpret_cast<void*>(&SetAchievement)},
    {"SteamAPI_ISteamUserStats_IndicateAchievementProgress", reinterpret_cast<void*>(&IndicateAchievementProgress)},
    {"SteamAPI_ISteamUserStats_SetStatInt32", reinterpret_cast<void*>(&SetStatInt32)},
    {"SteamAPI_ISteamUserStats_SetStatFloat", reinterpret_cast<void*>(&SetStatFloat)},
    {"SteamAPI_ISteamUserStats_UpdateAvgRateStat", reinterpret_cast<void*>(&UpdateAvgRateStat)},
    {"SteamAPI_ISteamUserStats_StoreStats", reinterpret_cast<void*>(&StoreStats)},
    {"SteamAPI_ISteamUserStats_UploadLeaderboardScore", reinterpret_cast<void*>(&UploadLeaderboardScore)},
    {"SteamAPI_ISteamUserStats_AttachLeaderboardUGC", reinterpret_cast<void*>(&AttachLeaderboardUGC)},
};

using FindFn = void* (*)(int32_t, const char*);
using UserFn = int32_t (*)();
using LoadFn = HMODULE(WINAPI*)(LPCWSTR, HANDLE, DWORD);

// Rekursiv: Das Laden der DLL und eine Anfrage der Schnittstelle können auf demselben Thread ineinander laufen.
std::recursive_mutex Lock;
Testing::Environment Env{};
std::atomic<bool> Attached = false;
uint32_t Offsets[std::size(Methods)] = {};
std::set<void*> Hooked;
FindFn OriginalFind = nullptr;
void* FindTarget = nullptr;
LoadFn OriginalLoad = nullptr;

// Sperrt die Methoden einer Schnittstelle. Alle Instanzen teilen die Funktionen, gesperrte werden übersprungen.
bool Protect(void* Interface)
{
    std::lock_guard Guard(Lock);
    void* const* Vtable = *static_cast<void* const* const*>(Interface);
    for (size_t Index = 0; Index < std::size(Methods); ++Index)
    {
        void* Function = Vtable[Offsets[Index] / sizeof(void*)];
        if (Hooked.contains(Function))
            continue;
        void* Original = nullptr;
        if (!Env.Install || !Env.Install(Function, Methods[Index].Detour, &Original))
        {
            Log::Error("Online: {} ließ sich nicht sperren", Methods[Index].Export);
            return false;
        }
        Hooked.insert(Function);
    }
    return true;
}

void* FindDetour(int32_t User, const char* Version)
{
    return Testing::OnInterface(Version, OriginalFind(User, Version));
}

std::string ModuleName(HMODULE Module)
{
    wchar_t Path[MAX_PATH] = {};
    const DWORD Length = GetModuleFileNameW(Module, Path, MAX_PATH);
    std::wstring_view Name(Path, Length);
    const size_t Slash = Name.find_last_of(L"\\/");
    if (Slash != std::wstring_view::npos)
        Name = Name.substr(Slash + 1);
    std::string Out;
    for (wchar_t Character : Name)
        Out.push_back(static_cast<char>(towlower(Character)));
    return Out;
}

void* RealExport(const char* Name)
{
    const HMODULE Module = GetModuleHandleW(L"steam_api64.dll");
    return Module ? reinterpret_cast<void*>(GetProcAddress(Module, Name)) : nullptr;
}

bool RealInstall(void* Target, void* Detour, void** Original)
{
    return Hooks::Install(Target, Detour, Original);
}

// Läuft bei jedem LoadLibrary im Prozess. Nach der Sperre nur noch ein Flag.
HMODULE WINAPI LoadDetour(LPCWSTR File, HANDLE Reserved, DWORD Flags)
{
    const HMODULE Module = OriginalLoad(File, Reserved, Flags);
    constexpr DWORD DataOnly = LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE;
    if (!Module || Attached.load() || (Flags & DataOnly) || ModuleName(Module) != "steam_api64.dll")
        return Module;
    if (Testing::Attach(Env))
        return Module;
    // Ohne Sperre kein Steam: Das Spiel läuft dann ohne Online-Subsystem, schreiben kann es nichts.
    Log::Error("Online: Sperre beim Laden von steam_api64.dll gescheitert, Steam bleibt für diesen Start aus");
    FreeLibrary(Module);
    SetLastError(ERROR_ACCESS_DENIED);
    return nullptr;
}
}

std::optional<uint32_t> VtableOffsetFromThunk(const uint8_t* Code, size_t Size)
{
    // mov rax,[rcx]
    if (!Code || Size < 7 || Code[0] != 0x48 || Code[1] != 0x8B || Code[2] != 0x01)
        return std::nullopt;
    const uint8_t* Rest = Code + 3;
    const size_t Left = Size - 3;
    // jmp qword ptr [rax+disp8]
    if (Left >= 4 && Rest[0] == 0x48 && Rest[1] == 0xFF && Rest[2] == 0x60)
        return Rest[3];
    // jmp qword ptr [rax+disp32]
    if (Left >= 7 && Rest[0] == 0x48 && Rest[1] == 0xFF && Rest[2] == 0xA0)
    {
        uint32_t Offset = 0;
        std::memcpy(&Offset, Rest + 3, 4);
        return Offset;
    }
    // mov r10,[rax+disp32]; jmp r10
    if (Left >= 10 && Rest[0] == 0x4C && Rest[1] == 0x8B && Rest[2] == 0x90 && Rest[7] == 0x49 && Rest[8] == 0xFF && Rest[9] == 0xE2)
    {
        uint32_t Offset = 0;
        std::memcpy(&Offset, Rest + 3, 4);
        return Offset;
    }
    return std::nullopt;
}

int BlockedCalls()
{
    return Blocked.load();
}

namespace Testing
{
bool Attach(const Environment& NewEnv)
{
    std::lock_guard Guard(Lock);
    if (Attached.load())
        return true;
    Env = NewEnv;
    if (!Env.Export || !Env.Install)
        return false;
    for (size_t Index = 0; Index < std::size(Methods); ++Index)
    {
        const auto* Thunk = static_cast<const uint8_t*>(Env.Export(Methods[Index].Export));
        const auto Offset = Thunk ? VtableOffsetFromThunk(Thunk, 16) : std::nullopt;
        if (!Offset || *Offset % sizeof(void*) != 0 || *Offset > 0x400)
        {
            Log::Error("Online: Einsprung {} nicht erkannt", Methods[Index].Export);
            return false;
        }
        Offsets[Index] = *Offset;
    }
    void* Find = Env.Export("SteamInternal_FindOrCreateUserInterface");
    if (!Find || !Env.Install(Find, reinterpret_cast<void*>(&FindDetour), reinterpret_cast<void**>(&OriginalFind)))
    {
        Log::Error("Online: SteamInternal_FindOrCreateUserInterface ließ sich nicht umleiten");
        return false;
    }
    FindTarget = Find;
    Attached = true;
    // Ist Steam schon gestartet, kann die Schnittstelle bereits ausgegeben sein. Dann gleich sperren.
    const auto User = reinterpret_cast<UserFn>(Env.Export("SteamAPI_GetHSteamUser"));
    const int32_t SteamUser = User ? User() : 0;
    if (SteamUser)
    {
        void* Existing = OriginalFind(SteamUser, SupportedVersion.data());
        if (Existing && !Protect(Existing))
            return false;
    }
    Log::Info("Online: Steam-Erfolge, -Statistiken und -Bestenlisten gesperrt, bevor das Spiel die Schnittstelle bekommt");
    return true;
}

void* OnInterface(const char* Version, void* Interface)
{
    if (!Version || !std::string_view(Version).starts_with(StatsInterface))
        return Interface;
    if (!Interface)
        return nullptr;
    if (std::string_view(Version) != SupportedVersion)
    {
        Log::Error("Online: unbekannte Schnittstelle {}, Spiel bekommt keine Statistiken", Version);
        return nullptr;
    }
    if (Protect(Interface))
        return Interface;
    Log::Error("Online: Sperre gescheitert, Spiel bekommt keine Statistiken");
    return nullptr;
}

void Reset()
{
    std::lock_guard Guard(Lock);
    for (void* Function : Hooked)
        Hooks::Remove(Function);
    if (FindTarget)
        Hooks::Remove(FindTarget);
    Attached = false;
    Hooked.clear();
    OriginalFind = nullptr;
    FindTarget = nullptr;
    Env = {};
}
}

bool Block()
{
    // Zuerst LoadLibrary, damit eine DLL, die genau jetzt geladen wird, nicht durchrutscht.
    const HMODULE KernelBase = GetModuleHandleW(L"kernelbase.dll");
    auto* Load = KernelBase ? reinterpret_cast<LoadFn>(GetProcAddress(KernelBase, "LoadLibraryExW")) : nullptr;
    {
        std::lock_guard Guard(Lock);
        Env = {&RealExport, &RealInstall};
    }
    if (!Load || !Hooks::Install(Load, &LoadDetour, &OriginalLoad))
    {
        Log::Error("Online: LoadLibraryExW ließ sich nicht umleiten");
        return false;
    }
    if (!GetModuleHandleW(L"steam_api64.dll"))
    {
        Log::Info("Online: steam_api64.dll noch nicht geladen, Sperre folgt beim Laden");
        return true;
    }
    return Testing::Attach({&RealExport, &RealInstall});
}
}
