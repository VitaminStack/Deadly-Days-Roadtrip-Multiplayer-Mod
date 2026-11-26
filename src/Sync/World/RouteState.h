#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Game/RouteMapPolicy.h"

// opt-routenkarte: Run-Info des Hosts für die Routenkarte des Clients (0x403), Format in docs/CONTROL_PROTOCOL.md. Little-Endian.
// Karten-Id (8), Seed (4), Gefahrenzuschlag (4), aktuelle Kennung Gruppe und Route (je 4, int32), Index und gemerkter Index (je
// 4, int32), Ende der Kette (1: 0 oder 1), vorige Kennung Gruppe und Route (je 4, -1 ohne), Anzahl der Wahl (1), je Wahl 1 Byte.
namespace Sync::RouteWire
{
inline constexpr uint16_t HostRouteType = 0x403;
inline constexpr size_t HeaderSize = 42;

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
inline int32_t GetInt(std::span<const uint8_t> In, size_t At)
{
    return static_cast<int32_t>(static_cast<uint32_t>(Get(In, At, 4)));
}
}

// Leer, wenn der Stand ungültig ist.
inline std::vector<uint8_t> Encode(const Game::RouteMap::HostRoute& Value)
{
    if (!Game::RouteMap::Valid(Value))
        return {};
    std::vector<uint8_t> Out;
    Out.reserve(HeaderSize + Value.Chosen.size());
    Detail::Put(Out, Value.MapId, 8);
    for (const int32_t Field : {Value.Seed, Value.ThreatIncrease, Value.Current.Group, Value.Current.Route, Value.MissionIndex,
                                Value.CachedMissionIndex})
        Detail::Put(Out, static_cast<uint32_t>(Field), 4);
    Out.push_back(Value.EndOfChain ? 1 : 0);
    Detail::Put(Out, static_cast<uint32_t>(Value.Previous.Group), 4);
    Detail::Put(Out, static_cast<uint32_t>(Value.Previous.Route), 4);
    Out.push_back(static_cast<uint8_t>(Value.Chosen.size()));
    for (const int32_t Index : Value.Chosen)
        Out.push_back(static_cast<uint8_t>(Index));
    return Out;
}

inline std::optional<Game::RouteMap::HostRoute> Decode(std::span<const uint8_t> In)
{
    if (In.size() < HeaderSize || In[32] > 1 || In.size() != HeaderSize + In[41])
        return std::nullopt;
    Game::RouteMap::HostRoute Value;
    Value.MapId = Detail::Get(In, 0, 8);
    Value.Seed = Detail::GetInt(In, 8);
    Value.ThreatIncrease = Detail::GetInt(In, 12);
    Value.Current = {Detail::GetInt(In, 16), Detail::GetInt(In, 20)};
    Value.MissionIndex = Detail::GetInt(In, 24);
    Value.CachedMissionIndex = Detail::GetInt(In, 28);
    Value.EndOfChain = In[32] == 1;
    Value.Previous = {Detail::GetInt(In, 33), Detail::GetInt(In, 37)};
    for (size_t At = HeaderSize; At < In.size(); ++At)
        Value.Chosen.push_back(In[At]);
    return Game::RouteMap::Valid(Value) ? std::optional(std::move(Value)) : std::nullopt;
}
}
