#include "Net/Net.h"

#include <windows.h>

#include <mutex>
#include <thread>

#include "Core/Log.h"

namespace Net
{
namespace
{
std::mutex Lock;
std::shared_ptr<Transport> Active;

// Das Spiel initialisiert Steam erst nach dem Laden der Mod. So lange wartet der Transport darauf.
constexpr int kSteamWaitSeconds = 120;
constexpr int kClientAttempts = 24;

void StartTransport(TransportRequest Request)
{
    std::shared_ptr<SteamSockets> Sockets;
    std::string Reason;
    for (int Second = 0; Second < kSteamWaitSeconds && !Sockets; ++Second)
    {
        Sockets = LoadSteamSockets(&Reason);
        if (!Sockets)
            Sleep(1000);
    }
    if (!Sockets)
    {
        Core::Log::Error("Transport: Steam nicht verfügbar, {}", Reason);
        return;
    }
    auto Tunnel = std::make_shared<Transport>(Sockets);
    if (Request.Role == TransportRequest::Kind::Host)
    {
        if (!Tunnel->StartHost())
        {
            Core::Log::Error("Transport: Start fehlgeschlagen, {}", Tunnel->Stats().Message);
            return;
        }
        std::lock_guard Guard(Lock);
        Active = std::move(Tunnel);
        return;
    }
    {
        std::lock_guard Guard(Lock);
        Active = Tunnel;
    }
    // Beide Seiten starten im Test gleichzeitig. Der Client versucht es weiter, bis der Host lauscht.
    for (int Attempt = 1; Attempt <= kClientAttempts; ++Attempt)
    {
        const bool Started = Request.Role == TransportRequest::Kind::Loopback ? Tunnel->StartLoopbackClient()
                                                                                : Tunnel->StartClient(Request.HostSteamId);
        if (Started)
        {
            for (int Wait = 0; Wait < 300 && Tunnel->Stats().State == TransportState::Connecting; ++Wait)
                Sleep(100);
            if (Tunnel->Stats().State == TransportState::Connected)
                return;
        }
        Core::Log::Warn("Transport: Versuch {} von {} ohne Verbindung, {}", Attempt, kClientAttempts, Tunnel->Stats().Message);
        Tunnel->Stop();
        Sleep(5000);
    }
    Core::Log::Error("Transport: keine Verbindung zum Host");
}
}

void Start()
{
    const auto Request = ParseTransportRequest(GetCommandLineW());
    if (!Request)
        return;
    Core::Log::Info("Transport: Testschalter in der Kommandozeile, Rolle {}",
                    Request->Role == TransportRequest::Kind::Host       ? "Host"
                    : Request->Role == TransportRequest::Kind::Loopback ? "Client über 127.0.0.1"
                                                                        : "Client über P2P");
    std::thread(StartTransport, *Request).detach();
}

std::shared_ptr<Transport> CurrentTransport()
{
    std::lock_guard Guard(Lock);
    return Active;
}
}
