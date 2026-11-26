#include <doctest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "Game/PatchPolicy.h"
#include "Game/ReaperAnnouncementPolicy.h"

using namespace Game;

TEST_CASE("opt-sensenmann-ansage-client: Ansage nur beim Client, sobald der Sensenmann des Hosts da ist, einmal je Spawner")
{
    CHECK(ReaperAnnouncementDue(true, true, false, true));
    CHECK_FALSE(ReaperAnnouncementDue(false, true, false, true)); // Host und Einzelspieler melden nativ selbst
    CHECK_FALSE(ReaperAnnouncementDue(true, false, false, true)); // Sensenmann ohne Spawner der Karte
    CHECK_FALSE(ReaperAnnouncementDue(true, true, true, true));   // schon gemeldet, keine zweite Ansage
    CHECK_FALSE(ReaperAnnouncementDue(true, true, false, false)); // eigener Zeitgeber abgelaufen, aber noch kein Sensenmann
}

TEST_CASE("opt-sensenmann-ansage-client: der Client erzeugt den Sensenmann weiter nicht selbst")
{
    CHECK_FALSE(Patches::ReaperMaySpawn(true));
    CHECK(Patches::ReaperMaySpawn(false));
}

TEST_CASE("opt-sensenmann-ansage-client: Spawn-Biome bleiben beim Host, die Anzeige läuft lokal")
{
    const std::array<std::string_view, 3> Biome = {"EnemyBiomeSpawnExpansionComponent", "ActorComponent", "Object"};
    const std::vector<std::string> Stopwatch = {"WBP_Stopwatch_C", "WBP_ScreenIconBase_C", "UserWidget", "Object"};
    const std::vector<std::string> Derived = {"BP_Expansion_C", "EnemyBiomeSpawnExpansionComponent", "ActorComponent"};
    CHECK_FALSE(ReaperReceiverRunsOnClient(Biome));
    CHECK_FALSE(ReaperReceiverRunsOnClient(Derived));
    CHECK(ReaperReceiverRunsOnClient(Stopwatch));
    CHECK(ReaperReceiverRunsOnClient(std::vector<std::string>{}));
}

TEST_CASE("opt-sensenmann-ansage-client: Sprechblase wie beim Host an fester Bildschirmstelle ohne Tween")
{
    CHECK(ReaperSpeech.UseAbsolutePosition);
    CHECK(ReaperSpeech.ScreenLocation[0] == -1.0);
    CHECK(ReaperSpeech.ScreenLocation[1] == -1.0);
    CHECK(ReaperSpeech.ScreenPercentages[0] == 0.5);
    CHECK(ReaperSpeech.ScreenPercentages[1] == 0.75);
    CHECK_FALSE(ReaperSpeech.TweenBubbleToNewPosition);
    CHECK(ReaperSpeech.TweenTime == 0.5f);
    CHECK(ReaperSpeech.TweenType == 8);
    CHECK_FALSE(ReaperSpeech.ShowBubbleDuringTween);
}
