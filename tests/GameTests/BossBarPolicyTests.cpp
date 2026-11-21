#include <doctest.h>

#include "Game/BossBarPolicy.h"

namespace
{
Game::BossBarState Fight()
{
    Game::BossBarState State;
    State.Client = true;
    State.LiveBoss = true;
    State.Health = 49135;
    State.MaxHealth = 80000;
    return State;
}
}

TEST_CASE("Boss bar is only built on the client")
{
    auto State = Fight();
    State.Client = false;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::None);
}

TEST_CASE("Boss bar is created for a live boss and not without one")
{
    auto State = Fight();
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Create);
    State.LiveBoss = false;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::None);
}

TEST_CASE("Boss bar waits before the first value, then syncs")
{
    auto State = Fight();
    State.HasObjective = true;
    State.Shown = true;
    for (int Ticks = 0; Ticks < Game::BossBarSettleTicks; ++Ticks)
    {
        State.SettleTicks = Ticks;
        CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Settle);
    }
    State.SettleTicks = Game::BossBarSettleTicks;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Sync);
}

TEST_CASE("Boss bar is removed when the boss is gone or dead, never set below one")
{
    auto State = Fight();
    State.HasObjective = true;
    State.Shown = true;
    State.SettleTicks = Game::BossBarSettleTicks;
    State.LiveBoss = false;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Remove);
    State.LiveBoss = true;
    State.Health = 0;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Remove);
    State.Health = 49135;
    State.MaxHealth = 0;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Remove);
    State.Shown = false;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::None);
}

TEST_CASE("Boss bar updates only on changed values")
{
    CHECK_FALSE(Game::BossBarNeedsUpdate(100, 200, 100, 200));
    CHECK(Game::BossBarNeedsUpdate(100, 200, 99, 200));
    CHECK(Game::BossBarNeedsUpdate(100, 200, 100, 300));
    CHECK(Game::BossBarNeedsUpdate(-1, -1, 100, 200));
}

TEST_CASE("Boss bar keeps the tracked boss and prefers a damaged one")
{
    const std::vector<Game::BossCandidate> Bosses{{0x100, 5000000, 5000000}, {0x200, 197363, 300000}};
    CHECK(Game::PickBoss(Bosses, 0) == 1);
    CHECK(Game::PickBoss(Bosses, 0x100) == 0);
    CHECK(Game::PickBoss(Bosses, 0x300) == Game::NoBoss);
    CHECK(Game::PickBoss({{0x100, 5000000, 5000000}}, 0) == 0);
    CHECK(Game::PickBoss({}, 0) == Game::NoBoss);
    // LazyBoss bleibt bei 0 HP stehen (18.11.2026): keine neue Leiste für ihn, ein lebender zweiter Boss bekommt sie.
    CHECK(Game::PickBoss({{0x100, 0, 370000}}, 0) == Game::NoBoss);
    CHECK(Game::PickBoss({{0x100, 0, 370000}, {0x200, 250000, 250000}}, 0) == 1);
}

// Reise von einer Boss-Karte in die nächste (boss-karte-absturz, gemessen 21.11.2026): Der Boss der alten Karte ist weg, der neue
// schon repliziert. Erst wird die alte Leiste entfernt, einen Takt später entsteht genau eine neue, danach kein weiteres Anlegen.
TEST_CASE("Boss bar after travel removes the old bar first, then creates exactly one")
{
    const std::vector<Game::BossCandidate> NewMap{{0x200, 80000, 80000}};
    CHECK(Game::PickBoss(NewMap, 0x100) == Game::NoBoss);
    Game::BossBarState State;
    State.Client = true;
    State.HasObjective = true;
    State.Shown = true;
    State.SettleTicks = Game::BossBarSettleTicks;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Remove);

    const size_t Picked = Game::PickBoss(NewMap, 0);
    REQUIRE(Picked == 0);
    State = Fight();
    State.Health = NewMap[Picked].Health;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Create);
    State.HasObjective = true;
    State.Shown = true;
    CHECK(Game::PickBoss(NewMap, 0x200) == 0);
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Settle);
    State.SettleTicks = Game::BossBarSettleTicks;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Sync);
}
