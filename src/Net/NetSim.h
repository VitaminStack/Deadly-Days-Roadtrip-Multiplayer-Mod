#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Netzsimulation nur für Tests auf einem PC: Verzögerung, Schwankung und Verlust im Tunnel der Mod. Unreals
// "Net PktLag" ist im Shipping-Build nicht enthalten. Jede Instanz verzögert ihre ausgehenden Pakete um die halbe
// Ping-Zeit, so ergibt sich mit beiden Seiten der Ping des Profils. Siehe docs/TESTING.md, Abschnitt Latenz.
namespace Net
{
struct NetSimProfile
{
    // Ziel-Ping hin und zurück. Jede Seite verzögert um die Hälfte.
    unsigned PingMs = 0;
    // Schwankung des Pings, ± in ms. Jede Seite schwankt um ± die Hälfte.
    unsigned JitterMs = 0;
    // Verlust je Richtung in Prozent, nur unzuverlässige Spielpakete.
    double LossPercent = 0;

    bool Enabled() const { return PingMs || JitterMs || LossPercent > 0; }
};

// Testschalter -ddcoop_netsim=<Ping>,<Schwankung>,<Verlust>, etwa -ddcoop_netsim=120,30,2. Fehlende Werte zählen 0.
// Ungültige oder unplausible Werte (Ping über 2000 ms, Verlust über 50 %) ergeben nullopt.
std::optional<NetSimProfile> ParseNetSim(std::wstring_view CommandLine);
std::string Describe(const NetSimProfile& Profile);
// Wie ParseNetSim, meldet einen ungültigen Schalter im Log und liefert dann ein leeres Profil.
NetSimProfile NetSimFromCommandLine(std::wstring_view CommandLine);

struct NetSimStats
{
    uint64_t Delayed = 0;
    uint64_t Lost = 0;
    uint64_t DelaySumMs = 0;
    unsigned MinMs = 0;
    unsigned MaxMs = 0;
};
std::string Describe(const NetSimStats& Stats);

// Warteschlange je Verbindung und Richtung, nur vom Netzwerkthread benutzt. Zeiten in ms, vom Aufrufer geliefert.
class NetSimQueue
{
public:
    NetSimQueue(NetSimProfile Profile, uint32_t Seed);

    // Unzuverlässig: kann verloren gehen und andere Pakete überholen. Zuverlässig: nur verzögert, Reihenfolge bleibt.
    // false, wenn das Paket als verloren gilt.
    bool Push(uint64_t NowMs, std::span<const uint8_t> Bytes, bool Reliable);
    // Gibt fällige Pakete in Sendereihenfolge an Send weiter.
    void PopReady(uint64_t NowMs, const std::function<void(std::span<const uint8_t> Bytes, bool Reliable)>& Send);
    size_t Pending() const { return Waiting.size(); }
    // Statistik der Spielpakete seit dem letzten TakeStats.
    NetSimStats TakeStats();

private:
    struct Item
    {
        uint64_t Release;
        uint64_t Order;
        bool Reliable;
        std::vector<uint8_t> Bytes;
        bool operator>(const Item& Other) const
        {
            return Release != Other.Release ? Release > Other.Release : Order > Other.Order;
        }
    };
    NetSimProfile Profile;
    std::mt19937 Random;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> Waiting;
    uint64_t NextOrder = 0;
    uint64_t LastReliable = 0;
    NetSimStats Current;
};
}
