#include <doctest.h>
#include "Sync/SessionLogState.h"

using namespace Sync::SessionLog;

TEST_CASE("Zustandszeile: unbekannte Werte als Strich, bekannte als Schlüssel=Wert")
{
    CHECK(Format({}) == "Rolle=- Karte=- Spieler=- Zeitfaktor=- Pause=- Zeitanfragen=- Schrott=- Tank=- Ping=- Verbindungen=0 "
                        "Kontrolle-abgelehnt=0");
    State S;
    S.Role = "Host";
    S.Map = "DDDynamic_Map";
    S.Players = 2;
    S.Dilation = 1.0f;
    S.TimeRequests = 0;
    S.Scrap = 120;
    S.FuelCurrent = 40.4f;
    S.FuelMax = 100.0f;
    S.PingMs = 35;
    S.Connections = 1;
    CHECK(Format(S) == "Rolle=Host Karte=DDDynamic_Map Spieler=2 Zeitfaktor=1.00 Pause=nein Zeitanfragen=0 Schrott=120 Tank=40/100 "
                       "Ping=35ms Verbindungen=1 Kontrolle-abgelehnt=0");
    S.Pauser = true;
    S.FuelMax.reset();
    CHECK(Format(S).find("Pause=ja") != std::string::npos);
    CHECK(Format(S).find("Tank=-") != std::string::npos);
}

TEST_CASE("Zustandszeile: erste, Weltwechsel, Marke und Takt")
{
    CHECK(Due(false, 0, 0, false, false));
    CHECK_FALSE(Due(true, 1000, 1000 + IntervalMs - 1, false, false));
    CHECK(Due(true, 1000, 1000 + IntervalMs, false, false));
    CHECK(Due(true, 1000, 1001, true, false));
    CHECK(Due(true, 1000, 1001, false, true));
    CHECK(Due(true, 5000, 1000, false, false)); // Uhr zurück
}
