#include <doctest.h>
#include "Game/RouteMapPolicy.h"

using namespace Game::RouteMap;

namespace
{
// Gemessen an Tag 1 eines Runs: Route 0 der ersten Gruppe, eine Mission, noch kein Fortschritt, gewählt Knoten 0.
HostRoute DayOne()
{
    HostRoute Value;
    Value.MapId = 0x1234;
    Value.Seed = 30477;
    Value.Current = {0, 0};
    Value.MissionIndex = 0;
    Value.CachedMissionIndex = -1;
    Value.Chosen = {0};
    return Value;
}
}

TEST_CASE("Routenkarte: gemessene Route des Hosts ist gültig, auch ohne vorige Route")
{
    CHECK(Valid(DayOne()));
    auto Later = DayOne();
    Later.Current = {4, 0};
    Later.Previous = {3, 2};
    Later.Chosen = {0, 2, 4, 4};
    Later.ThreatIncrease = 3;
    CHECK(Valid(Later));
}

TEST_CASE("Routenkarte: kaputte Kennungen, Indizes und Wahl werden abgelehnt")
{
    auto Value = DayOne();
    Value.MapId = 0;
    CHECK_FALSE(Valid(Value)); // ohne Karte gilt keine Route

    Value = DayOne();
    Value.Current = {-1, -1};
    CHECK_FALSE(Valid(Value)); // eine aktuelle Route braucht eine Kennung

    Value = DayOne();
    Value.Previous = {2, -1};
    CHECK_FALSE(Valid(Value)); // halbe Kennung

    Value = DayOne();
    Value.Current = {MaxGroups, 0};
    CHECK_FALSE(Valid(Value));

    Value = DayOne();
    Value.MissionIndex = -2;
    CHECK_FALSE(Valid(Value));

    Value = DayOne();
    Value.Chosen.assign(MaxChosen + 1, 0);
    CHECK_FALSE(Valid(Value));

    Value = DayOne();
    Value.Chosen = {0, -1};
    CHECK_FALSE(Valid(Value));

    Value = DayOne();
    Value.ThreatIncrease = -1;
    CHECK_FALSE(Valid(Value));
}

TEST_CASE("Routenkarte: der Client übernimmt die Route nur am Tagesende der Karte, auf der sie galt, und einmal je Ende")
{
    CHECK(ClientMayApplyRoute(7, 7, true, false));
    CHECK_FALSE(ClientMayApplyRoute(6, 7, true, false));  // Route einer älteren Karte
    CHECK_FALSE(ClientMayApplyRoute(0, 0, true, false));  // keine Route vom Host
    CHECK_FALSE(ClientMayApplyRoute(7, 7, false, false)); // Sieg oder Niederlage: keine Routenkarte, kein lokaler Sieg
    CHECK_FALSE(ClientMayApplyRoute(7, 7, true, true));   // dieses Ende ist schon übernommen
}

TEST_CASE("Routenkarte: die Kennung muss in der eigenen Missionsstruktur liegen")
{
    const std::vector<int32_t> Routes = {3, 3, 3, 3, 1, 3, 3, 1}; // gemessen: 8 Gruppen
    CHECK(IdentInStructure({0, 2}, Routes));
    CHECK(IdentInStructure({7, 0}, Routes));
    CHECK_FALSE(IdentInStructure({4, 1}, Routes)); // Gruppe mit nur einer Route
    CHECK_FALSE(IdentInStructure({8, 0}, Routes));
    CHECK_FALSE(IdentInStructure({-1, -1}, Routes));
    CHECK_FALSE(IdentInStructure({0, 0}, {}));
}

TEST_CASE("Routenkarte: nur der Client mit übernommener Route sieht beim Öffnen der Karte die Rolle Authority")
{
    CHECK(PanelNeedsAuthority(true, true));
    CHECK_FALSE(PanelNeedsAuthority(true, false)); // ohne Route bleibt es bei der leeren Karte
    CHECK_FALSE(PanelNeedsAuthority(false, true)); // Host und allein haben die Rolle selbst
}

TEST_CASE("Routenkarte: nach dem Weltwechsel gilt wieder die eigene Run-Info")
{
    CHECK(ShouldRestore(0x100, 0x200));
    CHECK(ShouldRestore(0x100, 0));
    CHECK_FALSE(ShouldRestore(0x100, 0x100));
    CHECK_FALSE(ShouldRestore(0, 0x200)); // nichts übernommen
}

TEST_CASE("Routenkarte: einen Knoten wählt nur der Host")
{
    CHECK(ClientMayChooseRoute(false));
    CHECK_FALSE(ClientMayChooseRoute(true));
}
