#pragma once

namespace BlockadeAuthority
{
// A network client must not schedule host-only blockade objectives/spawns. Standalone still owns them.
constexpr bool MayStart(bool HasWorld, bool HasDriver, bool HasServerConnection)
{
    return HasWorld && (!HasDriver || !HasServerConnection);
}
}
