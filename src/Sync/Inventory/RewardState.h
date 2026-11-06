#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Game/InventoryEditPlan.h"

// Airdrop-Auswahl über den Kontrollkanal (airdrop-auswahl, inventar-lokal). Host schickt das Sitzungsbild, der Client
// bearbeitet sein Inventar im nativen Panel lokal und antwortet mit einem Edit (Wahl und Endstand), der Host mit dem
// Ergebnis. Little-Endian, Texte UTF-8 und begrenzt.
namespace Sync::Reward
{
inline constexpr uint16_t OfferType = 0x300, EditType = 0x303, ResultType = 0x304;
inline constexpr size_t MaxChoices = 4, MaxName = 120;

// Eine Choice: lesbarer Name für Log und Overlay, Objektnamen von UInventoryItemData und URarity, mit denen der Client
// eine eigene Anzeige-Instanz anlegt.
struct Item
{
    std::string Name, Data, Rarity;
    bool Broken = false;
    bool operator==(const Item&) const = default;
};

struct Offer
{
    uint64_t Session = 0; // 0 = keine offene Sitzung, Client schließt die Auswahl
    uint64_t Revision = 0;
    int32_t Level = 0;
    bool ClientClaimed = false, HostClaimed = false, HostWaiting = false;
    std::vector<Item> Choices;
    bool operator==(const Offer&) const = default;
};

namespace Detail
{
inline void Put(std::vector<uint8_t>& Out, uint64_t Value, unsigned Bytes)
{
    for (unsigned i = 0; i < Bytes; ++i)
        Out.push_back(static_cast<uint8_t>(Value >> (8 * i)));
}
inline uint64_t Get(std::span<const uint8_t> In, size_t At, unsigned Bytes)
{
    uint64_t Value = 0;
    for (unsigned i = 0; i < Bytes; ++i)
        Value |= uint64_t(In[At + i]) << (8 * i);
    return Value;
}
}

inline std::string Clip(std::string Name)
{
    if (Name.size() <= MaxName)
        return Name;
    size_t End = MaxName;
    while (End && (static_cast<uint8_t>(Name[End]) & 0xC0) == 0x80) // nicht mitten in einem UTF-8-Zeichen trennen
        --End;
    Name.resize(End);
    return Name;
}

inline bool Valid(const Offer& Value)
{
    if (Value.Choices.size() > MaxChoices)
        return false;
    for (const auto& Choice : Value.Choices)
        if (Choice.Name.size() > MaxName || Choice.Data.size() > MaxName || Choice.Rarity.size() > MaxName)
            return false;
    return Value.Session ? Value.Revision != 0 : Value.Choices.empty();
}

inline std::vector<uint8_t> Encode(const Offer& Value)
{
    if (!Valid(Value))
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Session, 8);
    Detail::Put(Out, Value.Revision, 8);
    Detail::Put(Out, static_cast<uint32_t>(Value.Level), 4);
    Out.push_back(static_cast<uint8_t>((Value.ClientClaimed ? 1 : 0) | (Value.HostClaimed ? 2 : 0) | (Value.HostWaiting ? 4 : 0)));
    Out.push_back(static_cast<uint8_t>(Value.Choices.size()));
    for (const auto& Choice : Value.Choices)
    {
        Out.push_back(Choice.Broken ? 1 : 0);
        for (const std::string* Text : {&Choice.Name, &Choice.Data, &Choice.Rarity})
        {
            Out.push_back(static_cast<uint8_t>(Text->size()));
            Out.insert(Out.end(), Text->begin(), Text->end());
        }
    }
    return Out;
}

inline std::optional<Offer> DecodeOffer(std::span<const uint8_t> In)
{
    if (In.size() < 22 || In[20] > 7 || In[21] > MaxChoices)
        return std::nullopt;
    Offer Value;
    Value.Session = Detail::Get(In, 0, 8);
    Value.Revision = Detail::Get(In, 8, 8);
    Value.Level = static_cast<int32_t>(Detail::Get(In, 16, 4));
    Value.ClientClaimed = In[20] & 1;
    Value.HostClaimed = In[20] & 2;
    Value.HostWaiting = In[20] & 4;
    size_t At = 22;
    for (uint8_t i = 0; i < In[21]; ++i)
    {
        if (At >= In.size() || In[At] > 1)
            return std::nullopt;
        Item Choice;
        Choice.Broken = In[At++] != 0;
        for (std::string* Text : {&Choice.Name, &Choice.Data, &Choice.Rarity})
        {
            if (At >= In.size() || In[At] > MaxName || At + 1 + In[At] > In.size())
                return std::nullopt;
            Text->assign(reinterpret_cast<const char*>(In.data() + At + 1), In[At]);
            At += 1 + In[At];
        }
        Value.Choices.push_back(std::move(Choice));
    }
    if (At != In.size() || !Valid(Value))
        return std::nullopt;
    return Value;
}

namespace Detail
{
inline void PutTile(std::vector<uint8_t>& Out, const Game::InventoryEdit::Tile& At)
{
    Put(Out, static_cast<uint32_t>(At.first), 4);
    Put(Out, static_cast<uint32_t>(At.second), 4);
}
inline Game::InventoryEdit::Tile GetTile(std::span<const uint8_t> In, size_t At)
{
    return {static_cast<int32_t>(Get(In, At, 4)), static_cast<int32_t>(Get(In, At + 4, 4))};
}
}

// Edit, Client an Host: Kopf 26 Byte (Id 8, Sitzung 8, Revision 8, Wahl 1 mit 0xFF = keine, Anzahl neuer Felder 1), je neues
// Feld 8 Byte, dann Anzahl Items (1) und je Item 21 Byte plus 8 je belegtem Feld.
inline std::vector<uint8_t> Encode(const Game::InventoryEdit::Edit& Value)
{
    if (!Value.Id || !Game::InventoryEdit::WellFormed(Value))
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Id, 8);
    Detail::Put(Out, Value.Session, 8);
    Detail::Put(Out, Value.Revision, 8);
    Out.push_back(Value.Choice ? *Value.Choice : uint8_t{0xFF});
    Out.push_back(static_cast<uint8_t>(Value.NewSlots.size()));
    for (const auto& At : Value.NewSlots)
        Detail::PutTile(Out, At);
    Out.push_back(static_cast<uint8_t>(Value.Items.size()));
    for (const auto& Item : Value.Items)
    {
        Detail::Put(Out, Item.Source, 2);
        Detail::Put(Out, Item.DataHash, 4);
        Out.push_back(static_cast<uint8_t>((Item.Present ? 1 : 0) | (Item.InBackpack ? 2 : 0) | (Item.Trash ? 4 : 0) | (Item.AddEffect ? 8 : 0)));
        Out.push_back(Item.RemoveContext);
        Detail::Put(Out, static_cast<uint32_t>(Item.Turns), 4);
        Detail::PutTile(Out, Item.Pivot);
        Out.push_back(static_cast<uint8_t>(Item.Tiles.size()));
        for (const auto& At : Item.Tiles)
            Detail::PutTile(Out, At);
    }
    return Out;
}

inline std::optional<Game::InventoryEdit::Edit> DecodeEdit(std::span<const uint8_t> In)
{
    using namespace Game::InventoryEdit;
    if (In.size() < 26)
        return std::nullopt;
    Edit Value;
    Value.Id = Detail::Get(In, 0, 8);
    Value.Session = Detail::Get(In, 8, 8);
    Value.Revision = Detail::Get(In, 16, 8);
    if (In[24] != 0xFF)
        Value.Choice = In[24];
    size_t At = 26;
    if (In[25] > MaxNewSlots || At + 8 * size_t(In[25]) + 1 > In.size())
        return std::nullopt;
    for (uint8_t i = 0; i < In[25]; ++i, At += 8)
        Value.NewSlots.push_back(Detail::GetTile(In, At));
    const uint8_t Count = In[At++];
    if (Count > MaxItems)
        return std::nullopt;
    for (uint8_t i = 0; i < Count; ++i)
    {
        if (At + 21 > In.size() || In[At + 6] > 15 || In[At + 20] > MaxItemTiles || At + 21 + 8 * size_t(In[At + 20]) > In.size())
            return std::nullopt;
        ItemState Item;
        Item.Source = static_cast<uint16_t>(Detail::Get(In, At, 2));
        Item.DataHash = static_cast<uint32_t>(Detail::Get(In, At + 2, 4));
        Item.Present = In[At + 6] & 1;
        Item.InBackpack = In[At + 6] & 2;
        Item.Trash = In[At + 6] & 4;
        Item.AddEffect = In[At + 6] & 8;
        Item.RemoveContext = In[At + 7];
        Item.Turns = static_cast<int32_t>(Detail::Get(In, At + 8, 4));
        Item.Pivot = Detail::GetTile(In, At + 12);
        const uint8_t Tiles = In[At + 20];
        At += 21;
        for (uint8_t t = 0; t < Tiles; ++t, At += 8)
            Item.Tiles.push_back(Detail::GetTile(In, At));
        Value.Items.push_back(std::move(Item));
    }
    if (At != In.size() || !Value.Id || !WellFormed(Value))
        return std::nullopt;
    return Value;
}

// Ergebnis, Host an Client, genau 9 Byte: Id des Edits (8), 1 übernommen oder 0 abgelehnt.
struct Result
{
    uint64_t Id = 0;
    bool Accepted = false;
    bool operator==(const Result&) const = default;
};

inline std::vector<uint8_t> Encode(const Result& Value)
{
    if (!Value.Id)
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Id, 8);
    Out.push_back(Value.Accepted ? 1 : 0);
    return Out;
}

inline std::optional<Result> DecodeResult(std::span<const uint8_t> In)
{
    if (In.size() != 9 || In[8] > 1 || !Detail::Get(In, 0, 8))
        return std::nullopt;
    return Result{Detail::Get(In, 0, 8), In[8] == 1};
}
}
