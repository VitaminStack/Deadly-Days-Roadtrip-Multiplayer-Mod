#pragma once
#include <cstdint>

namespace Game
{
// Auch ein lokal erzeugter Client-Actor kann Role=Authority tragen. Das ist keine Host-Berechtigung.
constexpr bool AirdropMayRunGameplay(bool IsAirdrop, uint8_t ActorRole, bool IsNetworkClient)
{
    return !IsAirdrop || (ActorRole == 3 && !IsNetworkClient);
}
}
