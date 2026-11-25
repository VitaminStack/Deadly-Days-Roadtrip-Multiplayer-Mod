#include "Sync/Players/PauseState.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <map>
#include <random>
#include "Core/Log.h"
#include "Game/PauseAdapter.h"
#include "Game/RewardAdapter.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"
#include "Sync/Sync.h"

// Pausemenü des Clients beim Host (pausen-sync), Ablauf in PauseState.h.
namespace Sync
{
namespace
{
uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::function<void(std::string)> NoticeSink;
constexpr const char* RemotePauseText = "Der Host hat das Spiel angehalten";

uint64_t RandomId()
{
    std::random_device Random;
    return ((uint64_t(Random()) << 32) | Random()) | 1;
}

const char* Describe(Pause::Release Why)
{
    switch (Why)
    {
    case Pause::Release::Closed: return "Client hat geschlossen";
    case Pause::Release::Timeout: return "Zeitablauf ohne Meldung";
    case Pause::Release::World: return "Weltwechsel";
    case Pause::Release::Peer: return "Verbindung weg";
    default: return "";
    }
}

struct
{
    uint64_t Peer = 0, Run = 0, Revision = 0; // Peer: beim Client der Host
    bool Host = false;
    // Host: eine Pause je Client, die Welt steht, solange einer hält.
    std::map<uint64_t, Pause::HostHold> Holds;
    bool Applied = false;
    uint64_t NextRetry = 0;
    // Client
    std::optional<Pause::ClientReporter> Reporter;
    std::optional<bool> LastOpen;
    Pause::TravelCleanup Cleanup;
    Pause::RemotePauseNotice Notice;
    bool NoticeShown = false;
    // Beide: zuletzt geloggter Zustand der Weltzeit, für den Nachweis gleicher Weltzeit im Log.
    std::optional<bool> WorldPaused;
} P;

void LogWorldChange()
{
    const auto Time = Game::Pause::ReadWorldTime();
    if (!Time)
        return;
    const bool Paused = Time->Pauser || Time->Dilation < 0.01f;
    if (P.WorldPaused == Paused)
        return;
    P.WorldPaused = Paused;
    Core::Log::Info("Pause: Weltzeit {}, Faktor {} Pauser {}", Paused ? "angehalten" : "läuft", Time->Dilation, Time->Pauser);
}

void LogTime(const char* What)
{
    if (const auto Time = Game::Pause::ReadWorldTime())
        Core::Log::Info("Pause: {}, Weltzeit {} Pauser {}", What, Time->Dilation, Time->Pauser);
    else
        Core::Log::Info("Pause: {}", What);
}

// Hinweis beim Client an- oder abschalten, nur bei Wechsel.
void ShowNotice(bool Show)
{
    if (Show == P.NoticeShown)
        return;
    P.NoticeShown = Show;
    if (NoticeSink)
        NoticeSink(Show ? RemotePauseText : "");
    LogTime(Show ? "Hinweis beim Client, der Host hält die Weltzeit an" : "Hinweis beim Client weg");
}

void TickHost(std::span<const Net::ControlMessage> Messages, std::span<const uint64_t> Peers, uint64_t Now)
{
    const auto Present = [Peers](uint64_t Peer) { return std::find(Peers.begin(), Peers.end(), Peer) != Peers.end(); };
    for (const auto& Message : Messages)
    {
        if (!Present(Message.Peer) || Message.Value.Type != Pause::ClientPauseType)
            continue;
        if (const auto Value = Pause::Decode(Message.Value.Payload))
            P.Holds[Message.Peer].Receive(*Value, Now);
        else
            Core::Log::Warn("Pause: unlesbare Meldung vom Client verworfen");
    }
    bool Want = false;
    const uint64_t World = Game::Pause::WorldKey();
    for (auto It = P.Holds.begin(); It != P.Holds.end();)
    {
        // Ein getrennter Client gibt seine Pause frei (Peer 0), dann fällt sein Eintrag weg.
        const bool Here = Present(It->first);
        It->second.Tick(Here ? It->first : 0, World, Now);
        if (const auto Why = It->second.TakeRelease(); Why != Pause::Release::None && P.Applied)
            Core::Log::Info("Pause: Freigabe, {} (Peer {:x})", Describe(Why), It->first);
        Want |= It->second.Held();
        It = Here ? std::next(It) : P.Holds.erase(It);
    }
    if (Want == P.Applied || (Want && Now < P.NextRetry))
        return;
    if (Game::Pause::HoldForRemote(Want))
    {
        P.Applied = Want;
        LogTime(Want ? "Host hält die Pause des Clients" : "Host gibt die Pause des Clients frei");
    }
    else if (Want)
    {
        // Ohne lesbares Pausemenü des Hosts lieber keine Pause; in 2 s neu versuchen.
        P.NextRetry = Now + 2000;
        Core::Log::Warn("Pause: Zeitanfrage für den Client nicht möglich");
    }
    else
    {
        // Freigeben scheitert nur, wenn die Funktionen fehlen; dann gab es auch keine Anfrage.
        P.Applied = false;
    }
}

void TickClient(Net::Transport& Transport, uint64_t Now)
{
    if (!P.Reporter)
        P.Reporter.emplace(RandomId());
    // Nach einer Reise die Zeitanfragen der alten Welt abräumen, vor allem die des alten GameState aus FinishMission.
    if (P.Cleanup.Due(Game::Pause::WorldKey(), Now))
    {
        if (const auto Cleared = Game::Pause::ClearTimeRequests())
        {
            P.Cleanup.Done();
            if (*Cleared)
                LogTime(std::format("Client räumt nach der Reise {} Zeitanfrage(n) der alten Welt ab", *Cleared).c_str());
        }
    }
    const auto Open = Game::Pause::LocalMenuOpen();
    // Unlesbar zählt wie geschlossen: eine offene Sitzung endet, keine neue beginnt.
    const bool Value = Open.value_or(false);
    if (Open && Open != P.LastOpen)
        LogTime(*Open ? "Pausemenü beim Client offen" : "Pausemenü beim Client geschlossen");
    if (Open)
        P.LastOpen = Open;
    // Hinweis, solange nur der Host die Welt anhält. Nicht lesbar zählt wie eigene Pause: lieber kein Hinweis als ein falscher.
    const auto Time = Game::Pause::ReadWorldTime();
    const auto Requests = Game::Pause::ActiveTimeRequests();
    const bool WorldPaused = Time && (Time->Pauser || Time->Dilation < 0.01f);
    // Eine eigene offene Auswahl (Airdrop, Belohnung) zählt wie eine eigene Pause: Der Client sieht sein Panel und muss selbst wählen.
    const bool OwnChoice = Game::Rewards::ClientReadyState().has_value();
    const bool LocalHold = !Open || *Open || !Requests || *Requests > 0 || OwnChoice;
    ShowNotice(P.Notice.Update(true, WorldPaused, LocalHold, Game::Pause::WorldKey(), Now));
    const auto Report = P.Reporter->Update(Value, Now);
    if (!Report)
        return;
    if (!P.Run)
        P.Run = RandomId();
    Transport.SendControl(P.Peer, {P.Run, 1, ++P.Revision, Pause::ClientPauseType, Pause::Encode(*Report)});
}
}

void TickPause(std::span<const Net::ControlMessage> Messages)
{
    const auto Transport = Net::CurrentTransport();
    const auto Peers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const bool Host = Transport && Transport->Stats().IsHost;
    const uint64_t Peer = !Host && Peers.size() == 1 ? Peers[0] : 0;
    const uint64_t Now = NowMs();
    if (Peer != P.Peer || Host != P.Host)
    {
        P.Peer = Peer;
        P.Host = Host;
        if (P.Reporter)
            P.Reporter->Reset();
    }
    if (Transport && (Peer || (Host && !Peers.empty())))
        LogWorldChange();
    // Der Host gibt auch ohne Verbindung frei; der Tick läuft deshalb immer, solange er etwas hält.
    if (Host || P.Applied)
        TickHost(Host ? Messages : std::span<const Net::ControlMessage>{}, Host ? std::span<const uint64_t>(Peers) : std::span<const uint64_t>{}, Now);
    else if (Transport && Peer)
        TickClient(*Transport, Now);
    // Kein Client mit Verbindung mehr: Der Hinweis darf nicht stehen bleiben.
    if (Host || !Transport || !Peer)
        ShowNotice(P.Notice.Update(false, false, false, 0, Now));
}

void BindNoticeSink(std::function<void(std::string)> Sink)
{
    NoticeSink = std::move(Sink);
}
}
