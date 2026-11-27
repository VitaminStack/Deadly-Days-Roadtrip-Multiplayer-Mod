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

// busplatz-vor-figur: Den Busplatz in seiner Erzeugung (eigene Figur und Bus an den Busort, Freiräumung) setzt der Client nur,
// wenn seine eigene Figur da ist, sobald dieser Schritt läuft. Sonst fällt der ganze Busplatz aus, die Figur bleibt am Startpunkt
// und fällt im Labyrinth endlos. Beim Wiederbeitritt sind die Werte des Hosts oft vor ihr da (gemessen bis 860 ms). Darum
// wartet der Client vor dem Start auf sie, höchstens so lange ab dem Aufruf von StartMapGeneration; danach erzeugt er wie bisher.
inline constexpr uint64_t ClientFigureWaitMs = 10000;

inline bool WaitForOwnFigure(bool FigureReady, uint64_t WaitedMs)
{
    return !FigureReady && WaitedMs < ClientFigureWaitMs;
}

// Client: welche Erzeugung des Hosts schon verbaut und welches Rundenende schon nachgezogen ist. Jede Erzeugung nur einmal
// übernehmen, sonst baute der Client nach einer Reise die alte Karte, bevor die neuen Werte da sind. Nach einem Verbindungswechsel
// (wiedereintritt-karte) gilt das nicht: Der Client tritt derselben laufenden Mission in einer neuen Welt bei, und der Host
// schickt dieselbe Id noch einmal. Ohne Zurücksetzen wartete der Client 60 s und baute eine eigene Karte, die Figur fiel durch.
struct ClientMapLedger
{
    uint64_t UsedId = 0;
    uint64_t Uses = 0;       // zählt jede Übernahme, auch dieselbe Id nach einem Wiederbeitritt
    bool Reconnected = false; // Verbindung seit der letzten Übernahme gewechselt
    uint64_t FollowedEnd = 0, FollowedUse = 0;

    // Retry: das Spiel versucht die Erzeugung in derselben Welt mit demselben Generator neu (StartMapGenerationWithRetryAttempts).
    bool Fresh(uint64_t ReceivedId, bool Retry) const
    {
        return ReceivedId != UsedId || Reconnected || Retry;
    }
    void Use(uint64_t Id)
    {
        UsedId = Id;
        ++Uses;
        Reconnected = false;
    }
    // InUsedWorld: der Client steht beim Wechsel noch in der Welt der verbauten Karte. Ein kurzer Ausfall des Kontrollkanals in der
    // Mission gibt die Id nicht frei, sonst baute die nächste Reise die alte Karte. Beim echten Wiederbeitritt kommt die neue
    // Verbindung im Hauptmenü oder beim Laden zustande, also außerhalb dieser Welt.
    void ConnectionChanged(bool InUsedWorld)
    {
        if (!InUsedWorld)
            Reconnected = true;
    }
    // Ein Rundenende gilt nur für die Karte, auf der es nachgezogen wurde. Nach einem Wiederbeitritt mit neu gebauter Karte
    // kommt dasselbe Ende des Hosts noch einmal an und muss die Rundenübersicht wieder öffnen.
    bool Followed(uint64_t EndId) const
    {
        return EndId == FollowedEnd && FollowedUse == Uses;
    }
    void Follow(uint64_t EndId)
    {
        FollowedEnd = EndId;
        FollowedUse = Uses;
    }
};
}
