// Host: Leitet die lokale Reise des Spiels in servertravel um, damit der Client mitkommt.
#include "Spike.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kSetClientTravel = 0x3262830;
constexpr uintptr_t kSeamlessTravelToLevel = 0x5976AD0;
constexpr uintptr_t kLoadSettingsPackage = 0x10;

using SetClientTravelFn = void(__fastcall*)(uintptr_t, uintptr_t, const wchar_t*, uint32_t);
using SeamlessTravelFn = void(__fastcall*)(uintptr_t, uintptr_t);
SetClientTravelFn OriginalSetClientTravel;
SeamlessTravelFn OriginalSeamlessTravel;
bool RedirectTravel = false;

// Der Spielablauf reist per SetClientTravel, das nimmt den Client nicht mit. Scharf geschaltet wird daraus servertravel.
void __fastcall SetClientTravelHook(uintptr_t Engine, uintptr_t World, const wchar_t* Url, uint32_t Type)
{
    if (!RedirectTravel || wcsstr(Url, L"listen") || wcsstr(Url, L"MainMenu"))
    {
        OriginalSetClientTravel(Engine, World, Url, Type);
        return;
    }
    const std::wstring Target(Url);
    Log("Reise umgeleitet: " + std::string(Target.begin(), Target.end()));
    QueueCommand(L"servertravel " + Target + L"?listen");
}

// Missionen laufen ueber SeamlessTravelToLevel, das reist nur lokal. Settings: Level ab +8, Paketname als FName ab +0x10.
void __fastcall SeamlessTravelHook(uintptr_t Subsystem, uintptr_t Settings)
{
    const std::string Package = NameFromIndex(Read<int32_t>(Settings + kLoadSettingsPackage));
    // Das Hauptmenue beendet die Sitzung, die Reise laeuft dort lokal und mit dem Ladeablauf des Spiels.
    if (!RedirectTravel || Package.find("MainMenu") != std::string::npos)
    {
        OriginalSeamlessTravel(Subsystem, Settings);
        return;
    }
    Log("Reise umgeleitet: " + Package);
    QueueCommand(L"servertravel " + std::wstring(Package.begin(), Package.end()) + L"?listen");
}
}

void ArmTravelRedirect() { RedirectTravel = true; }

bool PatchTravel(CodeWriter& Code)
{
    const uintptr_t TravelSite = Base + kSetClientTravel;
    if (!Expect(TravelSite, {0x48, 0x89, 0x5C, 0x24, 0x08}, "in SetClientTravel"))
        return false;
    OriginalSetClientTravel = reinterpret_cast<SetClientTravelFn>(Code.Here());
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x08});                   // mov [rsp+8],rbx
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(TravelSite + 5);
    WriteJump(TravelSite, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&SetClientTravelHook)));

    const uintptr_t SeamlessSite = Base + kSeamlessTravelToLevel;
    if (!Expect(SeamlessSite, {0x48, 0x89, 0x5C, 0x24, 0x18}, "in SeamlessTravelToLevel"))
        return false;
    OriginalSeamlessTravel = reinterpret_cast<SeamlessTravelFn>(Code.Here());
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x18});                   // mov [rsp+18h],rbx
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(SeamlessSite + 5);
    WriteJump(SeamlessSite, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&SeamlessTravelHook)));
    return true;
}
}
