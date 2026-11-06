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
inline constexpr uint16_t OfferType = 0x300, EditType = 0x303, ResultType = 0x304, ReadyType = 0x305;
// Lager am Rundenende (bestaetigen-rundenende): Bereit des Clients und Stand beim Host.
inline constexpr uint16_t RoundEndClientType = 0x306, RoundEndStatusType = 0x307;
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
    bool ClientReady = false; // Host hat das Bereit des Clients für diese Revision (bestaetigen-alle)
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
    Out.push_back(static_cast<uint8_t>((Value.ClientClaimed ? 1 : 0) | (Value.HostClaimed ? 2 : 0) | (Value.HostWaiting ? 4 : 0) | (Value.ClientReady ? 8 : 0)));
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
    if (In.size() < 22 || In[20] > 15 || In[21] > MaxChoices)
        return std::nullopt;
    Offer Value;
    Value.Session = Detail::Get(In, 0, 8);
    Value.Revision = Detail::Get(In, 8, 8);
    Value.Level = static_cast<int32_t>(Detail::Get(In, 16, 4));
    Value.ClientClaimed = In[20] & 1;
    Value.HostClaimed = In[20] & 2;
    Value.HostWaiting = In[20] & 4;
    Value.ClientReady = In[20] & 8;
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
// Ein Item: 21 Byte plus 8 je belegtem Feld.
inline void PutItem(std::vector<uint8_t>& Out, const Game::InventoryEdit::ItemState& Item)
{
    Put(Out, Item.Source, 2);
    Put(Out, Item.DataHash, 4);
    Out.push_back(static_cast<uint8_t>((Item.Present ? 1 : 0) | (Item.InBackpack ? 2 : 0) | (Item.Trash ? 4 : 0) | (Item.AddEffect ? 8 : 0)));
    Out.push_back(Item.RemoveContext);
    Put(Out, static_cast<uint32_t>(Item.Turns), 4);
    PutTile(Out, Item.Pivot);
    Out.push_back(static_cast<uint8_t>(Item.Tiles.size()));
    for (const auto& At : Item.Tiles)
        PutTile(Out, At);
}
inline std::optional<Game::InventoryEdit::ItemState> GetItem(std::span<const uint8_t> In, size_t& At)
{
    using namespace Game::InventoryEdit;
    if (At + 21 > In.size() || In[At + 6] > 15 || In[At + 20] > MaxItemTiles || At + 21 + 8 * size_t(In[At + 20]) > In.size())
        return std::nullopt;
    ItemState Item;
    Item.Source = static_cast<uint16_t>(Get(In, At, 2));
    Item.DataHash = static_cast<uint32_t>(Get(In, At + 2, 4));
    Item.Present = In[At + 6] & 1;
    Item.InBackpack = In[At + 6] & 2;
    Item.Trash = In[At + 6] & 4;
    Item.AddEffect = In[At + 6] & 8;
    Item.RemoveContext = In[At + 7];
    Item.Turns = static_cast<int32_t>(Get(In, At + 8, 4));
    Item.Pivot = GetTile(In, At + 12);
    const uint8_t Tiles = In[At + 20];
    At += 21;
    for (uint8_t t = 0; t < Tiles; ++t, At += 8)
        Item.Tiles.push_back(GetTile(In, At));
    return Item;
}
inline void PutName(std::vector<uint8_t>& Out, const std::string& Name)
{
    Out.push_back(static_cast<uint8_t>(Name.size()));
    Out.insert(Out.end(), Name.begin(), Name.end());
}
inline std::optional<std::string> GetName(std::span<const uint8_t> In, size_t& At)
{
    if (At >= In.size() || In[At] > Game::InventoryEdit::MaxObjectName || At + 1 + In[At] > In.size())
        return std::nullopt;
    std::string Name(reinterpret_cast<const char*>(In.data() + At + 1), In[At]);
    At += 1 + In[At];
    return Name;
}
}

// Edit, Client an Host: Kopf 26 Byte (Id 8, Sitzung 8, Revision 8, Wahl 1 mit 0xFF = keine, Anzahl neuer Felder 1), je neues
// Feld 8 Byte, dann Anzahl Items (1) und je Item 21 Byte plus 8 je belegtem Feld. Seit Mod-Version 7 (werkbank-client) danach
// Anzahl Aufwertungen (1), je Aufwertung Anzahl Eingaben (1), je Eingabe 2 Byte, Name der Ausgabedaten und der Seltenheit
// (Länge 1, UTF-8) und die Ausgabe als Item.
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
        Detail::PutItem(Out, Item);
    Out.push_back(static_cast<uint8_t>(Value.Upgrades.size()));
    for (const auto& Next : Value.Upgrades)
    {
        Out.push_back(static_cast<uint8_t>(Next.Inputs.size()));
        for (const uint16_t Input : Next.Inputs)
            Detail::Put(Out, Input, 2);
        Detail::PutName(Out, Next.Data);
        Detail::PutName(Out, Next.Rarity);
        Detail::PutItem(Out, Next.Output);
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
        auto Item = Detail::GetItem(In, At);
        if (!Item)
            return std::nullopt;
        Value.Items.push_back(std::move(*Item));
    }
    if (At >= In.size() || In[At] > MaxUpgrades)
        return std::nullopt;
    const uint8_t Upgrades = In[At++];
    for (uint8_t i = 0; i < Upgrades; ++i)
    {
        if (At >= In.size() || In[At] > MaxUpgradeInputs || At + 1 + 2 * size_t(In[At]) > In.size())
            return std::nullopt;
        Upgrade Next;
        const uint8_t Inputs = In[At++];
        for (uint8_t k = 0; k < Inputs; ++k, At += 2)
            Next.Inputs.push_back(static_cast<uint16_t>(Detail::Get(In, At, 2)));
        auto Data = Detail::GetName(In, At);
        auto Rarity = Data ? Detail::GetName(In, At) : std::nullopt;
        auto Output = Rarity ? Detail::GetItem(In, At) : std::nullopt;
        if (!Output)
            return std::nullopt;
        Next.Data = std::move(*Data);
        Next.Rarity = std::move(*Rarity);
        Next.Output = std::move(*Output);
        Value.Upgrades.push_back(std::move(Next));
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

// Bereit, Client an Host, genau 17 Byte: Sitzung (8, ungleich 0), Revision (8), bereit (1, 0 oder 1). Der Client hat Bestätigen
// gedrückt (bestaetigen-alle) bzw. zurückgenommen; er schickt es erneut, bis das Sitzungsbild den Stand zeigt.
struct Ready
{
    uint64_t Session = 0, Revision = 0;
    bool Value = false;
    bool operator==(const Ready&) const = default;
};

inline std::vector<uint8_t> Encode(const Ready& Value)
{
    if (!Value.Session)
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Session, 8);
    Detail::Put(Out, Value.Revision, 8);
    Out.push_back(Value.Value ? 1 : 0);
    return Out;
}

inline std::optional<Ready> DecodeReady(std::span<const uint8_t> In)
{
    if (In.size() != 17 || In[16] > 1 || !Detail::Get(In, 0, 8))
        return std::nullopt;
    return Ready{Detail::Get(In, 0, 8), Detail::Get(In, 8, 8), In[16] == 1};
}

// Lager am Rundenende, Client an Host, genau 9 Byte: Id des Rundenendes (Game::Map::RoundEnd::Id, 8, ungleich 0), das der
// Client selbst geöffnet hat, und bereit (1, 0 oder 1): Bestätigen im Lager gedrückt oder schon ausgeführt. Der Client schickt
// es alle 500 ms, solange er in diesem Rundenende ist. Nur dann wartet der Host auf ihn; ein Zurücknehmen gibt es nicht.
struct RoundEndClient
{
    uint64_t Id = 0;
    bool Ready = false;
    bool operator==(const RoundEndClient&) const = default;
};

inline std::vector<uint8_t> Encode(const RoundEndClient& Value)
{
    if (!Value.Id)
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Id, 8);
    Out.push_back(Value.Ready ? 1 : 0);
    return Out;
}

inline std::optional<RoundEndClient> DecodeRoundEndClient(std::span<const uint8_t> In)
{
    if (In.size() != 9 || In[8] > 1 || !Detail::Get(In, 0, 8))
        return std::nullopt;
    return RoundEndClient{Detail::Get(In, 0, 8), In[8] == 1};
}

// Stand im Lager am Rundenende, Host an Client, genau 9 Byte: Id des Rundenendes (8, 0 = keins) und Flags (1): Bit 0 Host
// bereit, Bit 1 Host hat das Bereit des Clients. Ohne Rundenende sind die Flags 0. Bei Änderung sofort, sonst jede Sekunde.
struct RoundEndStatus
{
    uint64_t Id = 0;
    bool HostReady = false, ClientReady = false;
    bool operator==(const RoundEndStatus&) const = default;
};

inline std::vector<uint8_t> Encode(const RoundEndStatus& Value)
{
    if (!Value.Id && (Value.HostReady || Value.ClientReady))
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value.Id, 8);
    Out.push_back(static_cast<uint8_t>((Value.HostReady ? 1 : 0) | (Value.ClientReady ? 2 : 0)));
    return Out;
}

inline std::optional<RoundEndStatus> DecodeRoundEndStatus(std::span<const uint8_t> In)
{
    if (In.size() != 9 || (In[8] & ~3) || (!Detail::Get(In, 0, 8) && In[8]))
        return std::nullopt;
    return RoundEndStatus{Detail::Get(In, 0, 8), (In[8] & 1) != 0, (In[8] & 2) != 0};
}

// Seltenheiten, Host an Client (werkbank-client, Mod-Version 7): Anzahl (1, höchstens 96), je Item Platz in MyItems (2),
// Prüfsumme des ItemData-Objektnamens (4) und Objektname der Seltenheit (Länge 1, höchstens 120, UTF-8). Bei Änderung
// sofort, sonst alle 5 s.
inline constexpr uint16_t RarityType = 0x308;
inline constexpr size_t MaxRarityItems = 96;

struct Rarities
{
    struct Entry
    {
        uint16_t Index = 0;
        uint32_t DataHash = 0;
        std::string Rarity;
        bool operator==(const Entry&) const = default;
    };
    std::vector<Entry> Items;
    bool operator==(const Rarities&) const = default;
};

inline std::vector<uint8_t> Encode(const Rarities& Value)
{
    if (Value.Items.size() > MaxRarityItems)
        return {};
    std::vector<uint8_t> Out;
    Out.push_back(static_cast<uint8_t>(Value.Items.size()));
    for (const auto& Item : Value.Items)
    {
        if (Item.Rarity.size() > Game::InventoryEdit::MaxObjectName)
            return {};
        Detail::Put(Out, Item.Index, 2);
        Detail::Put(Out, Item.DataHash, 4);
        Detail::PutName(Out, Item.Rarity);
    }
    return Out;
}

inline std::optional<Rarities> DecodeRarities(std::span<const uint8_t> In)
{
    if (In.empty() || In[0] > MaxRarityItems)
        return std::nullopt;
    Rarities Value;
    size_t At = 1;
    for (uint8_t i = 0; i < In[0]; ++i)
    {
        if (At + 6 > In.size())
            return std::nullopt;
        Rarities::Entry Item;
        Item.Index = static_cast<uint16_t>(Detail::Get(In, At, 2));
        Item.DataHash = static_cast<uint32_t>(Detail::Get(In, At + 2, 4));
        At += 6;
        auto Name = Detail::GetName(In, At);
        if (!Name)
            return std::nullopt;
        Item.Rarity = std::move(*Name);
        Value.Items.push_back(std::move(Item));
    }
    if (At != In.size())
        return std::nullopt;
    return Value;
}
}
