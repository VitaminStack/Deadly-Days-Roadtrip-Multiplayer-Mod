#pragma once

#include <optional>

#include "Game/ZoneGoalPolicy.h"

// opt-zonenziel-anzeige: Zonenziel beim Client (etwa „Töte die Titanen“). Die Zonen-Komponente der Karte (BlockadeEventComponent)
// gibt es auf beiden Seiten, sie legt ihr Ziel aber nur beim Host an; das Ziel-Objekt gehört zur GameInstance und repliziert
// nicht. Der Host meldet den Stand (0xD00), der Client legt ein eigenes Objekt der Klasse an, die seine Komponente nennt, und
// zeigt es in der nativen Leiste. Gezählt und abgeschlossen wird nur beim Host. Entscheidung in Game/ZoneGoalPolicy.h,
// Befund in docs/faelle/opt-zonenziel-anzeige.md. Nur Game Thread.
namespace Game
{
// Host in einer Welt: Stand des laufenden Ziels, höchstens viermal je Sekunde neu gelesen. Leer als Client, ohne Welt oder
// wenn Subsystem oder Felder fehlen.
std::optional<ZoneGoalView> ReadHostZoneGoal();
// Client: neuester Stand des Hosts. Gilt für die aktuelle Welt, ein Weltwechsel verwirft ihn.
void SetHostZoneGoal(const ZoneGoalView& View);
// Client, jeder Takt (prüft selbst zweimal je Sekunde): Leiste anlegen, Wert setzen oder entfernen.
void TickZoneGoalDisplay();
// Ohne Verbindung: gemeldeten Stand verwerfen und eine eigene Leiste entfernen.
void ResetZoneGoal();
}
