#pragma once

namespace SneakTargetReadiness
{
constexpr bool Reuse(bool SameTarget, bool HasMesh, bool HasPlayerState)
{
    return SameTarget && HasMesh && HasPlayerState;
}

// Original comparison flags and rdi=component are live at the checked branch. No writes/calls/register clobbers.
template <typename Writer>
void Emit(Writer& Code, uintptr_t Retry, uintptr_t Ready)
{
    Code.Emit({0x0F, 0x85}); // Target changed: original initialization.
    Code.EmitRel(Retry);
    Code.Emit({0x48, 0x83, 0xBF, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x84});
    Code.EmitRel(Retry);
    Code.Emit({0x48, 0x83, 0xBF, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x84});
    Code.EmitRel(Retry);
    Code.Emit({0xE9});
    Code.EmitRel(Ready);
}
}
