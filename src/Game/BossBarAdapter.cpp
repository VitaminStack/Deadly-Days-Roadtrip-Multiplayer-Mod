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
// Unteranzeigen, die OnObjectiveStarted ohne Prüfung liest (boss-leiste-wiederbeitritt), siehe CanNotifyObjectiveWidget.
constexpr Reflection::FieldSpec WidgetDescription{Package, "DDObjectiveWidget", "DescriptionText", "ObjectProperty", 8};
constexpr Reflection::FieldSpec WidgetProgressText{Package, "DDObjectiveWidget", "ProgressText", "ObjectProperty", 8};
constexpr Reflection::FieldSpec WidgetProgressBar{Package, "DDObjectiveWidget", "ProgressBar", "ObjectProperty", 8};
constexpr Reflection::FieldSpec WidgetCurrentObjective{Package, "DDObjectiveWidget", "CurrentObjective", "ObjectProperty", 8};
constexpr Reflection::FieldSpec AttributeMaxHealth{Package, "CommonAttributeSet", "MaxHealth", "StructProperty", 16};
constexpr Reflection::FieldSpec AttributeHealth{Package, "CommonAttributeSet", "Health", "StructProperty", 16};
// Phasenbezogene Leiste (opt-boss-phasenleiste), siehe docs/OFFSETS.md.
constexpr Reflection::FieldSpec ObjectiveMarkers{Package, "DDBossObjective", "PhaseMarkers", "ArrayProperty", 16};
constexpr Reflection::FieldSpec TypeBlueprint{Package, "EnemyType", "EnemyBlueprint", "SoftClassProperty", 0x28};
constexpr Reflection::FieldSpec LookupInfos{Package, "BossSpawnLookup", "BossSpawnInfos", "ArrayProperty", 16};
constexpr Reflection::FieldSpec SpawnPhaseBased{Package, "BossfightSpawnPoint", "IsObjectivePhaseBased", "BoolProperty", 1};
// FBossSpawnInfo (0x68): BossEnemyType und SpawnPointClass sind Soft-Verweise, der Pfad (Paket, Asset als FName) steht 8 Byte hinter
// dem Anfang. Per Reflexion im Spiel gelesen, Build 26786272.
constexpr uintptr_t SpawnInfoSize = 0x68;
constexpr uintptr_t SpawnInfoEnemyType = 0x10;
constexpr uintptr_t SpawnInfoPointClass = 0x38;
constexpr uintptr_t SoftPathPackage = 0x8;
constexpr uintptr_t SoftPathAsset = 0x10;
// UDDBossObjective, nicht reflektiert: BossfightSpawnPoint::StartBossObjective kopiert IsObjectivePhaseBased hierher, bevor es den
// Boss anmeldet (Disassembly 0x58CBC2D). Die Anzeige liest es beim Start.
constexpr uintptr_t ObjectivePhaseBased = 0x98;
// Tags des Bosses: UAbilitySystemComponent.GameplayTagCountContainer (reflektiert), darin die Liste der eigenen Tags
// (ExplicitTags.GameplayTags, TArray<FGameplayTag>) bei +0xC8. Die Tags replizieren zum Client (gemessen 21.11.2026).
constexpr Reflection::FieldSpec AbilityTags{"/Script/GameplayAbilities", "AbilitySystemComponent", "GameplayTagCountContainer",
                                            "StructProperty", 0xF0};
constexpr uintptr_t TagContainerExplicit = 0xC8;
constexpr std::string_view PausedTag = "State.Pause";

enum class Kind
{
    Other,
    Boss,
    Objective,
    Widget,
    AttributeSet,
    Abilities
};

std::string_view KindName(Kind Value)
{
    switch (Value)
    {
    case Kind::Boss:
        return "Boss";
    case Kind::Objective:
        return "Objective";
    case Kind::Widget:
        return "Anzeige";
    case Kind::AttributeSet:
        return "Attributsatz";
    case Kind::Abilities:
        return "Fähigkeiten";
    default:
        return "anderes";
    }
}

struct ClassEntry
{
    CachedClassKind Check;
    Kind Value = Kind::Other;
};

// Nummer des laufenden Scans für ClassKindNeedsCheck, 0 heißt nie geprüft.
uint32_t ScanNumber = 0;

Kind ComputeKind(uintptr_t Class);

// Rolle einer Klasse über ihre Oberklassen, je Klasse gemerkt; es sind viele Objekte, aber wenige Klassen. Ein Eintrag gilt nur, solange
// an der Adresse dieselbe Klasse steht (boss-leiste-wiederbeitritt), geprüft einmal je Scan.
Kind KindOf(uintptr_t Class)
{
    static std::unordered_map<uintptr_t, ClassEntry> Cache;
    auto Hit = Cache.find(Class);
    if (Hit != Cache.end() && !ClassKindNeedsCheck(Hit->second.Check, ScanNumber))
        return Hit->second.Value;
    const int32_t NameNow = Memory::Read<int32_t>(Class + Offsets::UObject::Name).value_or(-1);
    if (Hit != Cache.end())
    {
        Hit->second.Check.CheckedScan = ScanNumber;
        if (ClassKindStillValid(Hit->second.Check, NameNow))
            return Hit->second.Value;
    }
    const Kind Result = ComputeKind(Class);
    if (Hit != Cache.end() && (Hit->second.Value != Kind::Other || Result != Kind::Other))
        Core::Log::Info("Boss-Leiste: Klasse an {:#x} gewechselt, vorher {}, jetzt {} ({})", Class, KindName(Hit->second.Value),
                        KindName(Result), Memory::NameOf(Class));
    Cache[Class] = {{NameNow, ScanNumber}, Result};
    return Result;
}

Kind ComputeKind(uintptr_t Class)
{
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
        else if (Name == "AbilitySystemComponent")
            Result = Kind::Abilities;
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
    return Result;
}

struct Scan
{
    std::vector<uintptr_t> Bosses; // lebende BP_BossEnemy_C
    uintptr_t Boss = 0;            // davon der für die Leiste gewählte, siehe PickBoss
    uintptr_t Objective = 0;
    std::vector<uintptr_t> Widgets;                    // nur fertig aufgebaute Anzeigen mit Objective-Manager
    size_t Unbuilt = 0;                                // lebende Anzeigen mit Manager, aber ohne Unteranzeigen, übergangen
    size_t Dying = 0;                                  // Anzeigen mit Manager, die das Spiel gerade freigibt, übergangen
    std::vector<std::pair<uintptr_t, uintptr_t>> Sets; // (Outer, CommonAttributeSet)
    std::vector<std::pair<uintptr_t, uintptr_t>> Abilities; // (Outer, AbilitySystemComponent)
};

// Alle je von uns angelegten Objectives. Das Spiel entfernt sie beim Client nie (das Subsystem der GameInstance hält sie), und sie
// überleben auch Reisen und das Zurücksetzen von S. Ohne diese Liste galt das vorletzte eigene Objective als fremd und sperrte jede
// weitere Leiste (gemessen 18.11.2026: nach SpikeBoss und Ballerina keine Leiste mehr für Frankenstein).
std::unordered_set<uintptr_t>& OwnObjectives()
{
    static std::unordered_set<uintptr_t> Own;
    return Own;
}

// Felder der Anzeige, per Reflexion aufgelöst.
struct WidgetFields
{
    uintptr_t Manager = 0;
    uintptr_t Description = 0;
    uintptr_t ProgressText = 0;
    uintptr_t ProgressBar = 0;
};

Scan ScanObjects(uintptr_t Current, const WidgetFields& Fields)
{
    if (++ScanNumber == 0)
        ScanNumber = 1;
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
        {
            // Die zweite Instanz ohne Subsystem stürzt bei OnObjective* ab, eine Anzeige ohne Unteranzeigen bei OnObjectiveStarted.
            // Eine Anzeige der vorigen Welt, die das Spiel gerade freigibt, steht noch im Objekt-Array.
            const auto Set = [Object](uintptr_t Offset) { return Memory::ReadPointer(Object + Offset).value_or(0) != 0; };
            const ObjectiveWidgetView View{Set(Fields.Manager), Set(Fields.Description), Set(Fields.ProgressText), Set(Fields.ProgressBar),
                                           Objects::IsLive({Object, i, 0})};
            if (CanNotifyObjectiveWidget(View))
                Result.Widgets.push_back(Object);
            else if (View.Manager && !View.Live)
                ++Result.Dying;
            else if (View.Manager)
                ++Result.Unbuilt;
            break;
        }
        case Kind::AttributeSet:
            if (const auto Outer = Memory::ReadPointer(Object + Offsets::UObject::Outer))
                Result.Sets.emplace_back(*Outer, Object);
            break;
        case Kind::Abilities:
            if (const auto Outer = Memory::ReadPointer(Object + Offsets::UObject::Outer))
                Result.Abilities.emplace_back(*Outer, Object);
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

// Ob der Host die Leiste dieses Bosses phasenbezogen führt. Der Client hat keinen Spawnpunkt, und der Typ des Bosses (EnemyBase.Type)
// repliziert nicht. Die Datentabelle DA_BossSpawnLookup ist aber auch bei ihm geladen: Sie nennt je Boss den EnemyType, dessen
// EnemyBlueprint die Klasse des Bosses ist, und die Spawnpunkt-Klasse, deren Vorgabe der Host ins Objective schreibt. Je Boss-Klasse
// einmal ermittelt. Ohne Ergebnis bleibt es bei der Leiste über die vollen HP.
bool ResolvePhaseBased(uintptr_t Boss)
{
    static std::unordered_map<std::string, bool> Known;
    const auto InfosField = Reflection::Property(LookupInfos);
    const auto PhaseField = Reflection::Property(SpawnPhaseBased);
    const auto BlueprintField = Reflection::Property(TypeBlueprint);
    const auto Class = Memory::ReadPointer(Boss + Offsets::UObject::Class);
    const std::string ClassName = Class ? Memory::NameOf(*Class) : std::string();
    if (ClassName.empty() || !InfosField || !PhaseField || !BlueprintField)
        return false;
    if (const auto Hit = Known.find(ClassName); Hit != Known.end())
        return Hit->second;
    const uintptr_t Lookup = Objects::FindInstance("BossSpawnLookup");
    const uintptr_t Data = Lookup ? Memory::ReadPointer(Lookup + InfosField->Offset).value_or(0) : 0;
    const int32_t Count = Lookup ? Memory::Read<int32_t>(Lookup + InfosField->Offset + 8).value_or(0) : 0;
    if (!Data || Count <= 0 || Count > 64)
    {
        // Die Tabelle kann noch fehlen; nicht merken, beim nächsten Anlegen neu versuchen.
        Core::Log::Warn("Boss-Leiste: Tabelle der Boss-Spawnpunkte nicht lesbar, Leiste über die vollen HP");
        return false;
    }
    const auto SoftName = [](uintptr_t Soft, uintptr_t Part) {
        const auto Index = Memory::Read<int32_t>(Soft + Part);
        return Index ? Memory::NameFromIndex(*Index) : std::string();
    };
    bool Result = false;
    std::string Found = "kein Eintrag";
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Info = Data + static_cast<uintptr_t>(i) * SpawnInfoSize;
        const std::string TypePackage = SoftName(Info + SpawnInfoEnemyType, SoftPathPackage);
        const std::string TypeAsset = SoftName(Info + SpawnInfoEnemyType, SoftPathAsset);
        if (TypePackage.empty() || TypeAsset.empty())
            continue;
        uintptr_t Type = Objects::FindObject(TypeAsset, TypePackage);
        if (!Type)
            Type = Objects::LoadAsset(TypePackage + "." + TypeAsset);
        if (!Type || !Objects::IsA(Type, "EnemyType") || SoftName(Type + BlueprintField->Offset, SoftPathAsset) != ClassName)
            continue;
        const std::string PointPackage = SoftName(Info + SpawnInfoPointClass, SoftPathPackage);
        const std::string PointAsset = SoftName(Info + SpawnInfoPointClass, SoftPathAsset);
        if (PointPackage.empty() || PointAsset.empty())
        {
            Found = TypeAsset + " ohne Spawnpunkt-Klasse";
            break;
        }
        uintptr_t PointClass = Objects::FindObject(PointAsset, PointPackage);
        if (!PointClass)
            PointClass = Objects::LoadAsset(PointPackage + "." + PointAsset);
        const uintptr_t Defaults = PointClass ? Objects::FindNamed("Default__" + PointAsset) : 0;
        const auto Flag = Defaults && Objects::IsA(Defaults, "BossfightSpawnPoint")
                              ? Memory::Read<uint8_t>(Defaults + PhaseField->Offset)
                              : std::nullopt;
        if (!Flag)
        {
            Found = PointAsset + " nicht ladbar";
            break;
        }
        Result = (*Flag & PhaseField->BoolMask) != 0;
        Found = PointAsset;
        break;
    }
    Known[ClassName] = Result;
    Core::Log::Info("Boss-Leiste: {} ist {} (Spawnpunkt {})", ClassName, Result ? "phasenbezogen" : "nicht phasenbezogen", Found);
    return Result;
}

// Ob der Boss das Tag State.Pause trägt: 1 ja, 0 nein, -1 nicht lesbar. Siehe BossFightStarted.
int ReadPaused(const Scan& Found, uintptr_t Boss)
{
    const auto Field = Reflection::Property(AbilityTags);
    if (!Field)
        return -1;
    for (const auto& [Outer, Component] : Found.Abilities)
    {
        if (Outer != Boss)
            continue;
        const uintptr_t List = Component + Field->Offset + TagContainerExplicit;
        const uintptr_t Data = Memory::ReadPointer(List).value_or(0);
        const auto Count = Memory::Read<int32_t>(List + 8);
        if (!Count || *Count < 0 || *Count > 256 || (*Count > 0 && !Data))
            return -1;
        for (int32_t i = 0; i < *Count; ++i)
        {
            const auto Index = Memory::Read<int32_t>(Data + static_cast<uintptr_t>(i) * 8);
            if (!Index)
                return -1;
            if (Memory::NameFromIndex(*Index) == PausedTag)
                return 1;
        }
        return 0;
    }
    return -1;
}

// PhaseMarkers des Objectives: Anteile am Maximum, an denen eine Phase endet.
std::vector<float> ReadMarkers(uintptr_t Objective)
{
    std::vector<float> Result;
    const auto Field = Reflection::Property(ObjectiveMarkers);
    const uintptr_t Data = Field ? Memory::ReadPointer(Objective + Field->Offset).value_or(0) : 0;
    const int32_t Count = Field ? Memory::Read<int32_t>(Objective + Field->Offset + 8).value_or(0) : 0;
    for (int32_t i = 0; Data && i < Count && i < 16; ++i)
        if (const auto Value = Memory::Read<float>(Data + static_cast<uintptr_t>(i) * sizeof(float)))
            Result.push_back(*Value);
    return Result;
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
    bool PhaseBased = false; // Leiste zeigt wie beim Host nur die HP der laufenden Phase
    int Phase = 0;           // zuletzt gezeigte Phase, 0 vor dem ersten Wert
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
    // Wie der Host: erst das Kennzeichen, dann den Boss anmelden.
    const bool PhaseBased = ResolvePhaseBased(Found.Boss);
    const uint8_t PhaseFlag = PhaseBased ? 1 : 0;
    Memory::WriteBytes(Objective + ObjectivePhaseBased, &PhaseFlag, sizeof(PhaseFlag));
    Objects::ProcessEvent(Objective, AddBoss, &AddParams);
    Memory::WriteBytes(Objective + MaxOffset, &Max, sizeof(Max));
    struct
    {
        uintptr_t World;
    } StartParams{*World};
    Objects::ProcessEvent(Objective, Start, &StartParams);
    // Diagnose (boss-leiste-wiederbeitritt): Ziel von OnObjectiveStarted mit frisch gelesener Klasse und bisherigem Objective.
    const auto CurrentField = Reflection::Property(WidgetCurrentObjective);
    const uintptr_t CurrentObjectiveOffset = CurrentField ? static_cast<uintptr_t>(CurrentField->Offset) : 0;
    for (const uintptr_t Widget : Found.Widgets)
    {
        const auto WidgetClass = Memory::ReadPointer(Widget + Offsets::UObject::Class);
        Core::Log::Info("Boss-Leiste: Anzeige {:#x} ({}), bisheriges Objective {:#x}", Widget, WidgetClass ? Memory::NameOf(*WidgetClass) : "?",
                        CurrentObjectiveOffset ? Memory::ReadPointer(Widget + CurrentObjectiveOffset).value_or(0) : 0);
    }
    NotifyWidgets(Found, Started, Objective);
    S.Ours = Objective;
    OwnObjectives().insert(Objective);
    S.Boss = Found.Boss;
    S.SettleTicks = 0;
    S.LastHealth = S.LastMax = -1;
    S.Failed = false;
    S.PhaseBased = PhaseBased;
    S.Phase = 0;
    Core::Log::Info("Boss-Leiste: lokal angelegt (Maximum {}, Anzeigen {}{})", Max, Found.Widgets.size(),
                    PhaseBased ? ", phasenbezogen" : "");
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
    const auto DescriptionField = Reflection::Property(WidgetDescription);
    const auto ProgressTextField = Reflection::Property(WidgetProgressText);
    const auto ProgressBarField = Reflection::Property(WidgetProgressBar);
    if (!ManagerField || !SetMaxField || !SetHealthField || !ObjectiveMaxField || !DescriptionField || !ProgressTextField || !ProgressBarField)
        return;
    const auto MaxOffset = static_cast<uintptr_t>(ObjectiveMaxField->Offset);
    const WidgetFields Fields{static_cast<uintptr_t>(ManagerField->Offset), static_cast<uintptr_t>(DescriptionField->Offset),
                              static_cast<uintptr_t>(ProgressTextField->Offset), static_cast<uintptr_t>(ProgressBarField->Offset)};

    Scan Found = ScanObjects(S.Ours, Fields);
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
    // Ohne Leiste und ohne wählbaren Boss, obwohl einer da ist: 0 HP oder nicht lesbar, sonst bliebe das stumm.
    if (!S.Ours && Picked == NoBoss && !Candidates.empty())
    {
        const auto Class = Memory::ReadPointer(Candidates[0].Id + Offsets::UObject::Class);
        size_t BossSets = 0;
        for (const auto& Entry : Found.Sets)
            BossSets += Entry.first == Candidates[0].Id ? 1 : 0;
        NoteWait(std::format("{} Boss(e) da, keiner mit lesbaren HP ueber 0 (erster {}: {} von {}, {} Attributsaetze)", Candidates.size(),
                             Class ? Memory::NameOf(*Class) : "?", Candidates[0].Health, Candidates[0].MaxHealth, BossSets));
    }
    Found.Boss = Picked == NoBoss ? 0 : Candidates[Picked].Id;
    int64_t Health = Picked == NoBoss ? 0 : Candidates[Picked].Health;
    int64_t Max = Picked == NoBoss ? 0 : Candidates[Picked].MaxHealth;
    const bool Readable = Found.Boss && Health >= 1 && Max > 1;
    BossBarState Input;
    Input.Client = true;
    Input.LiveBoss = Found.Boss != 0;
    // Nur vor dem Anlegen gebraucht: Der Host zeigt die Leiste erst, wenn der Kampf läuft.
    const int Paused = Found.Boss && !S.Ours ? ReadPaused(Found, Found.Boss) : -1;
    Input.Started = BossFightStarted(Paused, Health, Max);
    if (Found.Boss && !S.Ours && !Input.Started)
        NoteWait("Boss da, Kampf läuft noch nicht (State.Pause)");
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
            NoteWait(Found.Unbuilt || Found.Dying ? std::format("Boss da, keine fertige Anzeige ({} ohne Unteranzeigen, {} im Abbau)",
                                                                Found.Unbuilt, Found.Dying)
                                                  : std::string("Boss da, keine Anzeige mit Subsystem"));
        else
        {
            NoteWait({});
            if (Found.Unbuilt || Found.Dying)
                Core::Log::Info("Boss-Leiste: Anzeigen uebergangen, {} ohne Unteranzeigen, {} im Abbau", Found.Unbuilt, Found.Dying);
            Core::Log::Info("Boss-Leiste: Boss erkannt (Maximum {}), lege Objective an", Max);
            Create(Found, MaxOffset, Max);
        }
        break;
    case BossBarStep::Settle:
        ++S.SettleTicks;
        break;
    case BossBarStep::Sync:
    {
        BossPhaseView View{1, Health, Max};
        if (S.PhaseBased)
            View = PhaseOf(Health, Max, ReadMarkers(S.Ours));
        if (!BossBarNeedsUpdate(S.LastHealth, S.LastMax, View.Value, Max))
            break;
        const uintptr_t SetProgress = Reflection::Function(Package, "DDObjective", "SetCurrentProgressCounter");
        if (!SetProgress)
            break;
        Memory::WriteBytes(S.Ours + MaxOffset, &Max, sizeof(Max));
        struct
        {
            int64_t Progress;
        } Params{View.Value};
        Objects::ProcessEvent(S.Ours, SetProgress, &Params);
        if (S.LastHealth < 0)
            Core::Log::Info("Boss-Leiste: erste Werte {} von {}", Health, Max);
        if (S.PhaseBased && View.Phase != S.Phase)
            Core::Log::Info("Boss-Leiste: Phase {}, {} von {} (HP {} von {})", View.Phase, View.Value, View.Span, Health, Max);
        S.Phase = View.Phase;
        S.LastHealth = View.Value;
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
