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
    CHECK(Map(26669225, 0x057DBA40) == 0x057DC250); // OnRep_Armor, von Hand nachgetragen (opt-astronaut-ruestung)
    CHECK(Map(26669225, 0x057DCD60) == 0x057DD570); // OnRep_MaxArmor, von Hand nachgetragen (opt-astronaut-ruestung)
    CHECK(Map(26669225, 0x057DF8EB) == 0x057E00AB); // Klemmung der Rüstung in PostAttributeChange (opt-astronaut-ruestung)
    CHECK(Map(26669225, 0x05936BE0) == 0x05937870); // URunPersistencyManager::OnRoundChanged (client-runstand-abbruch)
    CHECK(Map(26669225, 0x0593EBA0) == 0x0593F830); // URunPersistencyManager::ResavePersistable (client-runstand-abbruch)
}

TEST_CASE("Adressen: Tabelle 26786272 ist sortiert, vollständig und deckt dieselben Adressen wie 26669225")
{
    const auto Entries = Table(26786272);
    REQUIRE_FALSE(Entries.empty());
    CHECK(Sorted(Entries));
    CHECK(Known(26786272));
    for (const Entry& Value : Entries)
    {
        CHECK(Value.Mapped != 0);
        CHECK(Map(26786272, Value.Reference) == Value.Mapped);
    }
    // Keine Adresse, die 26669225 kennt, fehlt auf 26786272.
    for (const Entry& Value : Table(26669225))
        CHECK(Map(26786272, Value.Reference) != 0);
    CHECK(Map(26786272, Entries.front().Reference + 1) == 0);
    CHECK(Map(26786272, 0x058B1130) == 0x058B25E0); // EndTackle, von Hand, Prolog geändert
    CHECK(Map(26786272, 0x05998D70) == 0x0599B984); // LootPawnRead, von Hand, Funktion umgebaut
    CHECK(Map(26786272, 0x058CE280) == 0x058CFD10); // TryBuyItem, über 26669225
    CHECK(Map(26786272, 0x093A1F98) == 0x093A7EF8); // vtable BackpackSlot, von Hand
    CHECK(Map(26786272, 0x0AA67B80) == 0x0AA6FC80); // GObjects
}
