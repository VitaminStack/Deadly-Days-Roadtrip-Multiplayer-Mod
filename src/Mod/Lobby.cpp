#include "Mod/Lobby.h"

#include <windows.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "Core/Log.h"
#include "Game/ConsoleAdapter.h"
#include "Game/GameThread.h"
#include "Game/WorldAdapter.h"
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
    Result.CanCreate = Status.CanCreate;
    Result.CanJoin = Status.CanJoin;
    Result.CanInvite = Status.CanInvite;
    Result.CanLeave = Status.CanLeave;
    return Result;
}

void OnAction(UI::LobbyRequest Request)
{
    auto& Lobby = State();
    if (!Lobby.Session)
        return;
    switch (Request.Action)
    {
    case UI::LobbyAction::Create: Lobby.Session->Create(); break;
    case UI::LobbyAction::Join: Lobby.Session->Join(Request.LobbyId); break;
    case UI::LobbyAction::Invite:
        if (!Lobby.Session->Invite())
            Core::Log::Warn("Lobby: Einladen nicht möglich, Steam-Overlay aus?");
        break;
    case UI::LobbyAction::Leave: Lobby.Session->Leave(); break;
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
    Lobby.Session = std::make_unique<Net::Session>(
        Api, [Sockets, Options] { return std::make_shared<Net::Transport>(Sockets, Options); },
        Net::SessionHooks{BecomeListenServer, ConnectToLocalPort, Disconnect, GameConnectedToHost, GameHosting}, Options);
    if (const uint64_t Invited = Net::ParseConnectLobby(Api->LaunchCommandLine()))
        Lobby.StartJoin = Invited;
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
        if (Lobby.StartCreate)
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
        Lobby.StartJoin = Invited;
    UI::OverlayStatus Waiting;
    Waiting.Message = "Wartet auf Steam";
    UI::Overlay().Publish(Waiting);
    Game::GameThread::AddTick(Tick);
}
}
