#include <doctest.h>

#include "Game/ZoneGoalPolicy.h"

using namespace Game;

namespace
{
// Client in einer Titanen-Zone, der Host meldet 3 von 10, Klasse und Anzeige sind da.
ZoneGoalClient Zone()
{
    ZoneGoalClient State;
    State.Client = true;
    State.HostKnown = true;
    State.Host = {true, 3, 10};
    State.HasClass = true;
    State.WidgetReady = true;
    return State;
}

ZoneGoalClient Shown(int64_t Current = 3)
{
    ZoneGoalClient State = Zone();
    State.Shown = true;
    State.ShownCurrent = Current;
    State.ShownTarget = 10;
    State.SettleTicks = ZoneGoalSettleTicks;
    return State;
}
}

TEST_CASE("Zonenziel: Stand des Hosts ohne Boss-Ziele und ohne unbrauchbares Soll")
{
    CHECK(ZoneGoalFromHost(true, false, 8, 10) == ZoneGoalView{true, 8, 10});
    CHECK(ZoneGoalFromHost(false, false, 8, 10) == ZoneGoalView{});
    CHECK(ZoneGoalFromHost(true, true, 40000, 80000) == ZoneGoalView{}); // Boss-Leiste baut der Client selbst
    CHECK(ZoneGoalFromHost(true, false, 0, 0) == ZoneGoalView{});
    CHECK(ZoneGoalFromHost(true, false, 0, ZoneGoalMax + 1) == ZoneGoalView{});
    CHECK(ZoneGoalFromHost(true, false, -5, 10) == ZoneGoalView{true, 0, 10});
    CHECK(ZoneGoalFromHost(true, false, ZoneGoalMax + 7, 10) == ZoneGoalView{true, ZoneGoalMax, 10});
    CHECK(ZoneGoalValid({}));
    CHECK(ZoneGoalValid({true, 0, 10}));
    CHECK(ZoneGoalValid({true, 10, 10}));
    CHECK_FALSE(ZoneGoalValid({false, 1, 0}));
    CHECK_FALSE(ZoneGoalValid({false, 0, 10}));
    CHECK_FALSE(ZoneGoalValid({true, 0, 0}));
    CHECK_FALSE(ZoneGoalValid({true, -1, 10}));
}

TEST_CASE("Zonenziel: Host sendet bei Änderung, wiederholt ein laufendes Ziel und schweigt nach drei Mal „kein Ziel“")
{
    const ZoneGoalView Three{true, 3, 10};
    ZoneGoalSent Sent;
    CHECK(ZoneGoalSendDue(Sent, Three, 1000)); // noch nie gesendet
    Sent = ZoneGoalAfterSend(Sent, Three, 1000, true);
    CHECK_FALSE(ZoneGoalSendDue(Sent, Three, 1000 + ZoneGoalRepeatMs - 1));
    CHECK(ZoneGoalSendDue(Sent, Three, 1000 + ZoneGoalRepeatMs)); // Wiederholung gegen Verlust und für Beitritte
    CHECK(ZoneGoalSendDue(Sent, {true, 4, 10}, 1001));            // Fortschritt sofort
    CHECK(ZoneGoalSendDue(Sent, Three, 500));                     // Uhr zurück
    // Ein laufendes Ziel wird beliebig oft wiederholt.
    uint64_t Now = 1000;
    for (int i = 0; i < 20; ++i)
    {
        Now += ZoneGoalRepeatMs;
        REQUIRE(ZoneGoalSendDue(Sent, Three, Now));
        Sent = ZoneGoalAfterSend(Sent, Three, Now, true);
    }
    // Abschluss: „kein Ziel“ sofort, dann drei Wiederholungen, danach Ruhe.
    CHECK(ZoneGoalSendDue(Sent, {}, Now + 1));
    Sent = ZoneGoalAfterSend(Sent, {}, Now + 1, true);
    for (int i = 0; i < ZoneGoalIdleRepeats; ++i)
    {
        Now += ZoneGoalRepeatMs + 1;
        REQUIRE(ZoneGoalSendDue(Sent, {}, Now));
        Sent = ZoneGoalAfterSend(Sent, {}, Now, true);
    }
    CHECK_FALSE(ZoneGoalSendDue(Sent, {}, Now + 100 * ZoneGoalRepeatMs));
    CHECK(ZoneGoalSendDue(Sent, Three, Now + 1)); // neues Ziel
    // Nicht angenommener Versand (kein Mitspieler): gilt als nicht gesendet. So setzt Sync den Stand auch für neue Mitspieler zurück.
    Sent = ZoneGoalAfterSend(Sent, {}, Now, false);
    CHECK(ZoneGoalSendDue(Sent, {}, Now));
}

TEST_CASE("Zonenziel: Client legt die Leiste erst an, wenn Host, Klasse und freie Anzeige da sind")
{
    CHECK(DecideZoneGoal(Zone()) == ZoneGoalStep::Create);
    ZoneGoalClient State = Zone();
    State.Client = false;
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::None); // Host und Einzelspieler haben ihr eigenes Ziel
    State = Zone();
    State.HostKnown = false;
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::None);
    State = Zone();
    State.Host = {};
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::None);
    State = Zone();
    State.HasClass = false; // Ziel einer Karte ohne Zonen-Komponente
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::None);
    State = Zone();
    State.WidgetReady = false;
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::None);
    State = Zone();
    State.WidgetBusy = true; // die Boss-Leiste hat Vorrang, das Zonenziel wartet
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::None);
    State = Zone();
    State.Host = {true, 0, 10}; // Beginn der Zone
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Create);
    State.Host = {true, 9, 10}; // Beitritt kurz vor dem Ende
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Create);
}

TEST_CASE("Zonenziel: Client setzt nie das Soll, den Abschluss bucht nur der Host")
{
    ZoneGoalClient State = Zone();
    State.Host = {true, 10, 10};
    CHECK_FALSE(ZoneGoalWanted(State));
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::None);
    State = Shown(9);
    State.Host = {true, 10, 10};
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Remove);
    State.Host = {true, 12, 10};
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Remove);
    State.Host = {true, 3, 0}; // ungültiger Stand
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Remove);
}

TEST_CASE("Zonenziel: Client folgt dem Stand des Hosts und entfernt die Leiste mit ihm")
{
    CHECK(DecideZoneGoal(Shown()) == ZoneGoalStep::None); // Wiederholung des Hosts
    ZoneGoalClient State = Shown();
    State.Host = {true, 5, 10}; // verlorene Zwischenstände: ein Sprung
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Update);
    State.SettleTicks = ZoneGoalSettleTicks - 1;
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Settle);
    State = Shown();
    State.Host = {}; // Abschluss beim Host
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Remove);
    State = Shown();
    State.HostKnown = false; // Weltwechsel oder Verbindung weg
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Remove);
    State = Shown();
    State.Host = {true, 3, 12}; // anderes Soll: neues Ziel, erst entfernen
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Remove);
    State = Shown(7);
    State.Host = {true, 2, 10}; // gesunkener Wert: neues Ziel, nie zurücksetzen
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Remove);
    // Eine bestehende Leiste braucht Klasse und Suche nicht mehr.
    State = Shown();
    State.HasClass = State.WidgetReady = false;
    State.Host = {true, 4, 10};
    CHECK(DecideZoneGoal(State) == ZoneGoalStep::Update);
}
