#include "Net/Session.h"
#include "Net/Net.h"

#include "Core/Log.h"

namespace Net
{
std::string_view ToString(SessionPhase Phase)
{
    switch (Phase)
    {
    case SessionPhase::Idle:
        return "keine Sitzung";
    case SessionPhase::CreatingLobby:
        return "erstellt Lobby";
    case SessionPhase::Hosting:
        return "Lobby offen";
    case SessionPhase::JoiningLobby:
        return "tritt Lobby bei";
    case SessionPhase::ConnectingTransport:
        return "verbindet";
    case SessionPhase::Playing:
        return "verbunden";
    case SessionPhase::Failed:
        return "fehlgeschlagen";
    }
    return "unbekannt";
}

Session::Session(std::shared_ptr<SteamLobbyApi> ApiIn, TransportFactory MakeTransportIn, SessionHooks HooksIn,
                 TransportOptions OptionsIn, std::string ModVersionIn)
    : Api(std::move(ApiIn)), MakeTransport(std::move(MakeTransportIn)), Hooks(std::move(HooksIn)), Options(OptionsIn),
      ModVersion(std::move(ModVersionIn))
{
}

Session::~Session()
{
    if (Tunnel)
        Tunnel->Stop();
    CloseLobby();
}

bool Session::Create()
{
    if (!Current.CanCreate)
        return false;
    PendingCall = Api->CreateFriendsLobby(kMaxMembers);
    Current = {};
    Current.IsHost = true;
    if (!PendingCall)
    {
        Fail("Steam hat das Erstellen der Lobby abgelehnt");
        return false;
    }
    Current.Phase = SessionPhase::CreatingLobby;
    Current.Message = "Lobby wird erstellt";
    StepSince = Clock::now();
    HostNotice.clear();
    Core::Log::Info("Sitzung: Lobby wird erstellt");
    Tick();
    return true;
}

bool Session::Join(uint64_t Lobby)
{
    if (!Current.CanJoin || !Lobby)
        return false;
    PendingCall = Api->JoinLobby(Lobby);
    Current = {};
    Current.Lobby = Lobby;
    if (!PendingCall)
    {
        Fail("Steam hat den Beitritt abgelehnt");
        return false;
    }
    Current.Phase = SessionPhase::JoiningLobby;
    Current.Message = "Tritt der Lobby bei";
    StepSince = Clock::now();
    Core::Log::Info("Sitzung: tritt Lobby {} bei", Lobby);
    Tick();
    return true;
}

bool Session::Invite()
{
    if (!Current.CanInvite)
        return false;
    return Api->OpenInviteDialog(Current.Lobby);
}

void Session::Leave()
{
    if (Current.Phase == SessionPhase::Idle)
        return;
    if (Tunnel)
        Tunnel->Stop();
    Tunnel.reset();
    if (GameConnected && !Current.IsHost && Hooks.Disconnect)
        Hooks.Disconnect();
    GameConnected = false;
    GameSeen = false;
    GameMissingSince.reset();
    HostGameSeen = false;
    HostGameMissingSince.reset();
    HostNotice.clear();
    CloseLobby();
    Current = {};
    Current.Message = "Sitzung verlassen";
    Core::Log::Info("Sitzung: verlassen");
}

void Session::CloseLobby()
{
    if (Current.Lobby && Current.Phase != SessionPhase::JoiningLobby && Current.Phase != SessionPhase::CreatingLobby)
        Api->LeaveLobby(Current.Lobby);
    PendingCall = 0;
}

void Session::Fail(std::string Message)
{
    Core::Log::Warn("Sitzung: {}", Message);
    if (Tunnel)
        Tunnel->Stop();
    Tunnel.reset();
    if (GameConnected && !Current.IsHost && Hooks.Disconnect)
        Hooks.Disconnect();
    GameConnected = false;
    GameSeen = false;
    GameMissingSince.reset();
    HostGameSeen = false;
    HostGameMissingSince.reset();
    HostNotice.clear();
    CloseLobby();
    const bool WasHost = Current.IsHost;
    Current = {};
    Current.IsHost = WasHost;
    Current.Phase = SessionPhase::Failed;
    Current.Message = std::move(Message);
}

void Session::StartTransportClient(uint64_t Owner)
{
    Tunnel = MakeTransport ? MakeTransport() : nullptr;
    if (!Tunnel)
    {
        Fail("Steam Networking ist nicht verfügbar");
        return;
    }
    // Zwei Instanzen auf einem PC laufen unter demselben Konto. P2P zu sich selbst lehnt Steam ab, dort hilft 127.0.0.1.
    const bool SameAccount = Owner == Api->OwnSteamId();
    const bool Started = SameAccount ? Tunnel->StartLoopbackClient() : Tunnel->StartClient(Owner);
    if (!Started)
    {
        Fail("Tunnel zum Host nicht gestartet: " + Tunnel->Stats().Message);
        return;
    }
    Core::Log::Info("Sitzung: Lobby betreten, verbinde mit dem Host über {}", SameAccount ? "127.0.0.1" : "P2P");
    UseSessionTransport(Tunnel);
    Current.Phase = SessionPhase::ConnectingTransport;
    Current.Message = "Verbindet mit dem Host";
    StepSince = Clock::now();
}

// Host: Nur wer laut Steam gerade in der eigenen Lobby ist, darf sich per P2P verbinden. Die Liste kommt jeden Tick
// frisch von Steam, verlässt jemand die Lobby, verliert er die Berechtigung.
void Session::SyncLobbyMembers()
{
    if (!Tunnel || !Current.IsHost || !Current.Lobby)
        return;
    Tunnel->SetLobbyMembers(Api->Members(Current.Lobby));
}

bool Session::Expired(std::chrono::milliseconds Limit) const
{
    return Clock::now() - StepSince >= Limit;
}

void Session::WatchGame()
{
    if (!Hooks.GameConnectedToHost)
        return;
    const std::optional<bool> Connected = Hooks.GameConnectedToHost();
    if (Connected == true)
    {
        if (!GameSeen)
            Core::Log::Info("Sitzung: Spiel ist mit dem Host verbunden");
        GameSeen = true;
        GameMissingSince.reset();
        return;
    }
    if (!GameSeen)
    {
        // Nach open lädt das Spiel erst die Welt des Hosts. Kommt es nie an, bleibt sonst eine Sitzung ohne Spiel.
        if (Expired(Timeouts.GameJoin))
            Fail("Das Spiel konnte sich nicht mit dem Host verbinden");
        return;
    }
    // Unbekannt, etwa während eine Karte lädt, zählt nicht als getrennt.
    if (Connected != false)
        return;
    const auto Now = Clock::now();
    if (!GameMissingSince)
    {
        GameMissingSince = Now;
        Core::Log::Info("Sitzung: Spiel läuft ohne Host, warte bis zu {} s", Timeouts.GameLost.count() / 1000);
        return;
    }
    if (Now - *GameMissingSince >= Timeouts.GameLost)
        Fail("Das Spiel hat die Verbindung zum Host verloren");
}

// Host: Eine Lobby ohne Server lässt Beitritte ins Leere laufen. Steht das Spiel im Hauptmenü, endet die Sitzung, der Tunnel
// sagt dem Client dabei „Host hat die Sitzung verlassen“.
void Session::WatchHostGame()
{
    if (!Hooks.GameHosting)
        return;
    const std::optional<bool> Hosting = Hooks.GameHosting();
    if (Hosting == true)
    {
        HostGameSeen = true;
        HostGameMissingSince.reset();
        return;
    }
    if (!HostGameSeen)
    {
        // Nach dem Erstellen lädt das Spiel erst die MetaBase als Listen-Server.
        if (Expired(Timeouts.HostListen))
            Fail("Lobby geschlossen: Das Spiel wurde kein Listen-Server");
        return;
    }
    if (Hosting != false)
        return;
    const auto Now = Clock::now();
    if (!HostGameMissingSince)
    {
        HostGameMissingSince = Now;
        return;
    }
    if (Now - *HostGameMissingSince >= Timeouts.HostGameLost)
        Fail("Lobby geschlossen: Das Spiel ist im Hauptmenü. Neue Lobby in der MetaBase erstellen");
}

void Session::Tick()
{
    for (const uint64_t Requested : Api->TakeJoinRequests())
    {
        if (Requested == Current.Lobby && Current.Phase != SessionPhase::Failed)
            continue;
        if (Current.Phase != SessionPhase::Idle && Current.Phase != SessionPhase::Failed)
        {
            // Erst verlassen, dann der Einladung folgen: So landet niemand in zwei Sitzungen.
            Core::Log::Info("Sitzung: Einladung angenommen, verlasse die laufende Sitzung");
            Leave();
        }
        Core::Log::Info("Sitzung: Einladung über Steam angenommen");
        Join(Requested);
    }

    switch (Current.Phase)
    {
    case SessionPhase::CreatingLobby:
        if (const auto Created = Api->PollCreated(PendingCall))
        {
            PendingCall = 0;
            if (!Created->Ok)
            {
                Fail("Steam konnte keine Lobby erstellen (Fehler " + std::to_string(Created->Result) + ")");
                break;
            }
            Current.Lobby = Created->Lobby;
            Current.Phase = SessionPhase::Hosting;
            Api->SetLobbyData(Created->Lobby, std::string(kModKey), ModVersion);
            // Die Lobby-ID lebt nur so lange wie die Lobby und gehört zu keinem Konto. Sie steht im Log, damit man
            // ohne Steam-Overlay von Hand beitreten kann.
            Core::Log::Info("Sitzung: Lobby {} erstellt", Created->Lobby);
            Tunnel = MakeTransport ? MakeTransport() : nullptr;
            if (!Tunnel || !Tunnel->StartHost())
            {
                Fail("Tunnel für Clients nicht gestartet");
                break;
            }
            UseSessionTransport(Tunnel);
            SyncLobbyMembers();
            StepSince = Clock::now();
            if (Hooks.BecomeListenServer)
                Hooks.BecomeListenServer();
            Current.Message = "Lobby offen, Freunde über Einladen holen";
        }
        else if (Expired(Timeouts.Lobby))
            Fail("Zeitlimit: Steam hat die Lobby nicht erstellt");
        break;
    case SessionPhase::JoiningLobby:
        if (const auto Entered = Api->PollEntered(PendingCall))
        {
            PendingCall = 0;
            if (!Entered->Ok)
            {
                // 2: k_EChatRoomEnterResponseDoesntExist, die Lobby ist zu.
                Fail(Entered->Response == 2 ? "Die Lobby gibt es nicht mehr, der Host hat sie geschlossen"
                                            : "Beitritt abgelehnt (Antwort " + std::to_string(Entered->Response) + ")");
                break;
            }
            // Ab hier ist der Spieler Mitglied. Fail verlässt die Lobby wieder.
            Current.Phase = SessionPhase::ConnectingTransport;
            const std::string Version = Api->LobbyData(Current.Lobby, std::string(kModKey));
            if (Version != ModVersion)
            {
                Fail(Version.empty() ? "Die Lobby gehört nicht zur Coop-Mod" : "Andere Mod-Version beim Host");
                break;
            }
            const uint64_t Owner = Api->LobbyOwner(Current.Lobby);
            if (!Owner)
            {
                Fail("Host der Lobby unbekannt");
                break;
            }
            StartTransportClient(Owner);
        }
        else if (Expired(Timeouts.Lobby))
        {
            // Kommt die Antwort doch noch, wäre der Spieler sonst Mitglied ohne Sitzung.
            Api->LeaveLobby(Current.Lobby);
            Fail("Zeitlimit: Steam hat den Beitritt nicht bestätigt");
        }
        break;
    case SessionPhase::ConnectingTransport:
    case SessionPhase::Playing:
    case SessionPhase::Hosting:
        if (Tunnel)
        {
            if (Current.IsHost)
                SyncLobbyMembers();
            const TransportStats Stats = Tunnel->Stats();
            Current.PingMs = Stats.PingMs;
            Current.Route = Stats.Route;
            if (!Current.IsHost && Stats.State == TransportState::Failed)
            {
                Fail(Stats.Message.empty() ? "Verbindung zum Host verloren" : Stats.Message);
                break;
            }
            if (!Current.IsHost && Stats.State == TransportState::Connected && !GameConnected)
            {
                GameConnected = true;
                StepSince = Clock::now();
                if (Hooks.ConnectToLocalPort)
                    Hooks.ConnectToLocalPort(Options.ClientPort);
                Current.Phase = SessionPhase::Playing;
                Current.Message = "Verbunden mit dem Host";
                Core::Log::Info("Sitzung: Tunnel steht, Spiel verbindet sich");
            }
            if (!Current.IsHost && !GameConnected && Expired(Timeouts.Connect))
            {
                Fail("Zeitlimit: keine Verbindung zum Host");
                break;
            }
            if (!Current.IsHost && GameConnected)
            {
                WatchGame();
                if (Current.Phase == SessionPhase::Failed)
                    break;
            }
            if (Current.IsHost)
            {
                WatchHostGame();
                if (Current.Phase == SessionPhase::Failed)
                    break;
                const bool Clients = Stats.Connections > 0;
                // Ging der Mitspieler, sagt der Host warum, bis ein neuer kommt.
                if (Current.Phase == SessionPhase::Playing && !Clients)
                {
                    HostNotice = Stats.Message;
                    Core::Log::Info("Sitzung: {}", HostNotice);
                }
                if (Clients)
                    HostNotice.clear();
                Current.Phase = Clients ? SessionPhase::Playing : SessionPhase::Hosting;
                Current.Message = Clients              ? "Mitspieler verbunden"
                                  : HostNotice.empty() ? "Lobby offen, Freunde über Einladen holen"
                                                       : HostNotice + ". Lobby offen, Freunde über Einladen holen";
            }
        }
        if (Current.Lobby)
            Current.Members = Api->MemberCount(Current.Lobby);
        break;
    case SessionPhase::Idle:
    case SessionPhase::Failed:
        break;
    }

    const bool Free = Current.Phase == SessionPhase::Idle || Current.Phase == SessionPhase::Failed;
    Current.CanCreate = Free;
    Current.CanJoin = Free;
    Current.CanInvite = !Free && Current.Lobby && Current.Phase != SessionPhase::JoiningLobby &&
                        Current.Phase != SessionPhase::CreatingLobby;
    Current.CanLeave = !Free;
}

SessionStatus Session::Status() const
{
    return Current;
}
}
