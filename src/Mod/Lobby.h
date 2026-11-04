#pragma once

// Steam-Lobby im Spiel: verbindet Net::Session mit dem Overlay und den Konsolenbefehlen des Spiels. Siehe src/Net/README.md.
namespace Mod::Lobby
{
// Registriert den Game-Thread-Tick. Vor Game::Start aufrufen.
void Install();
}
