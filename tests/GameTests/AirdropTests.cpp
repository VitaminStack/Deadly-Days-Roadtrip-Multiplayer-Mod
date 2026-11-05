#include <doctest.h>
#include "Game/AirdropPolicy.h"

TEST_CASE("Airdrop-Rewards laufen nur auf Autoritaetsactor des Hosts, nicht auf Client-Kopien")
{
    for (uint8_t Role = 0; Role < 4; ++Role)
    {
        CHECK_FALSE(Game::AirdropMayRunGameplay(true, Role, true));
        CHECK(Game::AirdropMayRunGameplay(true, Role, false) == (Role == 3));
        CHECK(Game::AirdropMayRunGameplay(false, Role, true)); // Fremde Kistentypen nicht veraendern.
    }
}
