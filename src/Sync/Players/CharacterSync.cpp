#include "Sync/Players/CharacterState.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <random>
#include "Core/Log.h"
#include "Game/CharacterAdapter.h"
#include "Game/RemotePlayers.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"

// Figurenwahl in beide Richtungen (charakter-wahl), Format in CharacterState.h, Anwenden in Game/CharacterPatches.cpp.
// Client schickt seine Wahl an den Host. Der Host ordnet sie über den Peer dem Controller und dessen PlayerId zu und schickt
// jedem Client die Wahl aller anderen Spieler.
namespace Sync
{
namespace
{
struct Sent
{
    std::vector<uint8_t> Payload;
    uint64_t NextSend = 0;
};

struct
{
    uint64_t Peer = 0; // Client: der Host
    uint64_t Run = 0, Revision = 0;
    std::map<uint64_t, Sent> Sends; // je Empfänger
    std::map<uint64_t, std::string> Clients; // Host: Wahl je Client-Peer
    std::optional<std::string> LoggedOwn;
} S;

uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Bei Änderung sofort, sonst alle zwei Sekunden.
bool Send(Net::Transport& Transport, uint64_t Peer, std::vector<uint8_t> Payload)
{
    const uint64_t Now = NowMs();
    Sent& Last = S.Sends[Peer];
    if (Payload.empty() || (Last.Payload == Payload && Now < Last.NextSend))
        return false;
    if (!S.Run)
    {
        std::random_device Random;
        S.Run = ((uint64_t(Random()) << 32) | Random()) | 1;
    }
    if (!Transport.SendControl(Peer, {S.Run, 1, ++S.Revision, Character::ChoiceType, Payload}))
        return false;
    Last = {std::move(Payload), Now + 2000};
    return true;
}

void LogOwn(const std::optional<std::string>& Own)
{
    if (Own && Own != S.LoggedOwn)
        Core::Log::Info("Charakterwahl: eigene Wahl {} gesendet", *Own);
    S.LoggedOwn = Own;
}

void TickClient(Net::Transport& Transport, std::span<const Net::ControlMessage> Messages)
{
    for (const auto& Message : Messages)
        if (Message.Peer == S.Peer && Message.Value.Type == Character::ChoiceType)
        {
            if (const auto Choices = Character::Decode(Message.Value.Payload))
                Game::Characters::SetPeerChoices(*Choices);
            else
                Core::Log::Warn("Charakterwahl: unlesbare Wahl vom Host verworfen");
        }
    const auto Own = Game::Characters::OwnChoice();
    if (!Own)
        return;
    const Character::PlayerChoice Mine[] = {{0, *Own}};
    if (Send(Transport, S.Peer, Character::Encode(Mine)))
        LogOwn(Own);
}

void TickHost(Net::Transport& Transport, std::span<const uint64_t> Peers, std::span<const Net::ControlMessage> Messages)
{
    for (const auto& Message : Messages)
        if (Message.Value.Type == Character::ChoiceType && std::find(Peers.begin(), Peers.end(), Message.Peer) != Peers.end())
        {
            // Nur der Name zählt; welcher Spieler ihn gewählt hat, weiß der Host aus dem Peer.
            if (const auto Choices = Character::Decode(Message.Value.Payload); Choices && Choices->size() == 1)
                S.Clients[Message.Peer] = Choices->front().Name;
            else
                Core::Log::Warn("Charakterwahl: unlesbare Wahl eines Clients verworfen");
        }
    // Wahl jedes Spielers mit PlayerId, der Peer 0 steht für den Host selbst.
    std::vector<std::pair<uint64_t, Character::PlayerChoice>> All;
    const auto Own = Game::Characters::OwnChoice();
    if (const auto Id = Game::Characters::LocalPlayerId(); Own && Id)
        All.push_back({0, {*Id, *Own}});
    std::vector<Character::PlayerChoice> Remote;
    for (const auto& [Peer, Name] : S.Clients)
        if (const auto Id = Game::Characters::PlayerIdOf(Game::PeerController(Peer)))
        {
            All.push_back({Peer, {*Id, Name}});
            Remote.push_back({*Id, Name});
        }
    Game::Characters::SetPeerChoices(Remote);
    for (const uint64_t Peer : Peers)
    {
        std::vector<Character::PlayerChoice> Others;
        for (const auto& [From, Choice] : All)
            if (From != Peer)
                Others.push_back(Choice);
        if (!Others.empty() && Send(Transport, Peer, Character::Encode(Others)))
            LogOwn(Own);
    }
}
}

void TickCharacter(std::span<const Net::ControlMessage> Messages)
{
    const auto Transport = Net::CurrentTransport();
    const auto Peers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const bool Host = Transport && Transport->Stats().IsHost;
    // Getrennte Peers verlieren ihren Stand, neue bekommen die Wahl sofort.
    const auto Gone = [&Peers](const auto& Entry) { return std::find(Peers.begin(), Peers.end(), Entry.first) == Peers.end(); };
    std::erase_if(S.Sends, Gone);
    std::erase_if(S.Clients, Gone);
    const uint64_t Peer = !Host && Peers.size() == 1 ? Peers[0] : 0;
    if (Peer != S.Peer)
    {
        S.Peer = Peer;
        Game::Characters::SetPeerChoices({});
    }
    if (!Transport)
    {
        Game::Characters::SetPeerChoices({});
        return;
    }
    if (Host)
        TickHost(*Transport, Peers, Messages);
    else if (Peer)
        TickClient(*Transport, Messages);
}
}
