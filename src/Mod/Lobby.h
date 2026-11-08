#pragma once

#include <string>

#include "Net/ControlProtocol.h"

// Steam-Lobby im Spiel: verbindet Net::Session mit dem Overlay und den Konsolenbefehlen des Spiels. Siehe src/Net/README.md.
namespace Mod::Lobby
{
// Registriert den Game-Thread-Tick. Vor Game::Start aufrufen.
void Install();

// Führt einen Befehl des Testkanals aus und liefert die JSON-Antwort. Nur auf dem Game Thread. Benutzt dieselben Wege wie die
// Knöpfe des Overlays (Net::Session) und die Konsole des Spiels, siehe src/Mod/Control.cpp.
std::string Execute(const Net::Control::Command& Command);
}
