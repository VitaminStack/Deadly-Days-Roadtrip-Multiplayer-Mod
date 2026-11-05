#pragma once
#include "Game/DesyncSnapshot.h"
#include "Game/ProgressAdapter.h"

namespace Game
{
struct DesyncCapture
{
    ProgressContext Context;
    Desync::Sample Image;
};
// Nur Game Thread; liest replizierte Werte. Unbekannt ist niemals eine leere/gleiche Sammlung.
std::optional<DesyncCapture> ReadDesyncCapture();
}
