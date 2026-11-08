#include "Game/BossBarAdapter.h"

#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Core/Log.h"
#include "Game/BossBarPolicy.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/Reflection.h"
#include "Game/WorldAdapter.h"

namespace Game
{
namespace
{
constexpr ULONGLONG CheckIntervalMs = 1000;
constexpr std::string_view Package = "/Script/DDSurvivors";
constexpr std::string_view ObjectiveAsset = "/Game/DDSurvivors/Blueprints/Objectives/DD_BaseBossObjective.DD_BaseBossObjective_C";
constexpr uint32_t NonInstanceFlags = 0x30;      // Klassen-Default- und Archetyp-Objekt
constexpr uintptr_t ActorFlagsOffset = 0x5D;     // AActor, Bit 0x02 = bActorIsBeingDestroyed
constexpr uint8_t ActorDestroyedMask = 0x02;
constexpr uintptr_t AttributeCurrentValue = 0xC; // FGameplayAttributeData: CurrentValue

constexpr Reflection::FieldSpec ObjectiveMaxHealth{Package, "DDBossObjective", "MaxHealth", "Int64Property", 8};
constexpr Reflection::FieldSpec WidgetManager{Package, "DDObjectiveWidget", "ObjectiveManager", "ObjectProperty", 8};
constexpr Reflection::FieldSpec AttributeMaxHealth{Package, "CommonAttributeSet", "MaxHealth", "StructProperty", 16};
constexpr Reflection::FieldSpec AttributeHealth{Package, "CommonAttributeSet", "Health", "StructProperty", 16};

enum class Kind
{
    Other,
    Boss,
    Objective,
    Widget,
    AttributeSet
};

// Rolle einer Klasse über ihre Oberklassen, je Klasse einmal berechnet; es sind viele Objekte, aber wenige Klassen.
Kind KindOf(uintptr_t Class)
{
    static std::unordered_map<uintptr_t, Kind> Cache;
    if (const auto Hit = Cache.find(Class); Hit != Cache.end())
        return Hit->second;
    Kind Result = Kind::Other;
    uintptr_t Walk = Class;
    for (int Depth = 0; Walk && Depth < 64 && Result == Kind::Other; ++Depth)
    {
        const std::string Name = Memory::NameOf(Walk);
        if (Name == "BP_BossEnemy_C")
            Result = Kind::Boss;
        else if (Name == "DDBossObjective")
            Result = Kind::Objective;
        else if (Name == "DDObjectiveWidget")
            Result = Kind::Widget;
        else if (Name == "CommonAttributeSet")
            Result = Kind::AttributeSet;
        Walk = Memory::ReadPointer(Walk + Offsets::UStruct::SuperStruct).value_or(0);
    }
    Cache[Class] = Result;
    return Result;
}

struct Scan
{
    uintptr_t Boss = 0;
    uintptr_t Objective = 0;
    std::vector<uintptr_t> Widgets;                    // nur Anzeigen mit Objective-Manager
    std::vector<std::pair<uintptr_t, uintptr_t>> Sets; // (Outer, CommonAttributeSet)
};

Scan ScanObjects(uintptr_t Ignore, uintptr_t WidgetManagerOffset)
{
    Scan Result;
    const int32_t Total = Objects::Count();
    for (int32_t i = 0; i < Total; ++i)
    {
        const uintptr_t Object = Objects::At(i);
        if (!Object)
            continue;
        const auto Flags = Memory::Read<uint32_t>(Object + Offsets::UObject::Flags);
        const auto Class = Memory::ReadPointer(Object + Offsets::UObject::Class);
        if (!Flags || (*Flags & NonInstanceFlags) || !Class)
            continue;
        switch (KindOf(*Class))
        {
        case Kind::Boss:
            // Das Flag liegt in AActor, erst lesen, wenn die Klasse sicher ein Boss ist.
            if (!Result.Boss)
            {
                const auto Actor = Memory::Read<uint8_t>(Object + ActorFlagsOffset);
                if (Actor && !(*Actor & ActorDestroyedMask))
                    Result.Boss = Object;
            }
            break;
        case Kind::Objective:
            if (!Result.Objective && Object != Ignore)
                Result.Objective = Object;
            break;
        case Kind::Widget:
            // Die zweite Instanz ohne Subsystem stürzt bei OnObjective* ab.
            if (Memory::ReadPointer(Object + WidgetManagerOffset))
                Result.Widgets.push_back(Object);
            break;
        case Kind::AttributeSet:
            if (const auto Outer = Memory::ReadPointer(Object + Offsets::UObject::Outer))
                Result.Sets.emplace_back(*Outer, Object);
            break;
        default:
            break;
        }
    }
    return Result;
}

// Lebenspunkte des Bosses: der Attributsatz mit dem großen Maximum (der Boss besitzt mehrere Sätze).
bool ReadBossHealth(const Scan& Found, uintptr_t HealthOffset, uintptr_t MaxOffset, int64_t& Health, int64_t& Max)
{
    float BestMax = 0, BestHealth = 0;
    for (const auto& [Outer, Set] : Found.Sets)
    {
        if (Outer != Found.Boss)
            continue;
        const auto SetMax = Memory::Read<float>(Set + MaxOffset + AttributeCurrentValue);
        if (!SetMax || *SetMax <= BestMax)
            continue;
        const auto SetHealth = Memory::Read<float>(Set + HealthOffset + AttributeCurrentValue);
        if (SetHealth)
        {
            BestMax = *SetMax;
            BestHealth = *SetHealth;
        }
    }
    Health = static_cast<int64_t>(BestHealth);
    Max = static_cast<int64_t>(BestMax);
    return BestMax > 1;
}

void NotifyWidgets(const Scan& Found, uintptr_t Function, uintptr_t Objective)
{
    struct
    {
        uintptr_t Objective;
    } Params{Objective};
    for (const uintptr_t Widget : Found.Widgets)
        Objects::ProcessEvent(Widget, Function, &Params);
}

struct State
{
    uintptr_t Ours = 0;    // von uns angelegtes Objective
    uintptr_t Retired = 0; // früher von uns angelegt und wieder entfernt, wird ignoriert
    int SettleTicks = 0;
    int64_t LastHealth = -1, LastMax = -1;
    bool Failed = false;   // Anlegen ist gescheitert, einmal melden
} S;

bool Create(const Scan& Found, uintptr_t MaxOffset, int64_t Max)
{
    const uintptr_t Subsystem = Objects::FindInstance("DDObjectiveSubsystem");
    uintptr_t Class = Objects::FindObject("DD_BaseBossObjective_C", "");
    if (!Class)
        Class = Objects::LoadAsset(ObjectiveAsset);
    const uintptr_t CreateFn = Reflection::Function(Package, "DDObjectiveSubsystem", "CreateAndStartObjective");
    const uintptr_t AddBoss = Reflection::Function(Package, "DDBossObjective", "AddBossEnemy");
    const uintptr_t Start = Reflection::Function(Package, "DDObjective", "StartTracking");
    const uintptr_t Started = Reflection::Function(Package, "DDObjectiveWidget", "OnObjectiveStarted");
    const auto World = Memory::ReadPointer(Memory::Rva(Offsets::GWorld));
    if (!Subsystem || !Class || !CreateFn || !AddBoss || !Start || !Started || !World)
    {
        if (!S.Failed)
            Core::Log::Error("Boss-Leiste: nicht moeglich (Subsystem {}, Klasse {}, Funktionen {}/{}/{}/{}, Welt {})", Subsystem != 0,
                             Class != 0, CreateFn != 0, AddBoss != 0, Start != 0, Started != 0, World.has_value());
        S.Failed = true;
        return false;
    }
    // Ohne AutoStart: Boss anmelden, MaxHealth setzen, dann starten. Die Phasenmarken rechnen beim Start mit MaxHealth.
    struct
    {
        uintptr_t Class;
        bool AutoStart;
        uint8_t Pad[7];
        uintptr_t Result;
    } CreateParams{Class, false, {}, 0};
    Objects::ProcessEvent(Subsystem, CreateFn, &CreateParams);
    const uintptr_t Objective = CreateParams.Result;
    if (!Objective)
    {
        if (!S.Failed)
            Core::Log::Error("Boss-Leiste: Objective nicht entstanden");
        S.Failed = true;
        return false;
    }
    struct
    {
        uintptr_t Boss;
        bool Result;
        uint8_t Pad[7];
    } AddParams{Found.Boss, false, {}};
    Objects::ProcessEvent(Objective, AddBoss, &AddParams);
    Memory::WriteBytes(Objective + MaxOffset, &Max, sizeof(Max));
    struct
    {
        uintptr_t World;
    } StartParams{*World};
    Objects::ProcessEvent(Objective, Start, &StartParams);
    NotifyWidgets(Found, Started, Objective);
    S.Ours = Objective;
    S.SettleTicks = 0;
    S.LastHealth = S.LastMax = -1;
    S.Failed = false;
    Core::Log::Info("Boss-Leiste: lokal angelegt (Maximum {}, Anzeigen {})", Max, Found.Widgets.size());
    return true;
}
}

void TickBossBar()
{
    static ULONGLONG LastCheck = 0;
    const ULONGLONG Now = GetTickCount64();
    if (Now - LastCheck < CheckIntervalMs)
        return;
    LastCheck = Now;

    const auto World = ReadWorld();
    if (!World || World->Role != NetRole::Client)
    {
        S = {};
        return;
    }
    const auto ManagerField = Reflection::Property(WidgetManager);
    const auto SetMaxField = Reflection::Property(AttributeMaxHealth);
    const auto SetHealthField = Reflection::Property(AttributeHealth);
    const auto ObjectiveMaxField = Reflection::Property(ObjectiveMaxHealth);
    if (!ManagerField || !SetMaxField || !SetHealthField || !ObjectiveMaxField)
        return;
    const auto MaxOffset = static_cast<uintptr_t>(ObjectiveMaxField->Offset);

    const Scan Found = ScanObjects(S.Retired, static_cast<uintptr_t>(ManagerField->Offset));
    // Ein fremdes Objective (etwa vom Spiel selbst angelegt) lassen wir in Ruhe.
    if (Found.Objective && Found.Objective != S.Ours)
        return;
    if (!Found.Objective)
        S.Ours = 0;

    int64_t Health = 0, Max = 0;
    const bool Readable = Found.Boss && ReadBossHealth(Found, static_cast<uintptr_t>(SetHealthField->Offset),
                                                       static_cast<uintptr_t>(SetMaxField->Offset), Health, Max);
    BossBarState Input;
    Input.Client = true;
    Input.LiveBoss = Found.Boss != 0;
    Input.HasObjective = S.Ours != 0;
    Input.Shown = S.Ours != 0;
    Input.SettleTicks = S.SettleTicks;
    Input.Health = Readable ? Health : 0;
    Input.MaxHealth = Readable ? Max : 0;

    switch (DecideBossBar(Input))
    {
    case BossBarStep::Create:
        // Das Maximum muss vor dem Start bekannt sein, sonst fehlen die Phasenmarken oder der Client stürzt ab.
        // Ohne Anzeige mit Subsystem (Ziel-Widget noch nicht bereit) später erneut versuchen.
        if (Readable && !Found.Widgets.empty())
        {
            Core::Log::Info("Boss-Leiste: Boss erkannt (Maximum {}), lege Objective an", Max);
            Create(Found, MaxOffset, Max);
        }
        break;
    case BossBarStep::Settle:
        ++S.SettleTicks;
        break;
    case BossBarStep::Sync:
    {
        if (!BossBarNeedsUpdate(S.LastHealth, S.LastMax, Health, Max))
            break;
        const uintptr_t SetProgress = Reflection::Function(Package, "DDObjective", "SetCurrentProgressCounter");
        if (!SetProgress)
            break;
        Memory::WriteBytes(S.Ours + MaxOffset, &Max, sizeof(Max));
        struct
        {
            int64_t Progress;
        } Params{Health};
        Objects::ProcessEvent(S.Ours, SetProgress, &Params);
        S.LastHealth = Health;
        S.LastMax = Max;
        break;
    }
    case BossBarStep::Remove:
    {
        // Das Objective bleibt als Objekt liegen, das Spiel entfernt es beim Client nie. Wir merken es uns und legen beim nächsten Boss neu an.
        if (const uintptr_t Removed = Reflection::Function(Package, "DDObjectiveWidget", "OnObjectiveRemoved"))
            NotifyWidgets(Found, Removed, S.Ours);
        Core::Log::Info("Boss-Leiste: Boss weg, Leiste entfernt");
        S.Retired = S.Ours;
        S.Ours = 0;
        break;
    }
    case BossBarStep::None:
        break;
    }
}
}
