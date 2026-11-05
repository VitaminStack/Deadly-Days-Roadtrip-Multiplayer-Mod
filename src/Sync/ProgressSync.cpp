#include "Sync/ProgressState.h"

#include <chrono>
#include <cstdint>
#include <random>
#include "Core/Log.h"
#include "Game/GameThread.h"
#include "Game/ExpAdapter.h"
#include "Game/ProgressAdapter.h"
#include "Net/Net.h"

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

void TickProgress()
{
    using Clock = std::chrono::steady_clock;
    static auto Next = Clock::now();
    if (Clock::now() < Next)
        return;
    Next = Clock::now() + std::chrono::milliseconds(250);
    static const uint64_t Run = NewRun();
    static uint64_t World = 0, State = 0, Epoch = 0, Revision = 0, Nonce = 0, Peer = 0;
    static uint64_t ClientRun = 0, ClientNonce = 0;
    static Progress::Mirror Mirror;
    const auto Transport = Net::CurrentTransport();
    const auto Context = Game::ReadProgressContext();
    const auto Peers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const uint64_t CurrentPeer = Peers.size() == 1 ? Peers[0] : 0;
    const uint64_t CurrentWorld = Context ? Context->LocalWorld : 0;
    const uint64_t CurrentState = Context ? Context->LocalState : 0;
    const bool Changed = CurrentPeer != Peer || CurrentWorld != World || CurrentState != State;
    if (Changed)
    {
        World = CurrentWorld;
        State = CurrentState;
        Peer = CurrentPeer;
        ++Epoch;
        ++Nonce;
        Mirror.Bind(Peer, Run, Nonce);
        ClientRun = ClientNonce = 0;
        Game::TakeExpSpawns(SIZE_MAX); // Baelle der alten Welt nicht in die neue tragen.
    }
    if (!Transport)
        return;
    const auto Messages = Transport->TakeControlMessages();
    if (!Context || !Peer)
        return;
    if (Context->Role == Game::NetRole::Host && Transport->Stats().IsHost)
    {
        for (const auto& Message : Messages)
            if (Message.Peer == Peer && Message.Value.Type == Progress::Subscribe && Message.Value.Payload.empty() &&
                Message.Value.Run && Message.Value.Epoch == 1 && Message.Value.Revision &&
                (!ClientRun || (Message.Value.Run == ClientRun && Message.Value.Revision >= ClientNonce)))
            {
                ClientRun = Message.Value.Run;
                ClientNonce = Message.Value.Revision;
            }
        const auto Value = Game::ReadSharedProgress();
        if (ClientRun && Value)
        {
            Game::PrepareAirdropReplication();
            Progress::State Image{ClientRun, ClientNonce, Context->Day, Value->Level, Value->Fraction, Context->Map, Context->StateGuid};
            if (const auto Crates = Game::ReadAirdropViews())
            {
                Image.AirdropsKnown = true;
                Image.Airdrops = *Crates;
            }
            const auto Payload = Progress::Encode(Image);
            if (!Payload.empty())
                Transport->SendControl(Peer, {Run, Epoch, ++Revision, Progress::Snapshot, Payload});
            // Ein fehlgeschlagener Send verliert keinen Gewinn: naechster Tick liefert wieder das Vollbild.
        }
        // XP-Baelle sind Ereignisse, nur Anzeige. Ohne Abonnenten verfallen sie, statt sich anzustauen.
        auto Spawns = Game::TakeExpSpawns(Progress::MaxOrbs);
        if (ClientRun && !Spawns.empty())
        {
            const auto Payload = Progress::EncodeOrbs({ClientRun, ClientNonce, Context->StateGuid, Context->Day, std::move(Spawns)});
            if (!Payload.empty())
                Transport->SendControl(Peer, {Run, Epoch, ++Revision, Progress::Orbs, Payload});
        }
    }
    else if (Context->Role == Game::NetRole::Client && !Transport->Stats().IsHost)
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

void InstallProgressSync()
{
    Game::GameThread::AddTick(TickProgress);
}
}
