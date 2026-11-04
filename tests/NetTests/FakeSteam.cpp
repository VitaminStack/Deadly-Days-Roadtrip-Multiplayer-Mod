#include "FakeSteam.h"

#include <deque>
#include <vector>

using namespace Net;

struct FakeSteamNetwork::Endpoint final : SteamSockets
{
    struct Pipe
    {
        Endpoint* Peer = nullptr;
        SteamConnection PeerHandle = 0;
        bool Connected = false;
        std::deque<std::vector<uint8_t>> Inbox;
    };

    Endpoint(std::shared_ptr<FakeSteamNetwork> NetworkIn, uint64_t Id) : Network(std::move(NetworkIn)), SteamId(Id) {}

    ~Endpoint() override
    {
        std::lock_guard Guard(Network->Lock);
        Network->Accounts.erase(SteamId);
    }

    uint64_t OwnSteamId() override { return SteamId; }
    void InitRelayNetworkAccess() override {}

    SteamListenSocket ListenP2P(int) override
    {
        std::lock_guard Guard(Network->Lock);
        ListeningP2P = true;
        return 100;
    }

    SteamListenSocket ListenLoopback(uint16_t Port) override
    {
        std::lock_guard Guard(Network->Lock);
        LoopbackPort = Port;
        return 101;
    }

    // Wie Steam: P2P zum eigenen Konto oder zu einem Konto ohne Listen-Socket wird abgelehnt.
    SteamConnection ConnectTo(Endpoint* Remote, SteamListenSocket ListenSocket)
    {
        if (!Remote || Remote == this || Network->Rejecting[Remote->SteamId])
            return 0;
        const SteamConnection Local = Network->NextHandle++;
        const SteamConnection Far = Network->NextHandle++;
        Pipes[Local] = {Remote, Far, false, {}};
        Remote->Pipes[Far] = {this, Local, false, {}};
        Changes.push_back({Local, 0, SteamConnectionState::Connecting, SteamConnectionState::None, 0, {}});
        Remote->Changes.push_back({Far, ListenSocket, SteamConnectionState::Connecting, SteamConnectionState::None, 0, {}});
        return Local;
    }

    SteamConnection ConnectP2P(uint64_t Remote, int) override
    {
        std::lock_guard Guard(Network->Lock);
        const auto It = Network->Accounts.find(Remote);
        return ConnectTo(It != Network->Accounts.end() && It->second->ListeningP2P ? It->second : nullptr, 100);
    }

    SteamConnection ConnectLoopback(uint16_t Port) override
    {
        std::lock_guard Guard(Network->Lock);
        Endpoint* Target = nullptr;
        for (auto& [Id, Candidate] : Network->Accounts)
            if (Candidate->LoopbackPort == Port)
                Target = Candidate;
        // Über 127.0.0.1 darf ein Konto sich selbst erreichen, wie bei Steam per IP.
        if (Target == this)
        {
            Target = nullptr;
            for (auto& [Id, Candidate] : Network->Accounts)
                if (Candidate != this && Candidate->LoopbackPort == Port)
                    Target = Candidate;
        }
        return ConnectTo(Target, 101);
    }

    bool Accept(SteamConnection Connection) override
    {
        std::lock_guard Guard(Network->Lock);
        const auto It = Pipes.find(Connection);
        if (It == Pipes.end())
            return false;
        It->second.Connected = true;
        auto& Far = It->second.Peer->Pipes[It->second.PeerHandle];
        Far.Connected = true;
        Changes.push_back({Connection, 100, SteamConnectionState::Connected, SteamConnectionState::Connecting, 0, {}});
        It->second.Peer->Changes.push_back(
            {It->second.PeerHandle, 0, SteamConnectionState::Connected, SteamConnectionState::Connecting, 0, {}});
        return true;
    }

    void CloseLocked(SteamConnection Connection)
    {
        const auto It = Pipes.find(Connection);
        if (It == Pipes.end())
            return;
        Endpoint* Peer = It->second.Peer;
        const SteamConnection Far = It->second.PeerHandle;
        Pipes.erase(It);
        if (Peer && Peer->Pipes.count(Far))
        {
            Peer->Pipes[Far].Connected = false;
            Peer->Pipes[Far].Peer = nullptr;
            Peer->Changes.push_back({Far, 0, SteamConnectionState::ClosedByPeer, SteamConnectionState::Connected, 1000,
                                     "closed by peer 10.1.2.3"});
        }
    }

    void Close(SteamConnection Connection) override
    {
        std::lock_guard Guard(Network->Lock);
        CloseLocked(Connection);
    }

    void CloseListen(SteamListenSocket Socket) override
    {
        std::lock_guard Guard(Network->Lock);
        if (Socket == 100)
            ListeningP2P = false;
        else
            LoopbackPort = 0;
    }

    bool Send(SteamConnection Connection, std::span<const uint8_t> Data) override
    {
        std::lock_guard Guard(Network->Lock);
        const auto It = Pipes.find(Connection);
        if (It == Pipes.end() || !It->second.Connected || !It->second.Peer)
            return false;
        It->second.Peer->Pipes[It->second.PeerHandle].Inbox.emplace_back(Data.begin(), Data.end());
        ++Sent;
        return true;
    }

    int Receive(SteamConnection Connection, int Max, const std::function<void(std::span<const uint8_t>)>& Sink) override
    {
        std::vector<std::vector<uint8_t>> Taken;
        {
            std::lock_guard Guard(Network->Lock);
            const auto It = Pipes.find(Connection);
            if (It == Pipes.end())
                return 0;
            while (!It->second.Inbox.empty() && static_cast<int>(Taken.size()) < Max)
            {
                Taken.push_back(std::move(It->second.Inbox.front()));
                It->second.Inbox.pop_front();
            }
        }
        for (const auto& Message : Taken)
            Sink(Message);
        return static_cast<int>(Taken.size());
    }

    void RunCallbacks() override {}

    std::vector<SteamStatusChange> TakeStatusChanges() override
    {
        std::lock_guard Guard(Network->Lock);
        std::vector<SteamStatusChange> Result;
        Result.swap(Changes);
        return Result;
    }

    SteamQuality Quality(SteamConnection) override { return {42, 1.0f}; }

    std::string DetailedStatus(SteamConnection) override
    {
        return "Ping:42ms\nPrimary router: fra#364 (155.133.1.2:27126)  Ping to relay = 20\n";
    }

    std::shared_ptr<FakeSteamNetwork> Network;
    uint64_t SteamId;
    bool ListeningP2P = false;
    uint16_t LoopbackPort = 0;
    std::map<SteamConnection, Pipe> Pipes;
    std::vector<SteamStatusChange> Changes;
    int Sent = 0;
};

std::shared_ptr<SteamSockets> FakeSteamNetwork::Account(uint64_t SteamId)
{
    auto Created = std::make_shared<Endpoint>(shared_from_this(), SteamId);
    std::lock_guard Guard(Lock);
    Accounts[SteamId] = Created.get();
    return Created;
}

void FakeSteamNetwork::DropPeerOf(uint64_t SteamId)
{
    std::lock_guard Guard(Lock);
    const auto It = Accounts.find(SteamId);
    if (It == Accounts.end())
        return;
    std::vector<std::pair<Endpoint*, SteamConnection>> Far;
    for (auto& [Handle, Pipe] : It->second->Pipes)
        if (Pipe.Peer)
            Far.emplace_back(Pipe.Peer, Pipe.PeerHandle);
    for (auto& [Peer, Handle] : Far)
        Peer->CloseLocked(Handle);
}
