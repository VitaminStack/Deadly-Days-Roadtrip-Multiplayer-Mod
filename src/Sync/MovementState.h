#pragma once

#include <bit>
#include <cmath>
#include <optional>
#include <span>
#include <vector>
#include "Game/DashAdapter.h"

// Client -> Host: Laufrichtung je eigenem Dodge. Nur die Eingabe, die GA_Dash beim Client gelesen hat; Kosten, Schub und
// Ende rechnet der Host weiter nativ. Siehe docs/CONTROL_PROTOCOL.md, Abschnitt Dodge-Richtung.
namespace Sync::Movement
{
inline constexpr uint16_t DashInput = 0x500;
inline constexpr size_t DashInputBytes = 10;

// Normierte Eingabe oder (0, 0), endlich. Etwas Spielraum für Rundung beim Normieren.
inline bool Valid(const Game::DashInput& Value)
{
    return std::isfinite(Value.X) && std::isfinite(Value.Y) && Value.X * Value.X + Value.Y * Value.Y <= 1.0201f;
}

inline std::vector<uint8_t> Encode(const Game::DashInput& Value)
{
    if (!Valid(Value))
        return {};
    std::vector<uint8_t> Out;
    const auto Put = [&Out](uint32_t Bits, unsigned Bytes) {
        for (unsigned i = 0; i < Bytes; ++i)
            Out.push_back(static_cast<uint8_t>(Bits >> (8 * i)));
    };
    Put(static_cast<uint16_t>(Value.Key), 2);
    Put(std::bit_cast<uint32_t>(Value.X), 4);
    Put(std::bit_cast<uint32_t>(Value.Y), 4);
    return Out;
}

inline std::optional<Game::DashInput> Decode(std::span<const uint8_t> In)
{
    if (In.size() != DashInputBytes)
        return std::nullopt;
    const auto Get = [In](size_t At, unsigned Bytes) {
        uint32_t Value = 0;
        for (unsigned i = 0; i < Bytes; ++i)
            Value |= uint32_t(In[At + i]) << (8 * i);
        return Value;
    };
    const Game::DashInput Value{static_cast<int16_t>(Get(0, 2)), std::bit_cast<float>(Get(2, 4)), std::bit_cast<float>(Get(6, 4))};
    return Valid(Value) ? std::optional(Value) : std::nullopt;
}
}
