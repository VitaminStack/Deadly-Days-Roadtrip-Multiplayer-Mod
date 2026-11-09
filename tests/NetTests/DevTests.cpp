#include <doctest.h>

#include <cmath>
#include <limits>
#include "Sync/DevState.h"

TEST_CASE("Dev requests carry a cursor target only for SpawnZombies")
{
    using Game::DevAction;
    const auto Plain = Sync::Dev::Encode({DevAction::SpawnRandomItem, std::nullopt});
    CHECK(Plain.size() == 1);
    const auto PlainBack = Sync::Dev::Decode(Plain);
    REQUIRE(PlainBack);
    CHECK(PlainBack->Action == DevAction::SpawnRandomItem);
    CHECK_FALSE(PlainBack->Target);

    const auto Zombies = Sync::Dev::Encode({DevAction::SpawnZombies, Game::DevPosition{100.5, -20, 3}});
    CHECK(Zombies.size() == 25);
    const auto ZombiesBack = Sync::Dev::Decode(Zombies);
    REQUIRE(ZombiesBack);
    REQUIRE(ZombiesBack->Target);
    CHECK(ZombiesBack->Target->X == 100.5);
    CHECK(ZombiesBack->Target->Y == -20);

    // Ohne Ziel, mit Ziel bei anderen Aktionen, unbekannte Aktion und kaputte Werte lehnt der Host ab.
    CHECK_FALSE(Sync::Dev::Decode(std::vector<uint8_t>{4}));
    CHECK_FALSE(Sync::Dev::Decode(Sync::Dev::Encode({DevAction::SpawnXp, Game::DevPosition{1, 2, 3}})));
    CHECK_FALSE(Sync::Dev::Decode(std::vector<uint8_t>{6}));
    CHECK_FALSE(Sync::Dev::Decode(std::vector<uint8_t>{}));
    const auto Bad = Sync::Dev::Encode({DevAction::SpawnZombies, Game::DevPosition{std::numeric_limits<double>::quiet_NaN(), 0, 0}});
    CHECK_FALSE(Sync::Dev::Decode(Bad));
}
