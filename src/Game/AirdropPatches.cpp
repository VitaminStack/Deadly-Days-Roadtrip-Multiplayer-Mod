#include "Game/PatchRuntime.h"
#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/AirdropPolicy.h"
#include <atomic>

namespace Game::Patches
{
namespace
{
constexpr uintptr_t OpenCrate = 0x5A43400;
constexpr uintptr_t OpenChoices = 0x5A42F50;
using OpenFn = void(__fastcall*)(uintptr_t, uintptr_t);
OpenFn OriginalCrate = nullptr, OriginalChoices = nullptr;
std::atomic<unsigned> LocalOpenAttempts = 0, LocalChoiceAttempts = 0;

bool ClientAirdrop(uintptr_t Actor)
{
    return Actor && !AirdropMayRunGameplay(IsA(Actor, "BP_Airdrop_Lootable_C"),
        Memory::Read<uint8_t>(Actor + 0x168).value_or(0), CurrentRole() == NetRole::Client);
}
void __fastcall CrateHook(uintptr_t Actor, uintptr_t Character)
{
    if (!ClientAirdrop(Actor))
        OriginalCrate(Actor, Character);
    else if (LocalOpenAttempts.fetch_add(1) < 3)
        Core::Log::Info("Airdrop: lokale Client-Oeffnung beim Host belassen");
}
void __fastcall ChoicesHook(uintptr_t Actor, uintptr_t Character)
{
    if (!ClientAirdrop(Actor))
        OriginalChoices(Actor, RewardCharacter(Character));
    else if (LocalChoiceAttempts.fetch_add(1) < 3)
        Core::Log::Info("Airdrop: lokale Client-Reward-Erzeugung ausgelassen");
}
bool CheckCrate()
{
    return Code::Expect(At(OpenCrate), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10}, "RewardChest.OpenCrate");
}
bool CheckChoices()
{
    return Code::Expect(At(OpenChoices), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20}, "RewardChest.OpenAirdropChoices");
}
bool ApplyCrate(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(OpenCrate)), reinterpret_cast<void*>(&CrateHook),
                                reinterpret_cast<void**>(&OriginalCrate));
}
bool ApplyChoices(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(OpenChoices)), reinterpret_cast<void*>(&ChoicesHook),
                                reinterpret_cast<void**>(&OriginalChoices));
}
// Je Patch alle benutzten Adressen (patch-signaturen): passt eine Signatur nicht, fällt nur dieser Patch aus.
constexpr uintptr_t CrateSites[] = {OpenCrate};
constexpr uintptr_t ChoicesSites[] = {OpenChoices};
const Patch List[] = {
    {"Airdrop-Oeffnung nur beim Host", CheckCrate, ApplyCrate, CrateSites},
    {"Airdrop-Rewards nur beim Host", CheckChoices, ApplyChoices, ChoicesSites},
};
}
std::span<const Patch> AirdropPatches() { return List; }
}
