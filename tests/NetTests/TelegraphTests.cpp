#include <doctest.h>
#include <limits>
#include "Sync/Enemies/TelegraphState.h"

using namespace Sync::Telegraph;
using Game::EnemyTelegraph;
using Game::TelegraphKind;

namespace
{
EnemyTelegraph Circle()
{
    EnemyTelegraph Value;
    Value.Kind = TelegraphKind::Circle;
    Value.X = 1234.5;
    Value.Y = -987.25;
    Value.Z = 40.0;
    Value.Delay = 0.25f;
    Value.Lifetime = 1.5f;
    Value.Size = 220.0f;
    Value.System = "/Game/VFX/NS_CircleTelegraph.NS_CircleTelegraph";
    Value.SizeName = "Size";
    Value.LifetimeName = "Lifetime";
    Value.DelayName = "Delay";
    return Value;
}

EnemyTelegraph Line(TelegraphKind Kind)
{
    EnemyTelegraph Value;
    Value.Kind = Kind;
    Value.Enemy = 0x123456789ull;
    Value.X = 10.0;
    Value.Percentage = Kind == TelegraphKind::LineUpdate ? 0.6f : 0.0f;
    return Value;
}
}

TEST_CASE("Warnanzeigen: Kreis und Strich kommen unverändert an")
{
    for (const EnemyTelegraph& Value : {Circle(), Line(TelegraphKind::LineStart), Line(TelegraphKind::LineUpdate), Line(TelegraphKind::LineEnd)})
    {
        const auto Bytes = Encode(Value);
        REQUIRE_FALSE(Bytes.empty());
        const auto Back = Decode(Bytes);
        REQUIRE(Back);
        CHECK(*Back == Value);
        for (size_t Size = 0; Size < Bytes.size(); ++Size)
            CHECK_FALSE(Decode(std::span(Bytes).first(Size)));
        auto Extra = Bytes;
        Extra.push_back(0);
        CHECK_FALSE(Decode(Extra));
    }
    CHECK(Encode(Line(TelegraphKind::LineEnd)).size() == FixedBytes + 4);
}

TEST_CASE("Warnanzeigen: kaputte oder unpassende Werte werden abgelehnt")
{
    auto NoSystem = Circle();
    NoSystem.System.clear();
    CHECK(Encode(NoSystem).empty());
    auto Long = Circle();
    Long.System.assign(MaxText + 1, 'a');
    CHECK(Encode(Long).empty());
    auto Control = Circle();
    Control.SizeName = "Si\nze";
    CHECK(Encode(Control).empty());
    auto Far = Circle();
    Far.X = MaxCoordinate * 2;
    CHECK(Encode(Far).empty());
    auto NaN = Circle();
    NaN.Lifetime = std::numeric_limits<float>::quiet_NaN();
    CHECK(Encode(NaN).empty());
    auto Negative = Circle();
    Negative.Delay = -1.0f;
    CHECK(Encode(Negative).empty());
    auto NoEnemy = Line(TelegraphKind::LineStart);
    NoEnemy.Enemy = 0;
    CHECK(Encode(NoEnemy).empty());
    auto LineWithText = Line(TelegraphKind::LineUpdate);
    LineWithText.System = "/Game/X.X";
    CHECK(Encode(LineWithText).empty());

    auto Bytes = Encode(Circle());
    Bytes[0] = 9; // unbekannte Art
    CHECK_FALSE(Decode(Bytes));
    Bytes[0] = 0;
    CHECK_FALSE(Decode(Bytes));
}
