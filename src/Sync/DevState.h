#pragma once

#include <cmath>
#include <cstring>
#include <optional>
#include <span>
#include <vector>
#include "Game/DevAdapter.h"

// Client -> Host: Anfrage eines Dev-Knopfes (Overlay-Tab "Dev"). Die Aktion, bei SpawnZombies dazu der Bodenpunkt
// unter dem Cursor des Clients (lokal bestimmt). Der Host entscheidet und fuehrt aus.
// Entwicklerwerkzeug, vor dem Release entfernen (TODO `opt-dev-tab`).
namespace Sync::Dev
{
inline constexpr uint16_t Request = 0x600;

struct Message
{
    Game::DevAction Action;
    std::optional<Game::DevPosition> Target;
};

inline std::vector<uint8_t> Encode(const Message& In)
{
    std::vector<uint8_t> Out{static_cast<uint8_t>(In.Action)};
    if (In.Target)
    {
        Out.resize(1 + sizeof(Game::DevPosition));
        std::memcpy(Out.data() + 1, &*In.Target, sizeof(Game::DevPosition));
    }
    return Out;
}

inline std::optional<Message> Decode(std::span<const uint8_t> In)
{
    if (In.empty() || In[0] < 1 || In[0] > Game::LastDevAction)
        return std::nullopt;
    Message Out{static_cast<Game::DevAction>(In[0]), std::nullopt};
    const bool NeedsTarget = Out.Action == Game::DevAction::SpawnZombies;
    if (In.size() != (NeedsTarget ? 1 + sizeof(Game::DevPosition) : 1))
        return std::nullopt;
    if (NeedsTarget)
    {
        Game::DevPosition Target;
        std::memcpy(&Target, In.data() + 1, sizeof(Target));
        if (!std::isfinite(Target.X) || !std::isfinite(Target.Y) || !std::isfinite(Target.Z))
            return std::nullopt;
        Out.Target = Target;
    }
    return Out;
}
}
