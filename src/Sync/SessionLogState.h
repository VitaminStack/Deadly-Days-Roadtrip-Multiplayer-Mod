#pragma once

#include <cstdint>
#include <format>
#include <optional>
#include <string>

// live-protokoll: Zustandszeile im Log, damit sich Probleme eines Live-Tests im Nachhinein zuordnen lassen. Nur lokal, keine
// Nachricht und kein Format zwischen Host und Client. Beide Seiten schreiben dieselben Felder; der Vergleich der Logs zeigt,
// wo Tank, Schrott oder Zeitanfragen auseinanderlaufen.
namespace Sync::SessionLog
{
// Abstand der Zustandszeile ohne Anlass. Weltwechsel und Marken schreiben sofort.
inline constexpr uint64_t IntervalMs = 60000;

struct State
{
    std::string Role;
    std::string Map;
    int Players = -1;
    std::optional<float> Dilation;
    bool Pauser = false;
    std::optional<int32_t> TimeRequests;
    std::optional<int32_t> Scrap;
    std::optional<float> FuelCurrent, FuelMax;
    int32_t PingMs = -1;
    unsigned Connections = 0;
    uint64_t ControlRefused = 0;
};

namespace Detail
{
template <class T>
std::string Or(const std::optional<T>& Value, const char* Fmt)
{
    return Value ? std::vformat(Fmt, std::make_format_args(*Value)) : std::string("-");
}
}

// Eine Zeile, unbekannte Werte als "-". Schlüssel=Wert, damit Auswertungsskripte sie ohne Kontext zerlegen können.
inline std::string Format(const State& S)
{
    const std::string Fuel = S.FuelCurrent && S.FuelMax ? std::format("{:.0f}/{:.0f}", *S.FuelCurrent, *S.FuelMax) : "-";
    return std::format("Rolle={} Karte={} Spieler={} Zeitfaktor={} Pause={} Zeitanfragen={} Schrott={} Tank={} Ping={} "
                       "Verbindungen={} Kontrolle-abgelehnt={}",
                       S.Role.empty() ? "-" : S.Role, S.Map.empty() ? "-" : S.Map,
                       S.Players < 0 ? std::string("-") : std::to_string(S.Players), Detail::Or(S.Dilation, "{:.2f}"),
                       S.Dilation ? (S.Pauser ? "ja" : "nein") : "-", Detail::Or(S.TimeRequests, "{}"),
                       Detail::Or(S.Scrap, "{}"), Fuel, S.PingMs < 0 ? std::string("-") : std::format("{}ms", S.PingMs),
                       S.Connections, S.ControlRefused);
}

// Zeile fällig: erste, nach Weltwechsel, nach Marke oder nach IntervalMs. Uhr zurück zählt als fällig.
constexpr bool Due(bool Ever, uint64_t Last, uint64_t Now, bool WorldChanged, bool Marked)
{
    return !Ever || WorldChanged || Marked || Now < Last || Now - Last >= IntervalMs;
}
}
