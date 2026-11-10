#pragma once
#include <cstdint>
#include <optional>

// Entwickler-Werkzeuge des Overlay-Tabs "Dev". Entfernen vor dem Release (TODO `opt-dev-tab`).
// Nur der Host fuehrt aus; Client-Knoepfe kommen als Anfrage beim Host an. Nur Game Thread.
namespace Game
{
enum class DevAction : uint8_t
{
    SpawnXp = 1, // XP-Baelle (200) um die Figur des Spielers, ueber UExpSubsystem::RequestExpSpawning
    FullTank = 2, // Tank am Missions-GameState auf sein Maximum, repliziert zum Client
    ToggleGodmode = 3, // Unverwundbarkeit der Figur des Spielers umschalten (DDCharacter.DD_ToggleInvincible)
    SpawnZombies = 4, // Gegner am Bodenpunkt unter dem Cursor des Drueckenden (AEnemySpawner::TrySpawnFreshEnemy)
    SpawnRandomItem = 5, // Itemsack mit zufaelligem Item neben der Figur (SpawnSpecificItemLoot)
};
inline constexpr uint8_t LastDevAction = static_cast<uint8_t>(DevAction::SpawnRandomItem);

struct DevPosition { double X, Y, Z; };

// Wahr, wenn die Aktion beim Host ausgefuehrt wurde. Player: 0 Host-Figur, 1 Client-Figur.
// Target: Weltpunkt fuer SpawnZombies, von der drueckenden Seite ueber CursorGroundPosition bestimmt.
bool RunDevAction(DevAction Action, int Player, std::optional<DevPosition> Target = {});

// Lokal auf jeder Seite: Bodenpunkt unter dem Mauszeiger auf Hoehe der eigenen Figur. Merkt sich den letzten
// gueltigen Wert, denn beim Klick im Overlay liegt der Zeiger nicht mehr ueber dem Spielbild. Jeden Takt aufrufen.
std::optional<DevPosition> CursorGroundPosition();
}
