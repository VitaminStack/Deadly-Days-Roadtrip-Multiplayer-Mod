#pragma once
#include "Core/Config.h"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// Abgleich zwischen Host und Client. Die Mod ruft Start einmal nach bestandener Versionsprüfung auf.
namespace Sync
{
using DiagnosticSink = std::function<void(std::string)>;
void Start(const Core::Config& Config = {}, DiagnosticSink Diagnostics = {});

// Airdrop-Auswahl für das Overlay. Beim Client mit Knöpfen je Choice, beim Host nur ein Hinweis, solange er wartet.
struct RewardPrompt
{
    uint64_t Session = 0, Revision = 0;
    int32_t Level = 0;
    std::vector<std::string> Choices;
    std::string Status;
    bool Selectable = false;
    bool operator==(const RewardPrompt&) const = default;
};
// Läuft auf dem Game Thread, nullopt schließt die Auswahl. Vor Start setzen.
using RewardSink = std::function<void(std::optional<RewardPrompt>)>;
void SetRewardSink(RewardSink Sink);
// Client: Wahl an den Host schicken. Nur Game Thread.
void ChooseReward(uint64_t Session, uint64_t Revision, uint32_t Index);
}
