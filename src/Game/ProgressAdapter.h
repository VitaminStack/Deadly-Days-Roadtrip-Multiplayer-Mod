#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include "Game/WorldAdapter.h"

namespace Game
{
struct ProgressContext
{
    uint64_t LocalWorld = 0; // Nur lokale Lebensdauer, niemals Netzwerkidentitaet.
    uint64_t LocalState = 0;
    uint64_t StateGuid = 0;
    std::string Map;
    int32_t Day = 0;
    NetRole Role = NetRole::Standalone;
};
struct SharedProgress
{
    int32_t Level = 1;
    float Fraction = 0;
};
// Ausschliesslich auf dem Game Thread. Keine XP-/Reward-/Save-Mutation.
std::optional<ProgressContext> ReadProgressContext();
std::optional<SharedProgress> ReadSharedProgress();
unsigned ShowSharedProgress(const SharedProgress& Value);
}
