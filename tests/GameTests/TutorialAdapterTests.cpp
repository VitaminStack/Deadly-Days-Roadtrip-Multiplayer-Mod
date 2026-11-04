#include <doctest.h>

#include "Game/TutorialAdapter.h"

TEST_CASE("Tutorials are only skipped in coop")
{
    using Game::NetRole;
    using Game::TutorialAction;
    CHECK(Game::DecideTutorials(NetRole::Standalone, 0) == TutorialAction::None);
    CHECK(Game::DecideTutorials(NetRole::Host, std::nullopt) == TutorialAction::None);
    CHECK(Game::DecideTutorials(NetRole::Host, 0) == TutorialAction::Skip);
    CHECK(Game::DecideTutorials(NetRole::Client, 0) == TutorialAction::Skip);
    CHECK(Game::DecideTutorials(NetRole::Client, 1) == TutorialAction::Done);
}
