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
