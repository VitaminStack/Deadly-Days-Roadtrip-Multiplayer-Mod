#include "Core/Protection.h"

#include "Core/CrashReports.h"
#include "Core/Log.h"
#include "Core/ReportHosts.h"

namespace Core::Protection
{
bool InstallAll(std::span<const Step> Steps)
{
    for (const Step& Entry : Steps)
    {
        if (Entry.Install && Entry.Install())
            continue;
        Log::Error("Schutz: {} nicht aktiv, die Mod startet nicht", Entry.Name);
        return false;
    }
    return true;
}

bool Install()
{
    static constexpr Step Steps[] = {
        // Vor allem anderen: Abstürze mit Mod dürfen nie beim Hersteller landen.
        {"Sperre des Crash-Reporters", &CrashReports::Block},
        // Ebenso Telemetrie, Bug-Reports und automatische Berichte: Mod-Fehler sollen nie als Spielfehler gemeldet werden.
        {"Sperre der Berichtsziele", &ReportHosts::Block},
    };
    return InstallAll(Steps);
}
}
