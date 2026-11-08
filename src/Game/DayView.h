#pragma once

#include <string>

// Nur für den Testkanal (tagesablauf): liest Tag, Uhr, Tank und Missionsziele beider Seiten zum Vergleich. Reine Diagnose,
// schreibt nichts ins Spiel. Felder werden über die Reflexion aufgezählt, ohne feste Offsets.
namespace Game
{
// JSON-Felder ohne äußere Klammern, für Net::Control::OkReply. Nur auf dem Game Thread.
std::string DescribeDayView();
}
