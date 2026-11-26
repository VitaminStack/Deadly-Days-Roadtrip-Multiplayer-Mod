#pragma once

#include <cstddef>
#include <optional>

#include "Game/RoundStatsPolicy.h"

// opt-ergebnis-werte: Werte der Rundenübersicht (UDDGameStatsSubsystem) lesen und beim Client durch die des Hosts ersetzen.
// Nur auf dem Game Thread. Regeln in RoundStatsPolicy.h, Ablauf in docs/faelle/opt-ergebnis-werte.md.
namespace Game::RoundStats
{
// Eigene Tageskennung und Werte, gesamt und je Spieler. nullopt, wenn das Subsystem fehlt, die Adressen nicht geprüft sind oder der
// Speicher nicht plausibel ist.
std::optional<Snapshot> ReadLocal();

// Client: Werte und, mit WriteTime, die Tageskennung auf die des Hosts setzen, ohne Benachrichtigungen des Spiels (keine Erfolge,
// Ziele oder Item-Effekte). Liefert die Zahl geschriebener Werte, nullopt, wenn nichts geschrieben werden konnte.
std::optional<size_t> ApplyHost(const Snapshot& Host, bool WriteTime);

// Client: Solange Host gesetzt ist, setzt der Hook auf UDDGameStatsSubsystem::OnRoundChanged die Werte nach der eigenen
// Tagesend-Buchung des Spiels (StopTracking: Dauer, DPS, Leben) noch einmal. nullptr beendet das. Der Zeiger muss so lange gelten.
void HoldForRoundChange(const Snapshot* Host, bool WriteTime = true);
// Wie oft der Hook seit dem letzten Aufruf nachgesetzt hat.
int TakeReapplied();

// Client: Der Host hat den Run in dieser Welt beendet (Sieg oder Niederlage). Bis zum Weltwechsel zeigt die Rundenübersicht dann
// wie beim Host die Summen aller Tage statt des letzten Tags. 0 hebt es auf.
void NoteRunEnded(uintptr_t World);
}
