// Local diagnostic adapter policy, not a production control channel.
#pragma once
#include "RoundPhase.h"
#include <limits>

namespace Spike::RoundPhase
{
class Publisher
{
public:
    bool Bind(uint64_t Session, uint64_t Run, uint64_t World, int32_t Day)
    {
        Snapshot Initial{Session, Run, World, 1, Day};
        if (!Valid(Initial) || (Active && (Session != State.Session || Run < State.Run ||
            (Run == State.Run && World <= State.World))))
            return false;
        State = Initial;
        Active = true;
        return true;
    }
    bool Observe(Phase Stage, Reason EndReason, uint16_t Progress = 0)
    {
        Snapshot Next = State;
        Next.Stage = Stage;
        Next.EndReason = EndReason;
        Next.HostProgress = Progress;
        if (!Active || Stage < State.Stage || !Valid(Next))
            return false;
        State = Next;
        return true;
    }
    bool Heartbeat(Wire& Bytes)
    {
        if (!Active || State.Sequence == (std::numeric_limits<uint64_t>::max)())
            return false;
        ++State.Sequence;
        return Encode(State, Bytes);
    }
    const Snapshot& View() const { return State; }
    void Disconnect() { State = {}; Active = false; }
private:
    Snapshot State;
    bool Active = false;
};

class Subscriber
{
public:
    bool Bind(uint64_t Session, uint64_t Run, uint64_t World, uint64_t Now)
    {
        if (!Client.Bind(Session, Run, World))
            return false;
        LastFresh = Now;
        TimedOut = false;
        return true;
    }
    Result Receive(const uint8_t* Bytes, size_t Size, uint64_t Now)
    {
        const Result Outcome = Client.Receive(Bytes, Size);
        // Re-reading an abandoned file must never renew its lease.
        if (Outcome == Result::Applied)
        {
            LastFresh = Now;
            TimedOut = false;
        }
        return Outcome;
    }
    void Tick(uint64_t Now)
    {
        if (Client.View().Attached && Now >= LastFresh && Now - LastFresh >= 3500)
            TimedOut = true;
    }
    Presentation View() const
    {
        auto Value = Client.View();
        if (Value.Attached && TimedOut)
            Value.Waiting = Value.InputBlocked = true;
        return Value;
    }
    void Disconnect() { Client.Disconnect(); LastFresh = 0; TimedOut = false; }
private:
    Receiver Client;
    uint64_t LastFresh = 0;
    bool TimedOut = false;
};
}
