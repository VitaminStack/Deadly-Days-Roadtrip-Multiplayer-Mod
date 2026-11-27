#include <doctest.h>

#include "Game/StallPolicy.h"

using Game::Stall::Action;
using Game::Stall::Next;
using Game::Stall::Settings;
using Game::Stall::State;

TEST_CASE("client-haenger-nach-niederlage: Wächter meldet erst ab der Schwelle, dann wenige Proben im Abstand")
{
    const Settings Limits{8000, 4000, 3};
    State Value;
    CHECK(Next(Value, 1000, 0, Limits) == Action::None); // noch nie getaktet
    CHECK(Next(Value, 5000, 1000, Limits) == Action::None);
    CHECK(Next(Value, 8999, 1000, Limits) == Action::None);
    CHECK(Next(Value, 9000, 1000, Limits) == Action::Sample);
    CHECK(Value.Samples == 1);
    CHECK(Next(Value, 12999, 1000, Limits) == Action::None);
    CHECK(Next(Value, 13000, 1000, Limits) == Action::Sample);
    CHECK(Next(Value, 17000, 1000, Limits) == Action::Sample);
    CHECK(Value.Samples == 3);
    // Höchstens MaxSamples je Hänger, auch wenn er Minuten dauert.
    CHECK(Next(Value, 21000, 1000, Limits) == Action::None);
    CHECK(Next(Value, 90000, 1000, Limits) == Action::None);
    // Ein neuer Takt beendet den Hänger einmal, danach beginnt die Zählung neu.
    CHECK(Next(Value, 91000, 90500, Limits) == Action::Recovered);
    CHECK_FALSE(Value.Stalled);
    CHECK(Next(Value, 92000, 90500, Limits) == Action::None);
    CHECK(Next(Value, 98500, 90500, Limits) == Action::Sample);
}

TEST_CASE("client-haenger-nach-niederlage: Wächter ohne Takt oder mit Uhr hinter dem Takt bleibt still")
{
    State Value;
    CHECK(Next(Value, 100000, 0) == Action::None);
    CHECK(Next(Value, 500, 1000) == Action::None);
    CHECK_FALSE(Value.Stalled);
    // Takt hängt ab (LastTick 0), etwa beim Beenden: kein Wiederanlauf-Log, kein offener Hänger.
    CHECK(Next(Value, 20000, 1000) == Action::Sample);
    CHECK(Next(Value, 21000, 0) == Action::None);
    CHECK_FALSE(Value.Stalled);
}

TEST_CASE("client-haenger-nach-niederlage: MaxSamples 0 nimmt keine Probe, meldet aber das Wiederanlaufen")
{
    const Settings Limits{1000, 1000, 0};
    State Value;
    CHECK(Next(Value, 5000, 1000, Limits) == Action::None);
    CHECK(Value.Stalled);
    CHECK(Next(Value, 6000, 5500, Limits) == Action::Recovered);
}
