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
