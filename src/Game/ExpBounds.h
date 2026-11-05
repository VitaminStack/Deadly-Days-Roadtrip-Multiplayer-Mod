#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace Game::ExpVisuals
{
using Vector = std::array<double, 3>;

// Wie der native Handler: maximale Entfernung der vier Bodenecken zur Figur,
// plus der konfigurierte Puffer. Keine feste Sicht-/Sammelreichweite erfinden.
inline std::optional<Vector> ViewBounds(const std::array<Vector, 4>& Corners, const Vector& Player,
                                       const std::array<double, 2>& Buffer)
{
    Vector Result{};
    for (size_t Axis = 0; Axis < 2; ++Axis)
    {
        if (!std::isfinite(Player[Axis]) || !std::isfinite(Buffer[Axis]) || Buffer[Axis] < 0)
            return std::nullopt;
        double Extent = 0;
        for (const auto& Corner : Corners)
        {
            if (!std::isfinite(Corner[Axis]))
                return std::nullopt;
            Extent = std::max(Extent, std::fabs(Corner[Axis] - Player[Axis]));
        }
        if (Extent <= 0 || Extent + Buffer[Axis] > 1e7)
            return std::nullopt;
        Result[Axis] = Extent + Buffer[Axis];
    }
    return Result;
}
}
