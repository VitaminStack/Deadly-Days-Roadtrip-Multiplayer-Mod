#include "Net/Session.h"

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
    Current.Phase = SessionPhase::ConnectingTransport;
    Current.Message = "Verbindet mit dem Host";
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
            if (Hooks.BecomeListenServer)
                Hooks.BecomeListenServer();
            Current.Message = "Lobby offen, Freunde über Einladen holen";
        }
        break;
    case SessionPhase::JoiningLobby:
        if (const auto Entered = Api->PollEntered(PendingCall))
        {
            PendingCall = 0;
            if (!Entered->Ok)
            {
                Fail("Beitritt abgelehnt (Antwort " + std::to_string(Entered->Response) + ")");
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
        break;
    case SessionPhase::ConnectingTransport:
    case SessionPhase::Playing:
    case SessionPhase::Hosting:
        if (Tunnel)
        {
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
                if (Hooks.ConnectToLocalPort)
                    Hooks.ConnectToLocalPort(Options.ClientPort);
                Current.Phase = SessionPhase::Playing;
                Current.Message = "Verbunden mit dem Host";
                Core::Log::Info("Sitzung: Tunnel steht, Spiel verbindet sich");
            }
            if (Current.IsHost)
            {
                const bool Clients = Stats.Connections > 0;
                Current.Phase = Clients ? SessionPhase::Playing : SessionPhase::Hosting;
                Current.Message = Clients ? "Mitspieler verbunden" : "Lobby offen, Freunde über Einladen holen";
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
