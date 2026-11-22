#pragma once

#include <cstdint>
#include <optional>

// opt-bedrohung-sound: Bedrohungsstufe beim Client. Die Stufe lebt in der Komponente ThreatLevel des GameState, hat kein
// repliziertes Feld (beim Client -1) und ihr Delegate OnThreatLevelChanged feuert nur beim Host. Darum bleibt die Zahl unter der
// Stoppuhr beim Client auf 0 und `UI_ThreatIncrease` aus. Der Host meldet die Stufe (0xC00), der Client ruft damit dasselbe
// Ereignis seiner eigenen Stoppuhr (WBP_Stopwatch_C) auf: Zahl, Animation und Sound bleiben lokal und nativ.
// Befund: docs/GAME_ANALYSIS.md#bedrohungsstufe-beim-client-opt-bedrohung-sound. Nur Game Thread.
namespace Game
{
// Host in einer Mission: aktuelle Stufe aus ThreatLevel.GetCurrentThreatLevel, höchstens viermal je Sekunde neu gelesen.
// Leer ohne Mission, als Client oder wenn die Komponente fehlt.
std::optional<int32_t> ReadHostThreatLevel();
// Client: gemeldete Stufe an der eigenen Stoppuhr zeigen. Ohne Stoppuhr passiert nichts, der Host wiederholt den Stand.
void ShowThreatLevel(int32_t Level);
}
