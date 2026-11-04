#pragma once

#include <map>
#include <memory>
#include <mutex>

#include "Net/SteamSockets.h"

// Nachgebautes Steam Networking für Tests: Mehrere Konten in einem Prozess, Verbindungen und Nachrichten nur im Speicher.
class FakeSteamNetwork : public std::enable_shared_from_this<FakeSteamNetwork>
{
public:
    std::shared_ptr<Net::SteamSockets> Account(uint64_t SteamId);

    // Schaltet für ein Konto das Annehmen ab oder trennt alle Verbindungen eines Kontos von der Gegenseite aus.
    void RejectConnectionsTo(uint64_t SteamId) { std::lock_guard Guard(Lock); Rejecting[SteamId] = true; }
    void DropPeerOf(uint64_t SteamId);

    struct Endpoint;
    std::mutex Lock;
    std::map<uint64_t, Endpoint*> Accounts;
    std::map<uint64_t, bool> Rejecting;
    Net::SteamConnection NextHandle = 1;
};
