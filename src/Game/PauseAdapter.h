#pragma once

#include <cstdint>
#include <optional>

// Pausemenü und Weltzeit für pausen-sync. Ohne SDK-Typen, nur Game Thread.
namespace Game::Pause
{
// Lokales Pausemenü (UPauseScreenManager am HUD des lokalen Spielers) offen: eigene Öffnungsanfrage und GetIsPaused.
// nullopt, solange HUD oder Manager fehlen oder nicht lesbar sind.
std::optional<bool> LocalMenuOpen();

// Host: Zeitanfrage für den Client setzen oder entfernen, mit denselben Werten wie das eigene Pausemenü. Anfragender ist
// die WorldSettings der aktuellen Welt, so trifft die Anfrage keine Anfrage des Spiels. Falsch, wenn etwas fehlt; dann
// pausiert der Host nicht (lieber keine Pause als eine hängende).
bool HoldForRemote(bool Hold);

// Lokale Kennung der aktuellen Welt, 0 ohne. Nur zum Vergleich, nie übertragen.
uint64_t WorldKey();

// Weltzeitfaktor und ob ein Pauser gesetzt ist, für das Log.
struct WorldTime
{
    float Dilation = 1;
    bool Pauser = false;
};
std::optional<WorldTime> ReadWorldTime();
}
