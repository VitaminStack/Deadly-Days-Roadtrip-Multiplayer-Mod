#include "Sync/World/RouteState.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <random>
#include "Core/Log.h"
#include "Game/MapAdapter.h"
#include "Game/RouteMapAdapter.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"

// opt-routenkarte: Host schickt seine Run-Info der laufenden Mission, der Client übernimmt sie am Tagesende für seine Routenkarte.
namespace Sync
{
namespace
{
// Host: gesendeter Stand je Client. Ein neuer Client bekommt ihn sofort, die übrigen bei Änderung und sonst alle 5 s.
struct Sent
{
    std::optional<Game::RouteMap::HostRoute> Route;
    uint64_t NextResend = 0;
};

struct
{
    uint64_t Run = 0, Revision = 0;
    uint64_t Peer = 0;                                 // Client: der Host
    std::map<uint64_t, Sent> Peers;                    // Host
    std::optional<Game::RouteMap::HostRoute> Snapshot; // Host: Stand vor dem Rundenende der aktuellen Karte
    uint64_t NextRead = 0;
} R;

uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

// Gelesen wird nur in der Mission vor dem Rundenende: Das eigene FinishMission führt die Route dann weiter, und genau diesen Schritt
// macht der Client selbst. Nach dem Ende bleibt der letzte Stand dieser Karte.
void ReadHost(uint64_t Now)
{
    const auto Map = Game::Map::CurrentHostMap();
    if (!Map || Game::Map::CurrentRoundEnd() || Now < R.NextRead)
        return;
    R.NextRead = Now + 1000;
    auto Route = Game::RouteMap::ReadHostRoute();
    if (!Route)
        return;
    Route->MapId = Map->Id;
    if (!R.Snapshot || *R.Snapshot != *Route)
        Core::Log::Info("Routenkarte: Host-Route {}.{} Tag {} (Seed {}, Wahl {})", Route->Current.Group, Route->Current.Route,
                        Route->MissionIndex, Route->Seed, Route->Chosen.size());
    R.Snapshot = std::move(Route);
}

void TickHost(Net::Transport& Transport, uint64_t Peer, Sent& State, uint64_t Now)
{
    if (!R.Snapshot || (State.Route == R.Snapshot && Now < State.NextResend))
        return;
    auto Payload = RouteWire::Encode(*R.Snapshot);
    if (Payload.empty())
        return;
    if (!R.Run)
    {
        std::random_device Random;
        R.Run = ((uint64_t(Random()) << 32) | Random()) | 1;
    }
    if (!Transport.SendControl(Peer, {R.Run, 1, ++R.Revision, RouteWire::HostRouteType, std::move(Payload)}))
        return;
    if (State.Route != R.Snapshot)
        Core::Log::Info("Routenkarte: Route an den Client gesendet (Peer {:x})", Peer);
    State.Route = R.Snapshot;
    State.NextResend = Now + 5000;
}
}

void TickRoute(std::span<const Net::ControlMessage> Messages)
{
    const auto Transport = Net::CurrentTransport();
    const auto Peers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const bool Host = Transport && Transport->Stats().IsHost;
    if (Host)
    {
        std::erase_if(R.Peers, [&Peers](const auto& Entry) { return std::find(Peers.begin(), Peers.end(), Entry.first) == Peers.end(); });
        const uint64_t Now = NowMs();
        ReadHost(Now);
        for (const uint64_t Peer : Peers)
            TickHost(*Transport, Peer, R.Peers[Peer], Now);
        return;
    }
    R.Peers.clear();
    R.Snapshot.reset();
    const uint64_t Peer = Peers.size() == 1 ? Peers[0] : 0;
    if (Peer != R.Peer)
    {
        R.Peer = Peer;
        Game::RouteMap::SetHostRoute(std::nullopt);
    }
    if (!Transport || !Peer)
        return;
    for (const auto& Message : Messages)
        if (Message.Peer == R.Peer && Message.Value.Type == RouteWire::HostRouteType)
        {
            if (auto Route = RouteWire::Decode(Message.Value.Payload))
                Game::RouteMap::SetHostRoute(std::move(*Route));
            else
                Core::Log::Warn("Routenkarte: unlesbare Route vom Host verworfen");
        }
}
}
