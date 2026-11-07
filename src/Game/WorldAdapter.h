#pragma once

#include <optional>
#include <string>
#include <string_view>

// Muster-Adapter: liest die aktuelle Welt. Der Header kommt ohne SDK-Typen aus, damit Sync und Tests ihn nutzen können.
namespace Game
{
enum class NetRole
{
    Standalone,
    Host,
    Client
};

struct WorldInfo
{
    std::string Map;
    NetRole Role = NetRole::Standalone;
    int Players = 0;
    // Nur für den Testkanal: Paketname (etwa /Game/Maps/DDDynamic_Map) und Zahl der verbundenen Clients des Hosts.
    std::string Package;
    int Clients = 0;

    bool operator==(const WorldInfo&) const = default;
};

std::string_view ToString(NetRole Role);
std::string Describe(const WorldInfo& Info);

// Rolle aus dem NetDriver der Welt: ohne Treiber allein, mit Verbindung zum Server Client, sonst Host.
NetRole RoleFrom(bool HasNetDriver, bool HasServerConnection);

// Liest die aktuelle Welt, nur auf dem Game Thread. nullopt, solange keine Welt geladen oder lesbar ist.
std::optional<WorldInfo> ReadWorld();

// Game-Thread-Tick: protokolliert jede Änderung von Karte, Rolle oder Spielerzahl.
void TickWorld();
}
