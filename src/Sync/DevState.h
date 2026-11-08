#pragma once

#include <optional>
#include <span>
#include <vector>
#include "Game/DevAdapter.h"

// Client -> Host: Anfrage eines Dev-Knopfes (Overlay-Tab "Dev"). Nur die Aktion, der Host entscheidet und fuehrt aus.
// Entwicklerwerkzeug, vor dem Release entfernen (TODO `opt-dev-tab`).
namespace Sync::Dev
{
inline constexpr uint16_t Request = 0x600;

inline std::vector<uint8_t> Encode(Game::DevAction Action) { return {static_cast<uint8_t>(Action)}; }

inline std::optional<Game::DevAction> Decode(std::span<const uint8_t> In)
{
    if (In.size() != 1 || (In[0] != static_cast<uint8_t>(Game::DevAction::SpawnXp) && In[0] != static_cast<uint8_t>(Game::DevAction::FullTank)))
        return std::nullopt;
    return static_cast<Game::DevAction>(In[0]);
}
}
