#pragma once

// Testkanal: lokale Named Pipe \\.\pipe\ddcoop-ctl-<pid>, nur mit Schalter -ddcoop_control. Testskripte schicken Befehle
// mit Antwort (Zustand, Lobby, Konsole, Reise) und brauchen dafür weder Spike noch Overlay-Fenster. Protokoll siehe
// src/Net/ControlProtocol.h, Ausführung in Mod::Lobby::Execute.
namespace Mod::Control
{
// Startet den Pipe-Thread, wenn der Schalter in der Kommandozeile steht. Vor Game::Start aufrufen, nach Lobby::Install.
void Install();
}
