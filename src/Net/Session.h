#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "Net/SteamLobbyApi.h"
#include "Net/Transport.h"

// Spielsitzung über eine Steam-Lobby: Der Host erstellt eine Lobby nur für Freunde und lädt über das Steam-Overlay ein.
// Wer beitritt, verbindet sich über den Transport mit dem Besitzer der Lobby. Kein SteamID-Austausch von Hand.
//
//   Host:   Create -> Lobby da -> Transport-Host -> Spiel wird Listen-Server -> Invite
//   Client: Join (Einladung, Freundesliste, +connect_lobby oder Lobby-ID) -> Besitzer -> Transport-Client
//           -> verbunden -> Spiel öffnet 127.0.0.1:7778
//
// Fasst keine Unreal-Objekte an, das machen die Hooks. Tick und alle Aufrufe nur auf dem Game Thread.
namespace Net
{
enum class SessionPhase
{
    Idle,
    CreatingLobby,
    Hosting,
    JoiningLobby,
    ConnectingTransport,
    Playing,
    Failed,
};

std::string_view ToString(SessionPhase Phase);

struct SessionStatus
{
    SessionPhase Phase = SessionPhase::Idle;
    bool IsHost = false;
    uint64_t Lobby = 0;
    int Members = 0;
    int32_t PingMs = -1;
    std::string Route;
    // Für Spieler lesbar, ohne IDs und Adressen.
    std::string Message = "Keine Sitzung";
    bool CanCreate = true;
    bool CanJoin = true;
    bool CanInvite = false;
    bool CanLeave = false;
};

// Was das Spiel tun soll. Die Mod verdrahtet sie mit Konsolenbefehlen aus src/Game.
struct SessionHooks
{
    // Host: Spiel als Listen-Server auf TransportOptions::GamePort.
    std::function<void()> BecomeListenServer;
    // Client: mit dem Tunnel auf 127.0.0.1:Port verbinden.
    std::function<void(uint16_t Port)> ConnectToLocalPort;
    // Client: Verbindung zum Host im Spiel trennen.
    std::function<void()> Disconnect;
    // Client: Ist das Spiel mit dem Host verbunden? true verbunden, false läuft ohne Host, nullopt unbekannt (etwa beim
    // Laden einer Karte). Ohne Hook prüft die Sitzung nur den Tunnel.
    std::function<std::optional<bool>()> GameConnectedToHost;
    // Host: Bietet das Spiel noch einen Server an? true Listen-Server, false sicher nicht (Hauptmenü), nullopt unbekannt
    // (Laden, Reise). Ohne Hook bleibt die Lobby offen, bis der Host sie verlässt.
    std::function<std::optional<bool>()> GameHosting;
};

// Zeitlimits der Sitzung. Steam und die Engine haben eigene, diese greifen, wenn von dort nichts mehr kommt.
struct SessionTimeouts
{
    // Steam bestätigt Erstellen oder Beitritt der Lobby nicht.
    std::chrono::milliseconds Lobby{15000};
    // Tunnel und Versions-Handshake zum Host. Steam gibt nach etwa 10 s selbst auf, der Handshake hat 5 s.
    std::chrono::milliseconds Connect{20000};
    // Nach open 127.0.0.1:7778 lädt das Spiel die Welt des Hosts.
    std::chrono::milliseconds GameJoin{90000};
    // Das Spiel läuft ohne Host, obwohl der Tunnel steht, etwa nach einem Abbruch durch die Engine. Großzügig, weil ein
    // Kartenwechsel kurz ohne Verbindung aussehen kann.
    std::chrono::milliseconds GameLost{30000};
    // Host: Das Spiel war Listen-Server und steht jetzt im Hauptmenü. Dann schließt die Sitzung die Lobby.
    std::chrono::milliseconds HostGameLost{2000};
    // Host: Nach dem Erstellen wird das Spiel nicht Listen-Server.
    std::chrono::milliseconds HostListen{60000};
};

// Erzeugt den Transport. Austauschbar für Tests.
using TransportFactory = std::function<std::shared_ptr<Transport>()>;

class Session
{
public:
    // Wert in den Lobby-Daten, damit nur Spieler mit der Mod beitreten.
    static constexpr std::string_view kModKey = "ddcoop";
    static constexpr int kMaxMembers = 4;

    Session(std::shared_ptr<SteamLobbyApi> Api, TransportFactory MakeTransport, SessionHooks Hooks,
            TransportOptions Options = {}, std::string ModVersion = "1");
    ~Session();

    bool Create();
    bool Join(uint64_t Lobby);
    bool Invite();
    void Leave();
    // Fragt Steam und Transport ab, nimmt Einladungen an, stößt die Hooks an.
    void Tick();

    SessionStatus Status() const;
    void SetTimeouts(SessionTimeouts Value) { Timeouts = Value; }

private:
    using Clock = std::chrono::steady_clock;

    void Fail(std::string Message);
    void StartTransportClient(uint64_t Owner);
    void CloseLobby();
    void WatchGame();
    void WatchHostGame();
    bool Expired(std::chrono::milliseconds Limit) const;

    std::shared_ptr<SteamLobbyApi> Api;
    TransportFactory MakeTransport;
    SessionHooks Hooks;
    TransportOptions Options;
    std::string ModVersion;

    SessionStatus Current;
    SteamApiCall PendingCall = 0;
    std::shared_ptr<Transport> Tunnel;
    bool GameConnected = false;
    SessionTimeouts Timeouts;
    // Beginn des aktuellen Schritts für die Zeitlimits.
    Clock::time_point StepSince{};
    // Client: Das Spiel war schon einmal mit dem Host verbunden, und seit wann es ohne Host läuft.
    bool GameSeen = false;
    std::optional<Clock::time_point> GameMissingSince;
    // Host: Das Spiel war schon Listen-Server, und seit wann es im Hauptmenü steht.
    bool HostGameSeen = false;
    std::optional<Clock::time_point> HostGameMissingSince;
    // Host: Grund, warum der letzte Mitspieler gegangen ist, bis ein neuer kommt.
    std::string HostNotice;
};
}
