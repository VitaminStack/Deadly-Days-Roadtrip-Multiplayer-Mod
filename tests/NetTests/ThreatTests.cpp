#include <doctest.h>
#include <vector>
#include "Sync/World/ThreatState.h"

using namespace Sync::Threat;
using namespace Game;

TEST_CASE("Bedrohungsstufe: genau 4 Byte, verlustfrei, ungültige Werte abgelehnt")
{
    for (const int32_t Level : {0, 1, 7, 255, 256, ThreatLevelMax})
    {
        const auto Bytes = Encode(Level);
        REQUIRE(Bytes.size() == 4);
        CHECK(Decode(Bytes) == Level);
    }
    CHECK(Encode(7) == std::vector<uint8_t>{7, 0, 0, 0});
    CHECK(Encode(-1).empty());
    CHECK(Encode(ThreatLevelMax + 1).empty());
    CHECK_FALSE(Decode(std::vector<uint8_t>{}));
    CHECK_FALSE(Decode(std::vector<uint8_t>{1, 0, 0}));
    CHECK_FALSE(Decode(std::vector<uint8_t>{1, 0, 0, 0, 0}));
    CHECK_FALSE(Decode(std::vector<uint8_t>{0xFF, 0xFF, 0xFF, 0xFF})); // -1: Stand eines Clients ohne Stufe
}

TEST_CASE("Bedrohungsstufe: Host sendet bei Änderung, nach neuer Welt und als Wiederholung")
{
    CHECK(ThreatSendDue(false, 0, 0, 0, 1000));                         // noch nie gesendet
    CHECK(ThreatSendDue(true, 3, 4, 1000, 1001));                       // Stufe steigt
    CHECK_FALSE(ThreatSendDue(true, 3, 3, 1000, 1000 + ThreatRepeatMs - 1));
    CHECK(ThreatSendDue(true, 3, 3, 1000, 1000 + ThreatRepeatMs));      // Wiederholung gegen Verlust
    CHECK(ThreatSendDue(true, 3, 3, 5000, 1000));                       // Uhr zurück
}

TEST_CASE("Bedrohungsstufe: erster Stand einer Welt setzt nur die Zahl, jeder Anstieg läuft einmal durch das Ereignis")
{
    ThreatShown Shown;
    CHECK(ThreatLevelChange(Shown, 4) == ThreatEvent{true, 4, 4}); // Beitritt oder Reise: kein Anstieg, kein Sound
    Shown = {true, 4};
    CHECK(ThreatLevelChange(Shown, 4) == ThreatEvent{});           // Wiederholung des Hosts
    CHECK(ThreatLevelChange(Shown, 5) == ThreatEvent{true, 4, 5});
    Shown = {true, 5};
    CHECK(ThreatLevelChange(Shown, 5) == ThreatEvent{});
    CHECK(ThreatLevelChange(Shown, 8) == ThreatEvent{true, 5, 8}); // verlorene Zwischenstände: ein Sprung, ein Sound
    CHECK(ThreatLevelChange(Shown, 2) == ThreatEvent{true, 5, 2}); // sinkt: Zahl folgt, das Ereignis spielt nichts
    CHECK(ThreatLevelChange(Shown, -1) == ThreatEvent{});
}

TEST_CASE("Bedrohungsstufe: fehlt die Stoppuhr, bleibt der Stand offen und die Wiederholung holt ihn nach")
{
    ThreatShown Shown; // ShowThreatLevel setzt Shown erst nach dem Aufruf der Stoppuhr
    CHECK(ThreatLevelChange(Shown, 6).Call);
    CHECK(ThreatLevelChange(Shown, 6).Call);
}

TEST_CASE("Bedrohung: die Haupt-UI bekommt jeden Stand, den sie noch nicht zeigt, auch nach der Reise")
{
    using Game::UiThreatLevelChange;
    using Game::UiThreatUpdate;
    CHECK(UiThreatLevelChange(false, -1, 3) == UiThreatUpdate{true, true, 3, 3});  // neue Welt: Haupt-UI suchen, nur die Zahl
    CHECK(UiThreatLevelChange(false, 3, 3) == UiThreatUpdate{true, true, 3, 3});   // alte UI weg, unveränderter Stand: trotzdem zeigen
    CHECK(UiThreatLevelChange(true, 3, 3) == UiThreatUpdate{});                     // Wiederholung des Hosts
    CHECK(UiThreatLevelChange(true, 3, 5) == UiThreatUpdate{true, false, 3, 5});   // Anstieg wie beim Host
    CHECK(UiThreatLevelChange(true, -1, 5) == UiThreatUpdate{true, false, 5, 5});  // gefunden, aber noch nichts gezeigt
    CHECK(UiThreatLevelChange(true, 3, -1) == UiThreatUpdate{});                    // ungültiger Stand
}
