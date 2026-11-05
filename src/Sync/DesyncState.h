#pragma once

#include <optional>
#include "Game/DesyncSnapshot.h"
#include "Net/Protocol.h"

namespace Sync::Desync
{
using Game::Desync::Sample;
using Game::Desync::Count;
using Game::Desync::All;
using Game::Desync::Bit;
inline constexpr uint16_t Probe = 0x200, Result = 0x201;
inline constexpr uint64_t IntervalMs = 2000, StableMs = 2000, FreshMs = 10000;
inline constexpr unsigned Confirmations = 3;

// Kleine absolute Messung. Echo-Kennungen binden die Antwort an genau eine lokale Probe.
struct Packet
{
    uint64_t ClientRun = 0, Nonce = 0, Sequence = 0, StateGuid = 0;
    Sample Image;
    uint8_t Stable = 0, Ignored = 0, Alerts = 0;
    bool operator==(const Packet&) const = default;
};
inline bool Valid(const Packet& P)
{
    if (!P.ClientRun || !P.Nonce || !P.Sequence || !P.StateGuid || P.StateGuid > 0x3FFFFFFFFFFFFFull ||
        ((P.Image.Known | P.Stable | P.Ignored | P.Alerts) & ~All) || (P.Stable & ~P.Image.Known) ||
        (P.Alerts & (~P.Stable | P.Ignored))) return false;
    for (unsigned i = 0; i < Count; ++i)
        if (!(P.Image.Known & Bit(i)) && P.Image.Values[i]) return false;
    return (!(P.Image.Known & Bit(Game::Desync::Day)) || P.Image.Values[Game::Desync::Day] <= 100000) &&
           (!(P.Image.Known & Bit(Game::Desync::Phase)) || P.Image.Values[Game::Desync::Phase] <= 1) &&
           (!(P.Image.Known & Bit(Game::Desync::Players)) || P.Image.Values[Game::Desync::Players] <= 2) &&
           (!(P.Image.Known & Bit(Game::Desync::Enemies)) || P.Image.Values[Game::Desync::Enemies] <= 20000);
}
inline std::vector<uint8_t> Encode(const Packet& P)
{
    if (!Valid(P)) return {};
    std::vector<uint8_t> Bytes;
    const auto Put = [&](uint64_t Value) { for (unsigned i = 0; i < 8; ++i) Bytes.push_back(static_cast<uint8_t>(Value >> (8 * i))); };
    Put(P.ClientRun); Put(P.Nonce); Put(P.Sequence); Put(P.StateGuid);
    Bytes.insert(Bytes.end(), {P.Image.Known, P.Stable, P.Ignored, P.Alerts});
    for (const auto Value : P.Image.Values) Put(Value);
    return Bytes;
}
inline std::optional<Packet> Decode(std::span<const uint8_t> Bytes)
{
    if (Bytes.size() != 36 + 8 * Count) return std::nullopt;
    const auto Get = [&](size_t At)
    {
        uint64_t Value = 0;
        for (unsigned i = 0; i < 8; ++i) Value |= uint64_t(Bytes[At + i]) << (8 * i);
        return Value;
    };
    Packet P{Get(0), Get(8), Get(16), Get(24)};
    P.Image.Known = Bytes[32]; P.Stable = Bytes[33]; P.Ignored = Bytes[34]; P.Alerts = Bytes[35];
    for (unsigned i = 0; i < Count; ++i) P.Image.Values[i] = Get(36 + 8 * i);
    return Valid(P) ? std::optional(P) : std::nullopt;
}
class Stability
{
public:
    uint8_t Update(const Sample& Next, uint64_t Now)
    {
        uint8_t Stable = 0;
        for (unsigned i = 0; i < Count; ++i)
        {
            if (!(Next.Known & Bit(i))) Since[i] = Now;
            else if (!(Previous.Known & Bit(i)) || Previous.Values[i] != Next.Values[i]) Since[i] = Now;
            else if (Now >= Since[i] && Now - Since[i] >= StableMs) Stable |= Bit(i);
        }
        Previous = Next;
        return Stable;
    }
private:
    Sample Previous;
    std::array<uint64_t, Count> Since{};
};
struct Comparison
{
    uint8_t Comparable = 0, Different = 0, Alerts = 0;
    bool operator==(const Comparison&) const = default;
};
class Monitor
{
public:
    Comparison Compare(const Packet& Host, const Packet& Client, uint8_t Ignored)
    {
        Comparison Out;
        Out.Comparable = Host.Stable & Client.Stable & static_cast<uint8_t>(~Ignored) & All;
        for (unsigned i = 0; i < Count; ++i)
        {
            const bool Different = (Out.Comparable & Bit(i)) && Host.Image.Values[i] != Client.Image.Values[i];
            if (!Different) Strikes[i] = 0;
            else
            {
                Out.Different |= Bit(i);
                if (LastHost[i] != Host.Image.Values[i] || LastClient[i] != Client.Image.Values[i]) Strikes[i] = 0;
                LastHost[i] = Host.Image.Values[i]; LastClient[i] = Client.Image.Values[i];
                if (Strikes[i] < Confirmations) ++Strikes[i];
                if (Strikes[i] >= Confirmations) Out.Alerts |= Bit(i);
            }
        }
        return Out;
    }
    void Invalidate(uint8_t Comparable)
    {
        for (unsigned i = 0; i < Count; ++i) if (!(Comparable & Bit(i))) Strikes[i] = 0;
    }
private:
    std::array<unsigned, Count> Strikes{};
    std::array<uint64_t, Count> LastHost{}, LastClient{};
};

// Pro Verbindung und lokaler Lebensdauer neu binden. Fremde, alte, doppelte und nach Travel
// verspaetete Antworten duerfen keinen Zustand oder Revisions-Wasserstand veraendern.
class ReplyGate
{
public:
    void Bind(uint64_t PeerIn, uint64_t RunIn, uint64_t NonceIn, uint64_t GuidIn)
    {
        Peer = PeerIn; Run = RunIn; Nonce = NonceIn; Guid = GuidIn;
        Pending = Sequence = HostRun = Epoch = Revision = SentAt = 0;
    }
    void Sent(uint64_t Seq, uint64_t Now) { Pending = Seq; SentAt = Now; }
    bool Waiting(uint64_t Now) const { return Pending && Now >= SentAt && Now - SentAt <= FreshMs; }
    bool Accept(uint64_t From, const Net::Protocol::Control& Envelope, const Packet& P, uint64_t Now)
    {
        if (!Peer || From != Peer || Envelope.Type != Result || !Envelope.Run || !Envelope.Epoch || !Envelope.Revision ||
            (HostRun && HostRun != Envelope.Run) || Envelope.Epoch < Epoch || Envelope.Revision <= Revision ||
            !Valid(P) || P.ClientRun != Run || P.Nonce != Nonce || P.StateGuid != Guid ||
            !Pending || P.Sequence != Pending || P.Sequence <= Sequence || Now < SentAt || Now - SentAt > FreshMs) return false;
        HostRun = Envelope.Run; Epoch = Envelope.Epoch; Revision = Envelope.Revision; Sequence = P.Sequence; Pending = 0;
        return true;
    }
private:
    uint64_t Peer = 0, Run = 0, Nonce = 0, Guid = 0, Pending = 0, Sequence = 0, SentAt = 0;
    uint64_t HostRun = 0, Epoch = 0, Revision = 0;
};
}
