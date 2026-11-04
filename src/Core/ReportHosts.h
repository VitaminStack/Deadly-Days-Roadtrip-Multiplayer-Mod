#pragma once

#include <string>
#include <string_view>

// Hält Telemetrie und Berichte vom Hersteller fern, solange die Mod läuft. Siehe src/Core/README.md.
namespace Core::ReportHosts
{
// Lässt im eigenen Prozess die Namensauflösung für Berichtsziele scheitern (getaddrinfo, GetAddrInfoW,
// GetAddrInfoExW in ws2_32). Jeder aufgelöste Name wird einmal geloggt. Braucht Hooks::Initialize.
bool Block();

// Sperrt zusätzlich einen Host samt Subdomains, etwa die Telemetrie-Adresse aus den Spieleinstellungen. Threadsicher.
void Add(std::wstring_view Host);

// Ob ein Hostname gesperrt ist: einer der Einträge selbst oder eine Subdomain davon, ohne Groß-/Kleinschreibung.
bool IsBlocked(std::wstring_view Host);

// Host aus einer URL wie https://Host:443/pfad, klein geschrieben. Leer, wenn keiner erkennbar ist.
std::wstring HostFromUrl(std::wstring_view Url);
}
