#include <doctest.h>

#include "Game/ItemAbilityPolicy.h"

using namespace Game;

TEST_CASE("client-items-wirkung: nur LocalOnly-Fähigkeiten, die das Item selbst auslöst, wandern zum Host")
{
    CHECK(ItemAbility::MovesToHost(ItemAbility::LocalOnly, false, true, false)); // GA_GrenadeThrower_C: Auslöse-Effekt
    CHECK(ItemAbility::MovesToHost(ItemAbility::LocalOnly, true, false, false)); // beim Vergeben
    CHECK_FALSE(ItemAbility::MovesToHost(ItemAbility::LocalOnly, false, false, false)); // von außen gestartet
    CHECK_FALSE(ItemAbility::MovesToHost(ItemAbility::LocalOnly, true, true, true));    // Eingabe-Fähigkeit
}

TEST_CASE("client-items-wirkung: alle anderen Richtlinien bleiben, wie sie sind")
{
    CHECK_FALSE(ItemAbility::MovesToHost(ItemAbility::LocalPredicted, true, false, false)); // GA_DelayedHeal_C läuft schon beim Host
    CHECK_FALSE(ItemAbility::MovesToHost(ItemAbility::ServerInitiated, true, true, false));
    CHECK_FALSE(ItemAbility::MovesToHost(ItemAbility::ServerOnly, true, true, false));
    CHECK_FALSE(ItemAbility::MovesToHost(4, true, true, false)); // unbekannter Wert
}
