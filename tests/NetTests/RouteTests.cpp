#include <doctest.h>
#include "Sync/World/RouteState.h"

using namespace Sync::RouteWire;

namespace
{
Game::RouteMap::HostRoute Sample()
{
    Game::RouteMap::HostRoute Value;
    Value.MapId = 0xFEDCBA9876543210ull;
    Value.Seed = -20795; // Seeds sind int32, auch negativ
    Value.ThreatIncrease = 2;
    Value.Current = {4, 0};
    Value.MissionIndex = 1;
    Value.CachedMissionIndex = 0;
    Value.EndOfChain = true;
    Value.Previous = {3, 2};
    Value.Chosen = {0, 2, 4, 4, 2};
    return Value;
}
}

TEST_CASE("Routenkarte: die Route des Hosts kommt unverändert an")
{
    const auto Value = Sample();
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == HeaderSize + Value.Chosen.size());
    const auto Back = Decode(Bytes);
    REQUIRE(Back.has_value());
    CHECK(*Back == Value);
}

TEST_CASE("Routenkarte: ohne vorige Route und ohne Wahl bleibt nur der Kopf")
{
    auto Value = Sample();
    Value.Previous = {};
    Value.Chosen.clear();
    Value.EndOfChain = false;
    const auto Bytes = Encode(Value);
    CHECK(Bytes.size() == HeaderSize);
    REQUIRE(Decode(Bytes).has_value());
    CHECK(*Decode(Bytes) == Value);
}

TEST_CASE("Routenkarte: kaputte Nachrichten werden abgelehnt")
{
    auto Invalid = Sample();
    Invalid.MapId = 0;
    CHECK(Encode(Invalid).empty());

    const auto Bytes = Encode(Sample());
    CHECK_FALSE(Decode(std::span(Bytes).first(Bytes.size() - 1)).has_value()); // Anzahl passt nicht zur Länge
    CHECK_FALSE(Decode(std::span(Bytes).first(HeaderSize - 1)).has_value());

    auto Flag = Bytes;
    Flag[32] = 2; // Ende der Kette nur 0 oder 1
    CHECK_FALSE(Decode(Flag).has_value());

    auto Group = Bytes;
    Group[19] = 0x80; // Gruppe negativ
    CHECK_FALSE(Decode(Group).has_value());

    auto Choice = Bytes;
    Choice[HeaderSize] = 200; // Knotenposition außerhalb
    CHECK_FALSE(Decode(Choice).has_value());
}
