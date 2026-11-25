#include "Sync/Inventory/RewardState.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <random>
#include "Core/Log.h"
#include "Game/RewardAdapter.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"
#include "Sync/Sync.h"

// Airdrop-Auswahl: Host schickt das Sitzungsbild, der Client bearbeitet sein Inventar lokal und schickt beim Bestätigen einen
// Edit, der Host übernimmt ihn oder lehnt ab (inventar-lokal). Der Host führt dafür je Client-Peer einen eigenen Stand; ein Client
// hat genau einen Peer, den Host.
namespace Sync
{
namespace
{
// Host: Stand je Client-Peer.
struct ClientLink
{
    std::optional<Reward::Offer> Sent;
    uint64_t NextResend = 0;
    uint64_t LastEdit = 0; // zuletzt beantworteter Edit
    Game::InventoryEdit::Verdict LastVerdict;
    std::optional<Reward::RoundEndStatus> SentEnd;
    uint64_t NextEndResend = 0;
    uint64_t EndClientAt = 0; // letzte Meldung 0x306, 0 ohne
    std::optional<Reward::Rarities> SentRarities;
    uint64_t NextRarityCheck = 0, NextRarityResend = 0;
    std::optional<Reward::Scrap> SentScrap;
    uint64_t NextScrapCheck = 0, NextScrapResend = 0;
};

struct
{
    uint64_t Run = 0, Revision = 0;
    uint64_t Peer = 0; // Client: der Host
    std::map<uint64_t, ClientLink> Clients; // Host
    // Client
    std::optional<Reward::Offer> Latest;
    uint64_t LatestAt = 0;
    uint64_t NextShow = 0; // nach einem gescheiterten Öffnen des Panels erst später erneut
    uint64_t EditSentId = 0, NextEditSend = 0;
    uint64_t DoneSession = 0; // Sitzung, deren Wahl der Host übernommen hat
    uint64_t NextReadySend = 0;
    bool ReadySent = false;
    // Lager am Rundenende (bestaetigen-rundenende)
    std::optional<Reward::RoundEndStatus> LatestEnd;
    uint64_t LatestEndAt = 0, NextEndClientSend = 0;
    bool EndClientIn = false; // war zuletzt im Rundenende
    uint64_t EndLeftUntil = 0; // bis dahin das Verlassen melden
    // Seltenheiten (werkbank-client) und Schrott (lager-handel-client): neuester Stand des Hosts
    std::optional<Reward::Rarities> LatestRarities;
    uint64_t NextRarityApply = 0;
    std::optional<Reward::Scrap> LatestScrap;
} R;

uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool Send(Net::Transport& Transport, uint64_t Peer, uint16_t Type, std::vector<uint8_t> Payload)
{
    if (!R.Run)
    {
        std::random_device Random;
        R.Run = ((uint64_t(Random()) << 32) | Random()) | 1;
    }
    return !Payload.empty() && Transport.SendControl(Peer, {R.Run, 1, ++R.Revision, Type, std::move(Payload)});
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
void SendRoundEndStatus(Net::Transport& Transport, uint64_t Peer, ClientLink& Link)
{
    const auto State = Game::Rewards::HostRoundEnd(Peer);
    const Reward::RoundEndStatus Status{State.Id, State.HostReady, State.ClientReady};
    const uint64_t Now = NowMs();
    if (Link.SentEnd && *Link.SentEnd == Status && Now < Link.NextEndResend)
        return;
    if (Send(Transport, Peer, Reward::RoundEndStatusType, Reward::Encode(Status)))
    {
        Link.SentEnd = Status;
        Link.NextEndResend = Now + 1000;
    }
}

// Client: eigenes Rundenende und Bereit alle 500 ms; den Stand des Hosts nur frisch (5 s) weitergeben.
void TickClientRoundEnd(Net::Transport& Transport)
{
    const uint64_t Now = NowMs();
    const auto Own = Game::Rewards::ClientRoundEnd();
    if (Own && Now >= R.NextEndClientSend &&
        Send(Transport, R.Peer, Reward::RoundEndClientType, Reward::Encode(Reward::RoundEndClient{Own->Id, Own->Ready})))
        R.NextEndClientSend = Now + 500;
    // lager-absturz-client: Verlassen 3 s lang ausdrücklich melden. Nur so unterscheidet der Host es von einem stummen Client.
    if (!Own && R.EndClientIn)
        R.EndLeftUntil = Now + 3000;
    R.EndClientIn = Own.has_value();
    if (!Own && Now < R.EndLeftUntil && Now >= R.NextEndClientSend &&
        Send(Transport, R.Peer, Reward::RoundEndClientType, Reward::Encode(Reward::RoundEndClient{0, false})))
        R.NextEndClientSend = Now + 500;
    if (R.LatestEnd && Now - R.LatestEndAt <= 5000)
        Game::Rewards::SetHostRoundEndStatus(Game::Rewards::RoundEndState{R.LatestEnd->Id, R.LatestEnd->HostReady, R.LatestEnd->ClientReady});
    else
        Game::Rewards::SetHostRoundEndStatus(std::nullopt);
}

// Host: Seltenheiten des Clientinventars jede Sekunde prüfen, bei Änderung sofort senden, sonst alle 5 s.
void SendRarities(Net::Transport& Transport, uint64_t Peer, ClientLink& Link)
{
    const uint64_t Now = NowMs();
    if (Now < Link.NextRarityCheck)
        return;
    Link.NextRarityCheck = Now + 1000;
    Reward::Rarities Value;
    for (const auto& Entry : Game::Rewards::ClientRarities(Peer))
        Value.Items.push_back({Entry.Index, Entry.DataHash, Reward::Clip(Entry.Rarity)});
    if (Value.Items.empty() || (Link.SentRarities && *Link.SentRarities == Value && Now < Link.NextRarityResend))
        return;
    if (Send(Transport, Peer, Reward::RarityType, Reward::Encode(Value)))
    {
        if (!Link.SentRarities || *Link.SentRarities != Value)
            Core::Log::Info("Seltenheiten: {} Items des Clients gesendet", Value.Items.size());
        Link.SentRarities = std::move(Value);
        Link.NextRarityResend = Now + 5000;
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

// Host: eigenen Schrott jede halbe Sekunde prüfen, bei Änderung sofort senden, sonst alle 5 s (lager-handel-client).
void SendScrap(Net::Transport& Transport, uint64_t Peer, ClientLink& Link)
{
    const uint64_t Now = NowMs();
    if (Now < Link.NextScrapCheck)
        return;
    Link.NextScrapCheck = Now + 500;
    const auto Amount = Game::Rewards::HostScrap();
    if (!Amount)
        return;
    const Reward::Scrap Value{*Amount};
    if (Link.SentScrap && *Link.SentScrap == Value && Now < Link.NextScrapResend)
        return;
    if (Send(Transport, Peer, Reward::ScrapType, Reward::Encode(Value)))
    {
        if (!Link.SentScrap || *Link.SentScrap != Value)
            Core::Log::Info("Schrott: Stand {} an den Client", Value.Amount);
        Link.SentScrap = Value;
        Link.NextScrapResend = Now + 5000;
    }
}

void HostReceive(Net::Transport& Transport, const Net::ControlMessage& Message, ClientLink& Link)
{
    const uint64_t Peer = Message.Peer;
    if (Message.Value.Type == Reward::RoundEndClientType)
    {
        if (const auto Client = Reward::DecodeRoundEndClient(Message.Value.Payload))
        {
            Game::Rewards::SetRoundEndClient(Peer, Client->Id, Client->Ready);
            Link.EndClientAt = Client->Id ? NowMs() : 0;
            // Ein neuer Stand eines Clients kann den Stand des Hosts für alle ändern: allen sofort zurück.
            for (auto& [Other, Next] : R.Clients)
                Next.NextEndResend = 0;
        }
        else
            Core::Log::Warn("Lager: Stand des Clients nicht lesbar ({} Byte)", Message.Value.Payload.size());
        return;
    }
    if (Message.Value.Type == Reward::ReadyType)
    {
        if (const auto Ready = Reward::DecodeReady(Message.Value.Payload))
        {
            Game::Rewards::SetClientReady(Peer, Ready->Session, Ready->Revision, Ready->Value);
            for (auto& [Other, Next] : R.Clients)
                Next.NextResend = 0; // Stand sofort im Bild zurück, auch HostWaiting der anderen
        }
        return;
    }
    if (Message.Value.Type != Reward::EditType)
        return;
    const auto Edit = Reward::DecodeEdit(Message.Value.Payload);
    if (!Edit)
    {
        Core::Log::Warn("Inventar lokal: Edit vom Client nicht lesbar ({} Byte)", Message.Value.Payload.size());
        return;
    }
    // Wiederholung: nur die Antwort erneut, nicht noch einmal übernehmen.
    if (Edit->Id != Link.LastEdit)
    {
        Link.LastVerdict = Game::Rewards::ApplyClientEdit(Peer, *Edit);
        Link.LastEdit = Edit->Id;
    }
    if (!Send(Transport, Peer, Reward::ResultType, Reward::Encode(Reward::Result{Edit->Id, Link.LastVerdict})))
        Core::Log::Warn("Inventar lokal: Antwort zu Edit {} nicht gesendet", Edit->Id);
    for (auto& [Other, Next] : R.Clients)
        Next.NextResend = 0; // neues Bild sofort, die Wahl ändert ClientClaimed und HostWaiting
}

void TickHost(Net::Transport& Transport, std::span<const uint64_t> Peers, std::span<const Net::ControlMessage> Messages)
{
    for (const uint64_t Peer : Peers)
        R.Clients.try_emplace(Peer);
    for (const auto& Message : Messages)
        if (const auto It = R.Clients.find(Message.Peer); It != R.Clients.end())
            HostReceive(Transport, Message, It->second);
    for (const uint64_t Peer : Peers)
    {
        ClientLink& Link = R.Clients[Peer];
        // Der Client meldet sich alle 500 ms, solange er im Rundenende ist, und meldet das Verlassen ausdrücklich (Id 0). Bleibt er
        // nur stumm, etwa nach einem Absturz, wartet der Host weiter, bis die Verbindung abläuft (lager-absturz-client); vorher
        // reiste er nach 2 s als „alle bereit“ ohne ihn weiter.
        if (Link.EndClientAt && NowMs() - Link.EndClientAt > 2000)
        {
            Core::Log::Warn("Lager: Client stumm, Host wartet auf ihn bis zum Verbindungsabbau");
            Link.EndClientAt = 0;
        }
        SendRoundEndStatus(Transport, Peer, Link);
        SendRarities(Transport, Peer, Link);
        SendScrap(Transport, Peer, Link);
        const auto Offer = ToWire(Game::Rewards::ReadOffer(Peer));
        const uint64_t Now = NowMs();
        if (!Link.Sent || *Link.Sent != Offer || Now >= Link.NextResend)
            if (Send(Transport, Peer, Reward::OfferType, Reward::Encode(Offer)))
            {
                if (!Link.Sent || Link.Sent->Session != Offer.Session)
                    Core::Log::Info("Airdrop-Auswahl: Sitzungsbild {} mit {} Choices an den Client", Offer.Session, Offer.Choices.size());
                Link.Sent = Offer;
                Link.NextResend = Now + 1000;
            }
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
    if (!Send(Transport, R.Peer, Reward::EditType, Reward::Encode(*Edit)))
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
    if (Send(Transport, R.Peer, Reward::ReadyType, Reward::Encode(Reward::Ready{Own->Session, Own->Revision, Own->Ready})))
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
        else if (Message.Value.Type == Reward::ScrapType)
        {
            if (const auto Value = Reward::DecodeScrap(Message.Value.Payload))
                R.LatestScrap = *Value;
            else
                Core::Log::Warn("Schrott: Nachricht vom Host nicht lesbar ({} Byte)", Message.Value.Payload.size());
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
                Core::Log::Info("Inventar lokal: Host hat Edit {} {}", Result->Id,
                                !Result->Verdict.Accepted ? "abgelehnt" : Result->Verdict.Complete ? "übernommen" : "teilweise übernommen");
                if (Result->Verdict.Accepted && Pending->Choice)
                    R.DoneSession = Pending->Session;
                Game::Rewards::ResolveClientEdit(Result->Id, Result->Verdict);
            }
    }
    TickClientRoundEnd(Transport);
    ApplyRarities();
    if (R.LatestScrap)
        Game::Rewards::ApplyScrap(R.LatestScrap->Amount);
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
        // bestaetigen-alle: erst wenn Host und alle anderen Clients bereit sind (HostWaiting) und das eigene Bereit bestätigt zurückkommt.
        if (const auto Own = Game::Rewards::ClientReadyState(); Own && Own->Ready && Own->Session == Offer.Session && Own->Revision == Offer.Revision)
            Game::Rewards::FinishClientIfAllReady(Offer.HostWaiting && Offer.ClientReady);
    }
}
}

void TickReward(std::span<const Net::ControlMessage> Messages)
{
    const auto Transport = Net::CurrentTransport();
    const bool Host = Transport && Transport->Stats().IsHost;
    const auto Peers = Host ? Transport->ControlPeers() : std::vector<uint64_t>{};
    // Host: Stand getrennter Clients verwerfen; ein neuer Peer beginnt leer (der Peer ist je Verbindung neu).
    std::erase_if(R.Clients, [&Peers](const auto& Entry) { return std::find(Peers.begin(), Peers.end(), Entry.first) == Peers.end(); });
    // Ohne Peers (Client, keine Sitzung) wartet beim Host niemand: Auswahl und Lager laufen wie ohne Mod, getrennte Clients sind vergessen.
    Game::Rewards::SetRemotePeers(Peers);
    // wiedereintritt-inventar: auch ohne Peer oder Transport, dann merkt der Host die Trennung. Auf einem PC löst Steam die Lobby beim
    // Verlassen auf und der Host erstellt eine neue; dazwischen gibt es keinen Transport, die Mission läuft aber weiter.
    std::vector<Game::Rewards::RemoteAccount> Accounts;
    for (const uint64_t Peer : Peers)
        Accounts.push_back({Peer, Transport->PeerAccount(Peer)});
    Game::Rewards::TickRejoin(Accounts);
    if (Host)
    {
        TickHost(*Transport, Peers, Messages);
        return;
    }
    const auto ClientPeers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const uint64_t Peer = ClientPeers.size() == 1 ? ClientPeers[0] : 0;
    if (Peer != R.Peer)
    {
        R.Peer = Peer;
        R.Latest.reset();
        R.EditSentId = 0;
        R.LatestEnd.reset();
        R.LatestRarities.reset();
        R.LatestScrap.reset();
        Game::Rewards::AbortClientEdit();
    }
    if (!Transport || !Peer)
    {
        // Ohne Host wartet niemand im Lager: der Client löst sein Bestätigen aus.
        Game::Rewards::SetHostRoundEndStatus(std::nullopt);
        return;
    }
    TickClient(*Transport, Messages);
}
}
