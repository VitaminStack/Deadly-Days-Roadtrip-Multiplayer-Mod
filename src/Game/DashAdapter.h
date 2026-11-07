#pragma once
#include <cstdint>
#include <vector>

// Laufrichtung des Clients für seinen Dodge (GA_Dash) beim Host, Befund in docs/GAME_ANALYSIS.md, Abschnitt
// `dodge-client-live`. Die Engine repliziert die Eingabe nicht; der Host sah die Client-Figur ohne Tempo und Beschleunigung.
// Der Client meldet je eigener Aktivierung die Eingabe, die sein GA_Dash liest. Zugeordnet wird über den
// Vorhersageschlüssel der Aktivierung, den Client und Host gleich sehen. Nur Game Thread.
namespace Game
{
struct DashInput
{
    int16_t Key = 0;
    float X = 0, Y = 0;
    bool operator==(const DashInput&) const = default;
};

// Client: seit dem letzten Aufruf gestartete eigene Dodges mit ihrer Eingabe, höchstens 16, ältere verfallen.
std::vector<DashInput> TakeDashInputs();
// Host: Eingabe des Clients für seine Aktivierung. Wartet die Aktivierung schon, läuft sie im nächsten Tick weiter.
void ReceiveDashInput(const DashInput& Value);
}
