#include <algorithm>
#include <doctest.h>
#include "Sync/Inventory/RewardState.h"

using namespace Sync::Reward;

TEST_CASE("Reward offer round-trips with names and flags")
{
    Offer Value{5, 77, 3, true, false, true, {{"Feuerschuhe (Selten)", "DA_FireShoes", "Rare", false},
                                       {"Kugel", "DA_Ball", "", true},
                                       {"\xC3\x9C" "berlebenspaket", "DA_Kit", "Common", false}}};
    const auto Bytes = Encode(Value);
    REQUIRE_FALSE(Bytes.empty());
    const auto Back = DecodeOffer(Bytes);
    REQUIRE(Back);
    CHECK(*Back == Value);
    const Offer Closed{};
    const auto ClosedBytes = Encode(Closed);
    REQUIRE(ClosedBytes.size() == 22);
    CHECK(DecodeOffer(ClosedBytes) == Closed);
}

TEST_CASE("Reward offer rejects malformed payloads")
{
    Offer Value{5, 77, 3, false, false, false, {{"A", "DA", "R"}, {"B", "DB", "R"}}};
    auto Bytes = Encode(Value);
    CHECK_FALSE(DecodeOffer(std::span(Bytes).first(Bytes.size() - 1)));
    Bytes.push_back(0);
    CHECK_FALSE(DecodeOffer(Bytes));
    const Item A{"A", "DA", "R"};
    CHECK(Encode(Offer{5, 0, 1, false, false, false, {A}}).empty()); // session without revision
    CHECK(Encode(Offer{0, 0, 1, false, false, false, {A}}).empty()); // choices without session
    CHECK(Encode(Offer{5, 1, 1, false, false, false, {A, A, A, A, A}}).empty());
    CHECK(Encode(Offer{5, 1, 1, false, false, false, {{std::string(MaxName + 1, 'x'), "DA", "R"}}}).empty());
    CHECK(Encode(Offer{5, 1, 1, false, false, false, {{"A", std::string(MaxName + 1, 'x'), "R"}}}).empty());
    auto BadBroken = Encode(Value);
    BadBroken[22] = 2; // broken flag of the first choice
    CHECK_FALSE(DecodeOffer(BadBroken));
    auto Flags = Encode(Value);
    Flags[20] = 16;
    CHECK_FALSE(DecodeOffer(Flags));
}

TEST_CASE("Reward offer carries the client ready flag")
{
    Offer Value{5, 77, 3, false, false, true, {{"A", "DA", "R"}}};
    Value.ClientReady = true;
    const auto Bytes = Encode(Value);
    REQUIRE_FALSE(Bytes.empty());
    CHECK(Bytes[20] == 12);
    const auto Back = DecodeOffer(Bytes);
    REQUIRE(Back);
    CHECK(Back->ClientReady);
    CHECK(*Back == Value);
}

TEST_CASE("Reward ready round-trips and rejects malformed payloads")
{
    const Ready Value{5, 77, true};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == 17);
    CHECK(DecodeReady(Bytes) == Value);
    CHECK(DecodeReady(Encode(Ready{5, 77, false})) == Ready{5, 77, false});
    CHECK(Encode(Ready{0, 77, true}).empty());
    CHECK_FALSE(DecodeReady(std::span(Bytes).first(16)));
    auto Bad = Bytes;
    Bad[16] = 2;
    CHECK_FALSE(DecodeReady(Bad));
    auto NoSession = Bytes;
    std::fill(NoSession.begin(), NoSession.begin() + 8, uint8_t{0});
    CHECK_FALSE(DecodeReady(NoSession));
}

TEST_CASE("Lager am Rundenende: Stand des Clients round-trips and rejects malformed payloads")
{
    const RoundEndClient Value{0x1122334455667788ull, true};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == 9);
    CHECK(DecodeRoundEndClient(Bytes) == Value);
    CHECK(DecodeRoundEndClient(Encode(RoundEndClient{9, false})) == RoundEndClient{9, false});
    CHECK(Encode(RoundEndClient{0, true}).empty());
    CHECK_FALSE(DecodeRoundEndClient(std::span(Bytes).first(8)));
    auto Bad = Bytes;
    Bad[8] = 2;
    CHECK_FALSE(DecodeRoundEndClient(Bad));
    std::vector<uint8_t> NoId(9, 0);
    CHECK_FALSE(DecodeRoundEndClient(NoId));
}

TEST_CASE("Lager am Rundenende: Stand des Hosts round-trips, kein Ende ist ohne Flags")
{
    for (const RoundEndStatus Value : {RoundEndStatus{7, false, false}, RoundEndStatus{7, true, false}, RoundEndStatus{7, false, true},
                                       RoundEndStatus{7, true, true}, RoundEndStatus{}})
    {
        const auto Bytes = Encode(Value);
        REQUIRE(Bytes.size() == 9);
        CHECK(DecodeRoundEndStatus(Bytes) == Value);
    }
    CHECK(Encode(RoundEndStatus{0, true, false}).empty());
    auto Bytes = Encode(RoundEndStatus{7, true, true});
    CHECK(Bytes[8] == 3);
    Bytes[8] = 4;
    CHECK_FALSE(DecodeRoundEndStatus(Bytes));
    std::vector<uint8_t> FlagsWithoutId(9, 0);
    FlagsWithoutId[8] = 1;
    CHECK_FALSE(DecodeRoundEndStatus(FlagsWithoutId));
    CHECK_FALSE(DecodeRoundEndStatus(std::span(Bytes).first(8)));
}

namespace
{
Game::InventoryEdit::Edit SampleEdit()
{
    using namespace Game::InventoryEdit;
    Edit Value;
    Value.Id = 0x1122334455667788ull;
    Value.Session = 5;
    Value.Revision = 77;
    Value.Choice = uint8_t{2};
    Value.NewSlots = {{3, -1}, {2, 0}};
    ItemState Moved{4, NameHash("DA_Revolver"), true, 0, true, false, 1, {0, 1}, {{0, 1}, {1, 1}}};
    ItemState Bag{7, NameHash("DA_Bag"), false, 3, false, false, 0, {}, {}};
    ItemState Picked{ChoiceSource, NameHash("DA_Pistol"), true, 0, true, true, -1, {3, -1}, {{3, -1}}, true};
    Value.Items = {Moved, Bag, Picked};
    return Value;
}
}

TEST_CASE("Inventory edit round-trips with choice, new slots and removed items")
{
    const auto Value = SampleEdit();
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == 26 + 2 * 8 + 1 + 3 * 21 + 3 * 8);
    const auto Back = DecodeEdit(Bytes);
    REQUIRE(Back);
    CHECK(*Back == Value);

    Game::InventoryEdit::Edit Plain;
    Plain.Id = 9;
    Plain.Items = {{0, 1, true, 0, false, true, 0, {}, {}}};
    const auto PlainBack = DecodeEdit(Encode(Plain));
    REQUIRE(PlainBack);
    CHECK(*PlainBack == Plain);
    CHECK_FALSE(PlainBack->Choice);
}

TEST_CASE("Inventory edit rejects malformed payloads")
{
    const auto Bytes = Encode(SampleEdit());
    CHECK_FALSE(DecodeEdit(std::span(Bytes).first(Bytes.size() - 1)));
    auto Longer = Bytes;
    Longer.push_back(0);
    CHECK_FALSE(DecodeEdit(Longer));
    CHECK_FALSE(DecodeEdit(std::span(Bytes).first(20)));

    auto NoId = SampleEdit();
    NoId.Id = 0;
    CHECK(Encode(NoId).empty());
    auto Doubled = SampleEdit();
    Doubled.Items[0].Tiles.push_back({3, -1}); // Feld der Wahl doppelt belegt
    CHECK(Encode(Doubled).empty());
    auto ChoiceWithoutSession = SampleEdit();
    ChoiceWithoutSession.Session = 0;
    CHECK(Encode(ChoiceWithoutSession).empty());

    auto BadFlags = Bytes;
    BadFlags[26 + 2 * 8 + 1 + 6] = 8;
    CHECK_FALSE(DecodeEdit(BadFlags));
}

TEST_CASE("Inventory edit result is exact")
{
    const Result Value{42, true};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == 9);
    CHECK(DecodeResult(Bytes) == Value);
    CHECK(DecodeResult(Encode(Result{42, false})) == Result{42, false});
    CHECK(Encode(Result{0, true}).empty());
    auto Bad = Bytes;
    Bad[8] = 2;
    CHECK_FALSE(DecodeResult(Bad));
    CHECK_FALSE(DecodeResult(std::span(Bytes).first(8)));
}

TEST_CASE("Reward names are clipped on UTF-8 boundaries")
{
    std::string Long(MaxName - 1, 'a');
    Long += "\xC3\xA4\xC3\xA4"; // two-byte characters across the limit
    const auto Clipped = Clip(Long);
    CHECK(Clipped.size() <= MaxName);
    CHECK(Clipped.size() == MaxName - 1);
    CHECK(Clip("kurz") == "kurz");
}
