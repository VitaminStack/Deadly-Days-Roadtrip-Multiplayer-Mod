#pragma once

#include <cstdint>

// busplatz-ohne-boden, Client: Boden unter der eigenen Figur am Busplatz, so wie ihre Bewegungskomponente ihn prüft.
// Fehlt er dort, hält die Mod die Figur am Busort, bis er da ist. Regeln ohne Spiel, getestet in tests/GameTests.
namespace Game::Patches
{
constexpr double BusFloorProbeAbove = 150;  // Prüfung beginnt so weit über dem Busort, unter Dach und Ästen
constexpr double BusFloorProbeBelow = 3000; // und reicht so weit darunter
constexpr double BusFloorMaxDrop = 300;     // ein Treffer tiefer unter dem Busort ist nicht der Boden am Bus
constexpr uint64_t BusFloorHoldLimitMs = 5000; // länger nie halten, auch wenn dort nie Boden kommt
constexpr uint64_t BusFloorTraceMs = 3000;     // Messzeilen je Frame höchstens so lange nach dem Busplatz
constexpr int BusFloorSteadyFrames = 3;        // Messung endet, wenn die Figur so viele Frames auf Boden geht

// Boden am Bus: ein Treffer nicht weit unter dem Busort.
inline bool FloorAtBus(bool Hit, double BusZ, double ImpactZ)
{
    return Hit && ImpactZ >= BusZ - BusFloorMaxDrop && ImpactZ <= BusZ + BusFloorProbeAbove;
}

enum class BusHold
{
    Done,    // Boden da, ohne gehalten zu haben: nichts tun
    Hold,    // kein Boden: Figur am Busort halten
    Release, // Boden kam, während die Figur gehalten wurde: an den Busort setzen und loslassen
    GiveUp,  // Grenze erreicht ohne Boden: loslassen wie ohne Mod
};

// Holding: die Figur wird schon gehalten. SinceBusMs: Zeit seit dem Busplatz des Clients.
inline BusHold NextBusHold(bool Holding, bool Floor, uint64_t SinceBusMs)
{
    if (Floor)
        return Holding ? BusHold::Release : BusHold::Done;
    if (SinceBusMs >= BusFloorHoldLimitMs)
        return Holding ? BusHold::GiveUp : BusHold::Done;
    return BusHold::Hold;
}

// Messung je Frame: läuft, bis die Figur einige Frames auf Boden geht (Modus 1), oder bis zur Zeitgrenze.
inline bool BusFloorTraceOver(int SteadyFrames, uint64_t SinceBusMs)
{
    return SteadyFrames >= BusFloorSteadyFrames || SinceBusMs >= BusFloorTraceMs;
}
}
