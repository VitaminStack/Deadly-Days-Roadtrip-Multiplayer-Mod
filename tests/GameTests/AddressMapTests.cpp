#include <doctest.h>

#include "Game/AddressMap.h"

using namespace Game::AddressMap;

TEST_CASE("Adressen: Referenz-Build bleibt unverändert, unbekannter Build liefert 0")
{
    CHECK(Map(ReferenceBuild, 0x0160C040) == 0x0160C040);
    CHECK(Known(ReferenceBuild));
    CHECK_FALSE(Known(1));
    CHECK(Map(1, 0x0160C040) == 0);
}

TEST_CASE("Adressen: Tabelle 26669225 ist sortiert und rechnet nur bekannte Einträge um")
{
    const auto Entries = Table(26669225);
    REQUIRE_FALSE(Entries.empty());
    CHECK(Sorted(Entries));
    CHECK(Known(26669225));
    for (const Entry& Value : Entries)
    {
        CHECK(Value.Mapped != 0);
        CHECK(Map(26669225, Value.Reference) == Value.Mapped);
    }
    // Zwischen zwei Einträgen liegt keine geratene Adresse, fehlende Einträge liefern 0.
    CHECK(Map(26669225, Entries.front().Reference + 1) == 0);
    CHECK(Map(26669225, 0x057DC810) == 0x057DD020); // OnRep_Health, von Hand nachgetragen (update-1-0-2-lebensleisten)
}
