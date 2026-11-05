#include "Net/Net.h"

#include <windows.h>

#include <mutex>
#include <thread>

#include "Core/Dispatcher.h"
#include "Core/Log.h"

namespace Net
{
namespace
{
std::mutex Lock;
std::shared_ptr<Transport> Active;
std::weak_ptr<Transport> SessionTransport;

// Das Spiel initialisiert Steam erst nach dem Laden der Mod. So lange wartet der Transport darauf.
constexpr int kSteamWaitSeconds = 120;
constexpr int kClientAttempts = 24;

void StartTransport(TransportRequest Request)
{
    // Die Zugriffsfunktionen der Steam-Laufzeit liefern die Schnittstelle nur auf dem Thread, der Steam initialisiert hat.
    // Von einem eigenen Thread kam im Spiel nullptr, im Spike auf dem Game Thread nicht. Danach gilt sie überall.
    struct Result
    {
        std::mutex Lock;
        std::shared_ptr<SteamSockets> Sockets;
        std::string Reason = "Game Thread noch nicht bereit";
        bool Done = false;
    };
    std::shared_ptr<SteamSockets> Sockets;
    std::string Reason;
    for (int Second = 0; Second < kSteamWaitSeconds && !Sockets; ++Second)
    {
        auto Shared = std::make_shared<Result>();
        Core::GameThread().Post([Shared] {
            std::string Why;
            auto Loaded = LoadSteamSockets(&Why);
            std::lock_guard Guard(Shared->Lock);
            Shared->Sockets = std::move(Loaded);
            Shared->Reason = std::move(Why);
            Shared->Done = true;
        });
        Sleep(1000);
        std::lock_guard Guard(Shared->Lock);
        Sockets = Shared->Sockets;
        Reason = Shared->Reason;
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
    if (auto Session = SessionTransport.lock(); Session && Session->Running())
        return Session;
    return Active && Active->Running() ? Active : nullptr;
}

void UseSessionTransport(const std::shared_ptr<Transport>& Value)
{
    std::lock_guard Guard(Lock);
    SessionTransport = Value;
}
}
