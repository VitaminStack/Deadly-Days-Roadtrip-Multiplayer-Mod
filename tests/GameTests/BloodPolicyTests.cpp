#include <doctest.h>

#include "Game/BloodPolicy.h"

using namespace Game;

TEST_CASE("opt-blut: nur der Client spielt Tod-Effekte, nur für lebende Gegner")
{
    CHECK(BloodShouldPlay(true, true, true, false, 0));
    CHECK_FALSE(BloodShouldPlay(false, true, true, false, 0)); // Host hat sie schon aus der Sterbe-Ability
    CHECK_FALSE(BloodShouldPlay(true, false, true, false, 0)); // Multi_OnDie eines Spielers
    CHECK_FALSE(BloodShouldPlay(true, true, false, false, 0)); // freigegeben oder Garbage
}

TEST_CASE("opt-blut: doppelte Zustellung und Obergrenze je Tick")
{
    CHECK_FALSE(BloodShouldPlay(true, true, true, true, 0));
    CHECK(BloodShouldPlay(true, true, true, false, BloodMaxPerTick - 1));
    CHECK_FALSE(BloodShouldPlay(true, true, true, false, BloodMaxPerTick));
}

TEST_CASE("opt-blut: derselbe Gegner gilt nur kurz als doppelt, gepoolte Gegner sterben später erneut")
{
    CHECK_FALSE(BloodRecent(0, 5000));
    CHECK(BloodRecent(5000, 5000));
    CHECK(BloodRecent(5000, 5000 + BloodRepeatMs - 1));
    CHECK_FALSE(BloodRecent(5000, 5000 + BloodRepeatMs));
    CHECK_FALSE(BloodRecent(9000, 5000)); // Uhr zurück: nicht unterdrücken
}
