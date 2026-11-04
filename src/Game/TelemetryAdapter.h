#pragma once

#include <cstdint>
#include <optional>

// Schaltet im Mod-Betrieb die Balance-Telemetrie und die automatischen Berichte über beschädigte Spielstände ab.
// Die Schalter liegen in den Klassen-Defaultobjekten der Einstellungen. Die Telemetrie-Adresse sperrt zusätzlich
// Core::ReportHosts, falls ein Weg die Schalter umgeht.
namespace Game
{
// Reine Entscheidung je Schalter: schreiben, wenn er lesbar ist und noch steht.
bool ShouldClear(std::optional<uint8_t> Flag);

// Game-Thread-Tick, prüft höchstens einmal je Sekunde.
void TickTelemetry();
}
