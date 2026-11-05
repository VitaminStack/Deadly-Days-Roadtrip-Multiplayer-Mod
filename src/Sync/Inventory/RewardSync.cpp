#include "Sync/Inventory/RewardState.h"

#include <chrono>
#include <random>
#include "Core/Log.h"
#include "Game/RewardAdapter.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"
#include "Sync/Sync.h"

// Airdrop-Auswahl: Host schickt das Sitzungsbild, Client wählt im Overlay, Host trägt die Wahl als Claim von P2 ein.
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
    // Client
    std::optional<Reward::Offer> Latest;
    uint64_t LatestAt = 0;
    Reward::Choice Chosen;
    uint64_t ChosenAt = 0;
    uint64_t NextShow = 0; // nach einem gescheiterten Öffnen des Panels erst später erneut
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
        if (Message.Peer == R.Peer && Message.Value.Type == Reward::ChoiceType)
            if (const auto Choice = Reward::DecodeChoice(Message.Value.Payload))
            {
                if (!Game::Rewards::ClaimForClient(Choice->Session, Choice->Revision, Choice->Index))
                    Core::Log::Warn("Airdrop-Auswahl: Wahl {} für Sitzung {} passt nicht mehr, verworfen", Choice->Index, Choice->Session);
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

bool Send(Net::Transport& Transport, const Reward::Choice& Choice)
{
    if (!Send(Transport, Reward::ChoiceType, Reward::Encode(Choice)))
        return false;
    R.Chosen = Choice;
    R.ChosenAt = NowMs(); // ohne Bestätigung im Bild nach 4 s wieder wählbar
    return true;
}

void TickClient(Net::Transport& Transport, std::span<const Net::ControlMessage> Messages)
{
    for (const auto& Message : Messages)
        if (Message.Peer == R.Peer && Message.Value.Type == Reward::OfferType)
            if (auto Offer = Reward::DecodeOffer(Message.Value.Payload))
            {
                if (Offer->Session && (!R.Latest || R.Latest->Session != Offer->Session))
                    Core::Log::Info("Airdrop-Auswahl: Sitzung {} vom Host, {} Choices", Offer->Session, Offer->Choices.size());
                R.Latest = std::move(*Offer);
                R.LatestAt = NowMs();
            }
    // Ohne frisches Bild (Verbindung weg, Host ohne Mod-Sitzung) keine Auswahl anzeigen.
    if (!R.Latest || !R.Latest->Session || NowMs() - R.LatestAt > 5000)
    {
        Game::Rewards::CloseForClient();
        return;
    }
    const auto& Offer = *R.Latest;
    if (const auto Pick = Game::Rewards::TakeClientPick())
    {
        const Reward::Choice Choice{Offer.Session, Offer.Revision, static_cast<uint8_t>(*Pick)};
        if (*Pick < Offer.Choices.size() && Send(Transport, Choice))
            Core::Log::Info("Airdrop-Auswahl: Wahl {} ({}) an den Host gesendet", *Pick, Offer.Choices[*Pick].Name);
        else
            Core::Log::Warn("Airdrop-Auswahl: Wahl {} nicht gesendet, Panel öffnet erneut", *Pick);
    }
    const bool Sent = R.Chosen.Session == Offer.Session && R.Chosen.Revision == Offer.Revision && NowMs() - R.ChosenAt < 4000;
    if (Offer.ClientClaimed)
        Game::Rewards::CloseForClient();
    else if (!Sent)
    {
        std::vector<Game::Rewards::ChoiceItem> Items;
        for (const auto& Choice : Offer.Choices)
            Items.push_back({Choice.Name, Choice.Data, Choice.Rarity, Choice.Broken});
        if (NowMs() >= R.NextShow && !Game::Rewards::ShowForClient(Offer.Session, Offer.Revision, Offer.Level, Items))
            R.NextShow = NowMs() + 5000;
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
        R.Chosen = {};
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
