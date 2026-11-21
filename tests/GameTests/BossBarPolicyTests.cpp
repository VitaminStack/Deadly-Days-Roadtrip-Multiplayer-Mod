#include <doctest.h>

#include <limits>
#include <vector>

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

// Phasenbezogene Leiste (opt-boss-phasenleiste). Werte des Hosts vom 21.11.2026: Ballerina 299.775 HP zeigt 149.775, bei 298.650 HP
// steht 148.650/150.000 über der Leiste.
TEST_CASE("Boss bar shows the health above the phase floor like the host")
{
    const auto First = Game::PhaseOf(299775, 300000, {0.5f});
    CHECK(First.Phase == 1);
    CHECK(First.Value == 149775);
    CHECK(First.Span == 150000);
    const auto Shot = Game::PhaseOf(298650, 300000, {0.5f});
    CHECK(Shot.Value == 148650);
    const auto Frankenstein = Game::PhaseOf(120000, 120000, {0.65f});
    CHECK(Frankenstein.Value == 42000);
    CHECK(Frankenstein.Span == 42000);
}

TEST_CASE("Boss bar derives later phases from health alone, as after a join mid-fight")
{
    const auto Second = Game::PhaseOf(140000, 300000, {0.5f});
    CHECK(Second.Phase == 2);
    CHECK(Second.Value == 140000);
    CHECK(Second.Span == 150000);
    // HQ-Endboss: Marken bei 0,8 und 0,4 von 5.000.000.
    const std::vector<float> Hq{0.8f, 0.4f};
    CHECK(Game::PhaseOf(4999460, 5000000, Hq).Value == 999460);
    const auto Middle = Game::PhaseOf(3000000, 5000000, Hq);
    CHECK(Middle.Phase == 2);
    CHECK(Middle.Value == 1000000);
    CHECK(Middle.Span == 2000000);
    const auto Last = Game::PhaseOf(1500000, 5000000, Hq);
    CHECK(Last.Phase == 3);
    CHECK(Last.Value == 1500000);
    CHECK(Last.Span == 2000000);
}

TEST_CASE("Boss bar never drops below one at a phase mark")
{
    // Genau auf der Marke läuft schon die nächste Phase, der Wert ist die volle nächste Phase und nie 0.
    const auto AtMark = Game::PhaseOf(150000, 300000, {0.5f});
    CHECK(AtMark.Phase == 2);
    CHECK(AtMark.Value == 150000);
    const auto JustAbove = Game::PhaseOf(150001, 300000, {0.5f});
    CHECK(JustAbove.Phase == 1);
    CHECK(JustAbove.Value == 1);
    for (int64_t Health = 1; Health <= 80000; Health += 37)
        CHECK(Game::PhaseOf(Health, 80000, {0.5f}).Value >= 1);
    CHECK(Game::PhaseOf(1, 5000000, {0.8f, 0.4f}).Value == 1);
}

// Kampfstart (opt-boss-phasenleiste): Tags des SpikeBoss beim Client vor dem Kampf mit State.Pause, 6 s nach TriggerBossfight ohne.
TEST_CASE("Boss bar waits for the fight to start")
{
    CHECK_FALSE(Game::BossFightStarted(1, 80000, 80000));
    CHECK(Game::BossFightStarted(0, 80000, 80000));
    // Tags nicht lesbar: lieber die Leiste zu früh als gar keine.
    CHECK(Game::BossFightStarted(-1, 80000, 80000));
    // Schaden heißt Kampf, auch wenn der Boss später wieder pausiert (Beitritt mitten im Kampf).
    CHECK(Game::BossFightStarted(1, 79999, 80000));
    CHECK_FALSE(Game::BossFightStarted(1, 0, 0));

    auto State = Fight();
    State.Started = false;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::None);
    State.Started = true;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Create);
    // Eine bestehende Leiste bleibt, auch wenn der Boss im Kampf pausiert.
    State.Started = false;
    State.HasObjective = true;
    State.Shown = true;
    State.SettleTicks = Game::BossBarSettleTicks;
    CHECK(Game::DecideBossBar(State) == Game::BossBarStep::Sync);
}

TEST_CASE("Boss bar ignores unusable phase marks")
{
    const auto None = Game::PhaseOf(49135, 80000, {});
    CHECK(None.Phase == 1);
    CHECK(None.Value == 49135);
    CHECK(None.Span == 80000);
    // Nicht fallend, außerhalb von 0 bis 1 oder NaN: übergehen statt einen Nenner von 0 oder einen negativen Wert zu bilden.
    const auto Odd = Game::PhaseOf(200000, 300000, {0.0f, 1.0f, 1.5f, -0.2f, 0.5f, 0.5f, 0.7f});
    CHECK(Odd.Phase == 1);
    CHECK(Odd.Value == 50000);
    CHECK(Odd.Span == 150000);
    CHECK(Game::PhaseOf(100000, 300000, {std::numeric_limits<float>::quiet_NaN(), 0.5f}).Value == 100000);
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
