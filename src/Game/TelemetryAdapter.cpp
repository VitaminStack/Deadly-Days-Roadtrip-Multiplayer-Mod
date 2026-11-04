#include "Game/TelemetryAdapter.h"

#include <windows.h>

#include <iterator>
#include <string>
#include <string_view>

#include "Core/Log.h"
#include "Core/ReportHosts.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"

namespace Game
{
namespace
{
constexpr ULONGLONG CheckIntervalMs = 1000;
constexpr std::string_view Package = "/Script/DDSurvivors";

struct Setting
{
    const char* Object;
    uintptr_t Offset;
    const char* Label;
};

// Der erste Eintrag ist das Telemetrie-Objekt, daraus kommt auch die Server-Adresse.
constexpr Setting Settings[] = {
    {"Default__DDBalanceTelemetrySettings", Offsets::UDDBalanceTelemetrySettings::Enabled, "Balance-Telemetrie"},
    {"Default__DDBalanceTelemetrySettings", Offsets::UDDBalanceTelemetrySettings::AttachSaveFile, "Spielstand an Telemetrie"},
    {"Default__DDCorruptedSaveAutoReportSettings", Offsets::UDDCorruptedSaveAutoReportSettings::EnableAutomaticReports,
     "Automatische Berichte über beschädigte Spielstände"},
};

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
    Done = true;
    const std::wstring Host = Core::ReportHosts::HostFromUrl(ReadString(TelemetrySettings + Offsets::UDDBalanceTelemetrySettings::ServerBaseURL));
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
        const auto Flag = Memory::Read<uint8_t>(Object + Entry.Offset);
        if (!ShouldClear(Flag))
            continue;
        const uint8_t Off = 0;
        if (Memory::WriteBytes(Object + Entry.Offset, &Off, 1))
            Core::Log::Info("Telemetrie: {} abgeschaltet", Entry.Label);
        else
            Core::Log::Error("Telemetrie: {} ließ sich nicht abschalten", Entry.Label);
    }
}
}
