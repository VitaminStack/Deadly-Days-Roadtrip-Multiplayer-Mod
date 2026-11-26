#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Game/MapAdapter.h"

// Karte des Hosts über den Kontrollkanal (missionsstart). Der Host schickt bei jeder Änderung und jede Sekunde den Stand,
// Id 0 heißt: gerade keine Karte. Little-Endian, Texte UTF-8 mit einem Längenbyte.
namespace Sync::MapWire
{
inline constexpr uint16_t HostMapType = 0x400;
inline constexpr size_t MaxText = 240, MaxTags = 32;

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
inline void PutText(std::vector<uint8_t>& Out, const std::string& Text)
{
    Out.push_back(static_cast<uint8_t>(Text.size()));
    Out.insert(Out.end(), Text.begin(), Text.end());
}
inline bool GetText(std::span<const uint8_t> In, size_t& At, std::string& Text)
{
    if (At >= In.size() || In[At] > MaxText || At + 1 + In[At] > In.size())
        return false;
    Text.assign(reinterpret_cast<const char*>(In.data() + At + 1), In[At]);
    At += 1 + In[At];
    return true;
}
}

inline bool Valid(const Game::Map::HostMap& Value)
{
    if (!Value.Id)
        return Value == Game::Map::HostMap{};
    if (Value.Profile.empty() || Value.Profile.size() > MaxText || Value.Tags.size() > MaxTags)
        return false;
    for (const auto& Tag : Value.Tags)
        if (Tag.empty() || Tag.size() > MaxText)
            return false;
    if (Value.GraphState == Game::Map::Graph::Set)
        return !Value.GraphPackage.empty() && !Value.GraphAsset.empty() && Value.GraphPackage.size() <= MaxText &&
               Value.GraphAsset.size() <= MaxText;
    return Value.GraphState <= Game::Map::Graph::None && Value.GraphPackage.empty() && Value.GraphAsset.empty();
}

// Leer, wenn der Wert nicht gültig ist. nullopt wird zur Nachricht „keine Karte“.
inline std::vector<uint8_t> Encode(const std::optional<Game::Map::HostMap>& Value)
{
    const Game::Map::HostMap Map = Value.value_or(Game::Map::HostMap{});
    if (!Valid(Map))
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Map.Id, 8);
    if (!Map.Id)
        return Out;
    for (const int32_t Field : {Map.Seed, Map.MaxTileX, Map.MaxTileY})
        Detail::Put(Out, static_cast<uint32_t>(Field), 4);
    Detail::PutText(Out, Map.Profile);
    Out.push_back(static_cast<uint8_t>(Map.Tags.size()));
    for (const auto& Tag : Map.Tags)
        Detail::PutText(Out, Tag);
    Out.push_back(static_cast<uint8_t>(Map.GraphState));
    if (Map.GraphState == Game::Map::Graph::Set)
    {
        Detail::PutText(Out, Map.GraphPackage);
        Detail::PutText(Out, Map.GraphAsset);
    }
    return Out;
}

// Äußeres nullopt: kaputte Nachricht. Inneres nullopt: Host hat keine Karte.
inline std::optional<std::optional<Game::Map::HostMap>> Decode(std::span<const uint8_t> In)
{
    if (In.size() < 8)
        return std::nullopt;
    Game::Map::HostMap Map;
    Map.Id = Detail::Get(In, 0, 8);
    if (!Map.Id)
        return In.size() == 8 ? std::optional<std::optional<Game::Map::HostMap>>(std::optional<Game::Map::HostMap>{}) : std::nullopt;
    if (In.size() < 20)
        return std::nullopt;
    Map.Seed = static_cast<int32_t>(Detail::Get(In, 8, 4));
    Map.MaxTileX = static_cast<int32_t>(Detail::Get(In, 12, 4));
    Map.MaxTileY = static_cast<int32_t>(Detail::Get(In, 16, 4));
    size_t At = 20;
    if (!Detail::GetText(In, At, Map.Profile) || At >= In.size() || In[At] > MaxTags)
        return std::nullopt;
    const uint8_t Tags = In[At++];
    for (uint8_t i = 0; i < Tags; ++i)
        if (!Detail::GetText(In, At, Map.Tags.emplace_back()))
            return std::nullopt;
    if (At >= In.size() || In[At] > static_cast<uint8_t>(Game::Map::Graph::Set))
        return std::nullopt;
    Map.GraphState = static_cast<Game::Map::Graph>(In[At++]);
    if (Map.GraphState == Game::Map::Graph::Set &&
        (!Detail::GetText(In, At, Map.GraphPackage) || !Detail::GetText(In, At, Map.GraphAsset)))
        return std::nullopt;
    if (At != In.size() || !Valid(Map))
        return std::nullopt;
    return std::optional<Game::Map::HostMap>(std::move(Map));
}
// Rundenende des Hosts (rundenende-client, run-ende): Karten-Id und Id des Endes, je 8 Bytes, dann der Ausgang des Runs
// (RunOutcome, 1 Byte). Alles 0 heißt kein Ende. Der Host schickt es bei jeder Änderung und jede Sekunde wie die Karte.
inline constexpr uint16_t RoundEndType = 0x401;
inline constexpr size_t RoundEndSize = 17;

inline std::vector<uint8_t> EncodeRoundEnd(const std::optional<Game::Map::RoundEnd>& Value)
{
    if (Value && (!Value->MapId || !Value->Id))
        return {};
    std::vector<uint8_t> Out;
    Detail::Put(Out, Value ? Value->MapId : 0, 8);
    Detail::Put(Out, Value ? Value->Id : 0, 8);
    Out.push_back(static_cast<uint8_t>(Value ? Value->Outcome : Game::Map::RunOutcome::None));
    return Out;
}

// Äußeres nullopt: kaputte Nachricht. Inneres nullopt: kein Rundenende.
inline std::optional<std::optional<Game::Map::RoundEnd>> DecodeRoundEnd(std::span<const uint8_t> In)
{
    if (In.size() != RoundEndSize || In[16] > static_cast<uint8_t>(Game::Map::RunOutcome::Won))
        return std::nullopt;
    const Game::Map::RoundEnd Value{Detail::Get(In, 0, 8), Detail::Get(In, 8, 8), static_cast<Game::Map::RunOutcome>(In[16])};
    if (!Value.MapId && !Value.Id && Value.Outcome == Game::Map::RunOutcome::None)
        return std::optional<Game::Map::RoundEnd>{};
    if (!Value.MapId || !Value.Id)
        return std::nullopt;
    return std::optional<Game::Map::RoundEnd>(Value);
}

// opt-ergebnis-werte: Stand der Rundenübersicht des Hosts, vor dem Rundenende 0x401 desselben Endes. Id des Rundenendes (8), bekannt
// (1: 0 oder 1), Tageskennung des Hosts mit Tag (4) und Teilmission (int8, 1), Anzahl (2), je Wert Tag (4), Teilmission (int8, 1),
// Typ (1), Spieler (1, 0xFF gesamt), Wert (8). Unbekannt heißt Tageskennung 0.0 ohne Werte. Jeder Schlüssel aus Spieler, Tag,
// Teilmission und Typ nur einmal.
inline constexpr uint16_t RoundStatsType = 0x402;
inline constexpr size_t RoundStatsHeader = 16, RoundStatsEntry = 15;

inline bool Valid(const Game::Map::RoundStatsImage& Value)
{
    const auto& Stats = Value.Values;
    if (!Value.EndId || !Game::RoundStats::ValidTime(Stats.Day, Stats.SubMission) || !Game::RoundStats::Valid(Stats.Entries))
        return false;
    return Value.Known || Stats == Game::RoundStats::Snapshot{};
}

inline std::vector<uint8_t> EncodeRoundStats(const Game::Map::RoundStatsImage& Value)
{
    if (!Valid(Value))
        return {};
    const auto& Stats = Value.Values;
    std::vector<uint8_t> Out;
    Out.reserve(RoundStatsHeader + Stats.Entries.size() * RoundStatsEntry);
    Detail::Put(Out, Value.EndId, 8);
    Out.push_back(Value.Known ? 1 : 0);
    Detail::Put(Out, static_cast<uint32_t>(Stats.Day), 4);
    Out.push_back(static_cast<uint8_t>(static_cast<int8_t>(Stats.SubMission)));
    Detail::Put(Out, Stats.Entries.size(), 2);
    for (const auto& Entry : Stats.Entries)
    {
        Detail::Put(Out, static_cast<uint32_t>(Entry.Day), 4);
        Out.push_back(static_cast<uint8_t>(static_cast<int8_t>(Entry.SubMission)));
        Out.push_back(Entry.Type);
        Out.push_back(static_cast<uint8_t>(static_cast<int8_t>(Entry.Player)));
        Detail::Put(Out, static_cast<uint64_t>(Entry.Value), 8);
    }
    return Out;
}

inline std::optional<Game::Map::RoundStatsImage> DecodeRoundStats(std::span<const uint8_t> In)
{
    if (In.size() < RoundStatsHeader || In[8] > 1)
        return std::nullopt;
    const size_t Count = Detail::Get(In, 14, 2);
    if (Count > Game::RoundStats::MaxEntries || In.size() != RoundStatsHeader + Count * RoundStatsEntry)
        return std::nullopt;
    Game::Map::RoundStatsImage Value{Detail::Get(In, 0, 8), In[8] == 1};
    Value.Values.Day = static_cast<int32_t>(Detail::Get(In, 9, 4));
    Value.Values.SubMission = static_cast<int8_t>(In[13]);
    for (size_t At = RoundStatsHeader; At < In.size(); At += RoundStatsEntry)
        Value.Values.Entries.push_back({static_cast<int32_t>(Detail::Get(In, At, 4)), static_cast<int8_t>(In[At + 4]), In[At + 5],
                                        static_cast<int8_t>(In[At + 6]), static_cast<int64_t>(Detail::Get(In, At + 7, 8))});
    return Valid(Value) ? std::optional(std::move(Value)) : std::nullopt;
}
}