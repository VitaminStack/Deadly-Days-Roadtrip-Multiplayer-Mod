#pragma once

#include <cstdint>
#include <optional>

#include "Game/RouteMapPolicy.h"

// opt-routenkarte: Run-Info des Hosts für die Routenkarte des Clients. Ablauf und Befund in docs/faelle/opt-routenkarte.md.
// Host: liest Seed, Route und Wahl seiner GameInstance. Client: setzt sie direkt vor dem Nachziehen eines Tagesendes in seine
// GameInstance, baut die Route aus seiner eigenen Missionsstruktur und stellt die eigene Run-Info zurück, sobald die Welt wechselt.
// Nur Game Thread.
namespace Game::RouteMap
{
// Host: eigene Run-Info, ohne MapId. nullopt ohne GameInstance, Felder oder gültige Route (etwa in der MetaBase vor dem Run).
std::optional<HostRoute> ReadHostRoute();

// Client: neueste Run-Info des Hosts, nullopt ohne Verbindung.
void SetHostRoute(std::optional<HostRoute> Value);

// Client, direkt vor FollowHostRoundEnd: Route des Hosts für das Rundenende EndId der Karte EndMapId übernehmen. DayEnd falsch am
// Run-Ende. Danach führt das lokale FinishMission sie nativ weiter wie beim Host.
void ApplyBeforeRoundEnd(uint64_t EndMapId, uint64_t EndId, bool DayEnd);

// Jede Rolle, jeder Takt: eigene Run-Info nach einem Weltwechsel zurückstellen.
void Tick();
}
