#include "Sync/Inventory/RewardState.h"

#include <chrono>
#include <random>
#include "Core/Log.h"
#include "Game/RewardAdapter.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"
#include "Sync/Sync.h"

// Airdrop-Auswahl: Host schickt das Sitzungsbild, der Client bearbeitet sein Inventar lokal und schickt beim Bestätigen einen
// Edit, der Host übernimmt ihn oder lehnt ab (inventar-lokal).
namespace Sync
{
namespace
{
struct
{
    uint64_t Run = 0, Revision = 0, Peer = 0;
    // Host
    std::optional<Reward::Offer> Sent;
    uint64_t NextResend = 0;
    uint64_t LastEdit = 0; // zuletzt beantworteter Edit
    bool LastAccepted = false;
    // Client
    std::optional<Reward::Offer> Latest;
    uint64_t LatestAt = 0;
    uint64_t NextShow = 0; // nach einem gescheiterten Öffnen des Panels erst später erneut
    uint64_t EditSentId = 0, NextEditSend = 0;
    uint64_t DoneSession = 0; // Sitzung, deren Wahl der Host übernommen hat
    uint64_t NextReadySend = 0;
    bool ReadySent = false;
} R;

uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool Send(Net::Transport& Transport, uint16_t Type, std::vector<uint8_t> Payload)
{
    if (!R.Run)
    {
        std::random_device Random;
        R.Run = ((uint64_t(Random()) << 32) | Random()) | 1;
    }
    return !Payload.empty() && Transport.SendControl(R.Peer, {R.Run, 1, ++R.Revision, Type, std::move(Payload)});
}

Reward::Offer ToWire(const std::optional<Game::Rewards::Offer>& Value)
{
    Reward::Offer Out;
    if (!Value || !Value->Session || Value->Choices.empty())
        return Out;
    Out.Session = Value->Session;
    Out.Revision = Value->Revision;
    Out.Level = Value->Level;
    Out.ClientClaimed = Value->ClientClaimed;
    Out.HostClaimed = Value->HostClaimed;
    Out.HostWaiting = Value->HostWaiting;
    Out.ClientReady = Value->ClientReady;
    for (size_t i = 0; i < Value->Choices.size() && i < Reward::MaxChoices; ++i)
    {
        const auto& Choice = Value->Choices[i];
        Out.Choices.push_back({Reward::Clip(Choice.Name), Reward::Clip(Choice.Data), Reward::Clip(Choice.Rarity), Choice.Broken});
    }
    return Out;
}

void TickHost(Net::Transport& Transport, std::span<const Net::ControlMessage> Messages)
{
    for (const auto& Message : Messages)
    {
        if (Message.Peer == R.Peer && Message.Value.Type == Reward::ReadyType)
        {
            if (const auto Ready = Reward::DecodeReady(Message.Value.Payload))
            {
                Game::Rewards::SetClientReady(Ready->Session, Ready->Revision, Ready->Value);
                R.NextResend = 0; // Stand sofort im Bild zurück
            }
            continue;
        }
        if (Message.Peer != R.Peer || Message.Value.Type != Reward::EditType)
            continue;
        const auto Edit = Reward::DecodeEdit(Message.Value.Payload);
        if (!Edit)
        {
            Core::Log::Warn("Inventar lokal: Edit vom Client nicht lesbar ({} Byte)", Message.Value.Payload.size());
            continue;
        }
        // Wiederholung: nur die Antwort erneut, nicht noch einmal übernehmen.
        if (Edit->Id != R.LastEdit)
        {
            R.LastAccepted = Game::Rewards::ApplyClientEdit(*Edit);
            R.LastEdit = Edit->Id;
        }
        if (!Send(Transport, Reward::ResultType, Reward::Encode(Reward::Result{Edit->Id, R.LastAccepted})))
            Core::Log::Warn("Inventar lokal: Antwort zu Edit {} nicht gesendet", Edit->Id);
        R.NextResend = 0; // neues Bild sofort
    }
    const auto Offer = ToWire(Game::Rewards::ReadOffer());
    const uint64_t Now = NowMs();
    if (!R.Sent || *R.Sent != Offer || Now >= R.NextResend)
        if (Send(Transport, Reward::OfferType, Reward::Encode(Offer)))
        {
            if (!R.Sent || R.Sent->Session != Offer.Session)
                Core::Log::Info("Airdrop-Auswahl: Sitzungsbild {} mit {} Choices an den Client", Offer.Session, Offer.Choices.size());
            R.Sent = Offer;
            R.NextResend = Now + 1000;
        }
}

// Client: offenen Edit senden, bis die Antwort kommt (jede Sekunde erneut, der Host übernimmt ihn nur einmal).
void SendEdit(Net::Transport& Transport)
{
    const auto Edit = Game::Rewards::PendingClientEdit();
    if (!Edit)
        return;
    const uint64_t Now = NowMs();
    if (Edit->Id == R.EditSentId && Now < R.NextEditSend)
        return;
    if (!Send(Transport, Reward::EditType, Reward::Encode(*Edit)))
    {
        Core::Log::Warn("Inventar lokal: Edit {} nicht gesendet, neuer Versuch", Edit->Id);
        R.NextEditSend = Now + 250;
        return;
    }
    if (Edit->Id != R.EditSentId)
        Core::Log::Info("Inventar lokal: Edit {} an den Host gesendet ({} Items)", Edit->Id, Edit->Items.size());
    R.EditSentId = Edit->Id;
    R.NextEditSend = Now + 1000;
}

// Client: eigenes Bereit senden, solange das Sitzungsbild einen anderen Stand zeigt (alle 500 ms erneut).
void SendReady(Net::Transport& Transport, const Reward::Offer& Offer)
{
    const auto Own = Game::Rewards::ClientReadyState();
    if (!Own || Own->Session != Offer.Session || Own->Revision != Offer.Revision || Own->Ready == Offer.ClientReady)
        return;
    const uint64_t Now = NowMs();
    if (Now < R.NextReadySend && Own->Ready == R.ReadySent)
        return;
    if (Send(Transport, Reward::ReadyType, Reward::Encode(Reward::Ready{Own->Session, Own->Revision, Own->Ready})))
    {
        R.ReadySent = Own->Ready;
        R.NextReadySend = Now + 500;
    }
}

void TickClient(Net::Transport& Transport, std::span<const Net::ControlMessage> Messages)
{
    for (const auto& Message : Messages)
    {
        if (Message.Peer != R.Peer)
            continue;
        if (Message.Value.Type == Reward::OfferType)
        {
            if (auto Offer = Reward::DecodeOffer(Message.Value.Payload))
            {
                if (Offer->Session && (!R.Latest || R.Latest->Session != Offer->Session))
                    Core::Log::Info("Airdrop-Auswahl: Sitzung {} vom Host, {} Choices", Offer->Session, Offer->Choices.size());
                R.Latest = std::move(*Offer);
                R.LatestAt = NowMs();
            }
        }
        else if (Message.Value.Type == Reward::ResultType)
            if (const auto Result = Reward::DecodeResult(Message.Value.Payload))
            {
                const auto Pending = Game::Rewards::PendingClientEdit();
                if (!Pending || Pending->Id != Result->Id)
                    continue;
                Core::Log::Info("Inventar lokal: Host hat Edit {} {}", Result->Id, Result->Accepted ? "übernommen" : "abgelehnt");
                if (Result->Accepted && Pending->Choice)
                    R.DoneSession = Pending->Session;
                Game::Rewards::ResolveClientEdit(Result->Id, Result->Accepted);
            }
    }
    SendEdit(Transport); // auch nach dem Ende des Host-Panels
    // Ohne frisches Bild (Verbindung weg, Host ohne Mod-Sitzung) keine Auswahl anzeigen.
    if (!R.Latest || !R.Latest->Session || NowMs() - R.LatestAt > 5000)
    {
        Game::Rewards::CloseForClient();
        return;
    }
    const auto& Offer = *R.Latest;
    if (Offer.ClientClaimed || Offer.Session == R.DoneSession)
        Game::Rewards::CloseForClient();
    else if (!Game::Rewards::PendingClientEdit())
    {
        std::vector<Game::Rewards::ChoiceItem> Items;
        for (const auto& Choice : Offer.Choices)
            Items.push_back({Choice.Name, Choice.Data, Choice.Rarity, Choice.Broken});
        if (NowMs() >= R.NextShow && !Game::Rewards::ShowForClient(Offer.Session, Offer.Revision, Offer.Level, Items))
            R.NextShow = NowMs() + 5000;
        SendReady(Transport, Offer);
        // bestaetigen-alle: erst wenn der Host wartet und das eigene Bereit bestätigt zurückkommt.
        if (const auto Own = Game::Rewards::ClientReadyState(); Own && Own->Ready && Own->Session == Offer.Session && Own->Revision == Offer.Revision)
            Game::Rewards::FinishClientIfAllReady(Offer.HostWaiting && Offer.ClientReady);
    }
}
}

void TickReward(std::span<const Net::ControlMessage> Messages)
{
    const auto Transport = Net::CurrentTransport();
    const auto Peers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const uint64_t Peer = Peers.size() == 1 ? Peers[0] : 0;
    const bool Host = Transport && Transport->Stats().IsHost;
    if (Peer != R.Peer)
    {
        R.Peer = Peer;
        R.Sent.reset();
        R.Latest.reset();
        R.EditSentId = 0;
        R.LastEdit = 0;
        Game::Rewards::AbortClientEdit();
    }
    Game::Rewards::SetRemoteReady(Host && Peer);
    if (!Transport || !Peer)
        return;
    if (Host)
        TickHost(*Transport, Messages);
    else
        TickClient(*Transport, Messages);
}
}
