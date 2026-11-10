#pragma once

#include <optional>
#include <string>

// charakter-wahl: Figurenwahl der Gegenseite. Jede Seite wählt ihre Figur nativ und lokal in der MetaBase. Die native
// Zuordnung kennt aber nur lokale Spieler: Eine Remote-Figur bekommt Index 0 und damit die eigene Wahl dieser Seite.
// Geteilt wird darum nur der Name der eigenen Definition (etwa CD_BananaPerson). Die Mod wendet die Wahl der Gegenseite
// mit der nativen Funktion auf die Remote-Figur an. Befund: docs/GAME_ANALYSIS.md#charakterwahl-in-der-metabase-charakter-wahl.
// Nur Game Thread.
namespace Game::Characters
{
// Name der eigenen gewählten Definition (Spielerindex 0). nullopt, solange kein Manager und keine Wahl da ist.
std::optional<std::string> OwnChoice();

// Wahl der Gegenseite. nullopt löscht sie, etwa ohne Verbindung.
void SetPeerChoice(std::optional<std::string> Name);
}
