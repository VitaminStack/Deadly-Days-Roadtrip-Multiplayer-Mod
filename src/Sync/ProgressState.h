#pragma once

#include <bit>
#include <cmath>
#include <string>
#include <set>
#include "Game/AirdropAdapter.h"
#include "Game/ExpAdapter.h"
#include "Net/Protocol.h"

// Absolutes Anzeigemodell, absichtlich keine AddExperience-/Reward-Operation.
namespace Sync::Progress
{
inline constexpr uint16_t Subscribe = 0x100, Snapshot = 0x101, Orbs = 0x102;
inline constexpr size_t MaxOrbs = 256, SpawnBytes = 80;
struct State
{
    uint64_t ClientRun = 0, Nonce = 0;
    int32_t Day = 0, Level = 1;
    float Fraction = 0;
    std::string Map;
    uint64_t StateGuid = 0;
    bool AirdropsKnown = false;
    std::vector<Game::AirdropView> Airdrops;
    bool operator==(const State&) const = default;
};
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
inline bool Valid(const State& Value)
{
    if (Value.Airdrops.size() > 64 || (!Value.AirdropsKnown && !Value.Airdrops.empty()))
        return false;
    std::set<uint64_t> Guids;
    for (const auto& Crate : Value.Airdrops)
        if (!Crate.Guid || Crate.Guid > 0x3FFFFFFFFFFFFFull || !Guids.insert(Crate.Guid).second ||
            !std::isfinite(Crate.Position) || Crate.Position < 0 || Crate.Position >= 36000 ||
            (!Crate.HasAnimation && (Crate.Position != 0 || Crate.Playing)))
            return false;
    return Value.ClientRun && Value.Nonce && Value.StateGuid && Value.Day >= 0 && Value.Day <= 100000 && Value.Level >= 1 && Value.Level <= 100000 &&
           std::isfinite(Value.Fraction) && Value.Fraction >= 0 && Value.Fraction <= 1 && !Value.Map.empty() && Value.Map.size() <= 128 &&
           Value.Map.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos;
}
inline std::vector<uint8_t> Encode(const State& Value)
{
    if (!Valid(Value))
        return {};
    std::vector<uint8_t> Out;
    Put(Out, Value.ClientRun, 8);
    Put(Out, Value.Nonce, 8);
    Put(Out, static_cast<uint32_t>(Value.Day), 4);
    Put(Out, static_cast<uint32_t>(Value.Level), 4);
    Put(Out, std::bit_cast<uint32_t>(Value.Fraction), 4);
    Put(Out, Value.StateGuid, 8);
    Put(Out, Value.Map.size(), 1);
    Out.insert(Out.end(), Value.Map.begin(), Value.Map.end());
    Put(Out, Value.AirdropsKnown ? 1 : 0, 1);
    Put(Out, Value.Airdrops.size(), 1);
    for (const auto& Crate : Value.Airdrops)
    {
        Put(Out, Crate.Guid, 8);
        Put(Out, (Crate.Opened ? 1u : 0u) | (Crate.Ready ? 2u : 0u) | (Crate.HasAnimation ? 4u : 0u) | (Crate.Playing ? 8u : 0u) |
            (Crate.Hidden ? 16u : 0u), 1);
        Put(Out, std::bit_cast<uint32_t>(Crate.Position), 4);
    }
    return Out;
}
inline std::optional<State> Decode(std::span<const uint8_t> In)
{
    if (In.size() < 40 || In.size() < size_t(39) + In[36])
        return std::nullopt;
    const size_t Tail = size_t(37) + In[36];
    if (In[Tail] > 1 || In[Tail + 1] > 64 || In.size() != Tail + 2 + size_t(In[Tail + 1]) * 13)
        return std::nullopt;
    State Value{Get(In, 0, 8), Get(In, 8, 8), static_cast<int32_t>(Get(In, 16, 4)),
        static_cast<int32_t>(Get(In, 20, 4)), std::bit_cast<float>(static_cast<uint32_t>(Get(In, 24, 4))),
        std::string(In.begin() + 37, In.begin() + Tail), Get(In, 28, 8)};
    Value.AirdropsKnown = In[Tail] != 0;
    for (size_t At = Tail + 2; At < In.size(); At += 13)
    {
        const uint8_t Flags = In[At + 8];
        if (Flags & 0xE0)
            return std::nullopt;
        Value.Airdrops.push_back({Get(In, At, 8), bool(Flags & 1), bool(Flags & 2), bool(Flags & 4), bool(Flags & 8),
            std::bit_cast<float>(static_cast<uint32_t>(Get(In, At + 9, 4))), bool(Flags & 16)});
    }
    return Valid(Value) ? std::optional(Value) : std::nullopt;
}

// Reine Anzeige: Host-XP-Ausstoesse seit dem letzten Bild. Gehen sie verloren, fehlen nur Baelle, kein XP.
struct OrbBatch
{
    uint64_t ClientRun = 0, Nonce = 0, StateGuid = 0;
    int32_t Day = 0;
    std::vector<Game::ExpSpawn> Spawns;
    bool operator==(const OrbBatch&) const = default;
};
inline bool ValidSpawn(const Game::ExpSpawn& Value)
{
    if (Value.Amount <= 0 || Value.Amount > 1000000 || !std::isfinite(Value.Extra) || !std::isfinite(Value.Tail))
        return false;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(Value.Start[i]) || !std::isfinite(Value.End[i]) || std::fabs(Value.Start[i]) > 1e7 || std::fabs(Value.End[i]) > 1e7)
            return false;
    for (const float Channel : Value.Color)
        if (!std::isfinite(Channel))
            return false;
    return true;
}
inline bool Valid(const OrbBatch& Value)
{
    if (!Value.ClientRun || !Value.Nonce || !Value.StateGuid || Value.Day < 0 || Value.Day > 100000 || Value.Spawns.empty() ||
        Value.Spawns.size() > MaxOrbs)
        return false;
    for (const auto& Spawn : Value.Spawns)
        if (!ValidSpawn(Spawn))
            return false;
    return true;
}
inline std::vector<uint8_t> EncodeOrbs(const OrbBatch& Value)
{
    if (!Valid(Value))
        return {};
    std::vector<uint8_t> Out;
    Put(Out, Value.ClientRun, 8);
    Put(Out, Value.Nonce, 8);
    Put(Out, Value.StateGuid, 8);
    Put(Out, static_cast<uint32_t>(Value.Day), 4);
    Put(Out, Value.Spawns.size(), 2);
    for (const auto& Spawn : Value.Spawns)
    {
        Put(Out, static_cast<uint32_t>(Spawn.Amount), 4);
        for (const double Axis : Spawn.Start)
            Put(Out, std::bit_cast<uint64_t>(Axis), 8);
        for (const double Axis : Spawn.End)
            Put(Out, std::bit_cast<uint64_t>(Axis), 8);
        for (const float Channel : Spawn.Color)
            Put(Out, std::bit_cast<uint32_t>(Channel), 4);
        Put(Out, std::bit_cast<uint64_t>(Spawn.Extra), 8);
        Put(Out, std::bit_cast<uint32_t>(Spawn.Tail), 4);
    }
    return Out;
}
inline std::optional<OrbBatch> DecodeOrbs(std::span<const uint8_t> In)
{
    if (In.size() < 30)
        return std::nullopt;
    const size_t Count = Get(In, 28, 2);
    if (Count > MaxOrbs || In.size() != 30 + Count * SpawnBytes)
        return std::nullopt;
    OrbBatch Value{Get(In, 0, 8), Get(In, 8, 8), Get(In, 16, 8), static_cast<int32_t>(Get(In, 24, 4))};
    for (size_t At = 30; At < In.size(); At += SpawnBytes)
    {
        Game::ExpSpawn Spawn{static_cast<int32_t>(Get(In, At, 4))};
        for (int i = 0; i < 3; ++i)
        {
            Spawn.Start[i] = std::bit_cast<double>(Get(In, At + 4 + 8 * i, 8));
            Spawn.End[i] = std::bit_cast<double>(Get(In, At + 28 + 8 * i, 8));
        }
        for (int i = 0; i < 4; ++i)
            Spawn.Color[i] = std::bit_cast<float>(static_cast<uint32_t>(Get(In, At + 52 + 4 * i, 4)));
        Spawn.Extra = std::bit_cast<double>(Get(In, At + 68, 8));
        Spawn.Tail = std::bit_cast<float>(static_cast<uint32_t>(Get(In, At + 76, 4)));
        Value.Spawns.push_back(Spawn);
    }
    return Valid(Value) ? std::optional(Value) : std::nullopt;
}

class Mirror
{
public:
    // Beim Welt-/Verbindungswechsel Sicht verwerfen; Revisions-Wasserstand bleibt pro Verbindung erhalten.
    void Bind(uint64_t PeerIn, uint64_t ClientRunIn, uint64_t NonceIn)
    {
        if (Peer != PeerIn)
        {
            Run = Epoch = Revision = OrbRevision = 0;
            Peer = PeerIn;
        }
        ClientRun = ClientRunIn;
        Nonce = NonceIn;
        Value.reset();
    }
    bool Accept(uint64_t From, const Net::Protocol::Control& Message, std::string_view Map, int32_t Day, uint64_t StateGuid)
    {
        if (!Peer || From != Peer || Message.Type != Snapshot || !Message.Run || !Message.Epoch || !Message.Revision ||
            (Run && Message.Run != Run) || Message.Epoch < Epoch || Message.Revision <= Revision)
            return false;
        auto Next = Decode(Message.Payload);
        if (!Next || Next->ClientRun != ClientRun || Next->Nonce != Nonce || Next->Map != Map || Next->Day != Day || Next->StateGuid != StateGuid)
            return false;
        Run = Message.Run;
        Epoch = Message.Epoch;
        Revision = Message.Revision;
        Value = std::move(Next);
        return true;
    }
    // Baelle nur fuer die gebundene Verbindung, Lebensdauer und Welt; jede Revision hoechstens einmal.
    std::optional<std::vector<Game::ExpSpawn>> AcceptOrbs(uint64_t From, const Net::Protocol::Control& Message, int32_t Day, uint64_t StateGuid)
    {
        if (!Peer || From != Peer || Message.Type != Orbs || !Message.Run || !Message.Epoch || !Message.Revision ||
            (Run && Message.Run != Run) || Message.Epoch < Epoch || Message.Revision <= OrbRevision)
            return std::nullopt;
        auto Next = DecodeOrbs(Message.Payload);
        if (!Next || Next->ClientRun != ClientRun || Next->Nonce != Nonce || Next->Day != Day || Next->StateGuid != StateGuid)
            return std::nullopt;
        OrbRevision = Message.Revision;
        return std::move(Next->Spawns);
    }
    std::optional<State> Value;
private:
    uint64_t Peer = 0, ClientRun = 0, Nonce = 0, Run = 0, Epoch = 0, Revision = 0, OrbRevision = 0;
};
}
