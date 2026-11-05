#pragma once
#include "Core/Config.h"
#include <functional>
#include <string>

// Abgleich zwischen Host und Client. Die Mod ruft Start einmal nach bestandener Versionsprüfung auf.
namespace Sync
{
using DiagnosticSink = std::function<void(std::string)>;
void Start(const Core::Config& Config = {}, DiagnosticSink Diagnostics = {});
}
