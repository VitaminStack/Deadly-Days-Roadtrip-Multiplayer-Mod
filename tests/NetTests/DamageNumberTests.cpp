#include <doctest.h>
#include <limits>
#include <vector>
#include "Sync/Enemies/DamageNumberState.h"

using namespace Sync::DamageNumbers;
using Game::DamageNumberHit;

TEST_CASE("Schadenszahlen: Anzahl plus 25 Byte je Treffer, verlustfrei")
{
    const std::vector<DamageNumberHit> Hits = {{0x1234567890ABull, 12.5f, 3, -100.f, 2500.25f, 90.f}, {2, 0.f, 0, 0.f, 0.f, 0.f}};
    const auto Bytes = Encode(Hits);
    REQUIRE(Bytes.size() == 1 + HitBytes * Hits.size());
    CHECK(Bytes[0] == 2);
    const auto Back = Decode(Bytes);
    REQUIRE(Back);
    CHECK(*Back == Hits);
}

TEST_CASE("Schadenszahlen: Grenzen der Liste")
{
    CHECK(Encode({}).empty());
    std::vector<DamageNumberHit> Full(Game::MaxDamageNumberHits, DamageNumberHit{7, 1.f, 0, 1.f, 2.f, 3.f});
    const auto Bytes = Encode(Full);
    REQUIRE_FALSE(Bytes.empty());
    CHECK(Decode(Bytes)->size() == Game::MaxDamageNumberHits);
    Full.push_back(Full.front());
    CHECK(Encode(Full).empty());
}

TEST_CASE("Schadenszahlen: ungültige Treffer und falsche Längen werden abgelehnt")
{
    const DamageNumberHit Good{5, 10.f, 1, 1.f, 1.f, 1.f};
    const auto With = [&](auto Change) {
        DamageNumberHit Hit = Good;
        Change(Hit);
        return std::vector<DamageNumberHit>{Hit};
    };
    CHECK(Encode(With([](auto& H) { H.Guid = 0; })).empty());
    CHECK(Encode(With([](auto& H) { H.Guid = MaxGuid + 1; })).empty());
    CHECK(Encode(With([](auto& H) { H.Damage = -1.f; })).empty());
    CHECK(Encode(With([](auto& H) { H.Damage = std::numeric_limits<float>::quiet_NaN(); })).empty());
    CHECK(Encode(With([](auto& H) { H.Damage = std::numeric_limits<float>::infinity(); })).empty());
    CHECK(Encode(With([](auto& H) { H.X = 2.0e7f; })).empty());
    CHECK(Encode(With([](auto& H) { H.Z = std::numeric_limits<float>::quiet_NaN(); })).empty());

    const auto Bytes = Encode(std::vector<DamageNumberHit>{Good});
    REQUIRE(Bytes.size() == 1 + HitBytes);
    for (size_t Size = 0; Size < Bytes.size(); ++Size)
        CHECK_FALSE(Decode(std::span(Bytes).first(Size)));
    auto Extra = Bytes;
    Extra.push_back(0);
    CHECK_FALSE(Decode(Extra));
    auto ZeroCount = Bytes;
    ZeroCount[0] = 0;
    CHECK_FALSE(Decode(ZeroCount));
    auto TooMany = Bytes;
    TooMany[0] = static_cast<uint8_t>(Game::MaxDamageNumberHits + 1);
    CHECK_FALSE(Decode(TooMany));
    auto NoGuid = Bytes;
    for (size_t i = 1; i <= 8; ++i)
        NoGuid[i] = 0;
    CHECK_FALSE(Decode(NoGuid));
}

TEST_CASE("Schadenszahlen: Lücken in den Revisionen des Hosts zählen fehlende Nachrichten")
{
    RevisionGaps Gaps;
    CHECK(Gaps.See(5) == 0); // Erste Nachricht nach dem Beitritt, der Host zählt seit seinem Start.
    CHECK(Gaps.See(6) == 0);
    CHECK(Gaps.See(9) == 2);
    CHECK(Gaps.See(10) == 0);
    CHECK(Gaps.See(1) == 0); // Neuer Host, Zählung beginnt neu.
    CHECK(Gaps.See(2) == 0);
    CHECK(Gaps.See(2) == 0);
}
