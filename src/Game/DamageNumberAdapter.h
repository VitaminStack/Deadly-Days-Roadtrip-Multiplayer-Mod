#pragma once
#include <cstdint>
#include <span>
#include <vector>

// opt-schadenszahlen: Schadenszahlen beim Client. Das Spiel meldet einen Treffer nur beim Host an den Handler des
// DDPlayerController (Schaden, Ziel, Flags), dort entstehen Timer, Pool-Actor und ShowDamageText. Gegner-HP repliziert nicht
// (0/0 beim Client), es gibt also kein natives Signal. Der Host meldet je Handler-Aufruf Ziel (NetGUID), Betrag und Flags,
// der Client ruft damit denselben nativen Handler seines eigenen Controllers auf: eigene Einstellung, Position, Pool und Anzeige
// bleiben lokal. Befund: docs/GAME_ANALYSIS.md#schadenszahlen-beim-client-opt-schadenszahlen. Nur Game Thread.
namespace Game
{
struct DamageNumberHit
{
    uint64_t Guid = 0; // NetGUID des getroffenen Actors (FDamageInformation.DamagedActor)
    float Damage = 0;
    uint8_t Flags = 0; // FDamageInformation.DamageFlags (Crit, Blocked, ...)
    float X = 0, Y = 0, Z = 0; // Ort des Ziels beim Host, für einen späteren Ersatz, wenn das Ziel beim Client schon weg ist
    bool operator==(const DamageNumberHit&) const = default;
};

inline constexpr size_t MaxDamageNumberHits = 64;

// Host: Treffer seit dem letzten Aufruf mit auflösbarer NetGUID, höchstens MaxDamageNumberHits, der Rest verfällt.
std::vector<DamageNumberHit> TakeDamageNumberHits();
// Client: Zahlen über den nativen Handler des eigenen Controllers. Unbekannte oder sterbende Ziele werden übersprungen.
void ShowDamageNumbers(std::span<const DamageNumberHit> Hits);
}
