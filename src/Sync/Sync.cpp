#include "Sync/Sync.h"
#include "Core/Log.h"
#include "Sync/ControlDispatch.h"
#include "Sync/DevState.h"
#include "Sync/Enemies/DamageNumberState.h"
#include "Sync/MovementState.h"
#include "Sync/Enemies/TelegraphState.h"
#include "Sync/Combat/WeaponSoundState.h"
#include "Sync/World/ThreatState.h"
#include "Game/ThreatAdapter.h"
#include "Sync/World/ZoneGoalState.h"
#include "Game/ZoneGoalAdapter.h"
#include "Sync/Players/PowerUpState.h"
#include "Game/PowerUpAdapter.h"
#include "Game/GameThread.h"
#include "Game/PatchRuntime.h"
#include "Game/RemotePlayers.h"
#include "Net/Net.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Sync
{
void ConfigureDesync(const Core::Config& Config, DiagnosticSink Diagnostics);
void TickSessionLog(Net::Transport* Transport);

namespace
{
std::function<uint32_t()> DevSource;

// Host: Game ordnet jedem Peer des Kontrollkanals seinen Controller zu, über den Port seines Tunnel-Links
// (docs/MEHR_SPIELER.md). Jeden Takt vor den Familien, die Liste ändert sich mit jedem Beitritt und jeder Trennung.
void UpdatePeerLinks(Net::Transport* Transport)
{
    std::vector<Game::PeerLink> Links;
    if (Transport && Transport->Stats().IsHost)
        for (const uint64_t Peer : Transport->ControlPeers())
            Links.push_back({Peer, Transport->PeerGamePort(Peer)});
    Game::SetPeerLinks(Links);
}

// Dev-Tab: Der Host fuehrt eigene Knoepfe direkt aus, Anfragen eines Clients nach Eingang fuer dessen Figur.
void TickDev(Net::Transport* Transport)
{
    const uint32_t Pressed = DevSource ? DevSource() : 0;
    if (Transport && Transport->Stats().IsHost)
    {
        for (const auto& Message : Transport->TakeControlMessages(Dev::Request))
            if (const auto Request = Dev::Decode(Message.Value.Payload))
            {
                // Ohne zugeordneten Controller keine Figur raten; Gegner und Tank brauchen keine.
                const uintptr_t Controller = Game::PeerController(Message.Peer);
                if (Controller || Request->Action == Game::DevAction::SpawnZombies || Request->Action == Game::DevAction::FullTank)
                    Game::RunDevAction(Request->Action, Controller, Request->Target);
                else
                    Core::Log::Warn("Dev: Anfrage eines Clients ohne zugeordnete Figur verworfen");
            }
    }
    // Jeden Takt, damit beim Klick im Overlay der letzte Cursorpunkt ueber dem Spielbild bereitliegt.
    const auto Cursor = Game::CursorGroundPosition();
    const auto Role = Game::Patches::CurrentRole();
    for (uint8_t Id = 1; Id <= Game::LastDevAction; ++Id)
    {
        const auto Action = static_cast<Game::DevAction>(Id);
        if (!(Pressed & (1u << Id)))
            continue;
        const auto Target = Action == Game::DevAction::SpawnZombies ? Cursor : std::nullopt;
        if (Role != Game::NetRole::Client)
        {
            Game::RunDevAction(Action, 0, Target);
            continue;
        }
        if (Action == Game::DevAction::SpawnZombies && !Target)
        {
            Core::Log::Warn("Dev: kein Cursorpunkt fuer Spawn Zombies");
            continue;
        }
        if (!Transport)
            continue;
        const auto Peers = Transport->ControlPeers();
        static uint64_t Revision = 0;
        if (Peers.size() == 1)
            Transport->SendControl(Peers[0], {1, 1, ++Revision, Dev::Request, Dev::Encode({Action, Target})});
    }
}

// Jeder Takt statt alle 250 ms: Die Dodge-Richtung muss beim Host ankommen, solange seine Aktivierung wartet.
void TickMovement(Net::Transport& Transport)
{
    const bool Host = Transport.Stats().IsHost;
    if (Host)
    {
        for (const auto& Message : Transport.TakeControlMessages(Movement::DashInput))
            if (const auto Value = Movement::Decode(Message.Value.Payload))
                Game::ReceiveDashInput(Game::PeerController(Message.Peer), *Value);
        return;
    }
    const auto Inputs = Game::TakeDashInputs();
    const auto Peers = Inputs.empty() ? std::vector<uint64_t>{} : Transport.ControlPeers();
    if (Peers.size() != 1)
        return;
    static uint64_t Revision = 0;
    for (const auto& Value : Inputs)
    {
        const auto Payload = Movement::Encode(Value);
        if (!Payload.empty())
            Transport.SendControl(Peers[0], {1, 1, ++Revision, Movement::DashInput, Payload});
    }
}

// opt-schadenszahlen: jeder Takt, damit die Zahl beim Client im selben Moment wie beim Host entsteht. Host schickt die Treffer
// seines Schadenszahl-Handlers, der Client ruft damit seinen eigenen auf. Format in Enemies/DamageNumberState.h.
// Client: Zahlen aus einem Paketfenster. Auch der Sammelabruf alle 250 ms kann eine `0xA00` enthalten, die nach dem eigenen
// Abruf im selben Takt ankam; ohne diesen Weg ginge sie verloren (opt-schadenszahlen-verlust, wie bei den Warnanzeigen).
void ReceiveDamageNumbers(std::span<const Net::ControlMessage> Messages, bool Bundled)
{
    using Note = Game::DamageNumberNote;
    static DamageNumbers::RevisionGaps Gaps;
    for (const auto& Message : Messages)
    {
        if (Message.Value.Type != DamageNumbers::HitsType)
            continue;
        if (const uint64_t Missing = Gaps.See(Message.Value.Revision))
            Game::NoteDamageNumbers(Note::RevisionGap, Missing);
        if (const auto Hits = DamageNumbers::Decode(Message.Value.Payload))
            Game::ShowDamageNumbers(*Hits, Bundled);
        else
            Game::NoteDamageNumbers(Note::DecodeFailed, 1);
    }
    // Ohne Treffer nur die Zählerzeile: So steht auch der Endstand nach dem letzten Treffer im Log.
    if (!Bundled)
        Game::ShowDamageNumbers({}, false);
}

void TickDamageNumbers(Net::Transport& Transport)
{
    using Note = Game::DamageNumberNote;
    if (Transport.Stats().IsHost)
    {
        const auto Hits = Game::TakeDamageNumberHits();
        if (Hits.empty())
            return;
        const auto Payload = DamageNumbers::Encode(Hits);
        const auto Peers = Transport.ControlPeers();
        // Je Client eine eigene Revision: Jeder Client sieht nur seine Nachrichten und erkennt an Lücken echte Verluste.
        static std::map<uint64_t, uint64_t> Revisions;
        std::erase_if(Revisions, [&Peers](const auto& Entry) { return std::find(Peers.begin(), Peers.end(), Entry.first) == Peers.end(); });
        if (Payload.empty())
            Game::NoteDamageNumbers(Note::EncodeFailed, Hits.size());
        else if (Peers.empty())
            Game::NoteDamageNumbers(Note::NoPeer, Hits.size());
        else
            for (const uint64_t Peer : Peers)
            {
                // Die Revision zählt nur angenommene Nachrichten.
                uint64_t& Revision = Revisions[Peer];
                const bool Ok = Transport.SendControl(Peer, {1, 1, Revision + 1, DamageNumbers::HitsType, Payload});
                Revision += Ok;
                Game::NoteDamageNumbers(Ok ? Note::Sent : Note::SendFailed, Hits.size());
            }
        return;
    }
    ReceiveDamageNumbers(Transport.TakeControlMessages(DamageNumbers::HitsType), false);
}

// opt-waffen-sound: jeder Takt, damit der Nachladesound beim Client zum Nachladen passt. Der Host schickt die Aufrufe von
// PlayReloadSound, der Client ruft die Funktion an seinen eigenen Händen auf. Format in Combat/WeaponSoundState.h.
void TickWeaponSounds(Net::Transport& Transport)
{
    if (Transport.Stats().IsHost)
    {
        const auto Sounds = Game::TakeReloadSounds();
        if (Sounds.empty())
            return;
        const auto Payload = WeaponSounds::Encode(Sounds);
        static uint64_t Revision = 0;
        if (!Payload.empty())
            for (const uint64_t Peer : Transport.ControlPeers())
                Transport.SendControl(Peer, {1, 1, ++Revision, WeaponSounds::ReloadType, Payload});
        return;
    }
    for (const auto& Message : Transport.TakeControlMessages(WeaponSounds::ReloadType))
        if (const auto Sounds = WeaponSounds::Decode(Message.Value.Payload))
            Game::PlayReloadSounds(*Sounds);
}

// opt-bedrohung-sound: Die Bedrohungsstufe repliziert nicht. Der Host schickt sie als Stand (bei Änderung und alle 2 s), der
// Client ruft damit das native Ereignis seiner Stoppuhr auf. Format in World/ThreatState.h.
void TickThreat(Net::Transport& Transport)
{
    if (Transport.Stats().IsHost)
    {
        static bool Sent = false;
        static int32_t LastLevel = 0;
        static uint64_t LastMs = 0, Revision = 0;
        const auto Level = Game::ReadHostThreatLevel();
        if (!Level)
        {
            Sent = false; // neue Welt oder keine Mission: den nächsten Stand sofort senden
            return;
        }
        const uint64_t Now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        if (!Game::ThreatSendDue(Sent, LastLevel, *Level, LastMs, Now))
            return;
        const auto Payload = Threat::Encode(*Level);
        bool Any = false;
        if (!Payload.empty())
            for (const uint64_t Peer : Transport.ControlPeers())
                Any |= Transport.SendControl(Peer, {1, 1, ++Revision, Threat::LevelType, Payload});
        // Ohne angenommenen Versand (kein Mitspieler, Warteschlange voll) gilt der Stand als nicht gesendet.
        Sent = Any;
        LastLevel = *Level;
        LastMs = Now;
        return;
    }
    // Nur der neueste Stand zählt.
    std::optional<int32_t> Latest;
    for (const auto& Message : Transport.TakeControlMessages(Threat::LevelType))
        if (const auto Level = Threat::Decode(Message.Value.Payload))
            Latest = Level;
    if (Latest)
        Game::ShowThreatLevel(*Latest);
}

// opt-zonenziel-anzeige: Das Zonenziel (etwa „Töte die Titanen“) legt nur der Host an, es repliziert nicht. Der Host schickt
// den Stand (bei Änderung und alle 2 s), der Client zeigt ihn in der nativen Leiste. Format in World/ZoneGoalState.h.
void TickZoneGoal(Net::Transport& Transport)
{
    if (Transport.Stats().IsHost)
    {
        static Game::ZoneGoalSent Sent;
        static std::vector<uint64_t> Peers;
        static uint64_t Revision = 0;
        const auto View = Game::ReadHostZoneGoal();
        const auto Now = Transport.ControlPeers();
        if (!View || Now != Peers)
        {
            // Neue Welt, keine Welt oder ein neuer Mitspieler: den nächsten Stand sofort senden.
            Sent = {};
            Peers = Now;
        }
        if (!View)
            return;
        const uint64_t NowMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        if (!Game::ZoneGoalSendDue(Sent, *View, NowMs))
            return;
        const auto Payload = ZoneGoal::Encode(*View);
        bool Any = false;
        if (!Payload.empty())
            for (const uint64_t Peer : Now)
                Any |= Transport.SendControl(Peer, {1, 1, ++Revision, ZoneGoal::StateType, Payload});
        Sent = Game::ZoneGoalAfterSend(Sent, *View, NowMs, Any);
        return;
    }
    // Nur der neueste Stand zählt.
    for (const auto& Message : Transport.TakeControlMessages(ZoneGoal::StateType))
        if (const auto View = ZoneGoal::Decode(Message.Value.Payload))
            Game::SetHostZoneGoal(*View);
    Game::TickZoneGoalDisplay();
}

// opt-powerups-client, jeder Takt: Host meldet jedem Client den Slot-Stand seiner Figur (0xE00) und löst die Slots aus, deren
// Auslösen der Client meldet (0xE01). Der Client meldet eigenes Auslösen und gleicht seine Slots an den Stand des Hosts an.
void TickPowerUps(Net::Transport& Transport)
{
    const uint64_t NowMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    static uintptr_t World = 0;
    const uintptr_t NowWorld = Game::PowerUpWorld();
    const bool NewWorld = NowWorld != World;
    World = NowWorld;
    static uint64_t Revision = 0;
    if (Transport.Stats().IsHost)
    {
        static std::map<uint64_t, Game::PowerUp::Sent> Sent;
        static std::map<uint64_t, uint32_t> Acked;
        const auto Peers = Transport.ControlPeers();
        std::erase_if(Acked, [&Peers](const auto& Entry) { return std::find(Peers.begin(), Peers.end(), Entry.first) == Peers.end(); });
        if (NewWorld)
            Sent.clear();
        for (const auto& Message : Transport.TakeControlMessages(PowerUps::ActivateType))
        {
            const auto Value = PowerUps::DecodeActivation(Message.Value.Payload);
            if (!Value)
            {
                Core::Log::Warn("Power-up: ungültige Meldung eines Clients verworfen");
                continue;
            }
            uint32_t& Last = Acked[Message.Peer];
            if (Value->Sequence <= Last)
                continue;
            Last = Value->Sequence;
            if (const uintptr_t Controller = Game::PeerController(Message.Peer))
                Game::ActivatePowerUp(Controller, Value->Definition);
            else
                Core::Log::Warn("Power-up: Meldung eines Clients ohne zugeordnete Figur verworfen");
        }
        for (const uint64_t Peer : Peers)
        {
            const auto Slots = Game::ReadPowerUps(Game::PeerController(Peer));
            if (!Slots)
                continue;
            const uint32_t PeerAcked = Acked.count(Peer) ? Acked[Peer] : 0;
            Game::PowerUp::Sent& Last = Sent[Peer];
            if (!Game::PowerUp::SendDue(Last, *Slots, PeerAcked, NowMs))
                continue;
            const auto Payload = PowerUps::Encode(PowerUps::State{PeerAcked, *Slots});
            if (!Payload.empty() && Transport.SendControl(Peer, {1, 1, ++Revision, PowerUps::StateType, Payload}))
                Last = {true, *Slots, PeerAcked, NowMs};
        }
        return;
    }
    static Game::PowerUp::Requests Open;
    if (NewWorld)
        Open.Clear();
    const auto Activations = Game::TakePowerUpActivations();
    const auto Peers = Activations.empty() ? std::vector<uint64_t>{} : Transport.ControlPeers();
    for (const std::string& Name : Activations)
    {
        const uint32_t Sequence = Peers.size() == 1 ? Open.Add(Name, NowMs) : 0;
        const auto Payload = Sequence ? PowerUps::Encode(PowerUps::Activation{Sequence, Name}) : std::vector<uint8_t>{};
        if (Payload.empty() || !Transport.SendControl(Peers[0], {1, 1, ++Revision, PowerUps::ActivateType, Payload}))
            Core::Log::Warn("Power-up: Auslösen von {} nicht an den Host gemeldet", Name);
    }
    // Nur der neueste Stand zählt.
    std::optional<PowerUps::State> Latest;
    for (const auto& Message : Transport.TakeControlMessages(PowerUps::StateType))
        if (auto Value = PowerUps::DecodeState(Message.Value.Payload))
            Latest = std::move(Value);
    if (!Latest)
        return;
    Open.Acknowledge(Latest->Acked);
    Open.Expire(NowMs);
    Game::ApplyHostPowerUps(Latest->Slots, Open.Pending());
}
}

namespace
{
// opt-gegner-indikatoren: Host meldet je Warnanzeige eines Gegnerangriffs den Auslöser, der Client zeigt sie lokal. Jeder Takt,
// die Anzeige ist zeitkritisch. Ohne Mitspieler werden die Meldungen nur verworfen.
void DropTelegraph(const char* What, int Kind)
{
    static int Dropped = 0;
    if (Dropped < 8 && ++Dropped)
        Core::Log::Warn("Warnanzeigen: {} verworfen (Art {})", What, Kind);
}

// Client: Anzeigen aus einem Paketfenster. Auch der gebündelte Abruf alle 250 ms kann eine `0x900` enthalten, die nach dem
// eigenen Abruf im selben Takt ankam; ohne diesen Weg ginge sie verloren.
void ReceiveTelegraphs(std::span<const Net::ControlMessage> Messages)
{
    for (const auto& Message : Messages)
    {
        if (Message.Value.Type != Telegraph::Type)
            continue;
        if (const auto Value = Telegraph::Decode(Message.Value.Payload))
            Game::ReceiveEnemyTelegraph(*Value);
        else
            DropTelegraph("Nachricht des Hosts", Message.Value.Payload.empty() ? 0 : Message.Value.Payload[0]);
    }
}

void TickTelegraphs(Net::Transport& Transport)
{
    const auto Drop = DropTelegraph;
    if (!Transport.Stats().IsHost)
    {
        ReceiveTelegraphs(Transport.TakeControlMessages(Telegraph::Type));
        return;
    }
    const auto Values = Game::TakeEnemyTelegraphs();
    if (Values.empty())
        return;
    static uint64_t Revision = 0;
    for (const uint64_t Peer : Transport.ControlPeers())
        for (const auto& Value : Values)
        {
            const auto Payload = Telegraph::Encode(Value);
            if (Payload.empty())
                Drop("ungültige Anzeige", static_cast<int>(Value.Kind));
            else if (!Transport.SendControl(Peer, {1, 1, ++Revision, Telegraph::Type, Payload}))
                Drop("Senden", static_cast<int>(Value.Kind));
        }
}
}

void BindDevSource(std::function<uint32_t()> Source)
{
    DevSource = std::move(Source);
}

void Start(const Core::Config& Config, DiagnosticSink Diagnostics)
{
    ConfigureDesync(Config, std::move(Diagnostics));
    Game::GameThread::AddTick([]
    {
        using Clock = std::chrono::steady_clock;
        const auto Transport = Net::CurrentTransport();
        UpdatePeerLinks(Transport.get());
        if (Transport)
        {
            const Game::GameThread::SlowScope Watch("Sync-Bewegung");
            TickMovement(*Transport);
        }
        if (Transport)
        {
            const Game::GameThread::SlowScope Watch("Sync-Warnanzeigen");
            TickTelegraphs(*Transport);
        }
        else
            Game::TakeEnemyTelegraphs();
        if (Transport)
        {
            const Game::GameThread::SlowScope Watch("Sync-Schadenszahlen");
            TickDamageNumbers(*Transport);
        }
        if (Transport)
        {
            const Game::GameThread::SlowScope Watch("Sync-Waffensound");
            TickWeaponSounds(*Transport);
        }
        else
            Game::TakeReloadSounds();
        if (Transport)
        {
            const Game::GameThread::SlowScope Watch("Sync-Bedrohung");
            TickThreat(*Transport);
        }
        if (Transport)
        {
            const Game::GameThread::SlowScope Watch("Sync-Zonenziel");
            TickZoneGoal(*Transport);
        }
        if (Transport)
        {
            const Game::GameThread::SlowScope Watch("Sync-Power-ups");
            TickPowerUps(*Transport);
        }
        else
            Game::ResetZoneGoal();
        {
            const Game::GameThread::SlowScope Watch("Sync-Dev");
            TickDev(Transport.get()); // Also handles offline/standalone dev actions.
        }
        static auto Next = Clock::now();
        if (Clock::now() < Next) return;
        Next = Clock::now() + std::chrono::milliseconds(250);
        const auto Messages = Transport ? Transport->TakeControlMessages() : std::vector<Net::ControlMessage>{};
        const auto Timed = [](const char* Name, auto Family)
        {
            return [Name, Family](std::span<const Net::ControlMessage> Batch)
            {
                const Game::GameThread::SlowScope Watch(Name);
                Family(Batch);
            };
        };
        if (Transport && !Transport->Stats().IsHost)
        {
            ReceiveTelegraphs(Messages);
            const Game::GameThread::SlowScope Watch("Sync-Schadenszahlen");
            ReceiveDamageNumbers(Messages, true);
        }
        {
            const Game::GameThread::SlowScope Watch("Sync-Protokoll");
            TickSessionLog(Transport.get());
        }
        DispatchControls(Messages, Timed("Sync-Fortschritt", TickProgress), Timed("Sync-Desync", TickDesync),
                         Timed("Sync-Belohnung", TickReward), Timed("Sync-Karte", TickMap), Timed("Sync-Routenkarte", TickRoute), Timed("Sync-Pause", TickPause),
                         Timed("Sync-Charakter", TickCharacter));
    }, "Sync");
}
}
