#include <doctest.h>

#include "Game/PowerUpPolicy.h"

using namespace Game;

TEST_CASE("opt-powerups-client: Host stellt LocalPredicted- und LocalOnly-Fähigkeiten auf ServerOnly")
{
    CHECK(PowerUp::AbilityMovesToHost(PowerUp::LocalPredicted)); // GA_PowerUp_Heal_C, GA_PowerUp_LevelUp_C
    CHECK(PowerUp::AbilityMovesToHost(PowerUp::LocalOnly));
    CHECK_FALSE(PowerUp::AbilityMovesToHost(PowerUp::ServerInitiated));
    CHECK_FALSE(PowerUp::AbilityMovesToHost(PowerUp::ServerOnly));
    CHECK_FALSE(PowerUp::AbilityMovesToHost(7));
}

TEST_CASE("opt-powerups-client: Power-ups bucht nur, wer die Slots führt")
{
    CHECK(PowerUp::LootMayBook(NetRole::Standalone));
    CHECK(PowerUp::LootMayBook(NetRole::Host));
    CHECK_FALSE(PowerUp::LootMayBook(NetRole::Client));
}

TEST_CASE("opt-powerups-client: Namen und Slot-Stände")
{
    CHECK(PowerUp::ValidName("POD_Vacuum"));
    CHECK(PowerUp::ValidName("POD_Stamina_Energy"));
    CHECK_FALSE(PowerUp::ValidName(""));
    CHECK_FALSE(PowerUp::ValidName("POD Vacuum"));
    CHECK_FALSE(PowerUp::ValidName(std::string(PowerUp::MaxNameBytes + 1, 'a')));
    CHECK(PowerUp::ValidSlots({{"POD_Vacuum", 1}, {"POD_Explosive", 2}}));
    CHECK_FALSE(PowerUp::ValidSlots({{"POD_Vacuum", -1}}));
    CHECK_FALSE(PowerUp::ValidSlots(std::vector<PowerUp::Slot>(PowerUp::MaxSlots + 1, {"POD_Vacuum", 1})));
}

TEST_CASE("opt-powerups-client: Client übernimmt den Slot-Stand des Hosts")
{
    SUBCASE("Host hat aufgesammelt, Client noch nicht")
    {
        const auto Plan = PowerUp::Reconcile({}, {{"POD_Vacuum", 1}}, {});
        CHECK(Plan.Remove.empty());
        REQUIRE(Plan.Add.size() == 1);
        CHECK(Plan.Add[0] == PowerUp::Slot{"POD_Vacuum", 1});
    }
    SUBCASE("Client hat eine Kopie, die der Host nicht hat")
    {
        const auto Plan = PowerUp::Reconcile({"POD_Explosive"}, {}, {});
        CHECK(Plan.Add.empty());
        CHECK(Plan.Remove == std::vector<std::string>{"POD_Explosive"});
    }
    SUBCASE("gleich, nur andere Reihenfolge")
    {
        CHECK(PowerUp::Reconcile({"POD_Explosive", "POD_Vacuum"}, {{"POD_Vacuum", 1}, {"POD_Explosive", 1}}, {}).Empty());
    }
    SUBCASE("zweimal dasselbe Power-up zählt je Slot")
    {
        const auto Plan = PowerUp::Reconcile({"POD_Vacuum"}, {{"POD_Vacuum", 1}, {"POD_Vacuum", 1}}, {});
        CHECK(Plan.Remove.empty());
        CHECK(Plan.Add.size() == 1);
    }
    SUBCASE("eben ausgelöst: ältere Meldung des Hosts legt es nicht zurück")
    {
        CHECK(PowerUp::Reconcile({}, {{"POD_Vacuum", 1}}, {"POD_Vacuum"}).Empty());
    }
    SUBCASE("ausgelöst, aber der Host hat noch ein zweites")
    {
        const auto Plan = PowerUp::Reconcile({}, {{"POD_Vacuum", 1}, {"POD_Vacuum", 1}}, {"POD_Vacuum"});
        CHECK(Plan.Add.size() == 1);
    }
    SUBCASE("gemeldet, der Host hatte es nie: nichts zurücklegen, nichts doppelt leeren")
    {
        CHECK(PowerUp::Reconcile({}, {}, {"POD_Vacuum"}).Empty());
    }
}

TEST_CASE("opt-powerups-client: Meldungen des Clients bis zur Bestätigung")
{
    PowerUp::Requests Open;
    CHECK(Open.Add("POD_Vacuum", 100) == 1);
    CHECK(Open.Add("POD_Explosive", 200) == 2);
    CHECK(Open.Add("kein Name", 200) == 0);
    CHECK(Open.Pending() == std::vector<std::string>{"POD_Vacuum", "POD_Explosive"});
    Open.Acknowledge(1);
    CHECK(Open.Pending() == std::vector<std::string>{"POD_Explosive"});
    Open.Expire(200 + PowerUp::RequestExpireMs - 1);
    CHECK(Open.Size() == 1);
    Open.Expire(200 + PowerUp::RequestExpireMs);
    CHECK(Open.Size() == 0);
    // Nach Clear läuft die Nummer weiter: eine späte Bestätigung der alten Nummer löscht nichts Neues.
    Open.Add("POD_Vacuum", 300);
    Open.Clear();
    CHECK(Open.Add("POD_Vacuum", 400) == 4);
    Open.Acknowledge(3);
    CHECK(Open.Size() == 1);
}

TEST_CASE("opt-powerups-client: Host sendet bei Änderung und als Wiederholung")
{
    PowerUp::Sent Last;
    const std::vector<PowerUp::Slot> One{{"POD_Vacuum", 1}};
    CHECK(PowerUp::SendDue(Last, {}, 0, 0));
    Last = {true, One, 0, 1000};
    CHECK_FALSE(PowerUp::SendDue(Last, One, 0, 1500));
    CHECK(PowerUp::SendDue(Last, {}, 0, 1500));
    CHECK(PowerUp::SendDue(Last, One, 1, 1500));
    CHECK(PowerUp::SendDue(Last, One, 0, 1000 + PowerUp::RepeatMs));
}
