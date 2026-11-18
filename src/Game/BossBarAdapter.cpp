#include "Game/BossBarAdapter.h"

#include <windows.h>

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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
constexpr std::string_view ObjectivePackage = "/Game/DDSurvivors/Blueprints/Objectives/DD_BaseBossObjective";
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
    bool Enemy = false;
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
        Enemy = Enemy || Name == "EnemyBase";
        Walk = Memory::ReadPointer(Walk + Offsets::UStruct::SuperStruct).value_or(0);
    }
    // Diagnose (opt-endboss-leiste): eine Boss-Klasse ausserhalb von BP_BossEnemy_C bliebe sonst stumm unerkannt.
    if (Result == Kind::Other && Enemy)
    {
        const std::string Name = Memory::NameOf(Class);
        if (Name.ends_with("Boss_C"))
            Core::Log::Warn("Boss-Leiste: Klasse {} erbt nicht von BP_BossEnemy_C, keine Leiste", Name);
    }
    Cache[Class] = Result;
    return Result;
}

struct Scan
{
    std::vector<uintptr_t> Bosses; // lebende BP_BossEnemy_C
    uintptr_t Boss = 0;            // davon der für die Leiste gewählte, siehe PickBoss
    uintptr_t Objective = 0;
    std::vector<uintptr_t> Widgets;                    // nur Anzeigen mit Objective-Manager
    std::vector<std::pair<uintptr_t, uintptr_t>> Sets; // (Outer, CommonAttributeSet)
};

// Alle je von uns angelegten Objectives. Das Spiel entfernt sie beim Client nie (das Subsystem der GameInstance hält sie), und sie
// überleben auch Reisen und das Zurücksetzen von S. Ohne diese Liste galt das vorletzte eigene Objective als fremd und sperrte jede
// weitere Leiste (gemessen 18.11.2026: nach SpikeBoss und Ballerina keine Leiste mehr für Frankenstein).
std::unordered_set<uintptr_t>& OwnObjectives()
{
    static std::unordered_set<uintptr_t> Own;
    return Own;
}

Scan ScanObjects(uintptr_t Current, uintptr_t WidgetManagerOffset)
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
            if (const auto Actor = Memory::Read<uint8_t>(Object + ActorFlagsOffset); Actor && !(*Actor & ActorDestroyedMask))
                Result.Bosses.push_back(Object);
            break;
        case Kind::Objective:
            // Das aktuelle eigene zählt, frühere eigene nicht, jedes andere ist fremd.
            if (!Result.Objective && (Object == Current || !OwnObjectives().contains(Object)))
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
bool ReadBossHealth(const Scan& Found, uintptr_t Boss, uintptr_t HealthOffset, uintptr_t MaxOffset, int64_t& Health, int64_t& Max)
{
    float BestMax = 0, BestHealth = 0;
    for (const auto& [Outer, Set] : Found.Sets)
    {
        if (Outer != Boss)
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

// Diagnose (opt-endboss-leiste): warum gerade keine Leiste entsteht, nur bei Wechsel gemeldet, nicht je Tick.
void NoteWait(std::string_view Reason)
{
    static std::string Last;
    if (Reason == Last)
        return;
    Last = std::string(Reason);
    if (!Reason.empty())
        Core::Log::Info("Boss-Leiste: wartet, {}", Reason);
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
    uintptr_t Boss = 0;    // der Boss, für den es angelegt ist
    int SettleTicks = 0;
    int64_t LastHealth = -1, LastMax = -1;
    bool Failed = false;   // Anlegen ist gescheitert, einmal melden
} S;

bool Create(const Scan& Found, uintptr_t MaxOffset, int64_t Max)
{
    const uintptr_t Subsystem = Objects::FindInstance("DDObjectiveSubsystem");
    // Nur die Klasse im Paket. Jede Instanz heißt ebenfalls DD_BaseBossObjective_C (nur mit Nummer); ohne Outer fand die Suche ab dem
    // zweiten Anlegen eine Instanz, CreateAndStartObjective baute daraus ein Objekt ohne Klasse und der Client stürzte ab (Lesen 0xD8,
    // opt-endboss-leiste, Signatur wie boss-karte-absturz).
    uintptr_t Class = Objects::FindObject("DD_BaseBossObjective_C", ObjectivePackage);
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
    OwnObjectives().insert(Objective);
    S.Boss = Found.Boss;
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

    Scan Found = ScanObjects(S.Ours, static_cast<uintptr_t>(ManagerField->Offset));
    // Ein fremdes Objective (etwa vom Spiel selbst angelegt) lassen wir in Ruhe.
    if (Found.Objective && Found.Objective != S.Ours)
    {
        NoteWait(!Found.Bosses.empty() ? "fremdes Objective vorhanden, Boss da" : "fremdes Objective vorhanden");
        return;
    }
    if (!Found.Objective)
        S.Ours = 0;

    const auto HealthOffset = static_cast<uintptr_t>(SetHealthField->Offset);
    const auto SetMaxOffset = static_cast<uintptr_t>(SetMaxField->Offset);
    std::vector<BossCandidate> Candidates;
    for (const uintptr_t Boss : Found.Bosses)
    {
        BossCandidate Candidate{Boss};
        ReadBossHealth(Found, Boss, HealthOffset, SetMaxOffset, Candidate.Health, Candidate.MaxHealth);
        Candidates.push_back(Candidate);
    }
    const size_t Picked = PickBoss(Candidates, S.Ours ? S.Boss : 0);
    Found.Boss = Picked == NoBoss ? 0 : Candidates[Picked].Id;
    int64_t Health = Picked == NoBoss ? 0 : Candidates[Picked].Health;
    int64_t Max = Picked == NoBoss ? 0 : Candidates[Picked].MaxHealth;
    const bool Readable = Found.Boss && Health >= 1 && Max > 1;
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
        if (!Readable)
        {
            size_t BossSets = 0;
            for (const auto& Entry : Found.Sets)
                BossSets += Entry.first == Found.Boss ? 1 : 0;
            const auto Class = Memory::ReadPointer(Found.Boss + Offsets::UObject::Class);
            NoteWait(std::format("Boss {} da, HP nicht lesbar ({} Attributsaetze am Boss)", Class ? Memory::NameOf(*Class) : "?", BossSets));
        }
        else if (Found.Widgets.empty())
            NoteWait("Boss da, keine Anzeige mit Subsystem");
        else
        {
            NoteWait({});
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
        if (S.LastHealth < 0)
            Core::Log::Info("Boss-Leiste: erste Werte {} von {}", Health, Max);
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
        S.Ours = 0;
        S.Boss = 0;
        break;
    }
    case BossBarStep::None:
        break;
    }
}
}
