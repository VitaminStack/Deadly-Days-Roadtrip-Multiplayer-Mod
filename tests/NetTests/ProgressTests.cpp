#include <doctest.h>
#include <limits>
#include "Sync/ProgressState.h"

using namespace Sync::Progress;

TEST_CASE("XP-Snapshot ist absolut und streng validiert")
{
    const State Value{10, 11, 1, 3, 0.25f, "DDDynamic_Map", 668};
    const auto Bytes = Encode(Value);
    REQUIRE(Decode(Bytes));
    CHECK(*Decode(Bytes) == Value);
    for (size_t Size = 0; Size < Bytes.size(); ++Size)
        CHECK_FALSE(Decode(std::span(Bytes).first(Size)));
    auto Extra = Bytes;
    Extra.push_back(0);
    CHECK_FALSE(Decode(Extra));
    auto Bad = Value;
    for (const float Fraction : {-1.f, 1.01f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
    {
        Bad.Fraction = Fraction;
        CHECK(Encode(Bad).empty());
    }
    Bad = Value;
    Bad.Level = 0;
    CHECK(Encode(Bad).empty());
    Bad = Value;
    Bad.Map = "untrusted/path";
    CHECK(Encode(Bad).empty());
}

TEST_CASE("XP-Anzeige verwirft doppelte und alte Zustaende statt Gewinne erneut zu addieren")
{
    Mirror View;
    View.Bind(7, 10, 11);
    Net::Protocol::Control Message{50, 1, 1, Snapshot, Encode({10, 11, 1, 3, 0.25f, "DDDynamic_Map", 668})};
    REQUIRE(View.Accept(7, Message, "DDDynamic_Map", 1, 668));
    CHECK_FALSE(View.Accept(7, Message, "DDDynamic_Map", 1, 668));
    Message.Revision = 2;
    CHECK_FALSE(View.Accept(8, Message, "DDDynamic_Map", 1, 668));
    CHECK_FALSE(View.Accept(7, Message, "MetaBase", 1, 668));
    CHECK_FALSE(View.Accept(7, Message, "DDDynamic_Map", 2, 668));
    CHECK_FALSE(View.Accept(7, Message, "DDDynamic_Map", 1, 670));
    CHECK(View.Accept(7, Message, "DDDynamic_Map", 1, 668));
    REQUIRE(View.Value);
    CHECK(View.Value->Fraction == 0.25f); // Keine zweite Gutschrift bei neuem Vollbild.
    Message.Run = 51;
    Message.Revision = 3;
    CHECK_FALSE(View.Accept(7, Message, "DDDynamic_Map", 1, 668));
}

TEST_CASE("Reise und Reconnect verlangen ein neues angefordertes Vollbild")
{
    Mirror View;
    View.Bind(7, 10, 11);
    Net::Protocol::Control Old{50, 1, 10, Snapshot, Encode({10, 11, 1, 3, 0.25f, "DDDynamic_Map", 668})};
    REQUIRE(View.Accept(7, Old, "DDDynamic_Map", 1, 668));
    View.Bind(7, 10, 12);
    CHECK_FALSE(View.Value);
    Old.Revision = 11;
    CHECK_FALSE(View.Accept(7, Old, "DDDynamic_Map", 1, 668));
    Net::Protocol::Control New{50, 2, 12, Snapshot, Encode({10, 12, 2, 4, 0.5f, "DDDynamic_Map", 888})};
    REQUIRE(View.Accept(7, New, "DDDynamic_Map", 2, 888));
    Old.Payload = New.Payload;
    Old.Revision = 13;
    CHECK_FALSE(View.Accept(7, Old, "DDDynamic_Map", 2, 888)); // Alte Welt trotz hoeherer Revision.
    View.Bind(8, 10, 13);
    CHECK_FALSE(View.Value);
    CHECK_FALSE(View.Accept(7, New, "DDDynamic_Map", 2, 888));
    New.Run = 99;
    New.Epoch = New.Revision = 1;
    New.Payload = Encode({10, 13, 2, 4, 0.5f, "DDDynamic_Map", 888});
    REQUIRE(View.Accept(8, New, "DDDynamic_Map", 2, 888));
}

TEST_CASE("Airdrops sind absolute netGUID-gebundene Anzeigezustaende, keine Reward-Anfragen")
{
    State Value{10, 11, 1, 3, 0.25f, "DDDynamic_Map", 668};
    Value.AirdropsKnown = true;
    Value.Airdrops = {{1002, false, true, true, true, 0.5f}, {1004, true, true, true, false, 2.0f}};
    Value.Airdrops[1].Hidden = true;
    const auto Bytes = Encode(Value);
    REQUIRE(Decode(Bytes));
    CHECK(*Decode(Bytes) == Value);
    for (size_t Size = 0; Size < Bytes.size(); ++Size)
        CHECK_FALSE(Decode(std::span(Bytes).first(Size)));
    auto Bad = Value;
    Bad.Airdrops.push_back(Bad.Airdrops[0]);
    CHECK(Encode(Bad).empty());
    Bad = Value;
    Bad.Airdrops[0].Guid = 0;
    CHECK(Encode(Bad).empty());
    Bad = Value;
    Bad.Airdrops[0].Position = std::numeric_limits<float>::quiet_NaN();
    CHECK(Encode(Bad).empty());
    Bad = Value;
    Bad.AirdropsKnown = false;
    CHECK(Encode(Bad).empty());
    auto InvalidFlags = Bytes;
    const size_t FlagsAt = 37 + Value.Map.size() + 2 + 8;
    InvalidFlags[FlagsAt] |= 0x80;
    CHECK_FALSE(Decode(InvalidFlags));
    Value.Airdrops.clear();
    REQUIRE(Decode(Encode(Value)));
    CHECK(Decode(Encode(Value))->AirdropsKnown);
}

TEST_CASE("XP-Baelle: Ausstoss-Bild ist begrenzt und gebunden")
{
    const OrbBatch Value{10, 11, 668, 1, {{5, {1.5, -2, 100}, {1.5, -2, 100}, {1, 0.5f, 0, 1}, 0.25, -1.f}, {200}}};
    const auto Bytes = EncodeOrbs(Value);
    REQUIRE(DecodeOrbs(Bytes));
    CHECK(*DecodeOrbs(Bytes) == Value);
    for (size_t Size = 0; Size < Bytes.size(); ++Size)
        CHECK_FALSE(DecodeOrbs(std::span(Bytes).first(Size)));
    auto Bad = Value;
    Bad.Spawns[0].Amount = 0;
    CHECK(EncodeOrbs(Bad).empty());
    Bad = Value;
    Bad.Spawns[0].End[2] = std::numeric_limits<double>::quiet_NaN();
    CHECK(EncodeOrbs(Bad).empty());
    Bad = Value;
    Bad.Spawns.assign(MaxOrbs + 1, {1});
    CHECK(EncodeOrbs(Bad).empty());
    Bad.Spawns.clear();
    CHECK(EncodeOrbs(Bad).empty());

    Mirror View;
    View.Bind(7, 10, 11);
    const Net::Protocol::Control Message{3, 1, 5, Orbs, Bytes};
    CHECK_FALSE(View.AcceptOrbs(8, Message, 1, 668));          // fremde Verbindung
    CHECK_FALSE(View.AcceptOrbs(7, Message, 2, 668));          // anderer Tag
    CHECK_FALSE(View.AcceptOrbs(7, Message, 1, 669));          // anderer GameState
    REQUIRE(View.AcceptOrbs(7, Message, 1, 668));
    CHECK(View.AcceptOrbs(7, Message, 1, 668) == std::nullopt); // Wiederholung
    View.Bind(7, 10, 12);
    const Net::Protocol::Control Later{3, 1, 6, Orbs, Bytes};
    CHECK_FALSE(View.AcceptOrbs(7, Later, 1, 668));            // alte Lebensdauer des Clients
}
