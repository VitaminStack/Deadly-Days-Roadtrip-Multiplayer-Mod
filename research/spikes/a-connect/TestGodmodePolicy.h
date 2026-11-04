#pragma once

namespace TestGodmodePolicy
{
// A dedicated additive contribution, independent of temporary gameplay invincibility.
constexpr int Delta(bool Enabled, bool Owned, bool HasTag)
{
    if (Enabled)
        return !Owned || !HasTag ? 1 : 0;
    return Owned && HasTag ? -1 : 0;
}
}
