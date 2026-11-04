#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "Net/SteamSockets.h"

// Transport-Tunnel: Der IpNetDriver des Spiels spricht weiter UDP auf 127.0.0.1, der Tunnel leitet jedes Paket
// unverändert über Steam Networking weiter. Keine Portfreigabe, Steam vermittelt und nutzt notfalls Relays.
//
//   Client-Spiel -> UDP 127.0.0.1:7778 -> Tunnel -> Steam -> Tunnel -> UDP 127.0.0.1:7777 -> Host-Spiel
//
// Der Tunnel fasst keine Unreal-Objekte an. Ob das Spiel als Listen-Server läuft oder sich mit 127.0.0.1:7778
// verbindet, entscheidet der Aufrufer.
namespace Net
{
struct TransportOptions
{
    // Port des Listen-Servers im Host-Spiel.
    uint16_t GamePort = 7777;
    // Port, mit dem sich das Client-Spiel verbindet: open 127.0.0.1:7778.
    uint16_t ClientPort = 7778;
    // Steam Networking per IP über 127.0.0.1, nur für zwei Instanzen auf einem PC mit einem Konto.
    uint16_t LoopbackPort = 7790;
    int VirtualPort = 0;
    // Abstand der Statistikzeilen im Log.
    unsigned StatsIntervalMs = 30000;
};

enum class TransportState
{
    Stopped,
    // Host wartet auf Clients.
    Listening,
    Connecting,
    Connected,
    // Client: Verbindung abgelehnt oder beendet. Host bleibt bei Listening, wenn ein Client geht.
    Failed,
};

struct TransportStats
{
    TransportState State = TransportState::Stopped;
    bool IsHost = false;
    unsigned Connections = 0;
    int32_t PingMs = -1;
    float Quality = -1.0f;
    uint64_t PacketsToSteam = 0;
    uint64_t PacketsFromSteam = 0;
    uint64_t BytesToSteam = 0;
    uint64_t BytesFromSteam = 0;
    // Pakete des Spiels, bevor die Steam-Verbindung stand.
    uint64_t Dropped = 0;
    std::string Route;
    // Grund für Failed oder letzte Meldung, ohne IDs und Adressen.
    std::string Message;
};

std::string_view ToString(TransportState State);

class Transport
{
public:
    explicit Transport(std::shared_ptr<SteamSockets> Sockets, TransportOptions Options = {});
    ~Transport();
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    // Nimmt Verbindungen per P2P und über 127.0.0.1 an, je Client ein eigener UDP-Socket zum Spiel.
    bool StartHost();
    // Verbindet per P2P zum Konto des Hosts.
    bool StartClient(uint64_t HostSteamId);
    // Verbindet über 127.0.0.1 zum Host auf demselben PC.
    bool StartLoopbackClient();
    // Beendet Verbindungen und Thread, gibt die UDP-Ports frei. Darf mehrfach aufgerufen werden.
    void Stop();

    TransportStats Stats() const;
    bool Running() const { return Active; }

private:
    struct Link;
    enum class Mode
    {
        Host,
        ClientP2P,
        ClientLoopback,
    };

    bool Begin(Mode Role, uint64_t Remote);
    bool Open(Mode Role, uint64_t Remote);
    void Run();
    void HandleChanges();
    void Pump(Link& Entry);
    void Close(uint32_t Connection);
    void Publish(TransportState State, std::string Message);
    void UpdateTotals();

    std::shared_ptr<SteamSockets> Sockets;
    TransportOptions Options;
    std::atomic<bool> Active = false;
    std::thread Worker;
    Mode Role = Mode::Host;
    SteamListenSocket ListenP2P = 0;
    SteamListenSocket ListenLoopback = 0;
    std::unique_ptr<std::vector<Link>> Links;
    uint64_t ClosedPacketsTo = 0, ClosedPacketsFrom = 0, ClosedBytesTo = 0, ClosedBytesFrom = 0, ClosedDropped = 0;

    mutable std::mutex StatsLock;
    TransportStats Current;
};

// Testschalter aus der Kommandozeile des Spiels: -ddcoop_transport=host, =loopback oder =<SteamID64 des Hosts>.
struct TransportRequest
{
    enum class Kind
    {
        Host,
        Loopback,
        Client,
    };
    Kind Role = Kind::Host;
    uint64_t HostSteamId = 0;
};

std::optional<TransportRequest> ParseTransportRequest(std::wstring_view CommandLine);
}
