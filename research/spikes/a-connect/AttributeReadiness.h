// A derived aggregate is publishable only after every participating character is ready.
// Missing dependencies are retried on the native update; never publish a partial maximum.
#pragma once
#include <cmath>
#include <cstdint>

namespace Spike::AttributeReadiness
{
template <class CharacterAt, class HasAttributes>
bool Complete(int32_t Count, int32_t Capacity, CharacterAt At, HasAttributes Ready)
{
    if (Count < 0 || Capacity < Count || Count > 4096)
        return false;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Character = At(i);
        // The original aggregate deliberately ignores null characters too.
        if (Character && !Ready(Character))
            return false;
    }
    return true;
}

inline float PendingValue(float LastComplete)
{
    // The native empty-roster result is zero; -1 is the handler's uninitialized sentinel.
    return std::isfinite(LastComplete) && LastComplete >= 0.0f ? LastComplete : 0.0f;
}
}
