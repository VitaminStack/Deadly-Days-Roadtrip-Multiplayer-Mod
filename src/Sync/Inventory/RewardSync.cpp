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
    // Lager am Rundenende (bestaetigen-rundenende)
    std::optional<Reward::RoundEndStatus> SentEnd; // Host
    uint64_t NextEndResend = 0;
    uint64_t EndClientAt = 0; // Host: letzte Meldung 0x306, 0 ohne
    std::optional<Reward::RoundEndStatus> LatestEnd; // Client
    uint64_t LatestEndAt = 0, NextEndClientSend = 0;
    bool EndClientIn = false; // Client: war zuletzt im Rundenende
    uint64_t EndLeftUntil = 0; // Client: bis dahin das Verlassen melden
    // Seltenheiten (werkbank-client): Host gesendeter Stand, Client neuester Stand
    std::optional<Reward::Rarities> SentRarities, LatestRarities;
    uint64_t NextRarityCheck = 0, NextRarityResend = 0, NextRarityApply = 0;
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

// Host: Stand im Lager bei Änderung sofort, sonst jede Sekunde.
void SendRoundEndStatus(Net::Transport& Transport)
{
    const auto State = Game::Rewards::HostRoundEnd();
    const Reward::RoundEndStatus Status{State.Id, State.HostReady, State.ClientReady};
    const uint64_t Now = NowMs();
    if (R.SentEnd && *R.SentEnd == Status && Now < R.NextEndResend)
        return;
    if (Send(Transport, Reward::RoundEndStatusType, Reward::Encode(Status)))
    {
        R.SentEnd = Status;
        R.NextEndResend = Now + 1000;
    }
}

// Client: eigenes Rundenende und Bereit alle 500 ms; den Stand des Hosts nur frisch (5 s) weitergeben.
void TickClientRoundEnd(Net::Transport& Transport)
{
    const uint64_t Now = NowMs();
    const auto Own = Game::Rewards::ClientRoundEnd();
    if (Own && Now >= R.NextEndClientSend &&
        Send(Transport, Reward::RoundEndClientType, Reward::Encode(Reward::RoundEndClient{Own->Id, Own->Ready})))
        R.NextEndClientSend = Now + 500;
    // lager-absturz-client: Verlassen 3 s lang ausdrücklich melden. Nur so unterscheidet der Host es von einem stummen Client.
    if (!Own && R.EndClientIn)
        R.EndLeftUntil = Now + 3000;
    R.EndClientIn = Own.has_value();
    if (!Own && Now < R.EndLeftUntil && Now >= R.NextEndClientSend &&
        Send(Transport, Reward::RoundEndClientType, Reward::Encode(Reward::RoundEndClient{0, false})))
        R.NextEndClientSend = Now + 500;
    if (R.LatestEnd && Now - R.LatestEndAt <= 5000)
        Game::Rewards::SetHostRoundEndStatus(Game::Rewards::RoundEndState{R.LatestEnd->Id, R.LatestEnd->HostReady, R.LatestEnd->ClientReady});
    else
        Game::Rewards::SetHostRoundEndStatus(std::nullopt);
}

// Host: Seltenheiten des Clientinventars jede Sekunde prüfen, bei Änderung sofort senden, sonst alle 5 s.
void SendRarities(Net::Transport& Transport)
{
    const uint64_t Now = NowMs();
    if (Now < R.NextRarityCheck)
        return;
    R.NextRarityCheck = Now + 1000;
    Reward::Rarities Value;
    for (const auto& Entry : Game::Rewards::ClientRarities())
        Value.Items.push_back({Entry.Index, Entry.DataHash, Reward::Clip(Entry.Rarity)});
    if (Value.Items.empty() || (R.SentRarities && *R.SentRarities == Value && Now < R.NextRarityResend))
        return;
    if (Send(Transport, Reward::RarityType, Reward::Encode(Value)))
    {
        if (!R.SentRarities || *R.SentRarities != Value)
            Core::Log::Info("Seltenheiten: {} Items des Clients gesendet", Value.Items.size());
        R.SentRarities = std::move(Value);
        R.NextRarityResend = Now + 5000;
    }
}

// Client: neuesten Stand jede Sekunde anwenden; neue replizierte Items bekommen so ihre Seltenheit.
void ApplyRarities()
{
    const uint64_t Now = NowMs();
    if (!R.LatestRarities || Now < R.NextRarityApply)
        return;
    R.NextRarityApply = Now + 1000;
    std::vector<Game::Rewards::RarityEntry> Entries;
    for (const auto& Item : R.LatestRarities->Items)
        Entries.push_back({Item.Index, Item.DataHash, Item.Rarity});
    Game::Rewards::ApplyRarities(Entries);
}

void TickHost(Net::Transport& Transport, std::span<const Net::ControlMessage> Messages)
{
    for (const auto& Message : Messages)
    {
        if (Message.Peer == R.Peer && Message.Value.Type == Reward::RoundEndClientType)
        {
            if (const auto Client = Reward::DecodeRoundEndClient(Message.Value.Payload))
            {
                Game::Rewards::SetRoundEndClient(Client->Id, Client->Ready);
                R.EndClientAt = Client->Id ? NowMs() : 0;
                R.NextEndResend = 0; // Stand sofort zurück
            }
            else
                Core::Log::Warn("Lager: Stand des Clients nicht lesbar ({} Byte)", Message.Value.Payload.size());
            continue;
        }
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
    // Der Client meldet sich alle 500 ms, solange er im Rundenende ist, und meldet das Verlassen ausdrücklich (Id 0). Bleibt er
    // nur stumm, etwa nach einem Absturz, wartet der Host weiter, bis die Verbindung abläuft (lager-absturz-client); vorher
    // reiste er nach 2 s als „alle bereit“ ohne ihn weiter.
    if (R.EndClientAt && NowMs() - R.EndClientAt > 2000)
    {
        Core::Log::Warn("Lager: Client stumm, Host wartet auf ihn bis zum Verbindungsabbau");
        R.EndClientAt = 0;
    }
    SendRoundEndStatus(Transport);
    SendRarities(Transport);
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
        if (Message.Value.Type == Reward::RoundEndStatusType)
        {
            if (const auto Status = Reward::DecodeRoundEndStatus(Message.Value.Payload))
            {
                R.LatestEnd = *Status;
                R.LatestEndAt = NowMs();
            }
            else
                Core::Log::Warn("Lager: Stand des Hosts nicht lesbar ({} Byte)", Message.Value.Payload.size());
        }
        else if (Message.Value.Type == Reward::RarityType)
        {
            if (auto Value = Reward::DecodeRarities(Message.Value.Payload))
            {
                if (!R.LatestRarities || *R.LatestRarities != *Value)
                    R.NextRarityApply = 0;
                R.LatestRarities = std::move(*Value);
            }
            else
                Core::Log::Warn("Seltenheiten: Nachricht vom Host nicht lesbar ({} Byte)", Message.Value.Payload.size());
        }
        else if (Message.Value.Type == Reward::OfferType)
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
    TickClientRoundEnd(Transport);
    ApplyRarities();
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
        R.SentEnd.reset();
        R.LatestEnd.reset();
        R.EndClientAt = 0;
        R.SentRarities.reset();
        R.LatestRarities.reset();
        Game::Rewards::AbortClientEdit();
    }
    Game::Rewards::SetRemoteReady(Host && Peer);
    if (!Transport || !Peer)
    {
        // Ohne Gegenseite wartet niemand im Lager: Host vergisst den Client, der Client löst sein Bestätigen aus.
        Game::Rewards::SetRoundEndClient(0, false);
        Game::Rewards::SetHostRoundEndStatus(std::nullopt);
        return;
    }
    if (Host)
        TickHost(*Transport, Messages);
    else
        TickClient(*Transport, Messages);
}
}
