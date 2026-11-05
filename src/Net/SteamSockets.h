#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

// Schmale Sicht auf ISteamNetworkingSockets. Die echte Umsetzung nutzt die Steam-Laufzeit des Spiels, Tests eine
// nachgebaute. Alle Aufrufe kommen vom Thread des Transports, nur Load darf von woanders kommen.
namespace Net
{
using SteamConnection = uint32_t;
using SteamListenSocket = uint32_t;

// Zustände wie ESteamNetworkingConnectionState.
enum class SteamConnectionState : int32_t
{
    None = 0,
    Connecting = 1,
    FindingRoute = 2,
    Connected = 3,
    ClosedByPeer = 4,
    ProblemDetectedLocally = 5,
};

// Eigene Gründe beim Schließen, Bereich k_ESteamNetConnectionEnd_App_* (1000 bis 1999). Die Gegenseite bekommt sie mit
// ClosedByPeer als EndReason und kann so eine passende Meldung zeigen.
namespace EndReason
{
constexpr int32_t Generic = 1000;
constexpr int32_t HostLeft = 1001;
constexpr int32_t ClientLeft = 1002;
// Build, Mod-Version oder Rolle passen nicht, oder der Handshake kam nicht rechtzeitig.
constexpr int32_t Rejected = 1003;
// Der Host hat schon einen Mitspieler.
constexpr int32_t Busy = 1004;
// Das Konto ist kein Mitglied der Lobby des Hosts, oder Steam hat keine Identität bestätigt.
constexpr int32_t NotAuthorized = 1005;
}

struct SteamStatusChange
{
    SteamConnection Connection = 0;
    // Gesetzt, wenn die Verbindung an einem eigenen Listen-Socket ankommt.
    SteamListenSocket ListenSocket = 0;
    SteamConnectionState State = SteamConnectionState::None;
    SteamConnectionState OldState = SteamConnectionState::None;
    int32_t EndReason = 0;
    std::string EndDebug;
    // Von Steam bestätigte SteamID64 der Gegenseite (SteamNetConnectionInfo::IdentityRemote), 0 ohne gültige
    // Steam-Identität, etwa über 127.0.0.1. Nie eine ID aus Paketen der Gegenseite dafür nehmen.
    uint64_t RemoteSteamId = 0;
};

// Einzelnes Konto im öffentlichen Steam-Universum: Universum 1, Kontotyp 1, Konto-Nummer nicht 0.
bool IsIndividualSteamId(uint64_t SteamId);

struct SteamQuality
{
    int32_t PingMs = -1;
    float Quality = -1.0f;
};

class SteamSockets
{
public:
    virtual ~SteamSockets() = default;

    // SteamID64 des eigenen Kontos, 0 ohne Steam.
    virtual uint64_t OwnSteamId() = 0;
    virtual void InitRelayNetworkAccess() = 0;
    // 0 heißt abgelehnt.
    virtual SteamListenSocket ListenP2P(int VirtualPort) = 0;
    virtual SteamListenSocket ListenLoopback(uint16_t Port) = 0;
    virtual SteamConnection ConnectP2P(uint64_t SteamId, int VirtualPort) = 0;
    virtual SteamConnection ConnectLoopback(uint16_t Port) = 0;
    virtual bool Accept(SteamConnection Connection) = 0;
    // Reason aus EndReason, die Gegenseite sieht ihn in ihrer Statusänderung.
    virtual void Close(SteamConnection Connection, int32_t Reason) = 0;
    virtual void CloseListen(SteamListenSocket Socket) = 0;
    // Unzuverlässig und ohne Nagle: Die Engine sorgt selbst für Zuverlässigkeit.
    virtual bool Send(SteamConnection Connection, std::span<const uint8_t> Data) = 0;
    // Geordnete, zuverlässige Steam-Nachrichten für Handshake und Kontrollzustand.
    virtual bool SendReliable(SteamConnection Connection, std::span<const uint8_t> Data) = 0;
    // Ruft Sink je empfangener Nachricht auf, liefert ihre Zahl. Höchstens Max Nachrichten.
    virtual int Receive(SteamConnection Connection, int Max, const std::function<void(std::span<const uint8_t>)>& Sink) = 0;
    virtual void RunCallbacks() = 0;
    // Statusänderungen seit dem letzten Aufruf, in Reihenfolge.
    virtual std::vector<SteamStatusChange> TakeStatusChanges() = 0;
    virtual SteamQuality Quality(SteamConnection Connection) = 0;
    // Ausführlicher Status von Steam, enthält IP-Adressen. Nie ungefiltert loggen, siehe MaskAddresses.
    virtual std::string DetailedStatus(SteamConnection Connection) = 0;
};

// Steam-Laufzeit des Spiels (steam_api64.dll, flache C-Schnittstelle). nullptr, solange die DLL fehlt oder Steam im
// Spiel noch nicht initialisiert ist; der Grund steht dann in Reason. Höchstens eine echte Instanz je Prozess.
std::shared_ptr<SteamSockets> LoadSteamSockets(std::string* Reason = nullptr);

// Ersetzt IPv4-Adressen durch <ip>, damit keine Adressen in Logs landen. 127.0.0.1 bleibt stehen.
std::string MaskAddresses(const std::string& Text);

// Kurzfassung der Route aus DetailedStatus: "Relay fra" bei Steam-Relays, "ohne Relay" bei anderer Angabe, "unbekannt" ohne Status.
std::string RouteSummary(const std::string& DetailedStatus);
}
