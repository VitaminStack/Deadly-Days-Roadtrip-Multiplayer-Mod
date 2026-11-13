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
// Bundled: Die Nachricht kam erst im Sammelabruf alle 250 ms an (nach dem eigenen Abruf im selben Takt), nur für die Zähler.
void ShowDamageNumbers(std::span<const DamageNumberHit> Hits, bool Bundled = false);

// opt-schadenszahlen-verlust: Zähler des Wegs zwischen Handler und Anzeige, damit „gesendet“ und „empfangen“ vergleichbar sind.
// Treffer, außer bei DecodeFailed und RevisionGap (Nachrichten). Nur Diagnose, ändert nichts am Ablauf.
enum class DamageNumberNote : uint8_t
{
    Sent,         // Host: SendControl hat die Nachricht angenommen
    SendFailed,   // Host: SendControl hat abgelehnt (Warteschlange voll oder Verbindung weg)
    NoPeer,       // Host: kein bestätigter Mitspieler
    EncodeFailed, // Host: Liste ungültig, ganzes Paket verworfen
    DecodeFailed, // Client: Nachrichten, deren Nutzlast nicht gelesen werden konnte
    RevisionGap,  // Client: fehlende Nachrichten laut Revision des Hosts
};
void NoteDamageNumbers(DamageNumberNote Note, size_t Count);
}
