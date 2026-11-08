#include "Sync/Players/PauseState.h"

#include <chrono>
#include <random>
#include "Core/Log.h"
#include "Game/PauseAdapter.h"
#include "Net/Net.h"
#include "Sync/ControlDispatch.h"

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
    uint64_t Peer = 0, Run = 0, Revision = 0;
    bool Host = false;
    // Host
    Pause::HostHold Hold;
    bool Applied = false;
    uint64_t NextRetry = 0;
    // Client
    std::optional<Pause::ClientReporter> Reporter;
    std::optional<bool> LastOpen;
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

void TickHost(std::span<const Net::ControlMessage> Messages, uint64_t Now)
{
    for (const auto& Message : Messages)
    {
        if (Message.Peer != P.Peer || Message.Value.Type != Pause::ClientPauseType)
            continue;
        if (const auto Value = Pause::Decode(Message.Value.Payload))
            P.Hold.Receive(*Value, Now);
        else
            Core::Log::Warn("Pause: unlesbare Meldung vom Client verworfen");
    }
    P.Hold.Tick(P.Peer, Game::Pause::WorldKey(), Now);
    if (const auto Why = P.Hold.TakeRelease(); Why != Pause::Release::None && P.Applied)
        Core::Log::Info("Pause: Freigabe, {}", Describe(Why));
    const bool Want = P.Hold.Held();
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
    const auto Open = Game::Pause::LocalMenuOpen();
    // Unlesbar zählt wie geschlossen: eine offene Sitzung endet, keine neue beginnt.
    const bool Value = Open.value_or(false);
    if (Open && Open != P.LastOpen)
        LogTime(*Open ? "Pausemenü beim Client offen" : "Pausemenü beim Client geschlossen");
    if (Open)
        P.LastOpen = Open;
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
    const uint64_t Peer = Peers.size() == 1 ? Peers[0] : 0;
    const bool Host = Transport && Transport->Stats().IsHost;
    const uint64_t Now = NowMs();
    if (Peer != P.Peer || Host != P.Host)
    {
        P.Peer = Peer;
        P.Host = Host;
        if (P.Reporter)
            P.Reporter->Reset();
    }
    if (Transport && Peer)
        LogWorldChange();
    // Der Host gibt auch ohne Verbindung frei; der Tick läuft deshalb immer, solange er etwas hält.
    if (Host || P.Applied)
        TickHost(Host ? Messages : std::span<const Net::ControlMessage>{}, Now);
    else if (Transport && Peer)
        TickClient(*Transport, Now);
}
}
