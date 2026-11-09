#include "Game/DevAdapter.h"

#include <chrono>
#include <cstddef>
#include <cmath>
#include <map>
#include <random>
#include <string_view>
#include <vector>

#include "Core/Dispatcher.h"
#include "Core/Log.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"

namespace Game
{
namespace
{
using Vector = DevPosition;

bool Call(uintptr_t Object, const char* Owner, const char* Name, void* Params)
{
    if (!Object)
        return false;
    static std::map<std::pair<std::string_view, std::string_view>, uintptr_t> Functions;
    auto& Function = Functions[{Owner, Name}];
    if (!Function)
        Function = Objects::FindObject(Name, Owner);
    if (!Function)
        return false;
    Objects::ProcessEvent(Object, Function, Params);
    return true;
}

uintptr_t MissionState()
{
    const uintptr_t World = Patches::CurrentWorld();
    const uintptr_t State = World ? Memory::ReadPointer(World + Offsets::UWorld::GameState).value_or(0) : 0;
    return State && Objects::IsA(State, "DDMissionGameState") ? State : 0;
}

// Figur des Spielers (0 Host, 1 Client) ueber die PlayerArray des Missions-GameState, 0 ohne Figur.
uintptr_t PlayerPawn(int Player)
{
    const uintptr_t State = MissionState();
    const uintptr_t Players = State ? Memory::ReadPointer(State + Offsets::AGameStateBase::PlayerArray).value_or(0) : 0;
    const int32_t Count = State ? Memory::Read<int32_t>(State + Offsets::AGameStateBase::PlayerArray + 8).value_or(0) : 0;
    if (!Players || Player < 0 || Player >= Count || Count > 2)
        return false;
    uintptr_t Pawn = 0;
    return Call(Memory::ReadPointer(Players + 8 * Player).value_or(0), "PlayerState", "GetPawn", &Pawn) &&
        Objects::IsA(Pawn, "DDCharacter") ? Pawn : 0;
}

bool SpawnXp(int Player)
{
    const uintptr_t Pawn = PlayerPawn(Player);
    Vector Position{};
    if (!Pawn || !Call(Pawn, "Actor", "K2_GetActorLocation", &Position))
        return false;
    // Parameterlayout wie im Spike (research/spikes/client-visuell/VisualProbe.cpp).
    struct { int32_t Amount; uint32_t Padding; Vector Position; } Request{200, 0, Position};
    static_assert(sizeof(Request) == 32);
    return Call(Objects::FindInstance("ExpSubsystem"), "ExpSubsystem", "RequestExpSpawning", &Request);
}

// Der Schalter wechselt bei jedem Aufruf (Exec-Funktion der Figur, im Spike als Testhilfe belegt, docs/OFFSETS.md).
bool ToggleGodmode(int Player)
{
    return Call(PlayerPawn(Player), "DDCharacter", "DD_ToggleInvincible", nullptr);
}

bool FillTank()
{
    const uintptr_t State = MissionState();
    if (!State)
        return false;
    // UFuelMeter (MaxRequiredFuel +0xF4, CurrentFuel +0xF8, docs/OFFSETS.md) mit dem GameState als Outer: der Gruppenwert.
    // Der Spike schreibt den Wert ebenso (tools/spike/fuel.py); die Replikation zum Client ist dort belegt.
    bool Done = false;
    for (int32_t Index = 0, Total = Objects::Count(); Index < Total; ++Index)
    {
        const uintptr_t Meter = Objects::At(Index);
        if (!Meter || Memory::ReadPointer(Meter + Offsets::UObject::Outer).value_or(0) != State ||
            !Objects::IsA(Meter, "FuelMeter"))
            continue;
        const float Max = Memory::Read<float>(Meter + 0xF4).value_or(0);
        if (!(std::isfinite(Max) && Max > 0 && Max < 100000))
            continue;
        Done |= Memory::WriteBytes(Meter + 0xF8, &Max, sizeof(Max));
    }
    return Done;
}

bool Finite(const Vector& Position)
{
    return std::isfinite(Position.X) && std::isfinite(Position.Y) && std::isfinite(Position.Z);
}

// Gegnertyp fuer den Dev-Spawn: der Ersatztyp der Spawner-Auswahl (AEnemySpawner::SelectionComponent +0x3E8,
// FallbackEnemyType +0xE0), sonst der Typ eines lebenden Gegners (AEnemyBase::Type +0x9B0).
uintptr_t ZombieType(uintptr_t Spawner)
{
    const uintptr_t Selection = Memory::ReadPointer(Spawner + 0x3E8).value_or(0);
    if (const uintptr_t Fallback = Selection ? Memory::ReadPointer(Selection + 0xE0).value_or(0) : 0)
        return Fallback;
    const uintptr_t Enemy = Objects::FindInstance("EnemyBase");
    return Enemy ? Memory::ReadPointer(Enemy + 0x9B0).value_or(0) : 0;
}

// Fuenf Gegner im Kreis um Target ueber den Spawner des Spiels, Stufe und Effekte wie beim normalen Spawn.
// (TrySpawnFreshEnemy lieferte im Test an Tag 1 immer false.) Gegner sind native replizierte Actors, der Client
// sieht sie ohne eigene Nachricht.
bool SpawnZombies(std::optional<Vector> Target)
{
    const uintptr_t Spawner = Objects::FindInstance("EnemySpawner");
    const uintptr_t Type = Spawner ? ZombieType(Spawner) : 0;
    if (!Type || !Target || !Finite(*Target))
        return false;
    int Spawned = 0;
    for (int Index = 0; Index < 5; ++Index)
    {
        const double Angle = Index * 1.2566370614359172; // 2 Pi / 5
        // FTransform mit Doubles: Rotation (Quat) +0x00, Translation +0x20, Scale3D +0x40, je auf 16 Byte ausgerichtet.
        struct alignas(16) Transform { double Rotation[4]; Vector Translation; double Pad1; Vector Scale; double Pad2; };
        struct
        {
            uintptr_t EnemyType, Instigator;
            Transform SpawnTransform;
            int32_t OverrideThreatLevel, OverrideBaseThreatLevel;
            bool ApplySpawnerGameplayEffects, ClampLocationToSpawnExtends; uint8_t Padding[6];
            uintptr_t ForcedSpawnBounds, ReturnValue;
        } Params{Type, 0,
            {{0, 0, 0, 1}, {Target->X + 80 * std::cos(Angle), Target->Y + 80 * std::sin(Angle), Target->Z}, 0, {1, 1, 1}, 0},
            -1, -1, true, false, {}, 0, 0};
        static_assert(sizeof(Params) == 0x90 && offsetof(decltype(Params), ReturnValue) == 0x88); // EnemySpawner_SpawnSpecificEnemy
        if (Call(Spawner, "EnemySpawner", "SpawnSpecificEnemy", &Params) && Params.ReturnValue)
            ++Spawned;
    }
    Core::Log::Info("Dev: {} von 5 Gegnern bei ({:.0f}, {:.0f}, {:.0f})", Spawned, Target->X, Target->Y, Target->Z);
    return Spawned > 0;
}

// Item-Daten, die normal als Loot vorkommen: kein Klassen-Defaultobjekt, nicht versteckt, kein Rucksack, mit Seltenheit.
std::vector<uintptr_t> DroppableItems()
{
    std::vector<uintptr_t> Items;
    for (int32_t Index = 0, Total = Objects::Count(); Index < Total; ++Index)
    {
        const uintptr_t Item = Objects::At(Index);
        const auto Flags = Item ? Memory::Read<uint32_t>(Item + Offsets::UObject::Flags) : std::nullopt;
        if (!Flags || (*Flags & Offsets::UObject::ClassDefaultObjectFlag) || !Objects::IsA(Item, "InventoryItemData"))
            continue;
        // UInventoryItemData: MinimumItemRarity +0xF0, IsBag +0x178, IsHiddenItem +0x190 (docs/OFFSETS.md).
        if (Memory::ReadPointer(Item + 0xF0).value_or(0) && !Memory::Read<bool>(Item + 0x178).value_or(true) &&
            !Memory::Read<bool>(Item + 0x190).value_or(true))
            Items.push_back(Item);
    }
    return Items;
}

// Vorlage des Itemsacks wie beim Pluendern: ALootSpot::ItemLootTemplate (+0x2E0) einer Lootstelle der Mission.
uintptr_t ItemLootTemplate()
{
    const uintptr_t Spot = Objects::FindInstance("LootSpot");
    return Spot ? Memory::ReadPointer(Spot + 0x2E0).value_or(0) : 0;
}

// Ein Itemsack mit einem zufaelligen Item neben der Figur. Das Aufsammeln laeuft ueber den vorhandenen Lootpfad.
bool SpawnRandomItem(int Player)
{
    const uintptr_t Pawn = PlayerPawn(Player);
    Vector Position{};
    if (!Pawn || !Call(Pawn, "Actor", "K2_GetActorLocation", &Position))
        return false;
    const auto Items = DroppableItems(); // Jedes Mal neu: nach Travel koennen alte Zeiger ungueltig sein.
    const uintptr_t Template = ItemLootTemplate();
    if (Items.empty() || !Template)
    {
        Core::Log::Warn("Dev: kein Item ({}) oder keine Itemsack-Vorlage", Items.size());
        return false;
    }
    static std::mt19937 Random{std::random_device{}()};
    const uintptr_t Item = Items[std::uniform_int_distribution<size_t>(0, Items.size() - 1)(Random)];
    struct
    {
        uintptr_t WorldContext, ItemData;
        bool Broken; uint8_t Padding[7];
        Vector Location;
        int32_t NumberOfItems; float Radius;
        uintptr_t SpawnTemplate;
        float ArcHeight; uint32_t Padding2;
        uintptr_t ReturnValue;
    } Params{Pawn, Item, false, {}, Position, 1, 150.f, Template, 100.f, 0, 0};
    static_assert(sizeof(Params) == 0x50); // LootBlueprintFunctionLibrary_SpawnSpecificItemLoot
    return Call(Objects::FindNamed("Default__LootBlueprintFunctionLibrary"), "LootBlueprintFunctionLibrary",
               "SpawnSpecificItemLoot", &Params) && Params.ReturnValue;
}
}

std::optional<DevPosition> CursorGroundPosition()
{
    static std::optional<DevPosition> Last;
    static auto Next = std::chrono::steady_clock::time_point{};
    const auto Now = std::chrono::steady_clock::now();
    if (Now < Next || !Core::GameThread().IsDrainThread())
        return Last;
    Next = Now + std::chrono::milliseconds(50);
    const uintptr_t World = Patches::CurrentWorld();
    if (!World || !MissionState())
        return Last = std::nullopt;
    // Erster lokaler Controller: beim Host der eigene, beim Client seiner. Nur er hat einen Viewport zum Projizieren.
    struct { uintptr_t WorldContext; int32_t PlayerIndex; uint32_t Padding; uintptr_t ReturnValue; } Get{World, 0, 0, 0};
    if (!Call(Objects::FindNamed("Default__GameplayStatics"), "GameplayStatics", "GetPlayerController", &Get) ||
        !Get.ReturnValue)
        return Last;
    struct { Vector Origin, Direction; bool ReturnValue; uint8_t Padding[7]; } Ray{};
    static_assert(sizeof(Ray) == 56); // PlayerController_DeprojectMousePositionToWorld
    uintptr_t Pawn = 0;
    Vector Feet{};
    if (!Call(Get.ReturnValue, "PlayerController", "DeprojectMousePositionToWorld", &Ray) || !Ray.ReturnValue ||
        !Call(Get.ReturnValue, "Controller", "K2_GetPawn", &Pawn) || !Pawn ||
        !Call(Pawn, "Actor", "K2_GetActorLocation", &Feet) || std::abs(Ray.Direction.Z) < 1e-3)
        return Last;
    // Strahl mit der Ebene auf Hoehe der Figur schneiden: die Kamera schaut schraeg von oben auf flachen Boden.
    const double Distance = (Feet.Z - Ray.Origin.Z) / Ray.Direction.Z;
    const Vector Hit{Ray.Origin.X + Distance * Ray.Direction.X, Ray.Origin.Y + Distance * Ray.Direction.Y, Feet.Z};
    if (Distance > 0 && Finite(Hit))
        Last = Hit;
    return Last;
}

bool RunDevAction(DevAction Action, int Player, std::optional<DevPosition> Target)
{
    if (!Core::GameThread().IsDrainThread() || Patches::CurrentRole() == NetRole::Client || !MissionState())
        return false;
    bool Done = false;
    switch (Action)
    {
    case DevAction::SpawnXp: Done = SpawnXp(Player); break;
    case DevAction::FullTank: Done = FillTank(); break;
    case DevAction::ToggleGodmode: Done = ToggleGodmode(Player); break;
    case DevAction::SpawnZombies: Done = SpawnZombies(Target); break;
    case DevAction::SpawnRandomItem: Done = SpawnRandomItem(Player); break;
    }
    Core::Log::Info("Dev: Aktion {} fuer Spieler {} {}", static_cast<int>(Action), Player, Done ? "ausgefuehrt" : "fehlgeschlagen");
    return Done;
}
}
