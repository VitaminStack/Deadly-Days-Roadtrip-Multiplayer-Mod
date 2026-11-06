#include "Sync/DesyncState.h"
#include "Sync/ControlDispatch.h"

#include <chrono>
#include <format>
#include <random>
#include "Core/Config.h"
#include "Core/Log.h"
#include "Game/DesyncAdapter.h"
#include "Net/Net.h"
#include "Sync/Sync.h"

namespace Sync
{
namespace
{
using namespace Desync;
bool Enabled = true;
DiagnosticSink PublishDiagnostics;
uint8_t Ignored = DefaultIgnored;
uint64_t NowMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
uint64_t NewRun()
{
    std::random_device Random;
    const auto Run = (uint64_t(Random()) << 32) | Random();
    return Run ? Run : 1;
}
std::string Fields(uint8_t Mask)
{
    std::string Text;
    for (unsigned i = 0; i < Count; ++i)
        if (Mask & Bit(i)) { if (!Text.empty()) Text += ", "; Text += Game::Desync::Names[i]; }
    return Text.empty() ? "-" : Text;
}
struct Runtime
{
    const uint64_t Run = NewRun();
    uint64_t World = 0, State = 0, Guid = 0, Peer = 0, Nonce = 0, Sequence = 0, ReplyRevision = 0;
    int32_t Day = -1;
    Game::NetRole Role = Game::NetRole::Standalone;
    uint64_t RemoteRun = 0, RemoteNonce = 0, RemoteSequence = 0;
    uint64_t NextCapture = 0, NextProbe = 0, LastReply = 0, NextHeartbeat = 0;
    std::optional<Game::DesyncCapture> Capture;
    Stability LocalStability;
    uint8_t Stable = 0;
    Monitor HostMonitor;
    ReplyGate Gate;
    Packet Sent;
    Comparison View;
    std::string LastText;
    uint8_t LastAlerts = 0;

    void Publish(std::string Text, uint64_t Now)
    {
        if (PublishDiagnostics) PublishDiagnostics(Text);
        if (Text != LastText || Now >= NextHeartbeat)
        {
            if (View.Alerts) Core::Log::Warn("{}", Text);
            else Core::Log::Info("{}", Text);
            LastText = Text;
            NextHeartbeat = Now + 30000;
        }
    }
    void Describe(uint8_t Exclusions, uint64_t Now)
    {
        const auto Required = static_cast<uint8_t>(All & ~Exclusions);
        std::string Text;
        if (!LastReply || Now - LastReply > FreshMs)
        {
            View = {};
            HostMonitor = {};
            Text = "Desync: wartet auf frische Gegenmessung";
        }
        else if (View.Alerts) Text = "Desync: ABWEICHUNG " + Fields(View.Alerts);
        else if (Required && View.Comparable == Required && !View.Different) Text = "Desync: OK " + Fields(Required);
        else Text = "Desync: Abgleich; offen " + Fields(static_cast<uint8_t>((Required & ~View.Comparable) | View.Different));
        Text += "\nAusgenommen: " + Fields(Exclusions);
        Publish(std::move(Text), Now);
    }
};
}

void ConfigureDesync(const Core::Config& Config, DiagnosticSink Diagnostics)
{
    PublishDiagnostics = std::move(Diagnostics);
    Enabled = Config.GetBool("desync_enabled", true);
    const char* Keys[Count] = {"desync_ignore_day", "desync_ignore_phase", "desync_ignore_players",
                             "desync_ignore_health", "desync_ignore_enemies", "desync_ignore_inventory"};
    Ignored = 0;
    for (unsigned i = 0; i < Count; ++i)
        if (Config.GetBool(Keys[i], (DefaultIgnored & Bit(i)) != 0)) Ignored |= Bit(i);
    Core::Log::Info("Desync: {} (diagnostisch, keine Zustandskorrektur); ausgenommen: {}",
                    Enabled ? "aktiv" : "aus", Fields(Ignored));
    if (!Enabled && PublishDiagnostics) PublishDiagnostics("Desync: deaktiviert (ddcoop.ini)");
}

void TickDesync(std::span<const Net::ControlMessage> Messages)
{
    if (!Enabled) return;
    static Runtime R;
    static uint8_t HostExclusions = Ignored;
    const auto Now = NowMs();
    const auto Transport = Net::CurrentTransport();
    const auto Context = Game::ReadProgressContext();
    const auto Peers = Transport ? Transport->ControlPeers() : std::vector<uint64_t>{};
    const uint64_t Peer = Peers.size() == 1 ? Peers[0] : 0;
    const bool Active = Context && Peer && Transport &&
        ((Context->Role == Game::NetRole::Host && Transport->Stats().IsHost) ||
         (Context->Role == Game::NetRole::Client && !Transport->Stats().IsHost));
    const uint64_t World = Active ? Context->LocalWorld : 0, State = Active ? Context->LocalState : 0;
    const uint64_t Guid = Active ? Context->StateGuid : 0;
    const auto Role = Active ? Context->Role : Game::NetRole::Standalone;
    const int32_t Day = Active ? Context->Day : -1;
    if (R.World != World || R.State != State || R.Guid != Guid || R.Peer != Peer || R.Day != Day || R.Role != Role)
    {
        R.World = World; R.State = State; R.Guid = Guid; R.Peer = Peer; R.Day = Day; R.Role = Role;
        ++R.Nonce;
        R.Capture.reset(); R.LocalStability = {}; R.Stable = 0; R.HostMonitor = {};
        R.RemoteRun = R.RemoteNonce = R.RemoteSequence = R.NextCapture = R.NextProbe = R.LastReply = 0;
        R.View = {}; R.LastAlerts = 0;
        HostExclusions = Ignored;
        R.Gate.Bind(Peer, R.Run, R.Nonce, Active ? Context->StateGuid : 0);
    }
    if (!Active)
    {
        R.Publish("Desync: wartet auf Kontrollkanal/Missionswelt", Now);
        return;
    }
    if (Now >= R.NextCapture)
    {
        R.NextCapture = Now + 1000;
        R.Capture = Game::ReadDesyncCapture();
        if (R.Capture && (R.Capture->Context.LocalWorld != World || R.Capture->Context.LocalState != State ||
            R.Capture->Context.StateGuid != Guid || R.Capture->Context.Day != Day)) R.Capture.reset();
        const auto Image = R.Capture ? R.Capture->Image : Sample{};
        R.Stable = R.LocalStability.Update(Image, Now);
        R.HostMonitor.Invalidate(R.Stable);
        R.View.Comparable &= R.Stable; R.View.Different &= R.Stable; R.View.Alerts &= R.Stable;
    }
    if (!R.Capture)
    {
        R.View = {}; R.LastReply = 0;
        R.Publish("Desync: lokale Messung unvollstaendig; kein Gleichheitsnachweis", Now);
        return;
    }
    for (const auto& Message : Messages)
    {
        const auto& E = Message.Value;
        if (Message.Peer != Peer || (E.Type != Probe && E.Type != Result)) continue;
        const auto P = Decode(E.Payload);
        if (!P || P->StateGuid != Context->StateGuid) continue;
        if (Role == Game::NetRole::Host)
        {
            if (E.Type != Probe || P->Ignored || P->Alerts || E.Run != P->ClientRun || E.Epoch != P->Nonce || E.Revision != P->Sequence ||
                (R.RemoteRun && R.RemoteRun != P->ClientRun) || P->Nonce < R.RemoteNonce ||
                (P->Nonce == R.RemoteNonce && P->Sequence <= R.RemoteSequence)) continue;
            // Proben nicht schneller als alle zwei Sekunden werten; Duplikate/Spam bestaetigen keinen Alarm.
            if (R.LastReply && Now - R.LastReply < IntervalMs - 250) continue;
            if (P->Nonce != R.RemoteNonce || P->Sequence != R.RemoteSequence + 1 ||
                (R.LastReply && Now - R.LastReply > FreshMs)) R.HostMonitor = {};
            R.RemoteRun = P->ClientRun; R.RemoteNonce = P->Nonce; R.RemoteSequence = P->Sequence;
            Packet Reply{P->ClientRun, P->Nonce, P->Sequence, Context->StateGuid, R.Capture->Image, R.Stable, Ignored};
            R.View = R.HostMonitor.Compare(Reply, *P, Ignored);
            Reply.Alerts = R.View.Alerts;
            const auto Payload = Encode(Reply);
            if (!Payload.empty()) Transport->SendControl(Peer, {R.Run, R.Nonce, ++R.ReplyRevision, Result, Payload});
            R.LastReply = Now;
            if (R.View.Alerts != R.LastAlerts)
            {
                for (unsigned i = 0; i < Count; ++i)
                    if (R.View.Alerts & Bit(i))
                        Core::Log::Warn("Desync: {} host={:016x} client={:016x}", Game::Desync::Names[i], Reply.Image.Values[i], P->Image.Values[i]);
                R.LastAlerts = R.View.Alerts;
            }
        }
        else if (E.Type == Result && R.Gate.Accept(Peer, E, *P, Now))
        {
            HostExclusions = P->Ignored; // Ausnahme-Policy des Hosts ist auf beiden Seiten sichtbar.
            R.View = {};
            R.View.Comparable = P->Stable & R.Sent.Stable & R.Stable & static_cast<uint8_t>(~HostExclusions) & All;
            for (unsigned i = 0; i < Count; ++i)
            {
                if (R.Capture->Image.Values[i] != R.Sent.Image.Values[i]) R.View.Comparable &= static_cast<uint8_t>(~Bit(i));
                if ((R.View.Comparable & Bit(i)) && P->Image.Values[i] != R.Sent.Image.Values[i]) R.View.Different |= Bit(i);
            }
            R.View.Alerts = P->Alerts & R.View.Different;
            R.LastReply = Now;
        }
    }
    if (Role == Game::NetRole::Client && Now >= R.NextProbe && !R.Gate.Waiting(Now))
    {
        R.NextProbe = Now + IntervalMs;
        Packet P{R.Run, R.Nonce, ++R.Sequence, Context->StateGuid, R.Capture->Image, R.Stable};
        if (Transport->SendControl(Peer, {R.Run, R.Nonce, R.Sequence, Probe, Encode(P)}))
        {
            R.Sent = P;
            R.Gate.Sent(R.Sequence, Now);
        }
    }
    R.Describe(Role == Game::NetRole::Host ? Ignored : HostExclusions, Now);
}
}
