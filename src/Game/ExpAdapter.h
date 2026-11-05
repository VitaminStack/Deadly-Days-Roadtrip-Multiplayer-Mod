#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace Game
{
// Ein nativer XP-Ausstoss des Hosts, Felder der Spawn-Anfrage an UExpSubsystem (Build 26187885, 0x54 Bytes):
// Menge, Start- und Zielort, Farbe und zwei Zusatzwerte, deren Bedeutung offen ist. Bitgenau uebertragen.
struct ExpSpawn
{
    int32_t Amount = 0;
    double Start[3]{}, End[3]{};
    float Color[4]{};
    double Extra = 0;
    float Tail = 0;
    bool operator==(const ExpSpawn&) const = default;
};

// Endliche Zahlen, positive Menge, Orte innerhalb der Karte. Gilt beim Aufzeichnen und Nachspielen.
bool ValidExpSpawn(const ExpSpawn& Value);

// Host: seit dem letzten Aufruf aufgezeichnete Ausstoesse, hoechstens Limit Stueck. Aeltere Ueberzaehlige verfallen.
std::vector<ExpSpawn> TakeExpSpawns(size_t Limit);
// Client, nur Game Thread: dieselben Ausstoesse rein zur Anzeige nachspielen. Aufnahme beim Client zaehlt nie,
// ReceiveExp bleibt dort gesperrt. Liefert die Zahl der nachgespielten Ausstoesse.
unsigned ReplayExpSpawns(std::span<const ExpSpawn> Values);
}
