#pragma once

#include <optional>
#include <string_view>

// Koop-Modus eines Runs (coop-modus-wahl): Der Host wählt ihn vor dem Erstellen der Lobby, er steht in den Lobby-Daten
// (docs/CONTROL_PROTOCOL.md#koop-modus-mod-kompatibilität-23) und gilt bis zum Ende der Sitzung. Reine Werte ohne Steam und Spiel.
namespace Net
{
enum class CoopMode : unsigned char
{
    // Standard: jeder Spieler hat sein eigenes Inventar.
    Own = 0,
    // Geteiltes Inventar. Bis es gebaut ist, verhält sich das Spiel in diesem Modus wie mit eigenen Inventaren.
    Shared = 1,
};

// Wert in den Lobby-Daten und im Testkanal. Ein Spielstand kann denselben Text verwenden.
constexpr std::string_view ToLobbyValue(CoopMode Mode)
{
    return Mode == CoopMode::Shared ? "shared" : "own";
}

// Nur die beiden festen Werte, alles andere ist unbekannt.
constexpr std::optional<CoopMode> ParseCoopMode(std::string_view Value)
{
    if (Value == "own")
        return CoopMode::Own;
    if (Value == "shared")
        return CoopMode::Shared;
    return std::nullopt;
}

// Für Spieler lesbar, in Overlay und Log.
constexpr std::string_view DisplayName(CoopMode Mode)
{
    return Mode == CoopMode::Shared ? "Geteiltes Inventar" : "Eigene Inventare";
}
}
