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
    Flags[20] = 8;
    CHECK_FALSE(DecodeOffer(Flags));
}

TEST_CASE("Reward choice is exact and bounded")
{
    const Choice Value{5, 77, 2};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == 17);
    CHECK(DecodeChoice(Bytes) == Value);
    CHECK(Encode(Choice{5, 77, MaxChoices}).empty());
    CHECK(Encode(Choice{0, 77, 0}).empty());
    auto Bad = Bytes;
    Bad[16] = 9;
    CHECK_FALSE(DecodeChoice(Bad));
    CHECK_FALSE(DecodeChoice(std::span(Bytes).first(16)));
}

TEST_CASE("Reward placement round-trips and is bounded")
{
    const Placement Value{5, 1, -2, 3, {{0, 1}, {1, 1}, {-1, 7}}};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == 21 + 3 * 8);
    CHECK(DecodePlacement(Bytes) == Value);
    CHECK_FALSE(DecodePlacement(std::span(Bytes).first(Bytes.size() - 1)));
    CHECK(Encode(Placement{0, 0, 0, 0, {{0, 0}}}).empty());
    CHECK(Encode(Placement{5, 0, 0, 0, {}}).empty());
    CHECK(Encode(Placement{5, 0, 0, 0, std::vector<std::pair<int32_t, int32_t>>(MaxSlots + 1)}).empty());
    auto Bad = Bytes;
    Bad[20] = 0;
    CHECK_FALSE(DecodePlacement(Bad));
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

TEST_CASE("Reward placement is not consumed before its choice is sent")
{
    PlacementDelivery Delivery;
    unsigned Taken = 0, Sent = 0;
    const Placement Value{5, 1, 2, 3, {{2, 3}, {3, 3}}};
    auto Take = [&](uint64_t Session) -> std::optional<Placement>
    {
        CHECK(Session == Value.Session);
        ++Taken;
        return Value;
    };
    auto Send = [&](const Placement& Out)
    {
        CHECK(Out == Value);
        ++Sent;
        return true;
    };
    Delivery.Flush(0, Take, Send);
    CHECK(Taken == 0);
    CHECK(Sent == 0);
    Delivery.Flush(Value.Session, Take, Send);
    CHECK(Taken == 1);
    CHECK(Sent == 1);
    CHECK_FALSE(Delivery.Pending);
}

TEST_CASE("Reward placement survives send failure and a closed offer")
{
    PlacementDelivery Delivery;
    unsigned Taken = 0, Attempts = 0;
    const Placement Value{5, 2, 1, 4, {{1, 4}}};
    auto Take = [&](uint64_t) -> std::optional<Placement>
    {
        ++Taken;
        return Value;
    };
    auto Send = [&](const Placement& Out)
    {
        CHECK(Out == Value);
        return ++Attempts == 2;
    };
    Delivery.Flush(5, Take, Send);
    REQUIRE(Delivery.Pending);
    CHECK(*Delivery.Pending == Value);
    Delivery.Flush(0, Take, Send); // Das Panel des Hosts ist inzwischen geschlossen.
    CHECK(Taken == 1);
    CHECK(Attempts == 2);
    CHECK_FALSE(Delivery.Pending);
    Delivery.Flush(0, Take, Send);
    CHECK(Attempts == 2);
}
