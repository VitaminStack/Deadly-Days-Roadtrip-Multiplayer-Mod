#include "Game/AirdropAdapter.h"

#include <chrono>
#include <cmath>
#include <map>
#include <string_view>
#include <set>
#include "Core/Dispatcher.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/NetworkObjects.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

#ifdef DDCOOP_WITH_SDK
#pragma warning(push, 0)
#include <SDK/DDSurvivors_classes.hpp>
#include <SDK/BP_Airdrop_Lootable_classes.hpp>
#pragma warning(pop)
static_assert(offsetof(SDK::ACrate, bIsOpened) == 0x2A8);
static_assert(offsetof(SDK::ACrate, bMapLootDistributionReady) == 0x2A9);
static_assert(offsetof(SDK::ARewardChest, CanBeOpened) == 0x2E1);
static_assert(offsetof(SDK::ABP_Airdrop_Lootable_C, SK_AirDropAnimMerged) == 0x358);
static_assert(offsetof(SDK::ABP_Airdrop_Lootable_C, LootableReady) == 0x360);
#endif

namespace Game
{
namespace
{
bool Ready(NetRole Role)
{
    return Core::GameThread().IsDrainThread() && Patches::CurrentState() == Patches::State::Active && Patches::CurrentRole() == Role;
}
bool Airdrop(uintptr_t Actor)
{
    const uintptr_t World = Patches::CurrentWorld();
    const auto Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel) : std::nullopt;
    const auto Flags = Memory::Read<uint32_t>(Actor + Offsets::UObject::Flags);
    return Level && Flags && !(*Flags & 0x30) && Memory::ReadPointer(Actor + Offsets::UObject::Outer) == Level &&
           !(Memory::Read<uint8_t>(Actor + 0x5D).value_or(2) & 2) && Objects::IsA(Actor, "BP_Airdrop_Lootable_C");
}
bool Call(uintptr_t Object, const char* Owner, const char* Name, void* Params = nullptr)
{
    if (!Object)
        return false;
    const auto Flags = Memory::Read<uint32_t>(Object + Offsets::UObject::Flags);
    if (!Flags || (*Flags & 0x18000))
        return false;
    static std::map<std::pair<std::string_view, std::string_view>, uintptr_t> Functions;
    auto& Function = Functions[{Owner, Name}];
    if (!Function)
        Function = Objects::FindObject(Name, Owner); // Ausschliesslich native, mit ihren Klassen geladene UFunctions.
    if (!Function)
        return false;
    Objects::ProcessEvent(Object, Function, Params);
    return true;
}
bool SingleAnimation(uintptr_t Mesh)
{
    uint8_t Mode = 255;
    return Call(Mesh, "SkeletalMeshComponent", "GetAnimationMode", &Mode) && Mode == 1;
}
}

void PrepareAirdropReplication()
{
    if (!Ready(NetRole::Host))
        return;
    static auto Next = std::chrono::steady_clock::now();
    if (std::chrono::steady_clock::now() < Next)
        return;
    Next = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    // Airdrops liegen im Level der Welt; dessen Actor-Liste statt des ganzen Objekt-Arrays (host-lagspike).
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t Level = World ? Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0) : 0;
    Objects::ForEachLevelActor(Level, [](uintptr_t Actor)
    {
        if (!Airdrop(Actor) || Memory::Read<uint8_t>(Actor + 0x168).value_or(0) != 3)
            return;
        bool Enabled = true, Changed = false;
        if (!(Memory::Read<uint8_t>(Actor + 0x5B).value_or(0) & 8))
            Changed |= Call(Actor, "Actor", "SetReplicates", &Enabled);
        if (!(Memory::Read<uint8_t>(Actor + 0x58).value_or(0) & 0x10))
            Changed |= Call(Actor, "Actor", "SetReplicateMovement", &Enabled);
        if (Changed)
        {
            Call(Actor, "Actor", "FlushNetDormancy");
            Call(Actor, "Actor", "ForceNetUpdate");
            Core::Log::Info("Airdrop: Actor- und Bewegungsreplikation aktiviert");
        }
    });
}

std::optional<std::vector<AirdropView>> ReadAirdropViews()
{
    if (!Ready(NetRole::Host))
        return std::nullopt;
    const auto Entries = ReadNetworkObjects();
    if (!Entries)
        return std::nullopt;
    std::vector<AirdropView> Values;
    for (const auto& Entry : *Entries)
    {
        if (!Airdrop(Entry.Object))
            continue;
        const auto Opened = Memory::Read<uint8_t>(Entry.Object + 0x2A8);
        const auto Lootable = Memory::Read<uint8_t>(Entry.Object + 0x360);
        const auto Flags = Memory::Read<uint8_t>(Entry.Object + 0x58);
        if (!Opened || !Lootable || !Flags || Values.size() >= 64)
            return std::nullopt;
        AirdropView Value{Entry.Guid, *Opened != 0, *Lootable != 0};
        Value.Hidden = (*Flags & 0x80) != 0;
        const uintptr_t Mesh = Memory::ReadPointer(Entry.Object + 0x358).value_or(0);
        if (SingleAnimation(Mesh))
        {
            bool Playing = false;
            float Position = 0;
            if (Call(Mesh, "SkeletalMeshComponent", "GetPosition", &Position) &&
                Call(Mesh, "SkeletalMeshComponent", "IsPlaying", &Playing) && std::isfinite(Position) && Position >= 0 && Position < 36000)
            {
                Value.HasAnimation = true;
                Value.Playing = Playing;
                Value.Position = Position;
            }
        }
        Values.push_back(Value);
    }
    return Values;
}

unsigned ShowAirdropViews(const std::vector<AirdropView>& Values, uint64_t LocalGeneration)
{
    if (!Ready(NetRole::Client) || Values.size() > 64 || !LocalGeneration)
        return 0;
    const auto Entries = ReadNetworkObjects();
    if (!Entries)
        return 0;
    static uint64_t Generation = 0;
    static std::set<uint64_t> Controlled;
    if (Generation != LocalGeneration)
    {
        Controlled.clear();
        Generation = LocalGeneration;
    }
    std::set<uint64_t> Present;
    for (const auto& Value : Values)
    {
        Present.insert(Value.Guid);
        Controlled.insert(Value.Guid);
    }
    // Ein Host-Airdrop kann beim Oeffnen bereits vor dem naechsten Snapshot verschwinden.
    // Nur zuvor zugeordnete Kopien ausblenden; keine Oeffnung/Belohnung aus Abwesenheit erfinden.
    for (const auto& Entry : *Entries)
        if (Controlled.contains(Entry.Guid) && !Present.contains(Entry.Guid) && Airdrop(Entry.Object))
        {
            bool Hidden = true, Collision = false;
            Call(Entry.Object, "Actor", "SetActorHiddenInGame", &Hidden);
            Call(Entry.Object, "Actor", "SetActorEnableCollision", &Collision);
        }
    unsigned Shown = 0;
    for (const auto& Value : Values)
        for (const auto& Entry : *Entries)
        {
            if (Entry.Guid != Value.Guid || !Airdrop(Entry.Object) || Memory::Read<uint8_t>(Entry.Object + 0x168).value_or(3) == 3)
                continue;
            const bool NotReadyForGameplay = false;
            bool Hidden = Value.Hidden, Collision = false;
            Call(Entry.Object, "Actor", "SetActorHiddenInGame", &Hidden);
            Call(Entry.Object, "Actor", "SetActorEnableCollision", &Collision);
            Memory::WriteBytes(Entry.Object + 0x2A9, &NotReadyForGameplay, 1);
            Memory::WriteBytes(Entry.Object + 0x2E1, &NotReadyForGameplay, 1);
            Memory::WriteBytes(Entry.Object + 0x360, &Value.Ready, 1);
            if (Value.Opened && !Memory::Read<uint8_t>(Entry.Object + 0x2A8).value_or(1))
                Call(Entry.Object, "Crate", "SetIsOpened"); // Nur Synchronisations-/Anzeigeweg, nicht OpenCrate/Choices.
            if (!Airdrop(Entry.Object))
                continue;
            const uintptr_t Mesh = Memory::ReadPointer(Entry.Object + 0x358).value_or(0);
            if (Value.HasAnimation && std::isfinite(Value.Position) && Value.Position >= 0 && Value.Position < 36000 && SingleAnimation(Mesh))
            {
                bool Playing = false;
                Call(Mesh, "SkeletalMeshComponent", "IsPlaying", &Playing);
                if (Playing != Value.Playing)
                {
                    bool Loop = false;
                    Call(Mesh, "SkeletalMeshComponent", Value.Playing ? "Play" : "Stop", Value.Playing ? &Loop : nullptr);
                }
                struct Params { float Position; bool FireNotifies = false; uint8_t Pad[3]{}; } Position{Value.Position};
                Call(Mesh, "SkeletalMeshComponent", "SetPosition", &Position); // Keine Animation-Notifies nachspielen.
            }
            ++Shown;
            break;
        }
    return Shown;
}
}
