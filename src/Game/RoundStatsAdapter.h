#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "Game/RoundStatsPolicy.h"

// opt-ergebnis-werte: Werte der Rundenübersicht (UDDGameStatsSubsystem) lesen und beim Client durch die des Hosts ersetzen.
// Nur auf dem Game Thread. Regeln in RoundStatsPolicy.h, Ablauf in docs/faelle/opt-ergebnis-werte.md.
namespace Game::RoundStats
{
// Eigene Werte, gesamt und je Spieler. nullopt, wenn das Subsystem fehlt, die Adressen nicht geprüft sind oder der Speicher
// nicht plausibel ist.
std::optional<std::vector<Entry>> ReadLocal();

// Client: Den eigenen Stand auf die Werte des Hosts setzen, ohne Benachrichtigungen des Spiels (keine Erfolge, Ziele oder
// Item-Effekte). Liefert die Zahl geschriebener Werte, nullopt, wenn nichts geschrieben werden konnte.
std::optional<size_t> ApplyHost(const std::vector<Entry>& Host);
}
