#include <doctest.h>

#include "Game/TelemetryAdapter.h"

TEST_CASE("Telemetrie-Schalter werden nur geschrieben, wenn sie lesbar sind und stehen")
{
    CHECK_FALSE(Game::ShouldClear(std::nullopt));
    CHECK_FALSE(Game::ShouldClear(uint8_t{0}));
    CHECK(Game::ShouldClear(uint8_t{1}));
    CHECK(Game::ShouldClear(uint8_t{0xFF}));
}
