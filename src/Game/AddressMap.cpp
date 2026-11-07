#include "Game/AddressMap.h"

#include <algorithm>

namespace Game::AddressMap
{
namespace
{
constexpr Entry Build26495302[] = {
#include "Game/RvaMap26495302.inc"
};

constexpr bool Sorted(std::span<const Entry> Entries)
{
    for (size_t Index = 1; Index < Entries.size(); ++Index)
        if (Entries[Index - 1].Reference >= Entries[Index].Reference)
            return false;
    return true;
}
static_assert(Sorted(Build26495302), "RvaMap26495302.inc muss aufsteigend und ohne doppelte Referenz-RVAs sein");
}

std::span<const Entry> Table(uint32_t Build)
{
    if (Build == 26495302)
        return Build26495302;
    return {};
}

bool Known(uint32_t Build)
{
    return Build == ReferenceBuild || !Table(Build).empty();
}

uint32_t Map(uint32_t Build, uint32_t ReferenceRva)
{
    if (Build == ReferenceBuild)
        return ReferenceRva;
    const auto Entries = Table(Build);
    const auto Found = std::ranges::lower_bound(Entries, ReferenceRva, {}, &Entry::Reference);
    return Found != Entries.end() && Found->Reference == ReferenceRva ? Found->Mapped : 0;
}
}
