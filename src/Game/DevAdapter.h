#pragma once
#include <cstdint>

// Entwickler-Werkzeuge des Overlay-Tabs "Dev". Entfernen vor dem Release (TODO `opt-dev-tab`).
// Nur der Host fuehrt aus; Client-Knoepfe kommen als Anfrage beim Host an. Nur Game Thread.
namespace Game
{
enum class DevAction : uint8_t
{
    SpawnXp = 1, // XP-Baelle (200) um die Figur des Spielers, ueber UExpSubsystem::RequestExpSpawning
    FullTank = 2, // Tank am Missions-GameState auf sein Maximum, repliziert zum Client
    ToggleGodmode = 3, // Unverwundbarkeit der Figur des Spielers umschalten (DDCharacter.DD_ToggleInvincible)
};

// Wahr, wenn die Aktion beim Host ausgefuehrt wurde. Player: 0 Host-Figur, 1 Client-Figur.
bool RunDevAction(DevAction Action, int Player);
}
