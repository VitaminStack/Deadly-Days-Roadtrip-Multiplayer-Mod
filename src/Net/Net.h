#pragma once

#include <memory>

#include "Net/Transport.h"

// Verbindung zwischen den Spielern. Die Mod ruft Start einmal nach bestandener Versionsprüfung auf.
namespace Net
{
// Startet den Transport, wenn die Kommandozeile -ddcoop_transport=... enthält (Testschalter, siehe src/Net/README.md).
// Kehrt sofort zurück, das Warten auf Steam läuft im Hintergrund.
void Start();

// Laufender Transport oder nullptr.
std::shared_ptr<Transport> CurrentTransport();
}
