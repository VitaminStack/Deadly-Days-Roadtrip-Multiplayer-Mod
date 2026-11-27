#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Game/PowerUpPolicy.h"

// opt-powerups-client: Power-up-Slots im Coop (Regeln in Game/PowerUpPolicy.h, Befund in docs/faelle/opt-powerups-client.md).
// Der Host füllt und löst die Slots aller Figuren aus und stellt Power-up-Fähigkeiten vor dem Auslösen auf ServerOnly. Der Client
// bucht keine Power-ups (InventoryPatches.cpp), zeigt den Slot-Stand des Hosts und meldet das Auslösen eigener Slots.
// Format der Nachrichten in Sync/Players/PowerUpState.h. Nur Game Thread.
namespace Game
{
// Host: verbrauchbare Slots der Figur, die Controller steuert, in der Reihenfolge des Inventars. Leer ohne Figur oder Inventar.
std::optional<std::vector<PowerUp::Slot>> ReadPowerUps(uintptr_t Controller);
// Host: den ersten verbrauchbaren Slot mit dieser Definition an der Figur von Controller auslösen, wie die Taste des Spielers
// (UPowerupSlot::TryActivateSlot). false, wenn kein solcher Slot da ist.
bool ActivatePowerUp(uintptr_t Controller, std::string_view Definition);
// Client: seit dem letzten Aufruf lokal ausgelöste verbrauchbare Slots der eigenen Figur, je Auslösen ein Definitionsname.
std::vector<std::string> TakePowerUpActivations();
// Client: eigene verbrauchbare Slots an den Stand des Hosts angleichen (PowerUp::Reconcile). false ohne eigenes Inventar.
bool ApplyHostPowerUps(const std::vector<PowerUp::Slot>& Host, const std::vector<std::string>& Pending);
// Aktuelle Welt, 0 ohne Welt. Ein Wechsel beginnt den Abgleich neu.
uintptr_t PowerUpWorld();
}
