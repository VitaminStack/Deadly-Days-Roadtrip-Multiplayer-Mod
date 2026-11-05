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
RewardSink Sink;
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
    std::optional<RewardPrompt> Shown;
    bool Published = false;
} R;

uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

void Present(std::optional<RewardPrompt> Prompt)
{
    if (R.Published && Prompt == R.Shown)
        return;
    R.Shown = Prompt;
    R.Published = true;
    if (Sink)
        Sink(std::move(Prompt));
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
        Out.Choices.push_back(Reward::Clip(Value->Choices[i]));
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
    if (Offer.Session && Offer.HostWaiting && !Offer.ClientClaimed)
        Present(RewardPrompt{Offer.Session, Offer.Revision, Offer.Level, {}, "Airdrop: Warte auf die Wahl deines Mitspielers.", false});
    else
        Present(std::nullopt);
}

void TickClient(Net::Transport&, std::span<const Net::ControlMessage> Messages)
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
        Present(std::nullopt);
        return;
    }
    const auto& Offer = *R.Latest;
    RewardPrompt Prompt{Offer.Session, Offer.Revision, Offer.Level, {}, {}, false};
    if (Offer.ClientClaimed)
        Prompt.Status = Offer.HostClaimed ? "Airdrop: Belohnung erhalten." : "Airdrop: Gewählt, warte auf den Host.";
    else if (R.Chosen.Session == Offer.Session && R.Chosen.Revision == Offer.Revision)
        Prompt.Status = "Airdrop: Wahl gesendet, warte auf den Host.";
    else
    {
        Prompt.Choices = Offer.Choices;
        Prompt.Selectable = true;
        Prompt.Status = "Airdrop (Stufe " + std::to_string(Offer.Level) + "): Wähle 1 Belohnung." +
                        (Offer.HostWaiting ? " Der Host wartet auf dich." : "");
    }
    Present(std::move(Prompt));
}
}

void SetRewardSink(RewardSink Value)
{
    Sink = std::move(Value);
}

void ChooseReward(uint64_t Session, uint64_t Revision, uint32_t Index)
{
    const auto Transport = Net::CurrentTransport();
    if (!Transport || Transport->Stats().IsHost || !R.Peer || !R.Latest || R.Latest->Session != Session ||
        R.Latest->Revision != Revision || R.Latest->ClientClaimed || Index >= R.Latest->Choices.size())
        return;
    const Reward::Choice Choice{Session, Revision, static_cast<uint8_t>(Index)};
    if (Send(*Transport, Reward::ChoiceType, Reward::Encode(Choice)))
    {
        R.Chosen = Choice;
        Core::Log::Info("Airdrop-Auswahl: Wahl {} ({}) an den Host gesendet", Index, R.Latest->Choices[Index]);
    }
    else
        R.Published = false; // Overlay wieder wählbar machen
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
    {
        Present(std::nullopt);
        return;
    }
    if (Host)
        TickHost(*Transport, Messages);
    else
        TickClient(*Transport, Messages);
}
}
