#include "Mod/Lobby.h"

#include <windows.h>

#include <chrono>
#include <format>
#include <memory>
#include <optional>
#include <string>

#include "Core/Log.h"
#include "Game/ConsoleAdapter.h"
#include "Game/DayView.h"
#include "Game/GameThread.h"
#include "Game/Patches.h"
#include "Game/WorldAdapter.h"
#include "Net/CoopMode.h"
#include "Net/Protocol.h"
#include "Net/Session.h"
#include "Net/SteamLobbyApi.h"
#include "Net/SteamSockets.h"
#include "Net/Transport.h"
#include "UI/UI.h"

namespace Mod::Lobby
{
namespace
{
using Clock = std::chrono::steady_clock;

// Das Spiel initialisiert Steam erst nach dem Laden der Mod. Bis dahin jede Sekunde neu versuchen.
constexpr auto kLoadInterval = std::chrono::seconds(1);
constexpr auto kPublishInterval = std::chrono::milliseconds(200);

// Treffpunkt beider Spieler. Der Host lädt ihn als Listen-Server, der Client folgt über den Tunnel.
constexpr wchar_t kListenCommand[] = L"open /Game/Maps/MetaBase?listen";

struct LobbyState
{
    bool Disabled = false;
    std::unique_ptr<Net::Session> Session;
    Clock::time_point NextLoad{};
    Clock::time_point NextPublish{};
    std::string LastReason;
    // Lobby aus +connect_lobby oder dem Testschalter, sobald Steam bereit ist.
    uint64_t StartJoin = 0;
    bool StartCreate = false;
    // Testschalter -ddcoop_lobby_delay=<s>: Erstellen oder Beitreten erst nach dieser Zeit, etwa bis der Spike geladen ist.
    Clock::time_point StartAfter{};
};

LobbyState& State()
{
    static LobbyState Instance;
    return Instance;
}

std::string Narrow(const std::wstring& Text)
{
    std::string Result;
    for (const wchar_t Character : Text)
        Result.push_back(Character < 0x80 ? static_cast<char>(Character) : '?');
    return Result;
}

// Wert eines Schalters -name=wert, leer ohne.
std::string SwitchValue(const std::string& CommandLine, std::string_view Key)
{
    const auto At = CommandLine.find(Key);
    if (At == std::string::npos)
        return {};
    const auto Start = At + Key.size();
    const auto End = CommandLine.find_first_of(" \t\"", Start);
    return CommandLine.substr(Start, End == std::string::npos ? std::string::npos : End - Start);
}

// Testschalter -ddcoop_lobby=create oder -ddcoop_lobby=<Lobby-ID>, dazu -ddcoop_lobby_delay=<s>.
void ParseLobbySwitch(const std::string& CommandLine, LobbyState& Lobby)
{
    const std::string Delay = SwitchValue(CommandLine, "-ddcoop_lobby_delay=");
    if (!Delay.empty() && Delay.size() <= 4 && Delay.find_first_not_of("0123456789") == std::string::npos)
        Lobby.StartAfter = Clock::now() + std::chrono::seconds(std::stoi(Delay));
    const std::string Value = SwitchValue(CommandLine, "-ddcoop_lobby=");
    if (Value.empty())
        return;
    if (Value == "create")
        Lobby.StartCreate = true;
    else
        Lobby.StartJoin = Net::ParseConnectLobby("+connect_lobby " + Value);
}

// Beleg fuer lobby-live-rest: Steam hat das Spiel nach einer angenommenen Einladung gestartet. Ohne Lobby-ID im Log.
void LogInvitedStart(const char* Source)
{
    Core::Log::Info("Lobby: Start über Steam-Einladung (+connect_lobby, {}), Beitritt sobald eine Welt geladen ist", Source);
}

void BecomeListenServer()
{
    const auto World = Game::ReadWorld();
    if (World && World->Role == Game::NetRole::Host)
    {
        Core::Log::Info("Lobby: Spiel ist schon Listen-Server ({})", World->Map);
        return;
    }
    Core::Log::Info("Lobby: Spiel wird Listen-Server in der MetaBase");
    if (!Game::ExecuteConsoleCommand(kListenCommand))
        Core::Log::Error("Lobby: Listen-Server nicht gestartet, Konsole nicht erreichbar");
}

void ConnectToLocalPort(uint16_t Port)
{
    Core::Log::Info("Lobby: Spiel verbindet sich mit dem Tunnel");
    if (!Game::ExecuteConsoleCommand(L"open 127.0.0.1:" + std::to_wstring(Port)))
        Core::Log::Error("Lobby: Beitritt nicht gestartet, Konsole nicht erreichbar");
}

void Disconnect()
{
    Core::Log::Info("Lobby: Spiel trennt die Verbindung zum Host");
    Game::ExecuteConsoleCommand(L"disconnect");
}

// Client: Hat die Welt eine Verbindung zum Server? Ohne lesbare Welt, etwa beim Laden, unbekannt.
std::optional<bool> GameConnectedToHost()
{
    const auto World = Game::ReadWorld();
    if (!World)
        return std::nullopt;
    return World->Role == Game::NetRole::Client;
}

// Host: Listen-Server ja, im Hauptmenü sicher nein. Während Laden und Reise unbekannt, das zählt nicht als geschlossen.
std::optional<bool> GameHosting()
{
    const auto World = Game::ReadWorld();
    if (!World)
        return std::nullopt;
    if (World->Role == Game::NetRole::Host)
        return true;
    if (World->Map == "MainMenu")
        return false;
    return std::nullopt;
}

// coop-modus-wahl: Warum der Koop-Modus gerade nicht wählbar ist, leer wenn er es ist. Fest ist er während einer Sitzung und
// während eines Runs: Wer mitten im Run die Lobby verlässt und neu erstellt, behält den Modus, mit dem der Run begann. Ein Run
// läuft außerhalb von Hauptmenü und MetaBase (wie TickRejoin in Game/RewardPatches.cpp). Ohne lesbare Welt, etwa beim Laden, nicht.
std::string ModeBlocker(const Net::SessionStatus& Status)
{
    if (Status.Phase != Net::SessionPhase::Idle && Status.Phase != Net::SessionPhase::Failed)
        return "Koop-Modus gilt bis zum Ende der Sitzung";
    const auto World = Game::ReadWorld();
    if (!World)
        return "Koop-Modus erst wählbar, wenn die Welt geladen ist";
    if (World->Map != "MainMenu" && World->Map != "MetaBase")
        return "Koop-Modus gilt bis zum Ende des Runs, wählbar im Hauptmenü oder in der MetaBase";
    return {};
}

// Gleicher Weg für Overlay und Testkanal.
bool ChooseMode(Net::CoopMode Mode, std::string* Reason = nullptr)
{
    auto& Lobby = State();
    if (!Lobby.Session)
        return false;
    std::string Blocker = ModeBlocker(Lobby.Session->Status());
    if (Blocker.empty() && !Lobby.Session->SetCoopMode(Mode))
        Blocker = "Koop-Modus gilt bis zum Ende der Sitzung";
    if (!Blocker.empty())
    {
        Core::Log::Warn("Lobby: Koop-Modus {} abgelehnt, {}", Net::ToLobbyValue(Mode), Blocker);
        if (Reason)
            *Reason = Blocker;
        return false;
    }
    return true;
}

UI::OverlayStatus ToOverlay(const Net::SessionStatus& Status)
{
    UI::OverlayStatus Result;
    switch (Status.Phase)
    {
    case Net::SessionPhase::Idle: Result.State = UI::SessionState::Offline; break;
    case Net::SessionPhase::CreatingLobby:
    case Net::SessionPhase::JoiningLobby:
    case Net::SessionPhase::ConnectingTransport: Result.State = UI::SessionState::Connecting; break;
    case Net::SessionPhase::Hosting: Result.State = UI::SessionState::Lobby; break;
    case Net::SessionPhase::Playing: Result.State = UI::SessionState::Playing; break;
    case Net::SessionPhase::Failed: Result.State = UI::SessionState::Failed; break;
    }
    const bool Active = Status.Phase != Net::SessionPhase::Idle && Status.Phase != Net::SessionPhase::Failed;
    Result.Role = !Active ? UI::SessionRole::None : Status.IsHost ? UI::SessionRole::Host : UI::SessionRole::Client;
    Result.Message = Status.Route.empty() ? Status.Message : Status.Message + " (" + Status.Route + ")";
    Result.LobbyId = Status.Lobby;
    Result.Players = Status.Members > 0 ? static_cast<unsigned>(Status.Members) : 0;
    Result.HasPing = Status.PingMs >= 0 && !Status.IsHost;
    Result.PingMs = Result.HasPing ? static_cast<unsigned>(Status.PingMs) : 0;
    // Weiterer Spiel-Build ohne die Kernlisten: kein Coop, siehe Game::Patches::CoopBlocked.
    const bool Blocked = Game::Patches::CoopBlocked();
    Result.CanCreate = Status.CanCreate && !Blocked;
    Result.CanJoin = Status.CanJoin && !Blocked;
    if (Blocked && !Active)
        Result.Message = "Diese Spielversion wird noch nicht unterstützt, Coop gesperrt";
    Result.CanInvite = Status.CanInvite;
    Result.CanLeave = Status.CanLeave;
    Result.Mode = Status.Mode;
    Result.ModeLocked = Status.ModeLocked;
    Result.CanChooseMode = ModeBlocker(Status).empty();
    return Result;
}

void OnAction(UI::LobbyRequest Request)
{
    auto& Lobby = State();
    if (!Lobby.Session)
        return;
    if ((Request.Action == UI::LobbyAction::Create || Request.Action == UI::LobbyAction::Join) && Game::Patches::CoopBlocked())
    {
        Core::Log::Warn("Lobby: Spiel-Build ohne Patches, Erstellen und Beitreten gesperrt");
        return;
    }
    switch (Request.Action)
    {
    case UI::LobbyAction::Create: Lobby.Session->Create(); break;
    case UI::LobbyAction::Join: Lobby.Session->Join(Request.LobbyId); break;
    case UI::LobbyAction::Invite:
        if (!Lobby.Session->Invite())
            Core::Log::Warn("Lobby: Einladen nicht möglich, Steam-Overlay aus?");
        break;
    case UI::LobbyAction::Leave: Lobby.Session->Leave(); break;
    case UI::LobbyAction::SetMode: ChooseMode(Request.Mode); break;
    }
    Lobby.NextPublish = {};
}

// Holt die Steam-Schnittstellen auf dem Game Thread, sobald Steam bereit ist.
bool TryLoad(LobbyState& Lobby)
{
    std::string Reason;
    auto Api = Net::LoadSteamLobbyApi(&Reason);
    std::shared_ptr<Net::SteamSockets> Sockets;
    if (Api)
        Sockets = Net::LoadSteamSockets(&Reason);
    if (!Api || !Sockets)
    {
        if (Reason != Lobby.LastReason)
            Core::Log::Info("Lobby: wartet auf Steam, {}", Reason);
        Lobby.LastReason = Reason;
        return false;
    }
    Sockets->InitRelayNetworkAccess();
    Net::TransportOptions Options;
    Options.Sim = Net::NetSimFromCommandLine(GetCommandLineW());
    // Mehrere Client-Instanzen auf einem PC brauchen je einen eigenen Port für das Spiel (nur Test).
    if (const auto Port = Net::ParseClientPort(GetCommandLineW()))
        Options.ClientPort = *Port;
    // Zwei Instanzen unter einem Konto auf einem PC brauchen 127.0.0.1 ohne Steam-Identität. Nur ausdrücklich im Test.
    Options.AllowLoopback = Narrow(GetCommandLineW()).find("-ddcoop_localtest") != std::string::npos;
    if (Options.AllowLoopback)
        Core::Log::Warn("Sitzung: lokaler Testmodus (-ddcoop_localtest), Host nimmt auch 127.0.0.1 ohne Steam-Identität an");
    Lobby.Session = std::make_unique<Net::Session>(
        Api, [Sockets, Options] { return std::make_shared<Net::Transport>(Sockets, Options); },
        Net::SessionHooks{BecomeListenServer, ConnectToLocalPort, Disconnect, GameConnectedToHost, GameHosting}, Options,
        // Lobby-Daten ddcoop: die echte Mod-Version, damit eine andere Version schon am Beitritt scheitert (seit Mod-Version 23).
        std::to_string(Net::Protocol::ModVersion));
    if (const uint64_t Invited = Net::ParseConnectLobby(Api->LaunchCommandLine()); Invited && !Lobby.StartJoin)
    {
        Lobby.StartJoin = Invited;
        LogInvitedStart("Startparameter von Steam");
    }
    UI::Overlay().Bind(OnAction);
    Core::Log::Info("Lobby: Steam bereit");
    return true;
}

void Tick()
{
    auto& Lobby = State();
    if (Lobby.Disabled)
        return;
    const auto Now = Clock::now();
    if (!Lobby.Session)
    {
        if (Now < Lobby.NextLoad)
            return;
        Lobby.NextLoad = Now + kLoadInterval;
        if (!TryLoad(Lobby))
            return;
    }
    // Ein Start über eine Einladung oder den Testschalter wartet auf eine geladene Welt, vorher gibt es keine Konsole.
    if ((Lobby.StartCreate || Lobby.StartJoin) && Now >= Lobby.StartAfter && Game::ReadWorld())
    {
        if (Game::Patches::CoopBlocked())
            Core::Log::Warn("Lobby: Spiel-Build ohne Patches, Start über Einladung oder Testschalter verworfen");
        else if (Lobby.StartCreate)
            Lobby.Session->Create();
        else
            Lobby.Session->Join(Lobby.StartJoin);
        Lobby.StartCreate = false;
        Lobby.StartJoin = 0;
    }
    Lobby.Session->Tick();
    if (Now >= Lobby.NextPublish)
    {
        Lobby.NextPublish = Now + kPublishInterval;
        UI::Overlay().Publish(ToOverlay(Lobby.Session->Status()));
    }
}
}

namespace
{
const char* RoleName(Game::NetRole Role)
{
    return Role == Game::NetRole::Host ? "host" : Role == Game::NetRole::Client ? "client" : "standalone";
}

// Zustand für state: Welt aus dem Spiel, Sitzung aus Net::Session. Lobby-ID als Text, sie passt nicht in jede JSON-Zahl.
std::string DescribeState()
{
    using Net::Control::JsonString;
    std::string Fields;
    if (const auto World = Game::ReadWorld())
        Fields += std::format("\"world\":{},\"package\":{},\"role\":\"{}\",\"players\":{},\"clients\":{}", JsonString(World->Map),
                              JsonString(World->Package), RoleName(World->Role), World->Players, World->Clients);
    else
        Fields += "\"world\":null";
    const auto& Lobby = State();
    if (!Lobby.Session)
        return Fields + ",\"session\":\"none\"";
    const auto Status = Lobby.Session->Status();
    // Gleicher Zustand wie im Overlay, als feste Kennung für Skripte: unavailable, offline, connecting, lobby, playing, failed.
    constexpr const char* kStates[] = {"unavailable", "offline", "connecting", "lobby", "playing", "failed"};
    const auto Overlay = ToOverlay(Status);
    return Fields + std::format(",\"state\":\"{}\",\"session\":{},\"lobby\":{},\"members\":{},\"ping\":{},\"message\":{},"
                                "\"canCreate\":{},\"canJoin\":{},\"canInvite\":{},\"canLeave\":{},"
                                "\"coopMode\":\"{}\",\"coopModeLocked\":{},\"canChooseMode\":{}",
                                kStates[static_cast<int>(Overlay.State)], JsonString(Net::ToString(Status.Phase)), JsonString(Status.Lobby ? std::to_string(Status.Lobby) : ""),
                                Status.Members, Status.PingMs, JsonString(Status.Message), Status.CanCreate, Status.CanJoin,
                                Status.CanInvite, Status.CanLeave, Net::ToLobbyValue(Status.Mode), Status.ModeLocked,
                                Overlay.CanChooseMode);
}

std::string Done(bool Accepted, const char* Rejected)
{
    return Accepted ? Net::Control::OkReply(DescribeState()) : Net::Control::ErrorReply(Rejected);
}
}

std::string Execute(const Net::Control::Command& Command)
{
    using Net::Control::CommandKind;
    auto& Lobby = State();
    if (Command.Kind == CommandKind::State)
        return Net::Control::OkReply(DescribeState());
    if (Command.Kind == CommandKind::Day)
        return Net::Control::OkReply(Game::DescribeDayView());
    if (Command.Kind == CommandKind::Console || Command.Kind == CommandKind::Travel)
    {
        if (Command.Kind == CommandKind::Travel)
        {
            const auto World = Game::ReadWorld();
            if (!World || World->Role != Game::NetRole::Host)
                return Net::Control::ErrorReply("travel geht nur als Host");
        }
        const std::string Text = Command.Kind == CommandKind::Travel ? Net::Control::TravelConsoleCommand(Command.Text) : Command.Text;
        Core::Log::Info("Testkanal: Konsole '{}'", Text);
        return Done(Game::ExecuteConsoleCommand(std::wstring(Text.begin(), Text.end())), "Konsole oder Welt nicht erreichbar");
    }
    if (!Lobby.Session)
        return Net::Control::ErrorReply("Steam noch nicht bereit");
    // Wie OnAction: danach sofort neu veröffentlichen, damit das Overlay nicht hinterherhinkt.
    Lobby.NextPublish = {};
    if ((Command.Kind == CommandKind::LobbyCreate || Command.Kind == CommandKind::LobbyJoin) && Game::Patches::CoopBlocked())
        return Net::Control::ErrorReply("Spiel-Build ohne Patches, Coop gesperrt");
    switch (Command.Kind)
    {
    case CommandKind::LobbyCreate: return Done(Lobby.Session->Create(), "Erstellen nicht möglich");
    case CommandKind::LobbyJoin: return Done(Lobby.Session->Join(Command.Lobby), "Beitreten nicht möglich");
    case CommandKind::LobbyInvite: return Done(Lobby.Session->Invite(), "Einladen nicht möglich");
    case CommandKind::LobbyLeave: Lobby.Session->Leave(); return Net::Control::OkReply(DescribeState());
    case CommandKind::LobbyMode:
    {
        const auto Mode = Net::ParseCoopMode(Command.Text);
        std::string Reason = "unbekannter Koop-Modus";
        if (!Mode || !ChooseMode(*Mode, &Reason))
            return Net::Control::ErrorReply(Reason);
        return Net::Control::OkReply(DescribeState());
    }
    default: return Net::Control::ErrorReply("unbekannter Befehl");
    }
}

void Install()
{
    auto& Lobby = State();
    const std::wstring CommandLine = GetCommandLineW();
    // Der Testschalter des Transports belegt dieselben Ports und dieselbe Steam-Schnittstelle.
    if (Net::ParseTransportRequest(CommandLine))
    {
        Lobby.Disabled = true;
        Core::Log::Info("Lobby: aus, Transport läuft über den Testschalter");
        return;
    }
    const std::string Narrowed = Narrow(CommandLine);
    ParseLobbySwitch(Narrowed, Lobby);
    if (const uint64_t Invited = Net::ParseConnectLobby(Narrowed))
    {
        Lobby.StartJoin = Invited;
        LogInvitedStart("Kommandozeile");
    }
    UI::OverlayStatus Waiting;
    Waiting.Message = "Wartet auf Steam";
    UI::Overlay().Publish(Waiting);
    Game::GameThread::AddTick(Tick, "Lobby");
}
}
