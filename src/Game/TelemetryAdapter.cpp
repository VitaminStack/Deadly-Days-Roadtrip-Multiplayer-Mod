#include "Game/TelemetryAdapter.h"

#include <windows.h>

#include <iterator>
#include <string>
#include <string_view>

#include "Core/Log.h"
#include "Core/ReportHosts.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Reflection.h"

namespace Game
{
namespace
{
constexpr ULONGLONG CheckIntervalMs = 1000;
constexpr std::string_view Package = "/Script/DDSurvivors";

constexpr std::string_view TelemetryClass = "DDBalanceTelemetrySettings";
constexpr std::string_view ReportClass = "DDCorruptedSaveAutoReportSettings";

struct Setting
{
    const char* Object;
    Reflection::FieldSpec Field;
    const char* Label;
};

// Der erste Eintrag ist das Telemetrie-Objekt, daraus kommt auch die Server-Adresse.
constexpr Setting Settings[] = {
    {"Default__DDBalanceTelemetrySettings", {Package, TelemetryClass, "bEnabled", "BoolProperty", 1}, "Balance-Telemetrie"},
    {"Default__DDBalanceTelemetrySettings", {Package, TelemetryClass, "bAttachSaveFile", "BoolProperty", 1}, "Spielstand an Telemetrie"},
    {"Default__DDCorruptedSaveAutoReportSettings", {Package, ReportClass, "bEnableAutomaticCorruptedSaveReports", "BoolProperty", 1},
     "Automatische Berichte über beschädigte Spielstände"},
};
constexpr Reflection::FieldSpec ServerBaseUrlField{Package, TelemetryClass, "ServerBaseURL", "StrProperty", 0x10};

// FString: Zeiger auf UTF-16, dann Anzahl der Zeichen samt Nullzeichen.
std::wstring ReadString(uintptr_t Address)
{
    const auto Data = Memory::ReadPointer(Address);
    const auto Count = Memory::Read<int32_t>(Address + 8);
    if (!Data || !Count || *Count <= 1 || *Count > 2048)
        return {};
    std::wstring Text(static_cast<size_t>(*Count - 1), L'\0');
    if (!Memory::ReadBytes(*Data, Text.data(), Text.size() * sizeof(wchar_t)))
        return {};
    return Text;
}

void BlockTelemetryServer(uintptr_t TelemetrySettings)
{
    static bool Done = false;
    if (Done || !TelemetrySettings)
        return;
    const auto Url = Reflection::Property(ServerBaseUrlField);
    if (!Url)
        return;
    Done = true;
    const std::wstring Host = Core::ReportHosts::HostFromUrl(ReadString(TelemetrySettings + Url->Offset));
    if (Host.empty())
        Core::Log::Info("Telemetrie: keine Server-Adresse eingetragen");
    else
        Core::ReportHosts::Add(Host);
}
}

bool ShouldClear(std::optional<uint8_t> Flag)
{
    return Flag && *Flag != 0;
}

void TickTelemetry()
{
    static ULONGLONG LastCheck = 0;
    const ULONGLONG Now = GetTickCount64();
    if (Now - LastCheck < CheckIntervalMs)
        return;
    LastCheck = Now;

    // Klassen-Defaultobjekte leben so lange wie das Spiel, einmal gefunden bleibt der Zeiger gültig.
    static uintptr_t Found[std::size(Settings)] = {};
    for (size_t Index = 0; Index < std::size(Settings); ++Index)
    {
        const Setting& Entry = Settings[Index];
        if (!Found[Index])
            Found[Index] = Objects::FindObject(Entry.Object, Package);
        const uintptr_t Object = Found[Index];
        if (!Object)
            continue;
        if (Index == 0)
            BlockTelemetryServer(Object);
        // Fehlt das Feld oder passt es nicht, bleibt nur dieser Schalter unberührt; Reflection hat das einmal geloggt.
        const auto Field = Reflection::Property(Entry.Field);
        if (!Field)
            continue;
        const uintptr_t Address = Object + Field->Offset;
        const auto Flag = Memory::Read<uint8_t>(Address);
        if (!ShouldClear(Flag))
            continue;
        const uint8_t Off = 0;
        if (Memory::WriteBytes(Address, &Off, 1))
            Core::Log::Info("Telemetrie: {} abgeschaltet", Entry.Label);
        else
            Core::Log::Error("Telemetrie: {} ließ sich nicht abschalten", Entry.Label);
    }
}
}
