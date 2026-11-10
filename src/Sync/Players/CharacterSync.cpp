#include "Sync/Players/CharacterState.h"

#include <chrono>
#include <random>
#include "Core/Log.h"
#include "Game/CharacterAdapter.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"

// Figurenwahl in beide Richtungen (charakter-wahl), Format in CharacterState.h, Anwenden in Game/CharacterPatches.cpp.
namespace Sync
{
namespace
{
struct
{
    uint64_t Peer = 0, Run = 0, Revision = 0, NextSend = 0;
    std::optional<std::string> Sent;
} S;

uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

void Send(Net::Transport& Transport)
{
    const auto Own = Game::Characters::OwnChoice();
    const uint64_t Now = NowMs();
    if (!Own || (S.Sent == Own && Now < S.NextSend))
        return;
    auto Payload = Character::Encode(*Own);
    if (Payload.empty())
    {
        if (S.Sent != Own)
            Core::Log::Warn("Charakterwahl: eigener Name nicht übertragbar");
        S.Sent = Own;
        S.NextSend = Now + 10000;
        return;
    }
    if (!S.Run)
    {
        std::random_device Random;
        S.Run = ((uint64_t(Random()) << 32) | Random()) | 1;
    }
    if (!Transport.SendControl(S.Peer, {S.Run, 1, ++S.Revision, Character::ChoiceType, std::move(Payload)}))
        return;
    if (S.Sent != Own)
        Core::Log::Info("Charakterwahl: eigene Wahl {} gesendet", *Own);
    S.Sent = Own;
    S.NextSend = Now + 2000;
}
}

void TickCharacter(std::span<const Net::ControlMessage> Messages)
{
    const auto Transport = Net::CurrentTransport();
    const auto Peers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const uint64_t Peer = Peers.size() == 1 ? Peers[0] : 0;
    if (Peer != S.Peer)
    {
        S.Peer = Peer;
        S.Sent.reset();
        Game::Characters::SetPeerChoice(std::nullopt);
    }
    if (!Transport || !Peer)
        return;
    for (const auto& Message : Messages)
        if (Message.Peer == S.Peer && Message.Value.Type == Character::ChoiceType)
        {
            if (auto Name = Character::Decode(Message.Value.Payload))
                Game::Characters::SetPeerChoice(std::move(*Name));
            else
                Core::Log::Warn("Charakterwahl: unlesbare Wahl der Gegenseite verworfen");
        }
    Send(*Transport);
}
}
