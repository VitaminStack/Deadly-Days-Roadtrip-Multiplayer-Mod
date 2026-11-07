#include <doctest.h>
#include <limits>
#include "Sync/MovementState.h"

using namespace Sync::Movement;

TEST_CASE("Dodge-Richtung: zehn Byte, normiert und streng geprüft")
{
    const Game::DashInput Value{-1234, 0.6f, -0.8f};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == DashInputBytes);
    REQUIRE(Decode(Bytes));
    CHECK(*Decode(Bytes) == Value);
    CHECK(*Decode(Encode({7, 0.f, 0.f})) == Game::DashInput{7, 0.f, 0.f}); // Dodge aus dem Stand bleibt (0, 0)
    for (size_t Size = 0; Size < Bytes.size(); ++Size)
        CHECK_FALSE(Decode(std::span(Bytes).first(Size)));
    auto Extra = Bytes;
    Extra.push_back(0);
    CHECK_FALSE(Decode(Extra));
    for (const float Bad : {2.f, -1.5f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
    {
        CHECK(Encode({1, Bad, 0.f}).empty());
        CHECK(Encode({1, 0.f, Bad}).empty());
    }
}
