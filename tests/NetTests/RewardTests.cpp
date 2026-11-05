#include <doctest.h>
#include "Sync/Inventory/RewardState.h"

using namespace Sync::Reward;

TEST_CASE("Reward offer round-trips with names and flags")
{
    Offer Value{5, 77, 3, true, false, true, {"Feuerschuhe (Selten)", "Kugel", "\xC3\x9C" "berlebenspaket"}};
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
    Offer Value{5, 77, 3, false, false, false, {"A", "B"}};
    auto Bytes = Encode(Value);
    CHECK_FALSE(DecodeOffer(std::span(Bytes).first(Bytes.size() - 1)));
    Bytes.push_back(0);
    CHECK_FALSE(DecodeOffer(Bytes));
    CHECK(Encode(Offer{5, 0, 1, false, false, false, {"A"}}).empty());          // session without revision
    CHECK(Encode(Offer{0, 0, 1, false, false, false, {"A"}}).empty());          // choices without session
    CHECK(Encode(Offer{5, 1, 1, false, false, false, {"A", "B", "C", "D", "E"}}).empty());
    CHECK(Encode(Offer{5, 1, 1, false, false, false, {std::string(MaxName + 1, 'x')}}).empty());
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

TEST_CASE("Reward names are clipped on UTF-8 boundaries")
{
    std::string Long(MaxName - 1, 'a');
    Long += "\xC3\xA4\xC3\xA4"; // two-byte characters across the limit
    const auto Clipped = Clip(Long);
    CHECK(Clipped.size() <= MaxName);
    CHECK(Clipped.size() == MaxName - 1);
    CHECK(Clip("kurz") == "kurz");
}
