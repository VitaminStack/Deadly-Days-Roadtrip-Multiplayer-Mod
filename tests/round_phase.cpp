#include "../research/spikes/a-connect/RoundPhase.h"
#include <cassert>
#include <iostream>
#include <limits>

using namespace Spike::RoundPhase;
int main()
{
    Receiver Client;
    Snapshot Host{11, 1, 1, 1, 2, Phase::Playing, Reason::NoReason, 0};
    Wire Bytes{};
    assert(Encode(Host, Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected);
    assert(Client.Bind(11, 1, 1));
    assert(Client.View().Waiting && Client.View().InputBlocked);
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Applied);
    assert(!Client.View().InputBlocked && Client.View().Host == Host);
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Duplicate);
    for (auto Stage : {Phase::RoundEnd, Phase::Stash, Phase::Route, Phase::Travel})
    {
        Host.Stage = Stage;
        Host.EndReason = Reason::NewDay;
        ++Host.Sequence;
        assert(Encode(Host, Bytes));
        assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Applied);
        assert(Client.View().InputBlocked && Client.View().Host == Host);
    }
    const auto Last = Client.View().Host;
    Host.Stage = Phase::Playing;
    ++Host.Sequence;
    assert(Encode(Host, Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected); // No local resume in same world.
    Host = Last;
    Host.HostProgress = 50;
    assert(Encode(Host, Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected); // Conflicting replay.
    Host.Sequence = 1;
    assert(Encode(Host, Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected); // Reordered packet.
    Host = Last;
    assert(Encode(Host, Bytes));
    const Wire Good = Bytes;
    for (size_t Field : {size_t(8), size_t(16), size_t(24)})
    {
        Bytes = Good;
        ++Bytes[Field];
        assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected);
    }
    Bytes = Good;
    Bytes[43] = 0x80;
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected); // Negative/out-of-range day.
    Bytes = Good;
    assert(Client.Receive(Bytes.data(), Bytes.size() + 1) == Result::Rejected);
    for (size_t i = 0; i < Bytes.size(); ++i)
        assert(Client.Receive(Bytes.data(), i) == Result::Rejected);
    assert(Client.Receive(nullptr, Bytes.size()) == Result::Rejected);
    for (size_t Field : {size_t(0), size_t(4), size_t(6), size_t(44), size_t(45), size_t(46)})
    {
        Bytes = Good;
        Bytes[Field] = 0xFF;
        assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected);
        assert(Client.View().Host == Last && Client.View().InputBlocked);
    }
    assert(!Client.Bind(11, 1, 1));
    assert(!Client.Bind(12, 1, 2)); // A packet/session change is not a handshake.
    assert(Client.Bind(11, 1, 2));
    assert(Client.Receive(Good.data(), Good.size()) == Result::Rejected);
    Host = {11, 1, 2, 1, 3, Phase::Playing, Reason::NoReason, 0};
    assert(Encode(Host, Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Applied);
    assert(!Client.View().InputBlocked);
    Client.Disconnect();
    assert(!Client.View().Attached && !Client.View().InputBlocked);
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected);
    assert(Client.Bind(12, 1, 1));
    Host = {12, 1, 1, 1, 0, Phase::Route, Reason::NoReason, 0}; // Initial route before day 1.
    assert(Encode(Host, Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Applied);
    Host.Sequence = std::numeric_limits<uint64_t>::max();
    Host.Stage = Phase::Travel;
    assert(Encode(Host, Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Applied);
    Host.Sequence = 1;
    assert(Encode(Host, Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected); // No sequence wrap.
    for (auto ReasonValue : {Reason::Win, Reason::Lose})
    {
        Client.Disconnect();
        assert(Client.Bind(12, 1, 1));
        Host = {12, 1, 1, 1, 2, Phase::RoundEnd, ReasonValue, 100};
        assert(Encode(Host, Bytes));
        assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Applied);
        assert(Client.View().InputBlocked && Client.View().Host.EndReason == ReasonValue);
    }
    Client.Disconnect();
    assert(Client.Bind(12, 1, 1));
    Host = {12, 1, 1, 40, 5, Phase::Route, Reason::NewDay, 0}; // Complete late-join snapshot, not an event replay.
    assert(Encode(Host, Bytes));
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Applied);
    assert(Client.View().InputBlocked && Client.View().Host.Stage == Phase::Route);
    assert(Client.Bind(12, 2, 1)); // Verified new run can restart its world generation.
    assert(Client.Receive(Bytes.data(), Bytes.size()) == Result::Rejected);
    std::cout << "Round phase codec/receiver tests passed\n";
}
