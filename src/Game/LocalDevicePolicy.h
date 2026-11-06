#pragma once

#include <cstdint>

#include "Game/WorldAdapter.h"

// Reine Regeln für eingaben: Lokale Geräte bleiben beim eigenen Spieler (Index 0). Im Netzwerkspiel ist P2 beim Host der
// Remote-Spieler und beim Client gibt es kein P2. Ein lokales Gerät darf dort nicht an P2 gebunden werden, und es darf
// kein zweiter lokaler Spieler entstehen. Ohne Netzwerk (Standalone) bleibt der native lokale Koop unverändert.
namespace Game::Patches
{
inline bool NetworkSession(NetRole Role)
{
    return Role == NetRole::Host || Role == NetRole::Client;
}

// AssignPhysicalDevice und DraftAssignPhysicalDevice: PlayerIndex 0 (eigener Spieler) und -1 (Zuordnung lösen) bleiben.
inline bool BlocksDeviceAssignment(NetRole Role, int32_t PlayerIndex)
{
    return NetworkSession(Role) && PlayerIndex > 0;
}

// SpawnLocalPlayer: Für Index 0 und ohne Netzwerk wie gehabt. Beim Host gibt die native Funktion einen schon eingetragenen
// P2 (den Remote-Spieler) nur zurück und legt nichts an, das bleibt. Sonst würde sie einen zweiten LocalPlayer anlegen.
inline bool BlocksLocalSpawn(NetRole Role, int32_t PlayerIndex, bool HostHasP2Controller)
{
    if (!NetworkSession(Role) || PlayerIndex <= 0)
        return false;
    return !(Role == NetRole::Host && HostHasP2Controller);
}
}
