#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "Game/ZoneGoalPolicy.h"

// opt-zonenziel-anzeige, Host -> Client: Stand des laufenden Zonenziels als Stand, nicht als Ereignis. Nutzlast, genau 9 Bytes,
// Little-Endian: Marken (1, Bit 0 = Ziel läuft, übrige Bits 0), Ist (uint32), Soll (uint32). Ohne Ziel sind Ist und Soll 0.
// Der Host sendet bei jeder Änderung und wiederholt den Stand alle 2 s; ein verlorenes Paket holt die Wiederholung nach.
// Siehe docs/CONTROL_PROTOCOL.md, Abschnitt Zonenziel.
namespace Sync::ZoneGoal
{
inline constexpr uint16_t StateType = 0xD00;
inline constexpr size_t Bytes = 9;

// Leer, wenn der Stand ungültig ist.
inline std::vector<uint8_t> Encode(const Game::ZoneGoalView& View)
{
    if (!Game::ZoneGoalValid(View))
        return {};
    std::vector<uint8_t> Out{static_cast<uint8_t>(View.Active ? 1 : 0)};
    for (const int64_t Value : {View.Current, View.Target})
        for (unsigned i = 0; i < 4; ++i)
            Out.push_back(static_cast<uint8_t>(static_cast<uint32_t>(Value) >> (8 * i)));
    return Out;
}

inline std::optional<Game::ZoneGoalView> Decode(std::span<const uint8_t> In)
{
    if (In.size() != Bytes || In[0] > 1)
        return std::nullopt;
    const auto Number = [In](size_t At) {
        uint32_t Bits = 0;
        for (unsigned i = 0; i < 4; ++i)
            Bits |= uint32_t(In[At + i]) << (8 * i);
        return static_cast<int64_t>(Bits);
    };
    const Game::ZoneGoalView View{In[0] == 1, Number(1), Number(5)};
    return Game::ZoneGoalValid(View) ? std::optional(View) : std::nullopt;
}
}
