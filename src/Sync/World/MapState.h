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
}
