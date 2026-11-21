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

TEST_CASE("opt-airdrop-sound: Landesound genau einmal, wenn die Fallanimation die Landung erreicht")
{
    Game::AirdropLanding Crate;
    CHECK_FALSE(Game::AirdropLandingSound(Crate, false, true, 0.4f)); // erste Beobachtung, Kiste fällt noch
    CHECK_FALSE(Game::AirdropLandingSound(Crate, false, true, 1.9f));
    CHECK_FALSE(Game::AirdropLandingSound(Crate, false, true, Game::AirdropLandSeconds - 0.01f));
    CHECK(Game::AirdropLandingSound(Crate, false, true, Game::AirdropLandSeconds));
    CHECK_FALSE(Game::AirdropLandingSound(Crate, false, true, 3.2f)); // nie doppelt
    CHECK_FALSE(Game::AirdropLandingSound(Crate, true, true, 4.5f));
    CHECK_FALSE(Game::AirdropLandingSound(Crate, true, false, 0.f));
}

TEST_CASE("opt-airdrop-sound: schon gelandete Kisten bleiben stumm (Beitritt, Reise, spät repliziert)")
{
    Game::AirdropLanding Ready;
    CHECK_FALSE(Game::AirdropLandingSound(Ready, true, false, 0.f));
    CHECK_FALSE(Game::AirdropLandingSound(Ready, true, true, 9.f));
    Game::AirdropLanding Late;
    CHECK_FALSE(Game::AirdropLandingSound(Late, false, true, 3.f)); // Animation schon hinter der Landung
    CHECK_FALSE(Game::AirdropLandingSound(Late, true, true, 4.6f));
}

TEST_CASE("opt-airdrop-sound: ohne Animationsposition spielt der Sound ersatzweise beim Bereitwerden")
{
    Game::AirdropLanding Crate;
    CHECK_FALSE(Game::AirdropLandingSound(Crate, false, false, 0.f));
    CHECK_FALSE(Game::AirdropLandingSound(Crate, false, false, 0.f));
    CHECK(Game::AirdropLandingSound(Crate, true, false, 0.f));
    CHECK_FALSE(Game::AirdropLandingSound(Crate, true, false, 0.f));
}
