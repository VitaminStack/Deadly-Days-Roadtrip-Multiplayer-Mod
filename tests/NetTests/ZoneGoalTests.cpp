#include <doctest.h>
#include <vector>
#include "Sync/World/ZoneGoalState.h"

using namespace Sync::ZoneGoal;
using Game::ZoneGoalView;

TEST_CASE("Zonenziel: genau 9 Byte, verlustfrei")
{
    for (const ZoneGoalView View : {ZoneGoalView{}, ZoneGoalView{true, 0, 10}, ZoneGoalView{true, 8, 10}, ZoneGoalView{true, 10, 10},
                                    ZoneGoalView{true, 70000, Game::ZoneGoalMax}})
    {
        const auto Payload = Encode(View);
        REQUIRE(Payload.size() == Bytes);
        CHECK(Decode(Payload) == View);
    }
    CHECK(Encode({true, 8, 10}) == std::vector<uint8_t>{1, 8, 0, 0, 0, 10, 0, 0, 0});
    CHECK(Encode({}) == std::vector<uint8_t>(Bytes, 0));
    CHECK(Encode({true, 0x1234, 0x56789}) == std::vector<uint8_t>{1, 0x34, 0x12, 0, 0, 0x89, 0x67, 0x05, 0});
}

TEST_CASE("Zonenziel: ungültige Stände und Pakete abgelehnt")
{
    CHECK(Encode({true, 0, 0}).empty());
    CHECK(Encode({true, -1, 10}).empty());
    CHECK(Encode({false, 3, 10}).empty());
    CHECK(Encode({true, 1, Game::ZoneGoalMax + 1}).empty());
    CHECK_FALSE(Decode(std::vector<uint8_t>{}));
    CHECK_FALSE(Decode(std::vector<uint8_t>{1, 8, 0, 0, 0, 10, 0, 0}));       // zu kurz
    CHECK_FALSE(Decode(std::vector<uint8_t>{1, 8, 0, 0, 0, 10, 0, 0, 0, 0})); // zu lang
    CHECK_FALSE(Decode(std::vector<uint8_t>{2, 8, 0, 0, 0, 10, 0, 0, 0}));    // unbekannte Marke
    CHECK_FALSE(Decode(std::vector<uint8_t>{1, 8, 0, 0, 0, 0, 0, 0, 0}));     // läuft ohne Soll
    CHECK_FALSE(Decode(std::vector<uint8_t>{0, 8, 0, 0, 0, 10, 0, 0, 0}));    // kein Ziel mit Werten
    CHECK_FALSE(Decode(std::vector<uint8_t>{1, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF})); // Soll über dem Bereich
}
