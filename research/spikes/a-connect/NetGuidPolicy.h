#pragma once
#include <cstdint>

namespace NetGuidPolicy
{
// Keep reloadable path identities, not actors/levels or strong UObject references. Called only at expiry.
constexpr bool RetainPath(bool CurrentClientCache, uint64_t Guid, uint32_t PathName, uint8_t Flags)
{
    constexpr uint8_t NoLoadOrBroken = 0x01 | 0x08;
    return CurrentClientCache && Guid > 1 && (Guid & 1) && PathName && !(Flags & NoLoadOrBroken);
}
}
