#include <doctest.h>

#include "Sync/Players/CharacterState.h"

using namespace Sync::Character;

TEST_CASE("Charakterwahl: Liste mit PlayerId und Name hin und zurück")
{
    const PlayerChoice Choices[] = {{256, "CD_BananaPerson"}, {-7, "CD_Default_1"}};
    const auto Payload = Encode(Choices);
    REQUIRE_FALSE(Payload.empty());
    const auto Back = Decode(Payload);
    REQUIRE(Back);
    REQUIRE(Back->size() == 2);
    CHECK((*Back)[0] == Choices[0]);
    CHECK((*Back)[1] == Choices[1]);
}

TEST_CASE("Charakterwahl: ungültige Listen und Namen werden abgelehnt")
{
    CHECK(Encode(std::span<const PlayerChoice>{}).empty());
    const PlayerChoice Bad[] = {{1, "CD Space"}};
    CHECK(Encode(Bad).empty());
    const PlayerChoice TooMany[] = {{1, "A"}, {2, "B"}, {3, "C"}, {4, "D"}, {5, "E"}};
    CHECK(Encode(TooMany).empty());
    const PlayerChoice Four[] = {{1, "A"}, {2, "B"}, {3, "C"}, {4, "D"}};
    CHECK_FALSE(Encode(Four).empty());

    CHECK_FALSE(Decode({}));
    auto Payload = Encode(Four);
    Payload.pop_back(); // Name abgeschnitten
    CHECK_FALSE(Decode(Payload));
    const uint8_t Short[] = {1, 0, 0}; // Kopf unvollständig
    CHECK_FALSE(Decode(Short));
    const uint8_t Empty[] = {1, 0, 0, 0, 0}; // leerer Name
    CHECK_FALSE(Decode(Empty));
    // Format vor ModVersion 17: nur der Name. Wird nicht als Liste gelesen.
    const std::string Old = "CD_BananaPerson";
    CHECK_FALSE(Decode(std::span(reinterpret_cast<const uint8_t*>(Old.data()), Old.size())));
}
