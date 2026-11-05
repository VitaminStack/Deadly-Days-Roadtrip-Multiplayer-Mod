#include "Game/DesyncAdapter.h"

#include <cmath>
#include <map>
#include <set>
#include "Core/Dispatcher.h"
#include "Game/Memory.h"
#include "Game/NetworkObjects.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

#ifdef DDCOOP_WITH_SDK
#pragma warning(push, 0)
#include <SDK/DDSurvivors_classes.hpp>
#pragma warning(pop)
static_assert(offsetof(SDK::ADDPlayerState, HealthAttributeSet) == 0x380);
static_assert(offsetof(SDK::UCommonAttributeSet, Health) + 0xC == 0xAC);
static_assert(offsetof(SDK::UCommonAttributeSet, MaxHealth) + 0xC == 0x3C);
static_assert(offsetof(SDK::UInventoryComponent, Slots) == 0x100);
static_assert(offsetof(SDK::UInventoryComponent, MyItems) == 0x120);
static_assert(offsetof(SDK::UBackpackSlot, Coordinates) == 0x28);
static_assert(offsetof(SDK::UBackpackSlot, Occupier) == 0x30);
static_assert(offsetof(SDK::UInventoryItemInstance, ZeroPivotTile) == 0x28);
static_assert(offsetof(SDK::UInventoryItemInstance, Turns) == 0xD8);
static_assert(offsetof(SDK::UInventoryItemInstance, ItemData) == 0xE0);
static_assert(offsetof(SDK::UInventoryItemInstance, Broken) == 0x110);
static_assert(offsetof(SDK::UInventoryItemInstance, IsInBackpack) == 0x180);
static_assert(offsetof(SDK::UInventoryItemInstance, IsReward) == 0x182);
static_assert(offsetof(SDK::UInventoryItemInstance, IsTrash) == 0x184);
static_assert(offsetof(SDK::ULevel, Actors) == 0xA0);
static_assert(offsetof(SDK::ULevel, WorldSettings) == 0x2B8);
static_assert(offsetof(SDK::AWorldSettings, TimeDilation) == 0x3E0);
static_assert(offsetof(SDK::AWorldSettings, PauserPlayerState) == 0x4A8);
static_assert(Game::Desync::ExcludedObjectFlags ==
    (static_cast<uint32_t>(SDK::EObjectFlags::ClassDefaultObject) |
     static_cast<uint32_t>(SDK::EObjectFlags::ArchetypeObject) |
     static_cast<uint32_t>(SDK::EObjectFlags::BeginDestroyed) |
     static_cast<uint32_t>(SDK::EObjectFlags::FinishDestroyed) |
     static_cast<uint32_t>(SDK::EObjectFlags::MirroredGarbage)));
#endif

namespace Game
{
namespace
{
struct Array { uintptr_t Data; int32_t Num, Max; };
std::optional<std::vector<uintptr_t>> Pointers(uintptr_t Address, int32_t Limit)
{
    const auto Header = Memory::Read<Array>(Address);
    if (!Header || Header->Num < 0 || Header->Max < Header->Num || Header->Num > Limit ||
        (Header->Num && !Header->Data)) return std::nullopt;
    std::vector<uintptr_t> Result(static_cast<size_t>(Header->Num));
    if (!Result.empty() && !Memory::ReadBytes(Header->Data, Result.data(), Result.size() * sizeof(uintptr_t)))
        return std::nullopt;
    return Result;
}
bool Live(uintptr_t Object)
{
    const auto Flags = Object ? Memory::Read<uint32_t>(Object + Offsets::UObject::Flags) : std::nullopt;
    return Flags && Desync::LiveObjectFlags(*Flags);
}
// Vollstaendiger Asset-Pfad. Dynamische/nummerierte Namen werden nicht geraten.
std::optional<std::string> AssetPath(uintptr_t Object)
{
    std::string Result;
    std::set<uintptr_t> Seen;
    for (unsigned Depth = 0; Object && Depth < 32; ++Depth)
    {
        if (!Seen.insert(Object).second || Memory::Read<int32_t>(Object + 0x1C) != 0) return std::nullopt;
        const auto Name = Memory::NameOf(Object);
        const auto Outer = Memory::Read<uintptr_t>(Object + Offsets::UObject::Outer);
        if (Name.empty() || !Outer || Result.size() + Name.size() > 1024) return std::nullopt;
        Result = Name + "/" + Result;
        Object = *Outer;
    }
    return !Object && !Result.empty() ? std::optional(Result) : std::nullopt;
}
std::optional<uint64_t> InventoryHash(uintptr_t State)
{
    static uintptr_t Class = 0, Function = 0;
    if (!Class) Class = Objects::FindObject("InventoryComponent", "/Script/DDSurvivors");
    if (!Class) Class = Objects::FindObject("InventoryComponent", "DDSurvivors");
    if (!Function) Function = Objects::FindObject("GetComponentByClass", "Actor");
    if (!Class || !Objects::IsA(Class, "Class") || !Function) return std::nullopt;
    struct { uintptr_t Class, Result; } Params{Class, 0};
    Objects::ProcessEvent(State, Function, &Params);
    const uintptr_t Inventory = Params.Result;
    if (!Live(Inventory) || !Objects::IsA(Inventory, "InventoryComponent") ||
        Memory::Read<uintptr_t>(Inventory + 0x20) != State) return std::nullopt;
    const auto Items = Pointers(Inventory + 0x120, 4096), Slots = Pointers(Inventory + 0x100, 4096);
    if (!Items || !Slots) return std::nullopt;
    std::map<uintptr_t, uint64_t> ItemHashes;
    std::vector<uint64_t> Records, SlotRecords;
    for (const auto Item : *Items)
    {
        if (!Live(Item) || !Objects::IsA(Item, "InventoryItemInstance") || ItemHashes.contains(Item)) return std::nullopt;
        const auto Data = Memory::ReadPointer(Item + 0xE0);
        const auto Path = Data ? AssetPath(*Data) : std::nullopt;
        const auto Pivot = Memory::Read<uint64_t>(Item + 0x28);
        const auto Turns = Memory::Read<int32_t>(Item + 0xD8);
        const auto Broken = Memory::Read<uint8_t>(Item + 0x110);
        const auto Backpack = Memory::Read<uint8_t>(Item + 0x180), Reward = Memory::Read<uint8_t>(Item + 0x182),
                   Trash = Memory::Read<uint8_t>(Item + 0x184);
        if (!Path || !Pivot || !Turns || !Broken || !Backpack || !Reward || !Trash ||
            *Broken > 1 || *Backpack > 1 || *Reward > 1 || *Trash > 1) return std::nullopt;
        Desync::Hash Hash;
        Hash.Text(*Path);
        Hash.Number(*Pivot);
        Hash.Number(static_cast<uint32_t>(*Turns));
        Hash.Number(*Broken | (*Backpack << 1) | (*Reward << 2) | (*Trash << 3));
        ItemHashes.emplace(Item, Hash.Value());
        Records.push_back(Hash.Value());
    }
    std::set<uintptr_t> SeenSlots;
    std::set<uint64_t> Coordinates;
    for (const auto Slot : *Slots)
    {
        if (!Live(Slot) || !Objects::IsA(Slot, "BackpackSlot") || !SeenSlots.insert(Slot).second) return std::nullopt;
        const auto Position = Memory::Read<uint64_t>(Slot + 0x28), Occupier = Memory::Read<uintptr_t>(Slot + 0x30);
        if (!Position || !Occupier || !Coordinates.insert(*Position).second ||
            (*Occupier && !ItemHashes.contains(*Occupier))) return std::nullopt;
        Desync::Hash Hash;
        Hash.Number(*Position);
        Hash.Number(*Occupier ? 1 : 0);
        if (*Occupier) Hash.Number(ItemHashes.at(*Occupier));
        SlotRecords.push_back(Hash.Value());
    }
    Desync::Hash Hash;
    Hash.Number(Desync::Multiset(std::move(Records)));
    Hash.Number(Desync::Multiset(std::move(SlotRecords)));
    return Hash.Value();
}
}

std::optional<DesyncCapture> ReadDesyncCapture()
{
    if (!Core::GameThread().IsDrainThread()) return std::nullopt;
    const auto Context = ReadProgressContext();
    if (!Context) return std::nullopt;
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t State = Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0);
    const uintptr_t Level = Memory::ReadPointer(World + Offsets::UWorld::PersistentLevel).value_or(0);
    if (!State || !Level) return std::nullopt;
    const auto Network = ReadNetworkObjects();
    if (!Network) return std::nullopt;
    std::map<uintptr_t, uint64_t> Guids;
    for (const auto& Entry : *Network) Guids.emplace(Entry.Object, Entry.Guid);
    DesyncCapture Result{*Context};
    auto& Out = Result.Image;
    Out.Known = Desync::Bit(Desync::Day);
    Out.Values[Desync::Day] = static_cast<uint64_t>(Context->Day);
    // Diagnosephase: laufende oder pausierte Missionswelt, keine erfundene Inventar-/Routen-UI-Phase.
    const auto Settings = Memory::ReadPointer(Level + 0x2B8);
    const auto Dilation = Settings ? Memory::Read<float>(*Settings + 0x3E0) : std::nullopt;
    const auto Pauser = Settings ? Memory::Read<uintptr_t>(*Settings + 0x4A8) : std::nullopt;
    if (Dilation && Pauser && std::isfinite(*Dilation) && *Dilation >= 0)
    {
        Out.Known |= Desync::Bit(Desync::Phase);
        Out.Values[Desync::Phase] = (*Pauser || *Dilation < 0.01f) ? 1 : 0;
    }
    const auto Players = Pointers(State + Offsets::AGameStateBase::PlayerArray, 2);
    if (Players && !Players->empty())
    {
        bool HealthKnown = true, InventoryKnown = true, RosterKnown = true;
        std::set<uintptr_t> Seen;
        std::vector<uint64_t> HealthRecords, InventoryRecords;
        for (const auto Player : *Players)
        {
            if (!Live(Player) || !Objects::IsA(Player, "DDPlayerState") || !Guids.contains(Player) ||
                !Seen.insert(Player).second) { RosterKnown = false; break; }
            const auto Attributes = Memory::ReadPointer(Player + 0x380);
            const auto HP = Attributes ? Memory::Read<float>(*Attributes + 0xAC) : std::nullopt;
            const auto MaxHP = Attributes ? Memory::Read<float>(*Attributes + 0x3C) : std::nullopt;
            if (HP && MaxHP && std::isfinite(*HP) && std::isfinite(*MaxHP) &&
                std::abs(*HP) < 1e8 && *MaxHP >= 0 && *MaxHP < 1e8)
            {
                Desync::Hash Hash;
                Hash.Number(Guids.at(Player));
                Hash.Number(static_cast<uint64_t>(std::llround(*HP * 10.0)));
                Hash.Number(static_cast<uint64_t>(std::llround(*MaxHP * 10.0)));
                HealthRecords.push_back(Hash.Value());
            }
            else HealthKnown = false;
            if (const auto Inventory = InventoryHash(Player))
            {
                Desync::Hash Hash;
                Hash.Number(Guids.at(Player));
                Hash.Number(*Inventory);
                InventoryRecords.push_back(Hash.Value());
            }
            else InventoryKnown = false;
        }
        if (RosterKnown)
        {
            Out.Known |= Desync::Bit(Desync::Players);
            Out.Values[Desync::Players] = Players->size();
            if (HealthKnown) { Out.Known |= Desync::Bit(Desync::Health); Out.Values[Desync::Health] = Desync::Multiset(std::move(HealthRecords)); }
            if (InventoryKnown) { Out.Known |= Desync::Bit(Desync::Inventory); Out.Values[Desync::Inventory] = Desync::Multiset(std::move(InventoryRecords)); }
        }
    }
    if (const auto Actors = Pointers(Level + 0xA0, 20000))
    {
        uint64_t Enemies = 0;
        bool Known = true;
        for (const auto Actor : *Actors)
            if (Actor && Live(Actor) && Objects::IsA(Actor, "EnemyBase"))
            {
                const auto Flags = Memory::Read<uint8_t>(Actor + 0x58);
                if (!Flags) { Known = false; break; }
                if (!(*Flags & 0x80)) ++Enemies; // keine versteckten Pool-Actors
            }
        if (Known) { Out.Known |= Desync::Bit(Desync::Enemies); Out.Values[Desync::Enemies] = Enemies; }
    }
    return Result;
}
}
