#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "Game/ThreatPolicy.h"

// opt-bedrohung-sound, Host -> Client: aktuelle Bedrohungsstufe der Mission als Stand, nicht als Ereignis. Nutzlast: Stufe
// (int32, Little-Endian, 0 bis 100000), genau 4 Bytes. Der Host sendet bei jeder Änderung und wiederholt den Stand alle 2 s;
// ein verlorenes Paket holt die Wiederholung nach. Siehe docs/CONTROL_PROTOCOL.md, Abschnitt Bedrohungsstufe.
namespace Sync::Threat
{
inline constexpr uint16_t LevelType = 0xC00;

// Leer, wenn die Stufe ungültig ist.
inline std::vector<uint8_t> Encode(int32_t Level)
{
    if (!Game::ThreatLevelValid(Level))
        return {};
    std::vector<uint8_t> Out;
    for (unsigned i = 0; i < 4; ++i)
        Out.push_back(static_cast<uint8_t>(static_cast<uint32_t>(Level) >> (8 * i)));
    return Out;
}

inline std::optional<int32_t> Decode(std::span<const uint8_t> In)
{
    if (In.size() != 4)
        return std::nullopt;
    uint32_t Bits = 0;
    for (unsigned i = 0; i < 4; ++i)
        Bits |= uint32_t(In[i]) << (8 * i);
    const int32_t Level = static_cast<int32_t>(Bits);
    return Game::ThreatLevelValid(Level) ? std::optional<int32_t>(Level) : std::nullopt;
}
}
