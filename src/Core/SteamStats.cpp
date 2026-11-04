#include "Core/SteamStats.h"

#include <windows.h>

#include <atomic>
#include <cstring>
#include <iterator>
#include <string>
#include <thread>

#include "Core/Hooks.h"
#include "Core/Log.h"

namespace Core::SteamStats
{
namespace
{
std::atomic<int> Blocked = 0;
std::atomic<bool> Active = false;

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

struct Target
{
    const char* Export;
    void* Detour;
};

const Target Targets[] = {
    {"SteamAPI_ISteamUserStats_SetAchievement", reinterpret_cast<void*>(&SetAchievement)},
    {"SteamAPI_ISteamUserStats_IndicateAchievementProgress", reinterpret_cast<void*>(&IndicateAchievementProgress)},
    {"SteamAPI_ISteamUserStats_SetStatInt32", reinterpret_cast<void*>(&SetStatInt32)},
    {"SteamAPI_ISteamUserStats_SetStatFloat", reinterpret_cast<void*>(&SetStatFloat)},
    {"SteamAPI_ISteamUserStats_UpdateAvgRateStat", reinterpret_cast<void*>(&UpdateAvgRateStat)},
    {"SteamAPI_ISteamUserStats_StoreStats", reinterpret_cast<void*>(&StoreStats)},
    {"SteamAPI_ISteamUserStats_UploadLeaderboardScore", reinterpret_cast<void*>(&UploadLeaderboardScore)},
    {"SteamAPI_ISteamUserStats_AttachLeaderboardUGC", reinterpret_cast<void*>(&AttachLeaderboardUGC)},
};

std::string ModuleOf(const void* Address)
{
    HMODULE Module = nullptr;
    char Path[MAX_PATH] = {};
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<const char*>(Address), &Module) ||
        !GetModuleFileNameA(Module, Path, MAX_PATH))
        return "?";
    // Nur der Dateiname, keine lokalen Pfade ins Log.
    const char* Name = strrchr(Path, '\\');
    return Name ? Name + 1 : Path;
}

enum class Attempt
{
    NotReady,
    Failed,
    Done
};

Attempt TryBlock()
{
    const HMODULE Api = GetModuleHandleW(L"steam_api64.dll");
    if (!Api)
        return Attempt::NotReady;
    // Nicht über SteamAPI_SteamUserStats_v013: Dessen Zwischenspeicher behält einen Nullwert, wenn der Aufruf in die
    // Initialisierung von Steam fällt. Das passierte im Test mit 50-ms-Takt, danach blieb die Sperre 120 s aus.
    using UserFn = int32_t (*)();
    using FindFn = void* (*)(int32_t, const char*);
    const auto User = reinterpret_cast<UserFn>(GetProcAddress(Api, "SteamAPI_GetHSteamUser"));
    const auto Find = reinterpret_cast<FindFn>(GetProcAddress(Api, "SteamInternal_FindOrCreateUserInterface"));
    if (!User || !Find)
    {
        Log::Error("Online: Steam-Exporte fehlen, Steam-Version passt nicht");
        return Attempt::Failed;
    }
    const int32_t SteamUser = User();
    void* Interface = SteamUser ? Find(SteamUser, "STEAMUSERSTATS_INTERFACE_VERSION013") : nullptr;
    if (!Interface)
        return Attempt::NotReady;
    const auto* Vtable = *static_cast<void* const* const*>(Interface);

    int Hooked = 0;
    std::string Module;
    for (const Target& Entry : Targets)
    {
        const auto* Thunk = reinterpret_cast<const uint8_t*>(GetProcAddress(Api, Entry.Export));
        const auto Offset = Thunk ? VtableOffsetFromThunk(Thunk, 16) : std::nullopt;
        if (!Offset || *Offset % sizeof(void*) != 0 || *Offset > 0x400)
        {
            Log::Error("Online: Einsprung {} nicht erkannt", Entry.Export);
            continue;
        }
        void* Function = Vtable[*Offset / sizeof(void*)];
        void* Original = nullptr;
        if (!Hooks::Install(Function, Entry.Detour, &Original))
            continue;
        Module = ModuleOf(Function);
        ++Hooked;
    }
    if (Hooked != static_cast<int>(std::size(Targets)))
    {
        Log::Error("Online: nur {} von {} Steam-Schreibfunktionen gesperrt", Hooked, std::size(Targets));
        return Attempt::Failed;
    }
    Log::Info("Online: Steam-Erfolge, -Statistiken und -Bestenlisten gesperrt ({} Funktionen in {}), bisher {} Schreibversuche", Hooked,
              Module, Blocked.load());
    return Attempt::Done;
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

void Block()
{
    if (Active.exchange(true))
        return;
    const Attempt First = TryBlock();
    if (First != Attempt::NotReady)
        return;
    // Steam lädt das Spiel über sein Online-Subsystem, das kann nach dem Start der Mod passieren.
    std::thread([] {
        // Kurzer Takt, damit zwischen Steam-Start und Sperre möglichst kein Schreibversuch durchrutscht.
        for (int Try = 0; Try < 2400; ++Try)
        {
            Sleep(50);
            if (TryBlock() != Attempt::NotReady)
                return;
        }
        Log::Error("Online: Steam nach 120 s nicht bereit, Erfolge und Bestenlisten nicht gesperrt");
    }).detach();
}
}
