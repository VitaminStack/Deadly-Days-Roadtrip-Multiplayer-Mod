// Host: Die lokale Reise des Spiels wird servertravel, damit der Client mitkommt. Aus Spike Travel.cpp.
#include <string>

#include "Core/Dispatcher.h"
#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/PatchPolicy.h"
#include "Game/PatchRuntime.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t SetClientTravel = 0x3262A30;
constexpr uintptr_t SeamlessTravelToLevel = 0x597ABB0;
constexpr uintptr_t LoadSettingsPackage = 0x10;
constexpr uintptr_t LoadingTimescaleManager = 0xE0; // ULoadingSubSystem::TimescaleManager
constexpr uintptr_t TimescaleRequestCount = 0x50;   // UTimescaleManager::TimeRequests, Plätze der SparseArray
constexpr uintptr_t TimescaleRequestFree = 0x7C;    // davon frei

using SetClientTravelFn = void(__fastcall*)(uintptr_t, uintptr_t, const wchar_t*, uint32_t);
using SeamlessTravelFn = void(__fastcall*)(uintptr_t, uintptr_t);
SetClientTravelFn OriginalSetClientTravel = nullptr;
SeamlessTravelFn OriginalSeamlessTravel = nullptr;

struct FString
{
    const wchar_t* Data;
    int32_t Num;
    int32_t Max;
};

void RunConsoleCommand(const std::wstring& Command)
{
    const uintptr_t Library = Objects::FindNamed("Default__KismetSystemLibrary");
    const uintptr_t Function = Objects::FindObject("ExecuteConsoleCommand", "KismetSystemLibrary");
    const uintptr_t World = CurrentWorld();
    if (!Library || !Function || !World)
    {
        Core::Log::Error("Reise: Konsolenbefehl nicht ausführbar");
        return;
    }
    struct
    {
        uintptr_t WorldContext;
        FString Command;
        uintptr_t SpecificPlayer;
    } Params{World, {Command.c_str(), static_cast<int32_t>(Command.size() + 1), static_cast<int32_t>(Command.size() + 1)}, 0};
    Objects::ProcessEvent(Library, Function, &Params);
}

// Nicht aus dem Hook heraus reisen, sondern im nächsten Takt, wie ein eingegebener Befehl.
void QueueServerTravel(const std::string& Target)
{
    Core::Log::Info("Reise umgeleitet: {}", Target);
    Core::GameThread().Post([Target] { RunConsoleCommand(L"servertravel " + std::wstring(Target.begin(), Target.end()) + L"?listen"); });
}

std::string Narrow(const wchar_t* Text)
{
    std::string Result;
    for (; Text && *Text; ++Text)
        Result += *Text < 0x80 ? static_cast<char>(*Text) : '?';
    return Result;
}

// Der Spielablauf reist per SetClientTravel, das nimmt den Client nicht mit.
void __fastcall SetClientTravelHook(uintptr_t Engine, uintptr_t World, const wchar_t* Url, uint32_t Type)
{
    const std::string Target = Narrow(Url);
    if (!ShouldRedirectTravel(CurrentRole(), Target))
    {
        OriginalSetClientTravel(Engine, World, Url, Type);
        return;
    }
    QueueServerTravel(Target);
}

// Das Original startet den Ladeablauf: eigene Pause, dann TimescaleManager::ClearAllRequests. Die Umleitung überspringt
// das, die Freeze-Anfrage aus FinishMission bliebe stehen und hielte jede weitere Runde an. Darum hier dasselbe Abräumen.
void ClearTimeRequests(uintptr_t LoadingSubsystem)
{
    const uintptr_t Timescale = Memory::ReadPointer(LoadingSubsystem + LoadingTimescaleManager).value_or(0);
    static const uintptr_t Function = Objects::FindObject("ClearAllRequests", "TimescaleManager");
    if (!Timescale || !Function)
    {
        Core::Log::Error("Reise: Zeitanfragen nicht abgeräumt, TimescaleManager fehlt");
        return;
    }
    const auto Active = [&] {
        return Memory::Read<int32_t>(Timescale + TimescaleRequestCount).value_or(0) - Memory::Read<int32_t>(Timescale + TimescaleRequestFree).value_or(0);
    };
    const int32_t Before = Active();
    struct
    {
        uintptr_t Data;
        int32_t Num;
        int32_t Max;
    } Exclude{};
    Objects::ProcessEvent(Timescale, Function, &Exclude);
    Core::Log::Info("Reise: Zeitanfragen abgeräumt {} -> {}", Before, Active());
}

// Missionen laufen über SeamlessTravelToLevel, das reist nur lokal. Settings: Paketname als FName bei +0x10.
void __fastcall SeamlessTravelHook(uintptr_t Subsystem, uintptr_t Settings)
{
    const std::string Package = Memory::NameFromIndex(Memory::Read<int32_t>(Settings + LoadSettingsPackage).value_or(-1));
    if (!ClientMayStartTravel(CurrentRole() == NetRole::Client, Package))
    {
        // Etwa ein Klick auf die eigene Routenkarte nach dem Rundenende. Der Host wählt den Tag (rundenende-client).
        Core::Log::Info("Reise: Client wollte selbst nach {} reisen, unterdrückt, der Host wählt", Package);
        return;
    }
    if (!ShouldRedirectTravel(CurrentRole(), Package))
    {
        OriginalSeamlessTravel(Subsystem, Settings);
        return;
    }
    ClearTimeRequests(Subsystem);
    QueueServerTravel(Package);
}

bool CheckTravel()
{
    return Code::Expect(At(SetClientTravel), {0x48, 0x89, 0x5C, 0x24, 0x08}, "in SetClientTravel") &&
           Code::Expect(At(SeamlessTravelToLevel), {0x48, 0x89, 0x5C, 0x24, 0x18}, "in SeamlessTravelToLevel");
}

bool ApplyTravel(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(SetClientTravel)), reinterpret_cast<void*>(&SetClientTravelHook),
                                reinterpret_cast<void**>(&OriginalSetClientTravel)) &&
           Core::Hooks::Install(reinterpret_cast<void*>(At(SeamlessTravelToLevel)), reinterpret_cast<void*>(&SeamlessTravelHook),
                                reinterpret_cast<void**>(&OriginalSeamlessTravel));
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t TravelSites[] = {SetClientTravel, SeamlessTravelToLevel};
const Patch List[] = {
    {"Reise des Hosts als servertravel", &CheckTravel, &ApplyTravel, TravelSites},
};
}

std::span<const Patch> TravelPatches()
{
    return List;
}
}
