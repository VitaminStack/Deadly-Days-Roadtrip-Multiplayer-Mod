#include "Sync/World/MapState.h"

#include <chrono>
#include <random>
#include "Core/Log.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"

// Host schickt die Werte seiner Karte, Client gibt sie an die Kartenpatches weiter (missionsstart).
namespace Sync
{
namespace
{
struct
{
    uint64_t Run = 0, Revision = 0, Peer = 0;
    // Host
    std::optional<std::optional<Game::Map::HostMap>> Sent;
    uint64_t NextResend = 0;
    std::optional<std::optional<Game::Map::RoundEnd>> SentEnd;
    uint64_t NextEndResend = 0;
} M;

uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Rundenende wie die Karte: bei Änderung sofort, sonst jede Sekunde, damit ein verlorenes Paket nachkommt.
void TickHostRoundEnd(Net::Transport& Transport)
{
    const auto End = Game::Map::CurrentRoundEnd();
    const uint64_t Now = NowMs();
    if (M.SentEnd && *M.SentEnd == End && Now < M.NextEndResend)
        return;
    auto Payload = MapWire::EncodeRoundEnd(End);
    if (Payload.empty() || !M.Run)
        return;
    if (!Transport.SendControl(M.Peer, {M.Run, 1, ++M.Revision, MapWire::RoundEndType, std::move(Payload)}))
        return;
    if (End && (!M.SentEnd || *M.SentEnd != End))
        Core::Log::Info("Rundenende: an den Client gesendet");
    M.SentEnd = End;
    M.NextEndResend = Now + 1000;
}

void TickHost(Net::Transport& Transport)
{
    const auto Map = Game::Map::CurrentHostMap();
    const uint64_t Now = NowMs();
    if (M.Sent && *M.Sent == Map && Now < M.NextResend)
        return;
    auto Payload = MapWire::Encode(Map);
    if (Payload.empty())
    {
        Core::Log::Error("Karte: Werte des Hosts nicht übertragbar (zu lang oder unvollständig)");
        M.Sent = Map;
        M.NextResend = Now + 10000;
        return;
    }
    if (!M.Run)
    {
        std::random_device Random;
        M.Run = ((uint64_t(Random()) << 32) | Random()) | 1;
    }
    if (!Transport.SendControl(M.Peer, {M.Run, 1, ++M.Revision, MapWire::HostMapType, std::move(Payload)}))
        return;
    if (Map && (!M.Sent || !*M.Sent || (*M.Sent)->Id != Map->Id || (*M.Sent)->GraphState != Map->GraphState))
        Core::Log::Info("Karte: Werte {} {} an den Client gesendet", Map->Seed, Map->Profile);
    M.Sent = Map;
    M.NextResend = Now + 1000;
}

void TickClient(std::span<const Net::ControlMessage> Messages)
{
    for (const auto& Message : Messages)
        if (Message.Peer == M.Peer && Message.Value.Type == MapWire::RoundEndType)
        {
            if (auto End = MapWire::DecodeRoundEnd(Message.Value.Payload))
                Game::Map::SetHostRoundEnd(*End);
            else
                Core::Log::Warn("Rundenende: unlesbare Nachricht vom Host verworfen");
        }
        else if (Message.Peer == M.Peer && Message.Value.Type == MapWire::HostMapType)
        {
            if (auto Map = MapWire::Decode(Message.Value.Payload))
                Game::Map::SetHostMap(std::move(*Map));
            else
                Core::Log::Warn("Karte: unlesbare Werte vom Host verworfen");
        }
}
}

void TickMap(std::span<const Net::ControlMessage> Messages)
{
    const auto Transport = Net::CurrentTransport();
    const auto Peers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const uint64_t Peer = Peers.size() == 1 ? Peers[0] : 0;
    const bool Host = Transport && Transport->Stats().IsHost;
    if (Peer != M.Peer)
    {
        M.Peer = Peer;
        M.Sent.reset();
        M.SentEnd.reset();
        if (!Host)
        {
            Game::Map::SetHostMap(std::nullopt);
            Game::Map::SetHostRoundEnd(std::nullopt);
        }
    }
    if (!Transport || !Peer)
        return;
    if (Host)
    {
        TickHost(*Transport);
        TickHostRoundEnd(*Transport);
    }
    else
        TickClient(Messages);
}
}
