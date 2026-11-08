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
void Start(const Core::Config& Config = {}, DiagnosticSink Diagnostics = {});
}
