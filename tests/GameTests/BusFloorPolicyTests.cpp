#include <doctest.h>

#include "Game/BusFloorPolicy.h"

using namespace Game::Patches;

TEST_CASE("Boden am Bus zählt nur nahe unter dem Busort")
{
    CHECK(FloorAtBus(true, 50, 0));
    CHECK(FloorAtBus(true, 50, 50 - BusFloorMaxDrop));
    CHECK_FALSE(FloorAtBus(true, 50, 49 - BusFloorMaxDrop)); // tiefere Ebene, nicht der Boden am Bus
    CHECK_FALSE(FloorAtBus(false, 50, 0));
    CHECK_FALSE(FloorAtBus(true, 50, 51 + BusFloorProbeAbove));
}

TEST_CASE("Halten am Bus nur ohne Boden und höchstens bis zur Grenze")
{
    // Boden schon beim Busplatz: nie halten, Verhalten wie ohne Mod.
    CHECK(NextBusHold(false, true, 0) == BusHold::Done);
    CHECK(NextBusHold(false, false, 0) == BusHold::Hold);
    CHECK(NextBusHold(true, false, BusFloorHoldLimitMs - 1) == BusHold::Hold);
    CHECK(NextBusHold(true, true, 120) == BusHold::Release);
    // Ohne Boden nie länger als die Grenze.
    CHECK(NextBusHold(true, false, BusFloorHoldLimitMs) == BusHold::GiveUp);
    CHECK(NextBusHold(false, false, BusFloorHoldLimitMs) == BusHold::Done);
}

TEST_CASE("Messung am Bus endet nach stetem Gehen oder an der Zeitgrenze")
{
    CHECK_FALSE(BusFloorTraceOver(0, 0));
    CHECK_FALSE(BusFloorTraceOver(BusFloorSteadyFrames - 1, BusFloorTraceMs - 1));
    CHECK(BusFloorTraceOver(BusFloorSteadyFrames, 10));
    CHECK(BusFloorTraceOver(0, BusFloorTraceMs));
}
