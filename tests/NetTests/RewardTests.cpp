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
    // Id 0 heißt verlassen und geht nur ohne bereit.
    std::vector<uint8_t> Left(9, 0);
    CHECK(DecodeRoundEndClient(Left) == RoundEndClient{0, false});
    CHECK(Encode(RoundEndClient{0, false}) == Left);
    Left[8] = 1;
    CHECK_FALSE(DecodeRoundEndClient(Left));
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
    REQUIRE(Bytes.size() == 26 + 2 * 8 + 1 + 3 * 21 + 3 * 8 + 1 + 1);
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

TEST_CASE("Inventory edit carries upgrades with inputs, names and output")
{
    using namespace Game::InventoryEdit;
    Edit Value;
    Value.Id = 77;
    Value.Items = {{0, NameHash("ItemData_Pistol"), false, 3, false, false, 0, {}, {}}, {1, NameHash("ItemData_Pistol"), false, 3, false, false, 0, {}, {}}};
    Upgrade Next;
    Next.Inputs = {1, 0};
    Next.Data = "ItemData_Pistol";
    Next.Rarity = "Rarity_Uncommon";
    Next.Output = {UpgradeSource(0), NameHash(Next.Data), true, 0, true, false, 1, {0, 0}, {{0, 0}, {0, 1}}};
    Value.Upgrades = {Next};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == 26 + 1 + 2 * 21 + 1 + 1 + 2 * 2 + 1 + 15 + 1 + 15 + 21 + 2 * 8 + 1);
    const auto Back = DecodeEdit(Bytes);
    REQUIRE(Back);
    CHECK(*Back == Value);
    CHECK_FALSE(DecodeEdit(std::span(Bytes).first(Bytes.size() - 1)));

    auto LongName = Value;
    LongName.Upgrades[0].Rarity = std::string(MaxObjectName + 1, 'x');
    CHECK(Encode(LongName).empty());
    auto BadInputs = Bytes;
    BadInputs[26 + 1 + 2 * 21 + 1] = static_cast<uint8_t>(MaxUpgradeInputs + 1);
    CHECK_FALSE(DecodeEdit(BadInputs));
}

TEST_CASE("Rarities round-trip and reject malformed payloads")
{
    Rarities Value;
    Value.Items = {{0, 11, "Rarity_0_Common"}, {3, 12, ""}};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == 1 + 6 + 1 + 15 + 6 + 1);
    const auto Back = DecodeRarities(Bytes);
    REQUIRE(Back);
    CHECK(*Back == Value);
    CHECK_FALSE(DecodeRarities(std::span(Bytes).first(Bytes.size() - 1)));
    auto Longer = Bytes;
    Longer.push_back(0);
    CHECK_FALSE(DecodeRarities(Longer));
    CHECK_FALSE(DecodeRarities({}));
    Rarities TooMany;
    TooMany.Items.resize(MaxRarityItems + 1);
    CHECK(Encode(TooMany).empty());
    Rarities LongName;
    LongName.Items = {{0, 1, std::string(Game::InventoryEdit::MaxObjectName + 1, 'x')}};
    CHECK(Encode(LongName).empty());
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

TEST_CASE("lager-handel-client: Edit trägt kaputt und verkauft, Flags über 31 sind ungültig")
{
    using namespace Game::InventoryEdit;
    Edit Value;
    Value.Id = 11;
    ItemState Repaired{2, NameHash("DA_Shotgun"), true, 0, true, false, 0, {0, 0}, {{0, 0}}};
    ItemState StillBroken{3, NameHash("DA_Knife"), true, 0, false, false, 0, {}, {}};
    StillBroken.Broken = true;
    ItemState Sold{4, NameHash("DA_Pistol"), false, SoldForScrap, false, false, 0, {}, {}};
    Value.Items = {Repaired, StillBroken, Sold};
    const auto Bytes = Encode(Value);
    REQUIRE_FALSE(Bytes.empty());
    const auto Back = DecodeEdit(Bytes);
    REQUIRE(Back);
    CHECK(*Back == Value);
    CHECK(Back->Items[1].Broken);
    CHECK(IsSold(Back->Items[2]));

    // Flags des zweiten Items: 26 Byte Kopf, 1 Byte Anzahl, erstes Item 21 + 8.
    auto BadFlags = Bytes;
    const size_t Flags = 26 + 1 + 21 + 8 + 6;
    REQUIRE(BadFlags[Flags] == 17); // vorhanden und kaputt
    BadFlags[Flags] = 33;
    CHECK_FALSE(DecodeEdit(BadFlags));
    auto BadContext = Bytes;
    BadContext[26 + 1 + 21 + 8 + 21 + 7] = 4; // RemoveContext über UsedAsIngredient
    CHECK_FALSE(DecodeEdit(BadContext));
}

TEST_CASE("lager-handel-client: Schrott round-trips und lehnt ungültige Stände ab")
{
    for (const int32_t Amount : {0, 1, 1234, MaxScrap})
    {
        const auto Bytes = Encode(Scrap{Amount});
        REQUIRE(Bytes.size() == 4);
        const auto Back = DecodeScrap(Bytes);
        REQUIRE(Back);
        CHECK(Back->Amount == Amount);
    }
    CHECK(Encode(Scrap{-1}).empty());
    CHECK(Encode(Scrap{MaxScrap + 1}).empty());
    const std::vector<uint8_t> Negative{0xFF, 0xFF, 0xFF, 0xFF};
    CHECK_FALSE(DecodeScrap(Negative));
    const auto Bytes = Encode(Scrap{5});
    CHECK_FALSE(DecodeScrap(std::span(Bytes).first(3)));
    auto Longer = Bytes;
    Longer.push_back(0);
    CHECK_FALSE(DecodeScrap(Longer));
}

TEST_CASE("Inventory edit carries purchases with names, cost and output")
{
    using namespace Game::InventoryEdit;
    Edit Value;
    Value.Id = 78;
    Purchase Next;
    Next.Data = "ItemData_Pistol";
    Next.Rarity = "Rarity_Uncommon";
    Next.Cost = 37;
    Next.Output = {PurchaseSource(0), NameHash(Next.Data), true, 0, true, false, 1, {0, 0}, {{0, 0}, {0, 1}}};
    Value.Purchases = {Next};
    const auto Bytes = Encode(Value);
    REQUIRE(Bytes.size() == 26 + 1 + 1 + 1 + 1 + 15 + 1 + 15 + 4 + 21 + 2 * 8);
    const auto Back = DecodeEdit(Bytes);
    REQUIRE(Back);
    CHECK(*Back == Value);
    CHECK_FALSE(DecodeEdit(std::span(Bytes).first(Bytes.size() - 1)));

    auto Negative = Value;
    Negative.Purchases[0].Cost = -5;
    CHECK(Encode(Negative).empty());
    auto TooMany = Bytes;
    TooMany[26 + 1 + 1] = static_cast<uint8_t>(MaxPurchases + 1);
    CHECK_FALSE(DecodeEdit(TooMany));
    // Ein Edit im Format vor Mod-Version 13 (ohne Anzahl der Käufe) ist ungültig.
    Edit Old;
    Old.Id = 79;
    Old.Items = {{0, 5, true, 0, false, true, 0, {}, {}}};
    auto OldBytes = Encode(Old);
    REQUIRE(!OldBytes.empty());
    OldBytes.pop_back();
    CHECK_FALSE(DecodeEdit(OldBytes));
}
