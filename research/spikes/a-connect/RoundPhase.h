// Display-only spike contract, deliberately independent of UE/host persistence.
// A future transport must bind Session/Run/World before delivering snapshots.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace Spike::RoundPhase
{
enum class Phase : uint8_t { Playing, RoundEnd, Stash, Route, Travel };
enum class Reason : uint8_t { NoReason, NewDay, Win, Lose };
struct Snapshot
{
    uint64_t Session = 0;
    uint64_t Run = 0;
    uint64_t World = 0;
    uint64_t Sequence = 0;
    int32_t Day = 0;
    Phase Stage = Phase::Playing;
    Reason EndReason = Reason::NoReason;
    uint16_t HostProgress = 0; // 0..100, not a locally inferred panel state.

    bool operator==(const Snapshot&) const = default;
};

inline bool Valid(const Snapshot& Value)
{
    return Value.Session && Value.Run && Value.World && Value.Sequence && Value.Day >= 0 &&
        static_cast<uint8_t>(Value.Stage) <= static_cast<uint8_t>(Phase::Travel) &&
        static_cast<uint8_t>(Value.EndReason) <= static_cast<uint8_t>(Reason::Lose) &&
        Value.HostProgress <= 100 &&
        (Value.Stage != Phase::RoundEnd || Value.EndReason != Reason::NoReason);
}

// Fixed little-endian wire form: no native struct padding, pointers, strings or commands.
constexpr size_t kWireSize = 48;
using Wire = std::array<uint8_t, kWireSize>;
inline void Put(Wire& Bytes, size_t Offset, uint64_t Value, size_t Count)
{
    for (size_t i = 0; i < Count; ++i)
        Bytes[Offset + i] = static_cast<uint8_t>(Value >> (i * 8));
}
inline uint64_t Get(const uint8_t* Bytes, size_t Offset, size_t Count)
{
    uint64_t Value = 0;
    for (size_t i = 0; i < Count; ++i)
        Value |= uint64_t(Bytes[Offset + i]) << (i * 8);
    return Value;
}
inline bool Encode(const Snapshot& Value, Wire& Bytes)
{
    if (!Valid(Value))
        return false;
    Wire Result{};
    Put(Result, 0, 0x50524444, 4); // DDRP
    Put(Result, 4, 1, 2); // Spike schema version, not the future production protocol.
    Put(Result, 6, kWireSize, 2);
    Put(Result, 8, Value.Session, 8);
    Put(Result, 16, Value.Run, 8);
    Put(Result, 24, Value.World, 8);
    Put(Result, 32, Value.Sequence, 8);
    Put(Result, 40, static_cast<uint32_t>(Value.Day), 4);
    Put(Result, 44, static_cast<uint8_t>(Value.Stage), 1);
    Put(Result, 45, static_cast<uint8_t>(Value.EndReason), 1);
    Put(Result, 46, Value.HostProgress, 2);
    Bytes = Result;
    return true;
}
inline bool Decode(const uint8_t* Bytes, size_t Size, Snapshot& Value)
{
    if (!Bytes || Size != kWireSize || Get(Bytes, 0, 4) != 0x50524444 ||
        Get(Bytes, 4, 2) != 1 || Get(Bytes, 6, 2) != kWireSize || Get(Bytes, 40, 4) > INT32_MAX)
        return false;
    Snapshot Result{Get(Bytes, 8, 8), Get(Bytes, 16, 8), Get(Bytes, 24, 8), Get(Bytes, 32, 8),
        static_cast<int32_t>(Get(Bytes, 40, 4)), static_cast<Phase>(Bytes[44]),
        static_cast<Reason>(Bytes[45]), static_cast<uint16_t>(Get(Bytes, 46, 2))};
    if (!Valid(Result))
        return false;
    Value = Result;
    return true;
}

struct Presentation
{
    bool Attached = false;
    bool Waiting = false;
    bool InputBlocked = false;
    Snapshot Host;
};
enum class Result { Applied, Duplicate, Rejected };

class Receiver
{
public:
    // Only a verified transport/world binding may call this, never an incoming arbitrary snapshot.
    // Keep generation high-water marks on travel; no packet can reopen an old world.
    bool Bind(uint64_t Session, uint64_t Run, uint64_t World)
    {
        if (!Session || !Run || !World || (State.Attached &&
            (Session != State.Host.Session || Run < State.Host.Run ||
                (Run == State.Host.Run && World <= State.Host.World))))
            return false;
        State = {true, true, true, {Session, Run, World}};
        HasSnapshot = false;
        return true;
    }

    Result Receive(const uint8_t* Bytes, size_t Size)
    {
        Snapshot Value;
        if (!Decode(Bytes, Size, Value) || !State.Attached || Value.Session != State.Host.Session ||
            Value.Run != State.Host.Run || Value.World != State.Host.World)
            return Result::Rejected;
        if (HasSnapshot)
        {
            if (Value.Sequence == State.Host.Sequence)
                return Value == State.Host ? Result::Duplicate : Result::Rejected;
            if (Value.Sequence < State.Host.Sequence || Value.Day != State.Host.Day || Value.Stage < State.Host.Stage)
                return Result::Rejected;
        }
        // One commit for both display and input policy; no UE function or persistence can be invoked here.
        State = {true, false, Value.Stage != Phase::Playing, Value};
        HasSnapshot = true;
        return Result::Applied;
    }

    void Disconnect()
    {
        State = {};
        HasSnapshot = false;
    }
    const Presentation& View() const { return State; }

private:
    Presentation State;
    bool HasSnapshot = false;
};
}
