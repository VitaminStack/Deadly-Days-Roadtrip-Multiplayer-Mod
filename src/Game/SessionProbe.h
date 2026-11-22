#pragma once

#include <optional>

// live-protokoll: lokale Werte für die Zustandszeile im Log. Ohne SDK-Typen, nur Game Thread, nur lesen.
namespace Game::Probe
{
struct Fuel
{
    float Current = 0;
    float Max = 0;
};
// Tank des Missions-GameStates (Gruppenwert), nullopt außerhalb einer Mission.
std::optional<Fuel> ReadFuel();
}
