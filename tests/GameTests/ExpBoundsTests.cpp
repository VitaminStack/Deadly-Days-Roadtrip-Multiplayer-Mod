#include <doctest.h>
#include <limits>
#include "Game/ExpBounds.h"

using namespace Game::ExpVisuals;

TEST_CASE("XP bounds use the full camera footprint, not the client fallback strip")
{
    const std::array<Vector, 4> Corners{{{-3000, -2000, 0}, {3000, -2000, 0}, {-3500, 4000, 0}, {3500, 4000, 0}}};
    const auto Bounds = ViewBounds(Corners, {0, 1000, 0}, {1000, 1000});
    REQUIRE(Bounds);
    CHECK((*Bounds)[0] == 4500);
    CHECK((*Bounds)[1] == 4000);
    CHECK((*Bounds)[2] == 0);
    auto Moved = Corners;
    for (auto& Corner : Moved)
    {
        Corner[0] += 10000;
        Corner[1] -= 20000;
    }
    CHECK(ViewBounds(Moved, {10000, -19000, 0}, {1000, 1000}) == Bounds);
}

TEST_CASE("XP bounds reject missing and invalid camera projections")
{
    std::array<Vector, 4> Corners{};
    CHECK_FALSE(ViewBounds(Corners, {}, {1000, 1000}));
    Corners = {{{-1, -1, 0}, {1, -1, 0}, {-1, 1, 0}, {1, 1, 0}}};
    CHECK_FALSE(ViewBounds(Corners, {}, {-1, 1000}));
    CHECK_FALSE(ViewBounds(Corners, {}, {1e8, 1000}));
    Corners[0][0] = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(ViewBounds(Corners, {}, {1000, 1000}));
}
