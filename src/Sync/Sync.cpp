#include "Sync/Sync.h"
#include "Core/Log.h"
#include "Sync/ControlDispatch.h"
#include "Sync/DevState.h"
#include "Sync/Enemies/DamageNumberState.h"
#include "Sync/MovementState.h"
#include "Sync/Enemies/TelegraphState.h"
#include "Game/GameThread.h"
#include "Game/PatchRuntime.h"
#include "Net/Net.h"
#include <chrono>
#include <span>

namespace Sync
{
void ConfigureDesync(const Core::Config& Config, DiagnosticSink Diagnostics);

namespace
{
std::function<uint32_t()> DevSource;

// Dev-Tab: Der Host fuehrt eigene Knoepfe direkt aus, Anfragen des Clients nach Eingang fuer dessen Figur.
void TickDev(Net::Transport* Transport)
{
    const uint32_t Pressed = DevSource ? DevSource() : 0;
    if (Transport && Transport->Stats().IsHost)
    {
        for (const auto& Message : Transport->TakeControlMessages(Dev::Request))
            if (const auto Request = Dev::Decode(Message.Value.Payload))
                Game::RunDevAction(Request->Action, 1, Request->Target);
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
                Game::ReceiveDashInput(*Value);
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
        static uint64_t Revision = 0;
        if (Payload.empty())
            Game::NoteDamageNumbers(Note::EncodeFailed, Hits.size());
        else if (Peers.empty())
            Game::NoteDamageNumbers(Note::NoPeer, Hits.size());
        else
            for (const uint64_t Peer : Peers)
            {
                // Die Revision zählt nur angenommene Nachrichten, damit der Client an Lücken echte Verluste erkennt.
                const bool Ok = Transport.SendControl(Peer, {1, 1, Revision + 1, DamageNumbers::HitsType, Payload});
                Revision += Ok;
                Game::NoteDamageNumbers(Ok ? Note::Sent : Note::SendFailed, Hits.size());
            }
        return;
    }
    ReceiveDamageNumbers(Transport.TakeControlMessages(DamageNumbers::HitsType), false);
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
        DispatchControls(Messages, Timed("Sync-Fortschritt", TickProgress), Timed("Sync-Desync", TickDesync),
                         Timed("Sync-Belohnung", TickReward), Timed("Sync-Karte", TickMap), Timed("Sync-Pause", TickPause),
                         Timed("Sync-Charakter", TickCharacter));
    }, "Sync");
}
}
