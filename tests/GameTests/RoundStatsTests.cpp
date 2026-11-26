#include <doctest.h>
#include <cstring>
#include "Game/RoundStatsPolicy.h"

using namespace Game::RoundStats;

namespace
{
constexpr uint8_t Kills = 2, DamageDealt = 3;
}

TEST_CASE("Rundenwerte: der Client übernimmt nur abweichende und fehlende Werte des Hosts")
{
    const std::vector<Entry> Local = {{1, 0, Kills, GlobalPlayer, 4}, {1, 0, DamageDealt, GlobalPlayer, 900}};
    const std::vector<Entry> Host = {{1, 0, Kills, GlobalPlayer, 4}, {1, 0, DamageDealt, GlobalPlayer, 1500}, {1, 0, Kills, 1, 2}};
    const auto Writes = PlanWrites(Local, Host);
    REQUIRE(Writes.size() == 2);
    CHECK(Writes[0] == Entry{1, 0, DamageDealt, GlobalPlayer, 1500});
    CHECK(Writes[1] == Entry{1, 0, Kills, 1, 2});
}

TEST_CASE("Rundenwerte: eigene Werte, die der Host nicht hat, werden 0")
{
    // Etwa Zählungen aus einem früheren Run oder eigene Treffer des Clients an einem Tag, den der Host anders führt.
    const std::vector<Entry> Local = {{3, 0, Kills, GlobalPlayer, 7}, {3, 0, DamageDealt, 0, 0}};
    const std::vector<Entry> Host = {{1, 0, Kills, GlobalPlayer, 5}};
    const auto Writes = PlanWrites(Local, Host);
    REQUIRE(Writes.size() == 2);
    CHECK(Writes[0] == Entry{1, 0, Kills, GlobalPlayer, 5});
    CHECK(Writes[1] == Entry{3, 0, Kills, GlobalPlayer, 0});
}

TEST_CASE("Rundenwerte: hat der Host gekürzt, bleiben eigene Werte vor seinem ältesten Tag")
{
    std::vector<Entry> Host;
    for (size_t i = 0; i < MaxEntries; ++i)
        Host.push_back({static_cast<int32_t>(10 + i / TypeCount), 0, static_cast<uint8_t>(i % TypeCount), GlobalPlayer, 1});
    const std::vector<Entry> Local = {{2, 0, Kills, GlobalPlayer, 9}, {500, 0, Kills, 1, 4}};
    const auto Writes = PlanWrites(Local, Host);
    CHECK(std::find(Writes.begin(), Writes.end(), Entry{2, 0, Kills, GlobalPlayer, 0}) == Writes.end());
    CHECK(std::find(Writes.begin(), Writes.end(), Entry{500, 0, Kills, 1, 0}) != Writes.end());
}

TEST_CASE("Rundenwerte: gleiche Stände brauchen keine Schreibzugriffe")
{
    const std::vector<Entry> Same = {{1, 0, Kills, GlobalPlayer, 4}, {2, 1, 0, 0, 3}};
    CHECK(PlanWrites(Same, Same).empty());
}

TEST_CASE("Rundenwerte: Gesamtwert und Spielerwerte desselben Tags sind verschiedene Schlüssel")
{
    const std::vector<Entry> Values = {{1, 0, Kills, GlobalPlayer, 6}, {1, 0, Kills, 0, 4}, {1, 0, Kills, 1, 2}};
    CHECK(Valid(Values));
    CHECK_FALSE(Valid(std::vector<Entry>{{1, 0, Kills, 0, 4}, {1, 0, Kills, 0, 5}}));
}

TEST_CASE("Rundenwerte: unbekannte Typen, Spieler und Tage sind ungültig")
{
    CHECK(Valid(Entry{8, 0, TypeCount - 1, MaxPlayer, -5}));
    CHECK_FALSE(Valid(Entry{1, 0, TypeCount, GlobalPlayer, 0}));
    CHECK_FALSE(Valid(Entry{1, 0, Kills, -2, 0}));
    CHECK_FALSE(Valid(Entry{1, 0, Kills, MaxPlayer + 1, 0}));
    CHECK(Valid(Entry{1, -1, Kills, GlobalPlayer, 0}));
    CHECK_FALSE(Valid(Entry{-2, 0, Kills, GlobalPlayer, 0}));
    CHECK_FALSE(Valid(Entry{1, -2, Kills, GlobalPlayer, 0}));
    CHECK_FALSE(Valid(Entry{1, MaxSubMission + 1, Kills, GlobalPlayer, 0}));
}

TEST_CASE("Rundenwerte: zu viele Werte behalten die jüngsten Tage")
{
    std::vector<Entry> Values;
    for (int32_t Day = 0; Day < 400; ++Day)
        for (uint8_t Type = 0; Type < TypeCount; ++Type)
            Values.push_back({Day, 0, Type, GlobalPlayer, Day});
    Values.push_back({5, 0, TypeCount, GlobalPlayer, 1}); // ungültig, fällt weg
    const auto Kept = Limit(Values);
    REQUIRE(Kept.size() == MaxEntries);
    CHECK(Valid(Kept));
    int32_t Oldest = MaxDay;
    for (const auto& Value : Kept)
        Oldest = std::min(Oldest, Value.Day);
    CHECK(Oldest == 400 - int32_t(MaxEntries / TypeCount) - 1);
    CHECK(std::find(Kept.begin(), Kept.end(), Entry{399, 0, Kills, GlobalPlayer, 399}) != Kept.end());
}

TEST_CASE("Rundenwerte: doppelte Schlüssel vom Speicher bleiben einmal")
{
    const auto Kept = Limit({{1, 0, Kills, GlobalPlayer, 3}, {1, 0, Kills, GlobalPlayer, 3}});
    CHECK(Kept.size() == 1);
}

TEST_CASE("Rundenwerte: die Übersicht wartet auf die Werte zu diesem Ende, höchstens MaxWaitMs")
{
    CHECK(ClientMayOpenRoundEnd(7, 7, 0));
    CHECK_FALSE(ClientMayOpenRoundEnd(7, 0, 0));
    CHECK_FALSE(ClientMayOpenRoundEnd(7, 6, MaxWaitMs - 1)); // Werte eines alten Endes zählen nicht
    CHECK(ClientMayOpenRoundEnd(7, 6, MaxWaitMs));
    CHECK(ClientMayOpenRoundEnd(0, 0, 0));
}

TEST_CASE("Rundenwerte: Kopf und Element des Containers wie im Spiel")
{
    SetHeader Set{};
    Set.Num = 3;
    Set.Max = 4;
    Set.NumBits = 3;
    Set.MaxBits = 128;
    Set.NumFree = 1;
    Set.Data = 0x1000;
    Set.InlineBits[0] = 0b101;
    CHECK(Plausible(Set));
    CHECK(Allocated(Set.InlineBits, 4, 0));
    CHECK_FALSE(Allocated(Set.InlineBits, 4, 1));
    CHECK(Allocated(Set.InlineBits, 4, 2));
    CHECK_FALSE(Allocated(Set.InlineBits, 4, 200));
    Set.Num = Set.NumBits = 200; // mehr als die Inline-Bits, ohne zweiten Speicher
    Set.Max = Set.MaxBits = 256;
    CHECK_FALSE(Plausible(Set));
    Set.Num = 2;
    CHECK_FALSE(Plausible(Set)); // NumBits passt nicht zu Num

    uint8_t Element[StatElementSize] = {};
    const int32_t Day = 4, Sub = 1;
    const int64_t Value = -1234567890123;
    std::memcpy(Element, &Day, 4);
    std::memcpy(Element + 4, &Sub, 4);
    Element[8] = DamageDealt;
    std::memcpy(Element + 16, &Value, 8);
    CHECK(DecodeStatElement(Element, 2) == Entry{4, 1, DamageDealt, 2, Value});
}
