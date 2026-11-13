#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
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
// werkbank-client: Ausgabe der k-ten Aufwertung hat Source UpgradeSourceBase + k. Ausgangs-Items liegen darunter.
inline constexpr uint16_t UpgradeSourceBase = 0xFF00;
inline constexpr size_t MaxUpgrades = 8, MaxUpgradeInputs = 4, MaxObjectName = 120;
// ERemoveItemContext: GeneralRemove 0, ReturnedToRewards 1, SoldForScrap 2, UsedAsIngredient 3 (lager-handel-client).
inline constexpr uint8_t SoldForScrap = 2, MaxRemoveContext = 3;

// opt-haendler-client: Das k-te gekaufte Item hat Source PurchaseSourceBase + k, unter den Ausgaben der Aufwertungen.
inline constexpr uint16_t PurchaseSourceBase = 0xFE00;
inline constexpr size_t MaxPurchases = 16;
inline constexpr int32_t MaxPrice = 1000000;

constexpr uint16_t UpgradeSource(size_t K)
{
    return static_cast<uint16_t>(UpgradeSourceBase + K);
}

constexpr uint16_t PurchaseSource(size_t K)
{
    return static_cast<uint16_t>(PurchaseSourceBase + K);
}

constexpr bool IsPurchaseSource(uint16_t Source)
{
    return Source >= PurchaseSourceBase && Source < UpgradeSourceBase;
}

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
    bool Broken = false; // UInventoryItemInstance.Broken; kaputt zu heil heißt repariert (lager-handel-client)
    bool operator==(const ItemState&) const = default;
};

// Eine Aufwertung (UInventoryComponent::UpgradeItems): Ablegen auf ein gleiches Item oder ein Rezept der Werkbank. Inputs
// sind Sources: Ausgangs-Items, die Belohnung (ChoiceSource) oder die Ausgabe einer früheren Aufwertung. Jede Eingabe steht
// als nicht mehr vorhanden im Edit. Data und Rarity sind die Objektnamen der Ausgabe, Output ihr Endstand.
struct Upgrade
{
    std::vector<uint16_t> Inputs;
    std::string Data, Rarity;
    ItemState Output;
    bool operator==(const Upgrade&) const = default;
};

// Ein Kauf beim Händler (UShopPanelItem::TryBuyItem): Data und Rarity sind die Objektnamen des gekauften Items, Cost der Preis,
// den das Panel des Clients verlangt hat, Output der Endstand des Items. Der Host legt das Item selbst an und bucht den Preis.
struct Purchase
{
    std::string Data, Rarity;
    int32_t Cost = 0;
    ItemState Output;
    bool operator==(const Purchase&) const = default;
};

struct Edit
{
    uint64_t Id = 0;
    uint64_t Session = 0, Revision = 0; // Airdrop-Sitzung, 0 ohne Belohnung
    std::optional<uint8_t> Choice;      // gewählte Belohnung
    std::vector<ItemState> Items;       // nur geänderte Items, die Belohnung mit Source ChoiceSource
    std::vector<Tile> NewSlots;
    std::vector<Upgrade> Upgrades;      // in der Reihenfolge, in der sie liefen
    std::vector<Purchase> Purchases;    // Käufe beim Händler (opt-haendler-client)
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
    return !A.Present || (SamePlacement(A, B) && A.Trash == B.Trash && A.AddEffect == B.AddEffect && A.Broken == B.Broken);
}

// lager-handel-client: verkauft = im Edit nicht mehr vorhanden mit RemoveContext SoldForScrap.
inline bool IsSold(const ItemState& Item)
{
    return !Item.Present && Item.RemoveContext == SoldForScrap;
}

// Repariert: vorher kaputt, im Ziel vorhanden und heil. Heil zu kaputt gibt es im Spiel nicht, der Host lehnt es ab.
inline bool IsRepair(const ItemState& Before, const ItemState& After)
{
    return Before.Present && Before.Broken && After.Present && !After.Broken;
}

// Host: Schrott nach Verkäufen und Reparaturen eines Edits. Erst alle Verkäufe gutschreiben, dann jede Reparatur abziehen; so
// ist die Reihenfolge des Clients egal, denn er konnte nur mit genug Schrott reparieren. Leer, wenn ein Preis ungültig ist oder
// der Schrott für eine Reparatur nicht reicht.
// opt-haendler-client: Käufe zählen wie Reparaturen, sie kosten nach den Verkäufen.
inline std::optional<int64_t> TradeBalance(int64_t Wallet, const std::vector<int32_t>& SellPrices, const std::vector<int32_t>& RepairPrices,
                                           const std::vector<int32_t>& BuyPrices = {})
{
    if (Wallet < 0)
        return std::nullopt;
    for (const int32_t Price : SellPrices)
    {
        if (Price < 0 || Price > MaxPrice)
            return std::nullopt;
        Wallet += Price;
    }
    for (const std::vector<int32_t>* Costs : {&BuyPrices, &RepairPrices})
        for (const int32_t Price : *Costs)
        {
            if (Price < 0 || Price > MaxPrice || Wallet < Price)
                return std::nullopt;
            Wallet -= Price;
        }
    return Wallet;
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
// Aufwertungen: Jede Eingabe ist ein im Edit entferntes Item, die Belohnung oder eine frühere, wieder verbrauchte Ausgabe
// und wird nur einmal verbraucht. Käufe: Jedes gekaufte Item hat seine eigene Quelle, ist heil und entweder vorhanden oder
// Eingabe einer Aufwertung; verkaufen lässt es sich im selben Edit nicht.
inline bool WellFormed(const Edit& Value)
{
    if (Value.Items.size() > MaxItems || Value.NewSlots.size() > MaxNewSlots || Value.Choice.value_or(uint8_t{0}) >= 8 ||
        Value.Upgrades.size() > MaxUpgrades || Value.Purchases.size() > MaxPurchases)
        return false;
    std::set<uint16_t> Sources;
    std::set<Tile> Used;
    size_t Choices = 0;
    const auto Placement = [&](const ItemState& Item)
    {
        if (Item.Tiles.size() > MaxItemTiles)
            return false;
        if (!Item.Present || !Item.InBackpack)
            return Item.Tiles.empty();
        if (Item.Tiles.empty())
            return false;
        for (const Tile& At : Item.Tiles)
            if (!Used.insert(At).second)
                return false;
        return true;
    };
    for (const ItemState& Item : Value.Items)
    {
        if ((Item.Source >= PurchaseSourceBase && Item.Source != ChoiceSource) || !Sources.insert(Item.Source).second || !Placement(Item) ||
            Item.RemoveContext > MaxRemoveContext || (Item.Present && Item.RemoveContext != 0) || (IsSold(Item) && Item.Source == ChoiceSource))
            return false;
        Choices += Item.Source == ChoiceSource;
    }
    if (Choices != (Value.Choice ? 1u : 0u) || (Choices != 0 && Value.Session == 0))
        return false;
    for (size_t K = 0; K < Value.Purchases.size(); ++K)
    {
        const Purchase& Next = Value.Purchases[K];
        if (Next.Data.empty() || Next.Data.size() > MaxObjectName || Next.Rarity.size() > MaxObjectName || Next.Cost < 0 || Next.Cost > MaxPrice ||
            Next.Output.Source != PurchaseSource(K) || Next.Output.DataHash != NameHash(Next.Data) || Next.Output.Broken ||
            Next.Output.RemoveContext > MaxRemoveContext || (Next.Output.Present && Next.Output.RemoveContext != 0) || IsSold(Next.Output) ||
            !Placement(Next.Output))
            return false;
    }
    std::set<uint16_t> Consumed;
    for (size_t K = 0; K < Value.Upgrades.size(); ++K)
    {
        const Upgrade& Next = Value.Upgrades[K];
        if (Next.Inputs.empty() || Next.Inputs.size() > MaxUpgradeInputs || Next.Data.empty() || Next.Data.size() > MaxObjectName ||
            Next.Rarity.size() > MaxObjectName || Next.Output.Source != UpgradeSource(K) || Next.Output.DataHash != NameHash(Next.Data) ||
            !Placement(Next.Output) || IsSold(Next.Output))
            return false;
        for (const uint16_t Input : Next.Inputs)
        {
            if (!Consumed.insert(Input).second)
                return false;
            if (IsPurchaseSource(Input))
            {
                const size_t From = Input - PurchaseSourceBase;
                if (From >= Value.Purchases.size() || Value.Purchases[From].Output.Present)
                    return false;
                continue;
            }
            if (Input >= UpgradeSourceBase && Input != ChoiceSource)
            {
                const size_t From = Input - UpgradeSourceBase;
                if (From >= K || Value.Upgrades[From].Output.Present)
                    return false;
                continue;
            }
            const auto It = std::find_if(Value.Items.begin(), Value.Items.end(), [&](const ItemState& Item) { return Item.Source == Input; });
            if (It == Value.Items.end() || It->Present || IsSold(*It)) // verkauft und aufgewertet zugleich geht nicht
                return false;
        }
    }
    // Ein gekauftes Item verschwindet nur als Eingabe einer Aufwertung.
    for (size_t K = 0; K < Value.Purchases.size(); ++K)
        if (!Value.Purchases[K].Output.Present && !Consumed.contains(PurchaseSource(K)))
            return false;
    return true;
}

// Sources, die eine Aufwertung verbraucht. Der Host entfernt sie nicht selbst, das macht UpgradeItems.
inline std::set<uint16_t> ConsumedSources(const Edit& Value)
{
    std::set<uint16_t> Result;
    for (const Upgrade& Next : Value.Upgrades)
        Result.insert(Next.Inputs.begin(), Next.Inputs.end());
    return Result;
}

// Art eines Items für die Regeln: Prüfsummen der Objektnamen von ItemData und Rarity, 0 = keine.
struct ItemKind
{
    uint32_t Data = 0, Rarity = 0;
    bool operator==(const ItemKind&) const = default;
};

// Ablegen auf ein gleiches Item (InventoryPanel.UpgradeDroppedItem): zwei Eingaben mit denselben Daten, die Ausgabe hat
// dieselben Daten und die nächste Seltenheit (URarity.NextUpgradableRarity) einer Eingabe. Next[i] gehört zu Inputs[i].
inline bool IsMerge(const std::vector<ItemKind>& Inputs, const std::vector<uint32_t>& Next, ItemKind Output)
{
    if (Inputs.size() != 2 || Next.size() != 2 || !Inputs[0].Data || Inputs[0].Data != Inputs[1].Data || Output.Data != Inputs[0].Data ||
        !Output.Rarity)
        return false;
    return Output.Rarity == Next[0] || Output.Rarity == Next[1];
}

// Ein Rezept der Werkbank (FRecipeTableRow): Ausgabe und benötigte Items. Rarity 0 heißt ohne Vorgabe.
struct Recipe
{
    ItemKind Output;
    std::vector<ItemKind> Required;
};

// Passt die Aufwertung zu einem Rezept? Jede benötigte Zutat braucht eine eigene Eingabe mit denselben Daten und, falls
// vorgegeben, derselben Seltenheit. Ohne Seltenheit im Rezept muss die Ausgabe die Seltenheit einer Eingabe haben.
inline bool IsRecipe(const std::vector<Recipe>& Recipes, const std::vector<ItemKind>& Inputs, ItemKind Output)
{
    if (Inputs.empty() || Inputs.size() > MaxUpgradeInputs || !Output.Data)
        return false;
    for (const Recipe& Candidate : Recipes)
    {
        if (Candidate.Output.Data != Output.Data || Candidate.Required.size() != Inputs.size())
            continue;
        if (Candidate.Output.Rarity ? Candidate.Output.Rarity != Output.Rarity
                                    : std::none_of(Inputs.begin(), Inputs.end(), [&](const ItemKind& In) { return In.Rarity == Output.Rarity; }))
            continue;
        std::vector<size_t> Order(Inputs.size());
        for (size_t i = 0; i < Order.size(); ++i)
            Order[i] = i;
        do
        {
            bool All = true;
            for (size_t i = 0; i < Order.size() && All; ++i)
            {
                const ItemKind& Need = Candidate.Required[i];
                const ItemKind& Have = Inputs[Order[i]];
                All = Need.Data == Have.Data && (!Need.Rarity || Need.Rarity == Have.Rarity);
            }
            if (All)
                return true;
        } while (std::next_permutation(Order.begin(), Order.end()));
    }
    return false;
}

// Host: Passt der Edit in den Rucksack? Slots sind die vorhandenen Felder, Others die Felder, die Items außerhalb des
// Edits belegen. Jedes Zielfeld muss existieren oder neu sein und darf nicht von einem anderen Item belegt sein.
inline bool Fits(const Edit& Value, const std::vector<Tile>& Slots, const std::vector<Tile>& Others)
{
    const std::set<Tile> Existing(Slots.begin(), Slots.end()), Taken(Others.begin(), Others.end());
    const std::set<Tile> Fresh(Value.NewSlots.begin(), Value.NewSlots.end());
    const auto Ok = [&](const ItemState& Item)
    {
        if (Item.Present && Item.InBackpack)
            for (const Tile& At : Item.Tiles)
                if ((!Existing.contains(At) && !Fresh.contains(At)) || Taken.contains(At))
                    return false;
        return true;
    };
    return std::all_of(Value.Items.begin(), Value.Items.end(), Ok) &&
           std::all_of(Value.Upgrades.begin(), Value.Upgrades.end(), [&](const Upgrade& Next) { return Ok(Next.Output); }) &&
           std::all_of(Value.Purchases.begin(), Value.Purchases.end(), [&](const Purchase& Next) { return Ok(Next.Output); });
}

enum class StepKind : uint8_t
{
    Repair,       // RepairItem, vor dem Ablegen (lager-handel-client)
    Unplace,      // UnoccupyBackpackSlots und RemoveFromBackpack
    Remove,       // RemoveFromInventory
    Add,          // AddToInventory (nur beim Zurückrollen eines verbrauchten Items)
    DestroySlots, // lokal angelegte Felder wieder entfernen (Zurückrollen)
    CreateSlots,  // Server_CreateSlotsFromBagItem
    Place,        // SetTurns, SetZeroPivotTile, AddToBackpack, OccupyBackpackSlots
    Trash,        // UpdateIsTrash
    Break,        // nur beim Zurückrollen: lokal reparierten Zustand wieder kaputt setzen
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
    for (size_t i = 0; i < Current.size() && i < Target.size(); ++i)
        if (IsRepair(Current[i], Target[i]))
            Steps.push_back({StepKind::Repair, i});
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
    for (size_t i = 0; i < Current.size() && i < Target.size(); ++i)
        if (Target[i].Present && Target[i].Broken && Current[i].Present && !Current[i].Broken)
            Steps.push_back({StepKind::Break, i});
    return Steps;
}
}
