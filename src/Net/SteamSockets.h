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

struct SteamStatusChange
{
    SteamConnection Connection = 0;
    // Gesetzt, wenn die Verbindung an einem eigenen Listen-Socket ankommt.
    SteamListenSocket ListenSocket = 0;
    SteamConnectionState State = SteamConnectionState::None;
    SteamConnectionState OldState = SteamConnectionState::None;
    int32_t EndReason = 0;
    std::string EndDebug;
};

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
    virtual void Close(SteamConnection Connection) = 0;
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
