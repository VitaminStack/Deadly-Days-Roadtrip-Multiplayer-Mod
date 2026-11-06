#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

// Lokale Inventarbearbeitung des Clients (inventar-lokal), reine Logik ohne Spielzugriff.
//
// Der Client bearbeitet sein Inventar im nativen Panel nur lokal. Beim Bestätigen beschreibt ein Edit den Endstand jedes
// geänderten Items und die neuen Rucksackfelder. Der Host übernimmt ihn mit denselben nativen Funktionen; lehnt er ab,
// stellt der Client mit demselben Plan den Ausgangsstand wieder her.
namespace Game::InventoryEdit
{
using Tile = std::pair<int32_t, int32_t>;

inline constexpr uint16_t ChoiceSource = 0xFFFF; // Source der gewählten Belohnung, kein Ausgangs-Item
inline constexpr size_t MaxItems = 96, MaxItemTiles = 32, MaxNewSlots = 64;

// Zustand eines Items. Source ist der Index im Ausgangsstand (MyItems in replizierter Reihenfolge), DataHash die
// Prüfsumme des Objektnamens seiner UInventoryItemData. Tiles sind die belegten Rucksackfelder, sortiert.
struct ItemState
{
    uint16_t Source = 0;
    uint32_t DataHash = 0;
    bool Present = true;
    uint8_t RemoveContext = 0; // ERemoveItemContext, wenn nicht mehr vorhanden
    bool InBackpack = false, Trash = false;
    int32_t Turns = 0;
    Tile Pivot{};
    std::vector<Tile> Tiles;
    bool AddEffect = false; // Panel hat ApplyAddToBackpackEffect gerufen, der Host löst ihn nach dem Ablegen aus
    bool operator==(const ItemState&) const = default;
};

struct Edit
{
    uint64_t Id = 0;
    uint64_t Session = 0, Revision = 0; // Airdrop-Sitzung, 0 ohne Belohnung
    std::optional<uint8_t> Choice;      // gewählte Belohnung
    std::vector<ItemState> Items;       // nur geänderte Items, die Belohnung mit Source ChoiceSource
    std::vector<Tile> NewSlots;
    bool operator==(const Edit&) const = default;
};

// FNV-1a, gleich bei Host und Client.
constexpr uint32_t NameHash(std::string_view Name)
{
    uint32_t Hash = 2166136261u;
    for (const char C : Name)
        Hash = (Hash ^ static_cast<uint8_t>(C)) * 16777619u;
    return Hash ? Hash : 1;
}

// Gleiche Lage: beide außerhalb des Rucksacks, oder beide drin mit gleicher Drehung, gleichem Pivot und gleichen Feldern.
inline bool SamePlacement(const ItemState& A, const ItemState& B)
{
    if (A.InBackpack != B.InBackpack)
        return false;
    return !A.InBackpack || (A.Turns == B.Turns && A.Pivot == B.Pivot && A.Tiles == B.Tiles);
}

inline bool SameState(const ItemState& A, const ItemState& B)
{
    if (A.Present != B.Present)
        return false;
    return !A.Present || (SamePlacement(A, B) && A.Trash == B.Trash && A.AddEffect == B.AddEffect);
}

inline std::vector<Tile> Sorted(std::vector<Tile> Tiles)
{
    std::sort(Tiles.begin(), Tiles.end());
    Tiles.erase(std::unique(Tiles.begin(), Tiles.end()), Tiles.end());
    return Tiles;
}

// Client: Edit aus Ausgangs- und Endstand. Base[i] hat Source i, Final enthält je Ausgangs-Item genau einen Eintrag
// (Present false, wenn weg) und höchstens eine Belohnung.
inline Edit Diff(const std::vector<ItemState>& Base, const std::vector<ItemState>& Final, const std::vector<Tile>& BaseSlots,
                 const std::vector<Tile>& FinalSlots)
{
    Edit Result;
    for (const ItemState& Item : Final)
    {
        if (Item.Source == ChoiceSource)
            Result.Items.push_back(Item);
        else if (Item.Source < Base.size() && !SameState(Base[Item.Source], Item))
            Result.Items.push_back(Item);
    }
    const std::vector<Tile> Before = Sorted(BaseSlots);
    for (const Tile& Slot : Sorted(FinalSlots))
        if (!std::binary_search(Before.begin(), Before.end(), Slot))
            Result.NewSlots.push_back(Slot);
    return Result;
}

// Prüft einen Edit ohne Spielzustand: Grenzen, eindeutige Quellen, Belohnung nur mit Wahl, keine doppelt belegten Felder.
inline bool WellFormed(const Edit& Value)
{
    if (Value.Items.size() > MaxItems || Value.NewSlots.size() > MaxNewSlots || Value.Choice.value_or(uint8_t{0}) >= 8)
        return false;
    std::set<uint16_t> Sources;
    std::set<Tile> Used;
    size_t Choices = 0;
    for (const ItemState& Item : Value.Items)
    {
        if (!Sources.insert(Item.Source).second || Item.Tiles.size() > MaxItemTiles)
            return false;
        Choices += Item.Source == ChoiceSource;
        if (!Item.Present || !Item.InBackpack)
        {
            if (!Item.Tiles.empty())
                return false;
            continue;
        }
        if (Item.Tiles.empty())
            return false;
        for (const Tile& At : Item.Tiles)
            if (!Used.insert(At).second)
                return false;
    }
    return Choices == (Value.Choice ? 1u : 0u) && (Choices == 0 || Value.Session != 0);
}

// Host: Passt der Edit in den Rucksack? Slots sind die vorhandenen Felder, Others die Felder, die Items außerhalb des
// Edits belegen. Jedes Zielfeld muss existieren oder neu sein und darf nicht von einem anderen Item belegt sein.
inline bool Fits(const Edit& Value, const std::vector<Tile>& Slots, const std::vector<Tile>& Others)
{
    const std::set<Tile> Existing(Slots.begin(), Slots.end()), Taken(Others.begin(), Others.end());
    const std::set<Tile> Fresh(Value.NewSlots.begin(), Value.NewSlots.end());
    for (const ItemState& Item : Value.Items)
        if (Item.Present && Item.InBackpack)
            for (const Tile& At : Item.Tiles)
                if ((!Existing.contains(At) && !Fresh.contains(At)) || Taken.contains(At))
                    return false;
    return true;
}

enum class StepKind : uint8_t
{
    Unplace,      // UnoccupyBackpackSlots und RemoveFromBackpack
    Remove,       // RemoveFromInventory
    Add,          // AddToInventory (nur beim Zurückrollen eines verbrauchten Items)
    DestroySlots, // lokal angelegte Felder wieder entfernen (Zurückrollen)
    CreateSlots,  // Server_CreateSlotsFromBagItem
    Place,        // SetTurns, SetZeroPivotTile, AddToBackpack, OccupyBackpackSlots
    Trash,        // UpdateIsTrash
};

struct Step
{
    StepKind Kind;
    size_t Item = 0; // Index in Current/Target, bei Feldschritten ohne Bedeutung
    bool operator==(const Step&) const = default;
};

// Reihenfolge der nativen Aufrufe, um von Current nach Target zu kommen (gleich lange Listen, je Item ein Paar). Erst alle
// Items aus dem Rucksack nehmen, dann entfernen und Felder anlegen, zuletzt neu ablegen: so blockieren getauschte Items
// einander nicht.
inline std::vector<Step> Plan(const std::vector<ItemState>& Current, const std::vector<ItemState>& Target, bool CreateSlots,
                              bool DestroySlots)
{
    std::vector<Step> Steps;
    std::vector<bool> Unplaced(Current.size());
    for (size_t i = 0; i < Current.size() && i < Target.size(); ++i)
        if (Current[i].Present && Current[i].InBackpack && (!Target[i].Present || !SamePlacement(Current[i], Target[i])))
        {
            Steps.push_back({StepKind::Unplace, i});
            Unplaced[i] = true;
        }
    for (size_t i = 0; i < Current.size() && i < Target.size(); ++i)
        if (Current[i].Present && !Target[i].Present)
            Steps.push_back({StepKind::Remove, i});
    for (size_t i = 0; i < Current.size() && i < Target.size(); ++i)
        if (!Current[i].Present && Target[i].Present)
            Steps.push_back({StepKind::Add, i});
    if (DestroySlots)
        Steps.push_back({StepKind::DestroySlots, 0});
    if (CreateSlots)
        Steps.push_back({StepKind::CreateSlots, 0});
    for (size_t i = 0; i < Current.size() && i < Target.size(); ++i)
        if (Target[i].Present && Target[i].InBackpack && (Unplaced[i] || !Current[i].Present || !Current[i].InBackpack))
            Steps.push_back({StepKind::Place, i});
    for (size_t i = 0; i < Current.size() && i < Target.size(); ++i)
        if (Target[i].Present && Target[i].Trash != (Current[i].Present && Current[i].Trash))
            Steps.push_back({StepKind::Trash, i});
    return Steps;
}
}
