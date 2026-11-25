#include "Sync/ProgressState.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <random>
#include "Core/Log.h"
#include "Game/GameThread.h"
#include "Game/ExpAdapter.h"
#include "Game/ProgressAdapter.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"

namespace Sync
{
namespace
{
uint64_t NewRun()
{
    std::random_device Random;
    const uint64_t Value = (uint64_t(Random()) << 32) | Random();
    return Value ? Value : 1;
}

// Host: Abo eines Clients, Lauf und Nonce seiner aktuellen Lebensdauer.
struct Subscription
{
    uint64_t Run = 0, Nonce = 0;
};
}

void TickProgress(std::span<const Net::ControlMessage> Messages)
{
    static const uint64_t Run = NewRun();
    static uint64_t World = 0, State = 0, Epoch = 0, Revision = 0, Nonce = 0, Peer = 0;
    static std::vector<uint64_t> PeerSet;
    static std::map<uint64_t, Subscription> Subscribers; // Host: je Client
    static Progress::Mirror Mirror;
    const auto Transport = Net::CurrentTransport();
    const auto Context = Game::ReadProgressContext();
    const auto Peers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const bool IsHost = Transport && Transport->Stats().IsHost;
    const uint64_t CurrentWorld = Context ? Context->LocalWorld : 0;
    const uint64_t CurrentState = Context ? Context->LocalState : 0;
    // Client: der eine Peer des Hosts. Host: jeder Client mit eigenem Abo.
    const uint64_t CurrentPeer = !IsHost && Peers.size() == 1 ? Peers[0] : 0;
    if (CurrentPeer != Peer || CurrentWorld != World || CurrentState != State)
    {
        World = CurrentWorld;
        State = CurrentState;
        Peer = CurrentPeer;
        ++Epoch;
        ++Nonce;
        Mirror.Bind(Peer, Run, Nonce);
        Subscribers.clear();
        Game::TakeExpSpawns(SIZE_MAX); // Baelle der alten Welt nicht in die neue tragen.
    }
    if (Peers != PeerSet)
    {
        // Getrennte Clients verlieren ihr Abo, die übrigen behalten es; neue melden sich mit ihrem nächsten Abo an.
        PeerSet = Peers;
        std::erase_if(Subscribers, [](const auto& Entry) { return std::find(PeerSet.begin(), PeerSet.end(), Entry.first) == PeerSet.end(); });
    }
    if (!Transport || !Context)
        return;
    if (Context->Role == Game::NetRole::Host && IsHost)
    {
        if (Peers.empty())
            return;
        for (const auto& Message : Messages)
        {
            if (Message.Value.Type != Progress::Subscribe || !Message.Value.Payload.empty() || !Message.Value.Run ||
                Message.Value.Epoch != 1 || !Message.Value.Revision || std::find(Peers.begin(), Peers.end(), Message.Peer) == Peers.end())
                continue;
            Subscription& Client = Subscribers[Message.Peer];
            if (!Client.Run || (Message.Value.Run == Client.Run && Message.Value.Revision >= Client.Nonce))
                Client = {Message.Value.Run, Message.Value.Revision};
        }
        const auto Value = Game::ReadSharedProgress();
        if (!Subscribers.empty() && Value)
        {
            Game::PrepareAirdropReplication();
            const auto Crates = Game::ReadAirdropViews();
            for (const auto& [ClientPeer, Client] : Subscribers)
            {
                Progress::State Image{Client.Run, Client.Nonce, Context->Day, Value->Level, Value->Fraction, Context->Map, Context->StateGuid};
                if (Crates)
                {
                    Image.AirdropsKnown = true;
                    Image.Airdrops = *Crates;
                }
                const auto Payload = Progress::Encode(Image);
                if (!Payload.empty())
                    Transport->SendControl(ClientPeer, {Run, Epoch, ++Revision, Progress::Snapshot, Payload});
                // Ein fehlgeschlagener Send verliert keinen Gewinn: naechster Tick liefert wieder das Vollbild.
            }
        }
        // XP-Baelle sind Ereignisse, nur Anzeige. Ohne Abonnenten verfallen sie, statt sich anzustauen.
        const auto Spawns = Game::TakeExpSpawns(Progress::MaxOrbs);
        if (!Spawns.empty())
            for (const auto& [ClientPeer, Client] : Subscribers)
            {
                const auto Payload = Progress::EncodeOrbs({Client.Run, Client.Nonce, Context->StateGuid, Context->Day, Spawns});
                if (!Payload.empty())
                    Transport->SendControl(ClientPeer, {Run, Epoch, ++Revision, Progress::Orbs, Payload});
            }
    }
    else if (Context->Role == Game::NetRole::Client && !IsHost && Peer)
    {
        // Nach Join/Travel wiederholen, bis ein Snapshot fuer die aktuelle lokale Lebensdauer vorliegt.
        Transport->SendControl(Peer, {Run, 1, Nonce, Progress::Subscribe, {}});
        bool Updated = false;
        unsigned Orbs = 0;
        for (const auto& Message : Messages)
        {
            Updated |= Mirror.Accept(Message.Peer, Message.Value, Context->Map, Context->Day, Context->StateGuid);
            if (const auto Spawns = Mirror.AcceptOrbs(Message.Peer, Message.Value, Context->Day, Context->StateGuid))
                Orbs += Game::ReplayExpSpawns(*Spawns);
        }
        static unsigned OrbLogs = 0;
        if (Orbs && OrbLogs < 5)
        {
            ++OrbLogs;
            Core::Log::Info("XP-Baelle: {} Host-Ausstoss(e) angezeigt", Orbs);
        }
        if (Mirror.Value && Mirror.Value->Day == Context->Day && Mirror.Value->Map == Context->Map)
        {
            const auto& Value = *Mirror.Value;
            const unsigned Shown = Game::ShowSharedProgress({Value.Level, Value.Fraction});
            if (Value.AirdropsKnown)
            {
                const unsigned CratesShown = Game::ShowAirdropViews(Value.Airdrops, Nonce);
                static size_t LastCrates = 0;
                if (CratesShown && CratesShown != LastCrates)
                    Core::Log::Info("Airdrop-Anzeige: {} Host-Actor(s) zugeordnet", CratesShown);
                LastCrates = CratesShown;
            }
            static int32_t LastLevel = -1;
            static float LastFraction = -1;
            if (Updated && Shown && (LastLevel != Value.Level || LastFraction != Value.Fraction))
            {
                Core::Log::Info("XP-Anzeige: Host-Level {}, Fortschritt {}, {} HUD(s)", Value.Level, Value.Fraction, Shown);
                LastLevel = Value.Level;
                LastFraction = Value.Fraction;
            }
        }
        else if (Mirror.Value)
            Mirror.Bind(Peer, Run, ++Nonce);
    }
}
}
