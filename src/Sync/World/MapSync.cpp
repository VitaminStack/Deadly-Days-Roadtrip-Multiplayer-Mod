#include "Sync/World/MapState.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <random>
#include "Core/Log.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"

// Host schickt die Werte seiner Karte, Client gibt sie an die Kartenpatches weiter (missionsstart).
namespace Sync
{
namespace
{
// Host: gesendeter Stand je Client, damit ein neuer Client die Karte sofort bekommt und die übrigen nur bei Änderung.
struct Sent
{
    std::optional<std::optional<Game::Map::HostMap>> Map;
    uint64_t NextResend = 0;
    std::optional<std::optional<Game::Map::RoundEnd>> End;
    uint64_t NextEndResend = 0;
    std::optional<Game::Map::RoundStatsImage> Stats; // zuletzt angenommenes Bild, eins je Rundenende
};

struct
{
    uint64_t Run = 0, Revision = 0;
    uint64_t Peer = 0; // Client: der Host
    std::map<uint64_t, Sent> Peers; // Host
} M;

uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

// opt-ergebnis-werte: Werte der Rundenübersicht vor dem Rundenende desselben Endes, auf demselben zuverlässigen Weg. Einmal je
// Ende, bis das Senden angenommen ist: Der Client übernimmt sie nur einmal, direkt vor dem Öffnen der Übersicht.
void TickHostRoundStats(Net::Transport& Transport, uint64_t Peer, Sent& State, const std::optional<Game::Map::RoundStatsImage>& Stats)
{
    if (!Stats || !M.Run || (State.Stats && State.Stats->EndId == Stats->EndId))
        return;
    auto Payload = MapWire::EncodeRoundStats(*Stats);
    if (Payload.empty())
    {
        Core::Log::Error("Rundenwerte: nicht übertragbar ({} Werte)", Stats->Entries.size());
        State.Stats = Stats;
        return;
    }
    if (!Transport.SendControl(Peer, {M.Run, 1, ++M.Revision, MapWire::RoundStatsType, std::move(Payload)}))
        return;
    Core::Log::Info("Rundenwerte: {} Werte{} an den Client gesendet (Peer {:x})", Stats->Entries.size(), Stats->Known ? "" : " (eigene unlesbar)",
                    Peer);
    State.Stats = Stats;
}

// Rundenende wie die Karte: bei Änderung sofort, sonst jede Sekunde, damit ein verlorenes Paket nachkommt.
void TickHostRoundEnd(Net::Transport& Transport, uint64_t Peer, Sent& State)
{
    const auto End = Game::Map::CurrentRoundEnd();
    const uint64_t Now = NowMs();
    if (State.End && *State.End == End && Now < State.NextEndResend)
        return;
    auto Payload = MapWire::EncodeRoundEnd(End);
    if (Payload.empty() || !M.Run)
        return;
    if (!Transport.SendControl(Peer, {M.Run, 1, ++M.Revision, MapWire::RoundEndType, std::move(Payload)}))
        return;
    if (End && (!State.End || *State.End != End))
        Core::Log::Info("Rundenende: an den Client gesendet (Peer {:x})", Peer);
    State.End = End;
    State.NextEndResend = Now + 1000;
}

void TickHost(Net::Transport& Transport, uint64_t Peer, Sent& State)
{
    const auto Map = Game::Map::CurrentHostMap();
    const uint64_t Now = NowMs();
    if (State.Map && *State.Map == Map && Now < State.NextResend)
        return;
    auto Payload = MapWire::Encode(Map);
    if (Payload.empty())
    {
        Core::Log::Error("Karte: Werte des Hosts nicht übertragbar (zu lang oder unvollständig)");
        State.Map = Map;
        State.NextResend = Now + 10000;
        return;
    }
    if (!M.Run)
    {
        std::random_device Random;
        M.Run = ((uint64_t(Random()) << 32) | Random()) | 1;
    }
    if (!Transport.SendControl(Peer, {M.Run, 1, ++M.Revision, MapWire::HostMapType, std::move(Payload)}))
        return;
    if (Map && (!State.Map || !*State.Map || (*State.Map)->Id != Map->Id || (*State.Map)->GraphState != Map->GraphState))
        Core::Log::Info("Karte: Werte {} {} an den Client gesendet (Peer {:x})", Map->Seed, Map->Profile, Peer);
    State.Map = Map;
    State.NextResend = Now + 1000;
}

void TickClient(std::span<const Net::ControlMessage> Messages)
{
    for (const auto& Message : Messages)
        if (Message.Peer == M.Peer && Message.Value.Type == MapWire::RoundStatsType)
        {
            if (auto Stats = MapWire::DecodeRoundStats(Message.Value.Payload))
                Game::Map::SetHostRoundStats(std::move(*Stats));
            else
                Core::Log::Warn("Rundenwerte: unlesbare Nachricht vom Host verworfen");
        }
        else if (Message.Peer == M.Peer && Message.Value.Type == MapWire::RoundEndType)
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
    const bool Host = Transport && Transport->Stats().IsHost;
    if (Host)
    {
        // Jeder Client bekommt dieselbe Karte; getrennte Clients verlieren ihren Stand, neue starten leer.
        std::erase_if(M.Peers, [&Peers](const auto& Entry) { return std::find(Peers.begin(), Peers.end(), Entry.first) == Peers.end(); });
        // Nur lesen, solange ein Client die Werte zu diesem Ende noch nicht hat; dann für alle Clients dieselben.
        const auto End = Game::Map::CurrentRoundEnd();
        const bool Needed = End && std::any_of(Peers.begin(), Peers.end(), [&End](uint64_t Peer) {
            const auto Found = M.Peers.find(Peer);
            return Found == M.Peers.end() || !Found->second.Stats || Found->second.Stats->EndId != End->Id;
        });
        std::optional<Game::Map::RoundStatsImage> Stats;
        if (Needed)
            Stats = Game::Map::CurrentRoundStats();
        for (const uint64_t Peer : Peers)
        {
            Sent& State = M.Peers[Peer];
            TickHost(*Transport, Peer, State);
            TickHostRoundStats(*Transport, Peer, State, Stats);
            TickHostRoundEnd(*Transport, Peer, State);
        }
        return;
    }
    M.Peers.clear();
    const uint64_t Peer = Peers.size() == 1 ? Peers[0] : 0;
    if (Peer != M.Peer)
    {
        M.Peer = Peer;
        Game::Map::NoteConnectionChanged();
        Game::Map::SetHostMap(std::nullopt);
        Game::Map::SetHostRoundEnd(std::nullopt);
        Game::Map::SetHostRoundStats(std::nullopt);
    }
    if (Transport && Peer)
        TickClient(Messages);
}
}
