#pragma once
#include <cstdint>
#include <optional>
#include <vector>

namespace Game
{
struct AirdropView
{
    uint64_t Guid = 0;
    bool Opened = false, Ready = false, HasAnimation = false, Playing = false;
    float Position = 0;
    bool Hidden = false;
    bool operator==(const AirdropView&) const = default;
};
// Nur Game Thread und nur mit aktiven Schutzpatches. Keine Reward-Ausfuehrung im Client.
void PrepareAirdropReplication();
std::optional<std::vector<AirdropView>> ReadAirdropViews();
unsigned ShowAirdropViews(const std::vector<AirdropView>& Values, uint64_t LocalGeneration);
}
