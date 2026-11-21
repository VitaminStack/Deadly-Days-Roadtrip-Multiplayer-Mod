#pragma once
#include "Game/DesyncSnapshot.h"
#include "Game/ProgressAdapter.h"

namespace Game
{
struct DesyncCapture
{
    ProgressContext Context;
    Desync::Sample Image;
    // Nur Client: Felder, die wegen einer eigenen offenen Bearbeitung nicht als stabil gelten (Desync::HeldByEdit), sonst 0.
    uint8_t Held = 0;
};
// Nur Game Thread; liest replizierte Werte. Unbekannt ist niemals eine leere/gleiche Sammlung.
std::optional<DesyncCapture> ReadDesyncCapture();
}
