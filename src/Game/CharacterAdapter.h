#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

// charakter-wahl: Figurenwahl der anderen Spieler. Jede Seite wählt ihre Figur nativ und lokal in der MetaBase. Die native
// Zuordnung kennt aber nur lokale Spieler: Eine Remote-Figur bekommt Index 0 und damit die eigene Wahl dieser Seite.
// Geteilt wird darum nur der Name der eigenen Definition (etwa CD_BananaPerson), zugeordnet über APlayerState::PlayerId,
// die auf allen Seiten gleich repliziert. Die Mod wendet die Wahl mit der nativen Funktion auf die Remote-Figur dieses
// Spielers an. Befund: docs/archive/GAME_ANALYSIS.md#charakterwahl-in-der-metabase-charakter-wahl. Nur Game Thread.
namespace Game::Characters
{
struct PlayerChoice
{
    int32_t PlayerId = 0;
    std::string Name;
    bool operator==(const PlayerChoice&) const = default;
};

// Name der eigenen gewählten Definition (Spielerindex 0). nullopt, solange kein Manager und keine Wahl da ist.
std::optional<std::string> OwnChoice();

// PlayerId aus dem PlayerState des Controllers, nullopt ohne PlayerState oder Feld.
std::optional<int32_t> PlayerIdOf(uintptr_t Controller);

// PlayerId des eigenen, lokalen Spielers dieser Seite.
std::optional<int32_t> LocalPlayerId();

// Wahl der anderen Spieler, ersetzt die vorige. Leer löscht sie, etwa ohne Verbindung.
void SetPeerChoices(std::span<const PlayerChoice> Choices);
}
