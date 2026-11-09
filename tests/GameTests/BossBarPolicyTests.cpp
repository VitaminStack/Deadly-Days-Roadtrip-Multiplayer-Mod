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
