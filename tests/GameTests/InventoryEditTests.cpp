#include <doctest.h>

#include "Game/InventoryEditPlan.h"

using namespace Game::InventoryEdit;

namespace
{
ItemState Placed(uint16_t Source, std::vector<Tile> Tiles, int32_t Turns = 0)
{
    ItemState Item;
    Item.Source = Source;
    Item.DataHash = 100 + Source;
    Item.InBackpack = true;
    Item.Turns = Turns;
    Item.Pivot = Tiles.front();
    Item.Tiles = Sorted(std::move(Tiles));
    return Item;
}

ItemState Stored(uint16_t Source)
{
    ItemState Item;
    Item.Source = Source;
    Item.DataHash = 100 + Source;
    return Item;
}

ItemState Gone(ItemState Item, uint8_t Context = 3)
{
    Item.Present = false;
    Item.InBackpack = false;
    Item.Tiles.clear();
    Item.RemoveContext = Context;
    return Item;
}
}

TEST_CASE("Inventar-Edit enthält nur geänderte Items, die Wahl und neue Felder")
{
    const std::vector<ItemState> Base{Placed(0, {{0, 0}, {1, 0}}), Stored(1), Placed(2, {{0, 1}})};
    ItemState Choice = Placed(0, {{3, -1}});
    Choice.Source = ChoiceSource;
    const std::vector<ItemState> Final{Placed(0, {{0, 0}, {1, 0}}), Gone(Base[1]), Placed(2, {{1, 1}}, 1), Choice};
    const Edit Value = Diff(Base, Final, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}, {{1, 1}, {0, 0}, {1, 0}, {0, 1}, {3, -1}, {2, 0}});
    REQUIRE(Value.Items.size() == 3);
    CHECK(Value.Items[0].Source == 1);
    CHECK_FALSE(Value.Items[0].Present);
    CHECK(Value.Items[1].Source == 2);
    CHECK(Value.Items[2].Source == ChoiceSource);
    CHECK(Value.NewSlots == std::vector<Tile>{{2, 0}, {3, -1}});
}

TEST_CASE("Gleiche Lage ignoriert Drehung und Pivot außerhalb des Rucksacks")
{
    ItemState A = Stored(1), B = Stored(1);
    B.Turns = 3;
    B.Pivot = {4, 4};
    CHECK(SamePlacement(A, B));
    CHECK(SameState(A, B));
    B.Trash = true;
    CHECK_FALSE(SameState(A, B));
    CHECK_FALSE(SamePlacement(Placed(0, {{0, 0}}), Placed(0, {{0, 0}}, 1)));
    CHECK(SameState(Gone(Placed(0, {{0, 0}})), Gone(Stored(0), 0)));
}

TEST_CASE("Inventar-Edit prüft Form, Grenzen und doppelte Felder")
{
    Edit Value;
    Value.Items = {Placed(0, {{0, 0}}), Placed(1, {{1, 0}})};
    CHECK(WellFormed(Value));
    Value.Items[1].Tiles.push_back({0, 0});
    CHECK_FALSE(WellFormed(Value));
    Value.Items = {Placed(0, {{0, 0}}), Placed(0, {{1, 0}})};
    CHECK_FALSE(WellFormed(Value)); // dieselbe Quelle zweimal
    Value.Items = {Stored(0)};
    Value.Items[0].Tiles = {{0, 0}};
    CHECK_FALSE(WellFormed(Value)); // Felder ohne Rucksack
    ItemState Choice = Stored(0);
    Choice.Source = ChoiceSource;
    Value.Items = {Choice};
    CHECK_FALSE(WellFormed(Value)); // Belohnung ohne Wahl
    Value.Choice = uint8_t{1};
    CHECK_FALSE(WellFormed(Value)); // Wahl ohne Sitzung
    Value.Session = 4;
    CHECK(WellFormed(Value));
    Value.NewSlots.resize(MaxNewSlots + 1);
    CHECK_FALSE(WellFormed(Value));
}

TEST_CASE("Host nimmt einen Edit nur an, wenn die Zielfelder frei sind oder neu entstehen")
{
    Edit Value;
    Value.Items = {Placed(0, {{0, 0}, {1, 0}})};
    CHECK(Fits(Value, {{0, 0}, {1, 0}}, {}));
    CHECK_FALSE(Fits(Value, {{0, 0}}, {}));
    Value.NewSlots = {{1, 0}};
    CHECK(Fits(Value, {{0, 0}}, {}));
    CHECK_FALSE(Fits(Value, {{0, 0}, {1, 0}}, {{1, 0}})); // von einem Item außerhalb des Edits belegt
    Value.Items = {Gone(Placed(0, {{0, 0}}))};
    CHECK(Fits(Value, {}, {{0, 0}}));
}

TEST_CASE("Plan nimmt erst alles aus dem Rucksack, dann Felder, dann neue Ablage")
{
    // Tausch zweier Items, ein Rucksack verbraucht, neue Felder.
    const std::vector<ItemState> Current{Placed(0, {{0, 0}}), Placed(1, {{1, 0}}), Stored(2)};
    const std::vector<ItemState> Target{Placed(0, {{1, 0}}), Placed(1, {{0, 0}}), Gone(Stored(2))};
    const auto Steps = Plan(Current, Target, true, false);
    const std::vector<Step> Expected{{StepKind::Unplace, 0}, {StepKind::Unplace, 1}, {StepKind::Remove, 2}, {StepKind::CreateSlots, 0},
                                     {StepKind::Place, 0},   {StepKind::Place, 1}};
    CHECK(Steps == Expected);
}

TEST_CASE("Plan ohne Änderung ist leer, Lager und Papierkorb ohne Rucksackschritte")
{
    const std::vector<ItemState> Same{Placed(0, {{0, 0}}), Stored(1)};
    CHECK(Plan(Same, Same, false, false).empty());
    ItemState Trashed = Stored(1);
    Trashed.Trash = true;
    const std::vector<ItemState> Target{Stored(0), Trashed};
    const std::vector<Step> Expected{{StepKind::Unplace, 0}, {StepKind::Trash, 1}};
    CHECK(Plan(Same, Target, false, false) == Expected);
}

TEST_CASE("Zurückrollen legt verbrauchte Items zurück und entfernt lokale Felder und Belohnungen")
{
    // Aktuell beim Client: Rucksack verbraucht, Wahl liegt auf einem neuen Feld. Ziel: Ausgangsstand.
    ItemState Choice = Placed(0, {{3, -1}});
    Choice.Source = ChoiceSource;
    ItemState ChoiceGone = Gone(Choice);
    const std::vector<ItemState> Current{Gone(Stored(0)), Choice};
    const std::vector<ItemState> Target{Stored(0), ChoiceGone};
    const std::vector<Step> Expected{{StepKind::Unplace, 1}, {StepKind::Remove, 1}, {StepKind::Add, 0}, {StepKind::DestroySlots, 0}};
    CHECK(Plan(Current, Target, false, true) == Expected);
}

TEST_CASE("Vorgemerkter Rucksackeffekt kommt in den Edit, ändert aber keinen Plan ohne neue Ablage")
{
    const std::vector<ItemState> Base{Placed(0, {{0, 0}})};
    ItemState Effect = Base[0];
    Effect.AddEffect = true;
    CHECK(Diff(Base, {Effect}, {{0, 0}}, {{0, 0}}).Items.size() == 1);
    CHECK(Plan(Base, {Effect}, false, false).empty());
    const std::vector<Step> Expected{{StepKind::Place, 0}};
    CHECK(Plan({Stored(0)}, {Effect}, false, false) == Expected);
}

TEST_CASE("Namensprüfsumme ist stabil und nie null")
{
    static_assert(NameHash("DA_Revolver") == NameHash("DA_Revolver"));
    CHECK(NameHash("DA_Revolver") != NameHash("DA_Pistol"));
    CHECK(NameHash("") != 0);
}

namespace
{
// Zwei gleiche Pistolen werden zu einer aufgewertet, die Ausgabe liegt auf dem Feld der zweiten.
Edit MergeEdit()
{
    Edit Value;
    Value.Items = {Gone(Placed(0, {{0, 0}})), Gone(Stored(1))};
    Upgrade Next;
    Next.Inputs = {1, 0};
    Next.Data = "ItemData_Pistol";
    Next.Rarity = "Rarity_Uncommon";
    Next.Output = Placed(0, {{0, 0}});
    Next.Output.Source = UpgradeSource(0);
    Next.Output.DataHash = NameHash(Next.Data);
    Value.Upgrades = {Next};
    return Value;
}
}

TEST_CASE("Aufwertung im Edit: Eingaben entfernt, Ausgabe mit eigener Quelle und Feldern")
{
    Edit Value = MergeEdit();
    CHECK(WellFormed(Value));
    CHECK(ConsumedSources(Value) == std::set<uint16_t>{0, 1});
    CHECK(Fits(Value, {{0, 0}}, {}));
    CHECK_FALSE(Fits(Value, {{0, 0}}, {{0, 0}})); // Feld der Ausgabe von einem fremden Item belegt

    auto Present = Value;
    Present.Items[0] = Placed(0, {{1, 0}});
    CHECK_FALSE(WellFormed(Present)); // Eingabe noch vorhanden
    auto Missing = Value;
    Missing.Items.pop_back();
    CHECK_FALSE(WellFormed(Missing)); // Eingabe fehlt im Edit
    auto Twice = Value;
    Twice.Upgrades[0].Inputs = {0, 0};
    CHECK_FALSE(WellFormed(Twice));
    auto WrongSource = Value;
    WrongSource.Upgrades[0].Output.Source = UpgradeSource(1);
    CHECK_FALSE(WellFormed(WrongSource));
    auto WrongHash = Value;
    WrongHash.Upgrades[0].Data = "ItemData_Revolver";
    CHECK_FALSE(WellFormed(WrongHash));
    auto Overlap = Value;
    Overlap.Items.push_back(Placed(2, {{0, 0}}));
    CHECK_FALSE(WellFormed(Overlap)); // Ausgabe und Item auf demselben Feld
    auto ItemWithUpgradeSource = Value;
    ItemWithUpgradeSource.Items.push_back(Stored(UpgradeSource(0)));
    CHECK_FALSE(WellFormed(ItemWithUpgradeSource));
    auto TooMany = Value;
    TooMany.Upgrades[0].Inputs = {0, 1, 2, 3, 4};
    CHECK_FALSE(WellFormed(TooMany));
}

TEST_CASE("Aufwertungen lassen sich verketten, die frühere Ausgabe muss dann verbraucht sein")
{
    Edit Value = MergeEdit();
    Value.Upgrades[0].Output = Gone(Value.Upgrades[0].Output);
    Value.Items.push_back(Gone(Stored(2)));
    Upgrade Second = Value.Upgrades[0];
    Second.Inputs = {UpgradeSource(0), 2};
    Second.Rarity = "Rarity_Rare";
    Second.Output = Placed(0, {{0, 0}});
    Second.Output.Source = UpgradeSource(1);
    Second.Output.DataHash = NameHash(Second.Data);
    Value.Upgrades.push_back(Second);
    CHECK(WellFormed(Value));
    CHECK(ConsumedSources(Value) == std::set<uint16_t>{0, 1, 2, UpgradeSource(0)});

    auto Kept = Value;
    Kept.Upgrades[0].Output = Placed(0, {{1, 1}});
    Kept.Upgrades[0].Output.Source = UpgradeSource(0);
    Kept.Upgrades[0].Output.DataHash = NameHash(Kept.Upgrades[0].Data);
    CHECK_FALSE(WellFormed(Kept)); // Ausgabe 0 noch vorhanden, aber als Eingabe verbraucht
    auto Forward = Value;
    Forward.Upgrades[0].Inputs = {UpgradeSource(1), 1};
    CHECK_FALSE(WellFormed(Forward)); // Eingabe aus einer späteren Aufwertung
}

TEST_CASE("Aufwertung mit der Belohnung als Eingabe braucht die Wahl")
{
    Edit Value;
    ItemState Choice = Gone(Stored(0));
    Choice.Source = ChoiceSource;
    Value.Items = {Gone(Placed(0, {{0, 0}})), Choice};
    Value.Choice = uint8_t{1};
    Value.Session = 3;
    Upgrade Next;
    Next.Inputs = {0, ChoiceSource};
    Next.Data = "ItemData_Pistol";
    Next.Output = Placed(0, {{0, 0}});
    Next.Output.Source = UpgradeSource(0);
    Next.Output.DataHash = NameHash(Next.Data);
    Value.Upgrades = {Next};
    CHECK(WellFormed(Value));
    Value.Choice.reset();
    Value.Items.pop_back();
    CHECK_FALSE(WellFormed(Value));
}

TEST_CASE("Ablegen auf ein gleiches Item: gleiche Daten, Ausgabe in der nächsten Seltenheit")
{
    const ItemKind Common{1, 10}, Other{2, 10};
    CHECK(IsMerge({Common, Common}, {11, 11}, {1, 11}));
    CHECK(IsMerge({Common, {1, 11}}, {11, 12}, {1, 12}));
    CHECK_FALSE(IsMerge({Common, Common}, {11, 11}, {1, 12})); // übersprungene Seltenheit
    CHECK_FALSE(IsMerge({Common, Common}, {11, 11}, {2, 11})); // andere Daten
    CHECK_FALSE(IsMerge({Common, Other}, {11, 11}, {1, 11}));
    CHECK_FALSE(IsMerge({Common, Common}, {0, 0}, {1, 0}));     // keine nächste Seltenheit
    CHECK_FALSE(IsMerge({Common, Common, Common}, {11, 11, 11}, {1, 11}));
}

TEST_CASE("Rezept der Werkbank: Zutaten in beliebiger Reihenfolge, Seltenheit nur wenn vorgegeben")
{
    const std::vector<Recipe> Recipes{{{50, 0}, {{1, 0}, {2, 0}}}, {{60, 77}, {{3, 10}}}};
    CHECK(IsRecipe(Recipes, {{2, 10}, {1, 10}}, {50, 10}));
    CHECK_FALSE(IsRecipe(Recipes, {{2, 10}, {1, 10}}, {50, 99})); // Seltenheit keiner Zutat
    CHECK_FALSE(IsRecipe(Recipes, {{2, 10}, {2, 10}}, {50, 10})); // Zutat fehlt
    CHECK_FALSE(IsRecipe(Recipes, {{1, 10}}, {50, 10}));          // zu wenige
    CHECK(IsRecipe(Recipes, {{3, 10}}, {60, 77}));
    CHECK_FALSE(IsRecipe(Recipes, {{3, 11}}, {60, 77}));          // Zutat in falscher Seltenheit
    CHECK_FALSE(IsRecipe(Recipes, {{3, 10}}, {60, 10}));          // Ausgabe ohne vorgegebene Seltenheit
    CHECK_FALSE(IsRecipe({}, {{3, 10}}, {60, 77}));
}

TEST_CASE("lager-handel-client: Reparatur steht im Edit, Plan repariert vor dem Ablegen")
{
    ItemState Broken = Stored(0);
    Broken.Broken = true;
    ItemState Repaired = Placed(0, {{0, 0}});
    CHECK(IsRepair(Broken, Repaired));
    CHECK_FALSE(IsRepair(Repaired, Broken));
    CHECK_FALSE(IsRepair(Broken, Gone(Repaired, SoldForScrap)));
    CHECK_FALSE(SameState(Broken, Stored(0))); // nur der Zustand geändert

    const Edit Value = Diff({Broken, Stored(1)}, {Stored(0), Stored(1)}, {}, {});
    REQUIRE(Value.Items.size() == 1);
    CHECK(Value.Items[0].Source == 0);
    CHECK_FALSE(Value.Items[0].Broken);

    const auto Steps = Plan({Broken}, {Repaired}, false, false);
    REQUIRE(Steps.size() == 2);
    CHECK(Steps[0] == Step{StepKind::Repair, 0});
    CHECK(Steps[1] == Step{StepKind::Place, 0});

    // Zurückrollen beim Client: erst aus dem Rucksack, dann wieder kaputt.
    const auto Back = Plan({Repaired}, {Broken}, false, false);
    REQUIRE(Back.size() == 2);
    CHECK(Back[0] == Step{StepKind::Unplace, 0});
    CHECK(Back[1] == Step{StepKind::Break, 0});
}

TEST_CASE("lager-handel-client: Verkauf nur für Ausgangs-Items, nicht für Wahl oder Aufwertung")
{
    Edit Sold;
    Sold.Items = {Gone(Placed(0, {{0, 0}}), SoldForScrap)};
    CHECK(WellFormed(Sold));
    CHECK(IsSold(Sold.Items[0]));
    CHECK_FALSE(IsSold(Gone(Stored(0)))); // UsedAsIngredient
    const auto Steps = Plan({Placed(0, {{0, 0}})}, Sold.Items, false, false);
    REQUIRE(Steps.size() == 2);
    CHECK(Steps[0] == Step{StepKind::Unplace, 0});
    CHECK(Steps[1] == Step{StepKind::Remove, 0});

    auto PresentWithContext = Sold;
    PresentWithContext.Items[0] = Stored(0);
    PresentWithContext.Items[0].RemoveContext = SoldForScrap;
    CHECK_FALSE(WellFormed(PresentWithContext));
    auto UnknownContext = Sold;
    UnknownContext.Items[0].RemoveContext = 4;
    CHECK_FALSE(WellFormed(UnknownContext));

    Edit SoldChoice;
    SoldChoice.Items = {Gone(Stored(ChoiceSource), SoldForScrap)};
    SoldChoice.Choice = uint8_t{0};
    SoldChoice.Session = 1;
    CHECK_FALSE(WellFormed(SoldChoice));

    Edit SoldInput = MergeEdit();
    SoldInput.Items[1].RemoveContext = SoldForScrap; // verkauft und zugleich aufgewertet
    CHECK_FALSE(WellFormed(SoldInput));
    Edit SoldOutput = MergeEdit();
    SoldOutput.Upgrades[0].Output = Gone(SoldOutput.Upgrades[0].Output, SoldForScrap);
    CHECK_FALSE(WellFormed(SoldOutput));
}

TEST_CASE("lager-handel-client: Schrott erst aus Verkäufen, dann für Reparaturen")
{
    CHECK(TradeBalance(10, {}, {}) == 10);
    CHECK(TradeBalance(10, {5, 7}, {}) == 22);
    CHECK(TradeBalance(10, {}, {10}) == 0);
    CHECK_FALSE(TradeBalance(10, {}, {11}));
    CHECK(TradeBalance(0, {8}, {8}) == 0);       // Verkauf bezahlt die Reparatur, Reihenfolge egal
    CHECK(TradeBalance(3, {4}, {5, 2}) == 0);
    CHECK_FALSE(TradeBalance(3, {4}, {5, 3}));
    CHECK_FALSE(TradeBalance(10, {-1}, {}));     // ungültiger Preis
    CHECK_FALSE(TradeBalance(10, {}, {-1}));
    CHECK_FALSE(TradeBalance(10, {2000000}, {})); // unplausibel hoch
    CHECK_FALSE(TradeBalance(-1, {}, {}));
}

namespace
{
Purchase Bought(size_t K, const std::string& Data, int32_t Cost, std::vector<Tile> Tiles = {})
{
    Purchase Next;
    Next.Data = Data;
    Next.Rarity = "Rarity_0_Common";
    Next.Cost = Cost;
    Next.Output = Tiles.empty() ? Stored(PurchaseSource(K)) : Placed(PurchaseSource(K), std::move(Tiles));
    Next.Output.DataHash = NameHash(Data);
    return Next;
}
}

TEST_CASE("opt-haendler-client: Kauf im Edit mit eigener Quelle, im Lager oder im Rucksack")
{
    Edit Value;
    Value.Id = 1;
    Value.Purchases = {Bought(0, "ItemData_Pistol", 30), Bought(1, "ItemData_Knife", 12, {{0, 0}})};
    CHECK(WellFormed(Value));
    CHECK(IsPurchaseSource(PurchaseSource(0)));
    CHECK_FALSE(IsPurchaseSource(UpgradeSource(0)));
    CHECK_FALSE(IsPurchaseSource(ChoiceSource));
    CHECK(Fits(Value, {{0, 0}}, {}));
    CHECK_FALSE(Fits(Value, {{0, 0}}, {{0, 0}})); // Feld von einem fremden Item belegt
    CHECK_FALSE(Fits(Value, {}, {}));             // Feld gibt es nicht

    auto WrongSource = Value;
    WrongSource.Purchases[1].Output.Source = PurchaseSource(0);
    CHECK_FALSE(WellFormed(WrongSource));
    auto WrongHash = Value;
    WrongHash.Purchases[0].Data = "ItemData_Revolver";
    CHECK_FALSE(WellFormed(WrongHash));
    auto Negative = Value;
    Negative.Purchases[0].Cost = -1;
    CHECK_FALSE(WellFormed(Negative));
    auto TooDear = Value;
    TooDear.Purchases[0].Cost = MaxPrice + 1;
    CHECK_FALSE(WellFormed(TooDear));
    auto Broken = Value;
    Broken.Purchases[0].Output.Broken = true;
    CHECK_FALSE(WellFormed(Broken));
    auto Vanished = Value;
    Vanished.Purchases[0].Output = Gone(Vanished.Purchases[0].Output, 0);
    CHECK_FALSE(WellFormed(Vanished)); // gekauft und weg, ohne Aufwertung
    auto Sold = Value;
    Sold.Purchases[0].Output = Gone(Sold.Purchases[0].Output, SoldForScrap);
    CHECK_FALSE(WellFormed(Sold));
    auto Overlap = Value;
    Overlap.Items.push_back(Placed(0, {{0, 0}}));
    CHECK_FALSE(WellFormed(Overlap));
    auto ItemWithPurchaseSource = Value;
    ItemWithPurchaseSource.Items.push_back(Stored(PurchaseSource(0)));
    CHECK_FALSE(WellFormed(ItemWithPurchaseSource));
    auto TooMany = Value;
    TooMany.Purchases.clear();
    for (size_t K = 0; K <= MaxPurchases; ++K)
        TooMany.Purchases.push_back(Bought(K, "ItemData_Pistol", 1));
    CHECK_FALSE(WellFormed(TooMany));
}

TEST_CASE("opt-haendler-client: gekauftes Item als Eingabe einer Aufwertung")
{
    Edit Value;
    Value.Id = 1;
    ItemState Old = Stored(0);
    Old.DataHash = NameHash("ItemData_Pistol");
    Value.Items = {Gone(Old)};
    Value.Purchases = {Bought(0, "ItemData_Pistol", 30)};
    Value.Purchases[0].Output = Gone(Value.Purchases[0].Output);
    Upgrade Next;
    Next.Inputs = {0, PurchaseSource(0)};
    Next.Data = "ItemData_Pistol";
    Next.Rarity = "Rarity_1_Uncommon";
    Next.Output = Placed(UpgradeSource(0), {{0, 0}});
    Next.Output.DataHash = NameHash(Next.Data);
    Value.Upgrades = {Next};
    CHECK(WellFormed(Value));
    CHECK(ConsumedSources(Value) == std::set<uint16_t>{0, PurchaseSource(0)});

    auto StillThere = Value;
    StillThere.Purchases[0].Output = Stored(PurchaseSource(0));
    StillThere.Purchases[0].Output.DataHash = NameHash("ItemData_Pistol");
    CHECK_FALSE(WellFormed(StillThere)); // verbraucht und zugleich vorhanden
    auto Unknown = Value;
    Unknown.Upgrades[0].Inputs = {0, PurchaseSource(1)};
    CHECK_FALSE(WellFormed(Unknown));
}

TEST_CASE("opt-haendler-client: Käufe kosten nach den Verkäufen, zusammen mit Reparaturen")
{
    CHECK(TradeBalance(30, {}, {}, {30}) == 0);
    CHECK_FALSE(TradeBalance(29, {}, {}, {30}));
    CHECK(TradeBalance(10, {25}, {}, {30}) == 5);   // Verkauf bezahlt den Kauf
    CHECK(TradeBalance(40, {}, {10}, {30}) == 0);
    CHECK_FALSE(TradeBalance(39, {}, {10}, {30}));
    CHECK_FALSE(TradeBalance(100, {}, {}, {-1}));
    CHECK_FALSE(TradeBalance(2000000, {}, {}, {MaxPrice + 1}));
}

TEST_CASE("opt-haendler-client: Untergrenze des Kaufpreises lässt Rabatte bis 75 % zu")
{
    CHECK(PurchaseFloor(12) == 3);
    CHECK(PurchaseFloor(24) == 6);
    CHECK(PurchaseFloor(3) == 0);
    CHECK(PurchaseFloor(0) == 0);
    CHECK(PurchaseFloor(-5) == 0);
    // Luftpumpe im Test: voller Preis 23 bis 24, Verkaufspreis 12, mit 75 % Rabatt etwa 6.
    CHECK(6 >= PurchaseFloor(12));
}
