#include <doctest.h>
#include "Sync/World/MapState.h"

using namespace Sync::MapWire;
using Game::Map::Graph;
using Game::Map::HostMap;

namespace
{
HostMap Sample()
{
    return {0x1234567890ABCDEFull, -42, 7, 9, "/Game/Map/MGP_Forest.MGP_Forest", {"Biome.Forest", "Mission.Rescue"}, Graph::Set,
            "/Game/PCG/PCG_Forest", "PCG_Forest"};
}
}

TEST_CASE("Karte: Werte des Hosts kommen unverändert an")
{
    for (const Graph State : {Graph::Unknown, Graph::None, Graph::Set})
    {
        HostMap Value = Sample();
        Value.GraphState = State;
        if (State != Graph::Set)
            Value.GraphPackage = Value.GraphAsset = "";
        const auto Bytes = Encode(Value);
        REQUIRE_FALSE(Bytes.empty());
        const auto Back = Decode(Bytes);
        REQUIRE(Back);
        REQUIRE(*Back);
        CHECK(**Back == Value);
    }
    HostMap NoTags = Sample();
    NoTags.Tags.clear();
    CHECK(**Decode(Encode(NoTags)) == NoTags);
}

TEST_CASE("Karte: keine Karte ist eine eigene, kurze Nachricht")
{
    const auto Bytes = Encode(std::nullopt);
    REQUIRE(Bytes.size() == 8);
    const auto Back = Decode(Bytes);
    REQUIRE(Back);
    CHECK_FALSE(*Back);
    auto Longer = Bytes;
    Longer.push_back(0);
    CHECK_FALSE(Decode(Longer));
}

TEST_CASE("Karte: kaputte oder unvollständige Werte werden abgelehnt")
{
    const auto Bytes = Encode(Sample());
    for (size_t Size = 0; Size < Bytes.size(); ++Size)
        CHECK_FALSE(Decode(std::span(Bytes).first(Size)));
    auto Extra = Bytes;
    Extra.push_back(0);
    CHECK_FALSE(Decode(Extra));
    auto BadGraph = Bytes;
    BadGraph[20 + 1 + Sample().Profile.size() + 1 + 1 + 12 + 1 + 14] = 3; // GraphState hinter Profil und Tags
    CHECK_FALSE(Decode(BadGraph));

    HostMap NoProfile = Sample();
    NoProfile.Profile.clear();
    CHECK(Encode(NoProfile).empty());
    HostMap LongText = Sample();
    LongText.Profile = std::string(MaxText + 1, 'x');
    CHECK(Encode(LongText).empty());
    HostMap ManyTags = Sample();
    ManyTags.Tags.assign(MaxTags + 1, "T");
    CHECK(Encode(ManyTags).empty());
    HostMap EmptyTag = Sample();
    EmptyTag.Tags.push_back("");
    CHECK(Encode(EmptyTag).empty());
    HostMap GraphWithoutName = Sample();
    GraphWithoutName.GraphAsset.clear();
    CHECK(Encode(GraphWithoutName).empty());
    HostMap NameWithoutGraph = Sample();
    NameWithoutGraph.GraphState = Graph::None;
    CHECK(Encode(NameWithoutGraph).empty());
    HostMap ZeroIdWithValues = Sample();
    ZeroIdWithValues.Id = 0;
    CHECK(Encode(ZeroIdWithValues).empty());
}

TEST_CASE("Rundenende: Karten-Id und Id kommen unverändert an, kein Ende ist eine eigene Nachricht")
{
    const Game::Map::RoundEnd End{0x1122334455667788ull, 0x99AABBCCDDEEFF01ull};
    const auto Bytes = EncodeRoundEnd(End);
    REQUIRE(Bytes.size() == 17);
    const auto Back = DecodeRoundEnd(Bytes);
    REQUIRE(Back.has_value());
    REQUIRE(Back->has_value());
    CHECK(**Back == End);
    CHECK((*Back)->Outcome == Game::Map::RunOutcome::None);

    const auto None = EncodeRoundEnd(std::nullopt);
    REQUIRE(None.size() == 17);
    const auto NoneBack = DecodeRoundEnd(None);
    REQUIRE(NoneBack.has_value());
    CHECK_FALSE(NoneBack->has_value());
}

TEST_CASE("Rundenende: Niederlage und Sieg kommen als Ausgang an")
{
    using Game::Map::RunOutcome;
    for (const RunOutcome Outcome : {RunOutcome::Lost, RunOutcome::Won})
    {
        const Game::Map::RoundEnd End{7, 9, Outcome};
        const auto Bytes = EncodeRoundEnd(End);
        REQUIRE(Bytes.size() == 17);
        CHECK(Bytes[16] == static_cast<uint8_t>(Outcome));
        const auto Back = DecodeRoundEnd(Bytes);
        REQUIRE(Back.has_value());
        REQUIRE(Back->has_value());
        CHECK(**Back == End);
    }
}

TEST_CASE("Rundenende: unbekannter Ausgang und Ausgang ohne Ids werden abgelehnt")
{
    using Game::Map::RunOutcome;
    auto Bytes = EncodeRoundEnd(Game::Map::RoundEnd{7, 9, RunOutcome::Won});
    Bytes[16] = 3;
    CHECK_FALSE(DecodeRoundEnd(Bytes).has_value());
    std::vector<uint8_t> OutcomeOnly(17, 0);
    OutcomeOnly[16] = 1;
    CHECK_FALSE(DecodeRoundEnd(OutcomeOnly).has_value());
    CHECK(EncodeRoundEnd(Game::Map::RoundEnd{0, 5, RunOutcome::Lost}).empty());
}

TEST_CASE("Rundenende: halbe oder falsch lange Nachrichten werden abgelehnt")
{
    CHECK(EncodeRoundEnd(Game::Map::RoundEnd{0, 5}).empty());
    CHECK(EncodeRoundEnd(Game::Map::RoundEnd{5, 0}).empty());
    auto Bytes = EncodeRoundEnd(Game::Map::RoundEnd{7, 9});
    Bytes.pop_back();
    CHECK_FALSE(DecodeRoundEnd(Bytes).has_value());
    std::vector<uint8_t> HalfZero(17, 0);
    HalfZero[0] = 1;
    CHECK_FALSE(DecodeRoundEnd(HalfZero).has_value());
    HalfZero = std::vector<uint8_t>(17, 0);
    HalfZero[8] = 1;
    CHECK_FALSE(DecodeRoundEnd(HalfZero).has_value());
}

TEST_CASE("Rundenwerte: Tageskennung und Werte des Hosts kommen unverändert an, gesamt und je Spieler")
{
    using Game::RoundStats::Entry;
    Game::Map::RoundStatsImage Image{0xABCDEF0123456789ull, true, {3, -1}};
    Image.Values.Entries = {Entry{1, 0, 2, Game::RoundStats::GlobalPlayer, 57}, Entry{1, 0, 2, 0, 40}, Entry{1, 0, 2, 3, 17},
                            Entry{8, 127, 10, 1, -3}, Entry{1, -1, 2, 2, 5}, Entry{-1, -1, 1, -1, 6},
                            Entry{100000, 0, 3, 7, 0x7FFFFFFFFFFFFFFFll}};
    const auto Bytes = EncodeRoundStats(Image);
    REQUIRE(Bytes.size() == RoundStatsHeader + Image.Values.Entries.size() * RoundStatsEntry);
    const auto Back = DecodeRoundStats(Bytes);
    REQUIRE(Back.has_value());
    CHECK(*Back == Image);
}

TEST_CASE("Rundenwerte: unlesbare Werte des Hosts sind eine kurze Nachricht ohne Tageskennung und Einträge")
{
    const Game::Map::RoundStatsImage Unknown{5, false};
    const auto Bytes = EncodeRoundStats(Unknown);
    CHECK(Bytes.size() == RoundStatsHeader);
    REQUIRE(DecodeRoundStats(Bytes).has_value());
    CHECK(*DecodeRoundStats(Bytes) == Unknown);
    CHECK(EncodeRoundStats(Game::Map::RoundStatsImage{5, false, {0, 0, {Game::RoundStats::Entry{1, 0, 2, -1, 3}}}}).empty());
    CHECK(EncodeRoundStats(Game::Map::RoundStatsImage{5, false, {2, 0}}).empty());
}

TEST_CASE("Rundenwerte: kaputte Nachrichten werden abgelehnt")
{
    using Game::RoundStats::Entry;
    CHECK(EncodeRoundStats(Game::Map::RoundStatsImage{0, true}).empty()); // ohne Rundenende
    CHECK(EncodeRoundStats(Game::Map::RoundStatsImage{5, true, {1, 0, {Entry{1, 0, 11, -1, 3}}}}).empty()); // Typ außerhalb
    CHECK(EncodeRoundStats(Game::Map::RoundStatsImage{5, true, {1, 0, {Entry{1, 0, 2, -1, 3}, Entry{1, 0, 2, -1, 4}}}}).empty());
    CHECK(EncodeRoundStats(Game::Map::RoundStatsImage{5, true, {-2, 0}}).empty()); // Tageskennung außerhalb
    auto Bytes = EncodeRoundStats(Game::Map::RoundStatsImage{5, true, {1, 0, {Entry{1, 0, 2, -1, 3}}}});
    REQUIRE_FALSE(Bytes.empty());
    auto Short = Bytes;
    Short.pop_back();
    CHECK_FALSE(DecodeRoundStats(Short).has_value());
    auto BadKnown = Bytes;
    BadKnown[8] = 2;
    CHECK_FALSE(DecodeRoundStats(BadKnown).has_value());
    auto BadPlayer = Bytes;
    BadPlayer[RoundStatsHeader + 6] = 0xFE; // Spieler -2
    CHECK_FALSE(DecodeRoundStats(BadPlayer).has_value());
    auto BadCount = Bytes;
    BadCount[14] = 2;
    CHECK_FALSE(DecodeRoundStats(BadCount).has_value());
}

TEST_CASE("Rundenwerte: die größte erlaubte Nachricht passt in den Kontrollkanal")
{
    Game::Map::RoundStatsImage Image{9, true, {300, 0}};
    for (size_t i = 0; i < Game::RoundStats::MaxEntries; ++i)
        Image.Values.Entries.push_back({static_cast<int32_t>(i / 11), 0, static_cast<uint8_t>(i % 11), Game::RoundStats::GlobalPlayer, 1});
    const auto Bytes = EncodeRoundStats(Image);
    REQUIRE_FALSE(Bytes.empty());
    CHECK(Bytes.size() <= 65536 - 26); // Nutzlast ohne die Control-Kennungen und den Typ
    Image.Values.Entries.push_back({999, 0, 0, Game::RoundStats::GlobalPlayer, 1});
    CHECK(EncodeRoundStats(Image).empty());
}