#pragma once

#include <cstdint>

// Entscheidungen der Kartenpatches ohne Spielzugriff, damit sie sich testen lassen (missionsstart).
namespace Game::Patches
{
// Ohne Werte des Hosts so lange warten, danach erzeugt der Client eine eigene Karte statt ewig zu laden. Das passiert,
// wenn der Host ohne Mod oder ohne Kontrollkanal spielt, etwa über den Tunnel des Spikes.
inline constexpr uint64_t HostMapWaitMs = 60000;
// Sind die Kartenwerte da, aber noch nicht der PCG-Graph, kurz auf ihn warten. Der Client setzt den Graphen erst später
// in seiner Erzeugung ein, ein Nachzügler kommt meist noch rechtzeitig.
inline constexpr uint64_t HostGraphWaitMs = 5000;

enum class MapStart
{
    Wait,
    HostValues, // mit den Werten des Hosts erzeugen
    Own,        // ohne Werte erzeugen, wie ohne Mod
};

// Client: HasValues heißt neue, noch nicht verbrauchte Werte des Hosts. WaitedMs zählt ab dem Aufruf von
// StartMapGeneration, ValuesForMs ab dem Eintreffen dieser Werte.
inline MapStart ClientMapStart(bool HasValues, bool GraphKnown, uint64_t WaitedMs, uint64_t ValuesForMs)
{
    if (HasValues)
        return GraphKnown || ValuesForMs >= HostGraphWaitMs ? MapStart::HostValues : MapStart::Wait;
    return WaitedMs >= HostMapWaitMs ? MapStart::Own : MapStart::Wait;
}
}
