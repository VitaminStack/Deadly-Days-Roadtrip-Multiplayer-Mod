#include <doctest.h>

#include "Game/ItemAbilityPolicy.h"

using namespace Game;

TEST_CASE("client-items-wirkung: nur lokal ausgeführte Item-Fähigkeiten stellt der Host um")
{
    CHECK(ItemAbility::RunsOnlyForLocalOwner(ItemAbility::LocalOnly));      // GA_GrenadeThrower_C
    CHECK(ItemAbility::RunsOnlyForLocalOwner(ItemAbility::LocalPredicted)); // GA_DelayedHeal_C
    CHECK_FALSE(ItemAbility::RunsOnlyForLocalOwner(ItemAbility::ServerInitiated));
    CHECK_FALSE(ItemAbility::RunsOnlyForLocalOwner(ItemAbility::ServerOnly));
    CHECK_FALSE(ItemAbility::RunsOnlyForLocalOwner(4)); // unbekannter Wert bleibt unberührt
}

TEST_CASE("client-items-wirkung: nur vom Item selbst ausgelöste Fähigkeiten ohne Eingabe-Tag wandern zum Host")
{
    CHECK(ItemAbility::MovesToHost(ItemAbility::LocalOnly, false, true, false));      // Granate: Auslöse-Effekt
    CHECK(ItemAbility::MovesToHost(ItemAbility::LocalPredicted, true, false, false)); // Heil-Kit: beim Vergeben
    CHECK_FALSE(ItemAbility::MovesToHost(ItemAbility::LocalPredicted, false, false, false)); // GA_SlideAbility_C: von außen gestartet
    CHECK_FALSE(ItemAbility::MovesToHost(ItemAbility::LocalPredicted, true, true, true));    // Eingabe-Fähigkeit
    CHECK_FALSE(ItemAbility::MovesToHost(ItemAbility::ServerOnly, true, true, false));
}
