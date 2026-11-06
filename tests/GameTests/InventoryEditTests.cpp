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

TEST_CASE("Namensprüfsumme ist stabil und nie null")
{
    static_assert(NameHash("DA_Revolver") == NameHash("DA_Revolver"));
    CHECK(NameHash("DA_Revolver") != NameHash("DA_Pistol"));
    CHECK(NameHash("") != 0);
}
