#pragma once

#include <cstdint>
#include <functional>
#include <memory>
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

private:
    void Fail(std::string Message);
    void StartTransportClient(uint64_t Owner);
    void CloseLobby();

    std::shared_ptr<SteamLobbyApi> Api;
    TransportFactory MakeTransport;
    SessionHooks Hooks;
    TransportOptions Options;
    std::string ModVersion;

    SessionStatus Current;
    SteamApiCall PendingCall = 0;
    std::shared_ptr<Transport> Tunnel;
    bool GameConnected = false;
};
}
