#pragma once
#include "Core/Config.h"
#include <cstdint>
#include <functional>
#include <string>

// Abgleich zwischen Host und Client. Die Mod ruft Start einmal nach bestandener Versionsprüfung auf.
namespace Sync
{
using DiagnosticSink = std::function<void(std::string)>;
// Entwicklerwerkzeug: liefert die seit dem letzten Aufruf gedrueckten Dev-Knoepfe als Bitmaske (1 << Game::DevAction).
void BindDevSource(std::function<uint32_t()> Source);
// Hinweistext für den Spieler, leer nimmt ihn weg. Bisher nur: Der Host hält die Weltzeit an (opt-boss-tafel-client).
void BindNoticeSink(std::function<void(std::string)> Sink);
void Start(const Core::Config& Config = {}, DiagnosticSink Diagnostics = {});
}
