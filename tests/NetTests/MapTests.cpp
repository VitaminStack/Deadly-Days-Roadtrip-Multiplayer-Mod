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
