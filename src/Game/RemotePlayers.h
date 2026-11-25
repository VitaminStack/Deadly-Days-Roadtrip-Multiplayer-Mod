#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "Game/RemoteP2Policy.h"

// Host mit mehreren Clients: welcher Peer des Kontrollkanals welchen Remote-Controller steuert (docs/MEHR_SPIELER.md).
// Net kennt Peers und Tunnel-Ports, Game kennt Controller; Sync gibt Game die Peers je Takt. Nur Game Thread.
namespace Game
{
// Host, jeden Takt vor den Sync-Familien: alle Peers des Kontrollkanals mit dem Port ihres Tunnel-Links.
void SetPeerLinks(std::span<const PeerLink> Links);

// Host: Controller des Peers in der aktuellen Welt, 0 solange er nicht eindeutig zuzuordnen ist (Reise, erste Replikation).
uintptr_t PeerController(uint64_t Peer);

// Host: Peer zum Remote-Controller, 0 wenn unbekannt.
uint64_t ControllerPeer(uintptr_t Controller);

// Host: alle Remote-Controller der aktuellen Welt in der Reihenfolge der PlayerArray, höchstens MaxPlayers - 1.
std::vector<uintptr_t> RemoteControllers();
}
