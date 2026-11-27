#include <doctest.h>
#include <vector>
#include "Sync/Players/PowerUpState.h"

using namespace Sync::PowerUps;

TEST_CASE("Power-ups: Slot-Stand verlustfrei")
{
    for (const State& Value : {State{}, State{7, {{"POD_Vacuum", 1}}}, State{0x01020304, {{"POD_Vacuum", 1}, {"POD_Explosive", 3}}}})
    {
        const auto Payload = Encode(Value);
        REQUIRE_FALSE(Payload.empty());
        CHECK(DecodeState(Payload) == Value);
    }
    CHECK(Encode(State{2, {{"AB", 0x0102}}}) == std::vector<uint8_t>{2, 0, 0, 0, 1, 0x02, 0x01, 2, 'A', 'B'});
    CHECK(Encode(State{}) == std::vector<uint8_t>{0, 0, 0, 0, 0});
}

TEST_CASE("Power-ups: ungültige Stände und Pakete abgelehnt")
{
    CHECK(Encode(State{0, {{"", 1}}}).empty());
    CHECK(Encode(State{0, {{"POD Vacuum", 1}}}).empty());
    CHECK(Encode(State{0, std::vector<Game::PowerUp::Slot>(Game::PowerUp::MaxSlots + 1, {"POD_Vacuum", 1})}).empty());
    CHECK_FALSE(DecodeState(std::vector<uint8_t>{}));
    CHECK_FALSE(DecodeState(std::vector<uint8_t>{0, 0, 0, 0}));                         // ohne Anzahl
    CHECK_FALSE(DecodeState(std::vector<uint8_t>{0, 0, 0, 0, 1, 1, 0, 2, 'A'}));        // Name zu kurz
    CHECK_FALSE(DecodeState(std::vector<uint8_t>{0, 0, 0, 0, 0, 9}));                   // zu lang
    CHECK_FALSE(DecodeState(std::vector<uint8_t>{0, 0, 0, 0, 1, 1, 0, 2, 'A', ' '}));   // ungültiges Zeichen
    CHECK_FALSE(DecodeState(std::vector<uint8_t>{0, 0, 0, 0, 9}));                      // mehr als MaxSlots
}

TEST_CASE("Power-ups: Auslösen verlustfrei, Nummer ab 1")
{
    const Activation Value{5, "POD_Stamina_Energy"};
    const auto Payload = Encode(Value);
    REQUIRE(Payload.size() == 4 + 1 + Value.Definition.size());
    CHECK(DecodeActivation(Payload) == Value);
    CHECK(Encode(Activation{0, "POD_Vacuum"}).empty());
    CHECK(Encode(Activation{1, ""}).empty());
    CHECK_FALSE(DecodeActivation(std::vector<uint8_t>{0, 0, 0, 0, 1, 'A'}));  // Nummer 0
    CHECK_FALSE(DecodeActivation(std::vector<uint8_t>{1, 0, 0, 0, 2, 'A'}));  // Name zu kurz
    CHECK_FALSE(DecodeActivation(std::vector<uint8_t>{1, 0, 0, 0, 1, 'A', 'B'})); // zu lang
}
