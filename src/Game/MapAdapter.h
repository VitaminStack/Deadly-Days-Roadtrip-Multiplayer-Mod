#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Gleiche Karte bei Host und Client (missionsstart). Die Karte entsteht beim Client lokal aus Seed, Profil, Größe und Tags,
// die Vegetation aus einem PCG-Graphen. Ohne die Werte des Hosts erzeugt der Client eine andere Karte, und die Figuren
// stehen nicht zusammen oder fallen durch den Boden. Der Host liest die Werte bei seiner Erzeugung, der Kontrollkanal
// bringt sie zum Client, dessen Erzeugung wartet so lange. Alles nur auf dem Game Thread.
namespace Game::Map
{
enum class Graph : uint8_t
{
    Unknown = 0, // Host hat das PCG-Volume noch nicht aufgesetzt
    None = 1,    // Host nimmt keinen Graphen aus dem Biom
    Set = 2,
};

struct HostMap
{
    uint64_t Id = 0; // zufällig je Erzeugung des Hosts, nie 0
    int32_t Seed = 0, MaxTileX = 0, MaxTileY = 0;
    std::string Profile; // "/Game/.../MGP_X.MGP_X"
    std::vector<std::string> Tags;
    Graph GraphState = Graph::Unknown;
    std::string GraphPackage, GraphAsset; // FName-Texte, nur bei Graph::Set
    bool operator==(const HostMap&) const = default;
};

// Host: Karte der aktuellen Welt, nullopt ohne Erzeugung seit dem letzten Kartenwechsel.
std::optional<HostMap> CurrentHostMap();

// Client: Neueste Werte des Hosts, nullopt, wenn der Host gerade keine Karte hat oder die Verbindung fehlt.
void SetHostMap(std::optional<HostMap> Value);

// Rundenende des Hosts (rundenende-client). Der Client beendet seine Runde damit lokal und sieht Rundenübersicht und
// Lager selbst. MapId bindet es an die Karte, die beide gerade spielen, damit kein altes Ende eine neue Mission beendet.
enum class RunOutcome : uint8_t
{
    None = 0, // normales Tagesende, der Run geht weiter
    Lost = 1, // run-ende: alle Spieler tot
    Won = 2,  // run-ende: letzter Tag geschafft
};

struct RoundEnd
{
    uint64_t MapId = 0; // HostMap::Id der Mission, nie 0
    uint64_t Id = 0;    // zufällig je Rundenende, nie 0
    RunOutcome Outcome = RunOutcome::None;
    bool operator==(const RoundEnd&) const = default;
};

// Host: Rundenende in der aktuellen Welt, nullopt ohne.
std::optional<RoundEnd> CurrentRoundEnd();

// Client: Neuestes Rundenende des Hosts, nullopt ohne oder ohne Verbindung.
void SetHostRoundEnd(std::optional<RoundEnd> Value);
}
