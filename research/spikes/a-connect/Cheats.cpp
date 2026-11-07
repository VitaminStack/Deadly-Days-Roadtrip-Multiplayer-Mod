// Testhilfen: CheatManager fuer die DD_-Befehle des Spiels und Laden einzelner Assets, etwa fuer einen SDK-Dump mit Bossen.
#include "Spike.h"
#include "TestGodmodePolicy.h"

namespace Spike
{
namespace
{
constexpr uintptr_t kControllerCheatManager = 0x410;
bool TestGodmodeEnabled = false; // Explicit opt-in; Spike only, never a production mod setting.
struct ProtectedPawn
{
    uintptr_t Pawn;
    uintptr_t AbilitySystem;
    int32_t Name;
    int32_t Index;
};
std::vector<ProtectedPawn> GodmodeOwned;
}

// Uses the game's loose-tag API, not healing or fabricated replicated health. A private +1 contribution can be
// removed without clearing invincibility granted by gameplay effects. Never dereference a cached old-world pawn.
void TickTestGodmode()
{
    if (!TestGodmodeEnabled && GodmodeOwned.empty())
        return;
    const uint64_t Tag = Read<uint64_t>(Base + Rva(0xAC61FE8));
    if (!Tag || NameFromIndex(static_cast<int32_t>(Tag)) != "State.Invincible")
        return;
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t State = World ? Read<uintptr_t>(World + kWorldGameState) : 0;
    const uintptr_t Players = State ? Read<uintptr_t>(State + kGameStatePlayerArray) : 0;
    const int32_t Count = State ? Read<int32_t>(State + kGameStatePlayerArray + 8) : 0;
    if (!Players || Count <= 0 || Count > 16)
        return;
    std::vector<ProtectedPawn> Roster;
    // Do not forget ownership while the roster is partially replicated; the same pawn may return next tick.
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Player = Read<uintptr_t>(Players + i * 8);
        const uintptr_t Pawn = Player ? Read<uintptr_t>(Player + 0x320) : 0;
        if (!Pawn || (Read<uint32_t>(Pawn + 8) & 0x10))
            return;
        bool IsCharacter = false;
        for (uintptr_t Class = Read<uintptr_t>(Pawn + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
            if (NameOf(Class) == "DDCharacter") { IsCharacter = true; break; }
        if (!IsCharacter)
            return;
        const uintptr_t Asc = Read<uintptr_t>(Pawn + 0x6E0);
        if (!Asc)
            return;
        bool Duplicate = false;
        for (const auto& Existing : Roster)
            Duplicate |= Existing.AbilitySystem == Asc;
        if (!Duplicate)
            Roster.push_back({Pawn, Asc, Read<int32_t>(Pawn + 0x18), Read<int32_t>(Pawn + 0x0C)});
    }
    std::vector<ProtectedPawn> Current;
    for (const auto& Entry : Roster)
    {
        const uintptr_t Pawn = Entry.Pawn;
        const uintptr_t Asc = Entry.AbilitySystem;
        bool Owned = false;
        for (const auto& Old : GodmodeOwned)
            Owned |= Old.Pawn == Pawn && Old.AbilitySystem == Asc && Old.Name == Entry.Name && Old.Index == Entry.Index;
        using HasTagFn = bool(__fastcall*)(uintptr_t, uint64_t);
        const uintptr_t Interface = Asc + 0x148;
        const bool HasTag = Read<HasTagFn*>(Interface)[3](Interface, Tag);
        using UpdateTagFn = void(__fastcall*)(uintptr_t, const uint64_t*, int32_t, uint8_t);
        const auto Update = reinterpret_cast<UpdateTagFn>(Base + Rva(0x4B7D7D0));
        const int Delta = TestGodmodePolicy::Delta(TestGodmodeEnabled, Owned, HasTag);
        if (Delta > 0)
        {
            Update(Asc, &Tag, 1, 0);
            Owned = true;
            Log("Test-Godmode: State.Invincible gesetzt fuer " + NameOf(Pawn));
        }
        else if (!TestGodmodeEnabled && Owned)
        {
            if (Delta < 0)
                Update(Asc, &Tag, -1, 0);
            Owned = false;
            Log("Test-Godmode: eigener Tag-Beitrag entfernt fuer " + NameOf(Pawn));
        }
        if (Owned)
            Current.push_back(Entry);
    }
    GodmodeOwned = std::move(Current);
}

void SetTestGodmode(bool Enabled)
{
    TestGodmodeEnabled = Enabled;
    Log(Enabled ? "Test-Godmode: EIN, bleibt ueber Reisen aktiv" : "Test-Godmode: AUS");
    TickTestGodmode();
}

// Im Shipping-Build bleibt APlayerController::CheatManager leer, die Konsole verwirft dann alle DD_-Befehle.
// GameplayStatics.SpawnObject legt den Manager mit dem Controller als Outer an, wie es die Engine selbst taete.
void EnableCheats()
{
    const uintptr_t Statics = FindObject("Default__GameplayStatics", nullptr);
    const uintptr_t SpawnObject = FindObject("SpawnObject", "GameplayStatics");
    const uintptr_t CheatClass = FindObject("DDCheatManager", nullptr);
    if (!Statics || !SpawnObject || !CheatClass)
    {
        Log("Cheats: Klassen nicht gefunden");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    int Enabled = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        bool IsController = false;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class && !IsController; Class = Read<uintptr_t>(Class + 0x40))
            IsController = NameOf(Class) == "PlayerController";
        // Das Feld erst lesen, wenn das Objekt sicher ein Controller ist, kleinere Objekte enden davor.
        if (!IsController || Read<uintptr_t>(Object + kControllerCheatManager))
            continue;
        struct { uintptr_t ObjectClass; uintptr_t Outer; uintptr_t ReturnValue; } Params{CheatClass, Object, 0};
        Read<ProcessEventFn*>(Statics)[kProcessEventIndex](Statics, SpawnObject, &Params);
        if (!Params.ReturnValue)
            continue;
        *reinterpret_cast<uintptr_t*>(Object + kControllerCheatManager) = Params.ReturnValue;
        ++Enabled;
    }
    Log("Cheats: CheatManager fuer " + std::to_string(Enabled) + " Controller angelegt");
}

// Testhilfe (werkbank-client): Host legt jedem Spielerinventar ein Item an, "!give ItemData_Pistol [Rarity]". Natives
// CreateAndAddItem, das Item repliziert wie jede Vergabe. Nur beim Host sinnvoll, beim Client entstuende eine lokale Instanz.
void GiveItemCommand(const std::wstring& Args)
{
    const std::string Text(Args.begin(), Args.end());
    const size_t Space = Text.find(' ');
    const std::string DataName = Text.substr(0, Space), RarityName = Space == std::string::npos ? std::string{} : Text.substr(Space + 1);
    const uintptr_t Data = FindObject(DataName.c_str(), nullptr);
    const uintptr_t Rarity = RarityName.empty() ? 0 : FindObject(RarityName.c_str(), nullptr);
    const uintptr_t Create = FindObject("CreateAndAddItem", "InventoryComponent");
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    if (!Data || !Create || !World || !Read<uintptr_t>(World + kWorldGameMode) || (!RarityName.empty() && !Rarity))
    {
        Log("Give: " + DataName + " nicht moeglich (Daten, Seltenheit oder kein Host)");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    int Given = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0 || NameOf(Read<uintptr_t>(Object + 0x10)) != "InventoryComponent")
            continue;
        const uintptr_t Owner = Read<uintptr_t>(Object + 0x20);
        if (!Owner || NameOf(Read<uintptr_t>(Owner + 0x10)).find("PlayerState") == std::string::npos)
            continue;
        struct
        {
            uintptr_t Data, Rarity;
            bool Broken, FlagAsNew, SystemAdd;
            uint8_t Pad;
            int32_t Position;
            bool AllowP2Reserve;
            uint8_t Pad2[7];
            uintptr_t Result;
        } Params{Data, Rarity, false, true, true, 0, -1, false, {}, 0};
        static_assert(sizeof(Params) == 0x28);
        Read<ProcessEventFn*>(Object)[kProcessEventIndex](Object, Create, &Params);
        Given += Params.Result != 0;
    }
    Log("Give: " + DataName + (RarityName.empty() ? "" : " " + RarityName) + " an " + std::to_string(Given) + " Inventare");
}

// Testhilfe (gegner-alle): Host spawnt einen Boss ueber EnemySpawner::SpawnSpecificEnemy, "!boss SpikeBoss [Abstand]".
// Namen der EnemyType-Assets: SpikeBoss, FrankensteinBoss, BlobBallBoss, BallerinaBoss, LazyBoss, HQBoss. Das Asset muss geladen sein,
// sonst vorher "!load <Pfad>". Spawn Abstand (Standard 600) vor der ersten Spielerfigur. Nur beim Host, die Replikation macht das Spiel.
void BossCommand(const std::wstring& Args)
{
    const std::string Text(Args.begin(), Args.end());
    const size_t Space = Text.find(' ');
    const std::string TypeName = Text.substr(0, Space);
    const double Distance = Space == std::string::npos ? 600.0 : atof(Text.c_str() + Space + 1);
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    uintptr_t Type = FindObject(TypeName.c_str(), TypeName.c_str());
    if (!Type)
    {
        // Nicht geladen: Pfad aus DA_BossSpawnLookup lesen (BossSpawnInfos +0x28, je 0x68, BossID +0x00, weicher Verweis auf den
        // EnemyType ab +0x20: Paketname, Assetname) und laden. Alle Eintraege landen im Log.
        const uintptr_t Lookup = FindObject("DA_BossSpawnLookup", nullptr);
        const uintptr_t Entries = Lookup ? Read<uintptr_t>(Lookup + 0x28) : 0;
        const int32_t EntryCount = Lookup ? Read<int32_t>(Lookup + 0x30) : 0;
        Log("Boss: Lookup " + std::string(Lookup ? "gefunden" : "fehlt") + ", Eintraege " + std::to_string(EntryCount));
        for (int32_t e = 0; Entries && e < EntryCount && e < 16 && !Type; ++e)
        {
            const uintptr_t Entry = Entries + e * 0x68;
            const std::string Id = NameFromIndex(Read<int32_t>(Entry)), Package = NameFromIndex(Read<int32_t>(Entry + 0x18)),
                              Asset = NameFromIndex(Read<int32_t>(Entry + 0x20));
            Log("Boss: Lookup " + Id + " -> " + Package + "." + Asset);
            if (Asset == TypeName || Id == TypeName)
                Type = LoadObjectByPath(Package + "." + Asset);
        }
    }
    const uintptr_t Spawn = FindObject("SpawnSpecificEnemy", "EnemySpawner");
    const uintptr_t GetLocation = FindObject("K2_GetActorLocation", "Actor");
    if (!World || !Read<uintptr_t>(World + kWorldGameMode) || !Type || !Spawn || !GetLocation)
    {
        Log("Boss: Host " + std::to_string(World && Read<uintptr_t>(World + kWorldGameMode) != 0) + ", Typ " + std::to_string(Type != 0) +
            ", SpawnSpecificEnemy " + std::to_string(Spawn != 0) + ", K2_GetActorLocation " + std::to_string(GetLocation != 0));
        Log("Boss: " + TypeName + " nicht moeglich (kein Host, Typ nicht geladen oder Funktion fehlt)");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    uintptr_t Spawner = 0, Character = 0;
    for (int32_t i = 0; i < Count && !(Spawner && Character); ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
            continue;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
        {
            const std::string Name = NameOf(Class);
            if (!Spawner && Name == "EnemySpawner")
                Spawner = Object;
            if (!Character && Name == "DDCharacter")
                Character = Object;
        }
    }
    if (!Spawner || !Character)
    {
        Log("Boss: kein EnemySpawner oder keine Spielerfigur");
        return;
    }
    struct { double X, Y, Z; } Location{};
    Read<ProcessEventFn*>(Character)[kProcessEventIndex](Character, GetLocation, &Location);
    struct
    {
        uintptr_t EnemyType, Instigator;
        double Rotation[4];      // FTransform ab 0x10: FQuat
        double Translation[3];   // 0x30, danach 8 Byte Fuellung
        double Pad0;
        double Scale[3];         // 0x50
        double Pad1;             // FTransform endet bei 0x70, Gesamt 0x60
        int32_t ThreatLevel, BaseThreatLevel;
        bool ApplySpawnerEffects, ClampToExtends;
        uint8_t Pad2[6];
        uintptr_t Bounds, Result;
    } Params{};
    static_assert(sizeof(Params) == 0x90);
    Params.EnemyType = Type;
    Params.Rotation[3] = 1.0;
    Params.Translation[0] = Location.X + Distance;
    Params.Translation[1] = Location.Y;
    Params.Translation[2] = Location.Z;
    Params.Scale[0] = Params.Scale[1] = Params.Scale[2] = 1.0;
    Params.ThreatLevel = Params.BaseThreatLevel = -1;
    Params.ApplySpawnerEffects = true;
    Params.ClampToExtends = false;
    Read<ProcessEventFn*>(Spawner)[kProcessEventIndex](Spawner, Spawn, &Params);
    Log("Boss: " + TypeName + (Params.Result ? " gespawnt (" + NameOf(Read<uintptr_t>(Params.Result + 0x10)) + ")" : " nicht entstanden"));
}

// Testhilfe (gegner-alle): Host fuegt dem Boss ueber GameplayStatics::ApplyDamage Schaden zu, "!bossdmg 30000". Der Weg ist der des
// Spiels (OnTakeAnyDamage -> DamageToGASTranslator), Phasenwechsel und Tod laufen wie im Kampf. Nur beim Host.
void BossDamageCommand(const std::wstring& Args)
{
    const float Amount = static_cast<float>(_wtof(Args.c_str()));
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Statics = FindObject("Default__GameplayStatics", nullptr);
    const uintptr_t Apply = FindObject("ApplyDamage", "GameplayStatics");
    if (!World || !Read<uintptr_t>(World + kWorldGameMode) || !Statics || !Apply || Amount <= 0)
    {
        Log("BossDmg: nicht moeglich (kein Host, Funktion fehlt oder Schaden <= 0)");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    uintptr_t Boss = 0, Controller = 0;
    for (int32_t i = 0; i < Count && !(Boss && Controller); ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
            continue;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
        {
            const std::string Name = NameOf(Class);
            if (!Boss && Name == "BP_BossEnemy_C")
                Boss = Object;
            if (!Controller && Name == "DDPlayerController")
                Controller = Object;
        }
    }
    if (!Boss)
    {
        Log("BossDmg: kein Boss");
        return;
    }
    struct { uintptr_t Actor; float Damage; uint32_t Pad; uintptr_t Instigator, Causer, DamageType; float Result; uint32_t Pad2; } Params{Boss, Amount, 0, Controller, Boss, 0, 0, 0};
    static_assert(sizeof(Params) == 0x30);
    Read<ProcessEventFn*>(Statics)[kProcessEventIndex](Statics, Apply, &Params);
    Log("BossDmg: " + std::to_string(static_cast<int>(Amount)) + " Schaden, angewendet " + std::to_string(static_cast<int>(Params.Result)));
}

// Testhilfe (gegner-alle): Host setzt einen BossfightSpawnPoint in die Welt und startet den Bosskampf wie das Spiel (Intro, Objective,
// Lebensleiste, Phasen), "!bossfight SpikeBoss [Abstand]". Klasse /Game/DDSurvivors/Blueprints/Enemies/Bosses/BP_<Name>SpawnPoint.
// Nur beim Host, die Replikation macht das Spiel.
void BossFightCommand(const std::wstring& Args)
{
    const std::string Text(Args.begin(), Args.end());
    const size_t Space = Text.find(' ');
    const std::string TypeName = Text.substr(0, Space);
    const double Distance = Space == std::string::npos ? 800.0 : atof(Text.c_str() + Space + 1);
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Statics = FindObject("Default__GameplayStatics", nullptr);
    const uintptr_t Begin = FindObject("BeginDeferredActorSpawnFromClass", "GameplayStatics");
    const uintptr_t Finish = FindObject("FinishSpawningActor", "GameplayStatics");
    const uintptr_t GetLocation = FindObject("K2_GetActorLocation", "Actor");
    const uintptr_t Trigger = FindObject("TriggerBossfight", "BossfightSpawnPoint");
    const std::string ClassName = "BP_" + TypeName + "SpawnPoint_C";
    uintptr_t Class = FindObject(ClassName.c_str(), nullptr);
    if (!Class)
        Class = LoadObjectByPath("/Game/DDSurvivors/Blueprints/Enemies/Bosses/BP_" + TypeName + "SpawnPoint.BP_" + TypeName + "SpawnPoint_C");
    if (!World || !Read<uintptr_t>(World + kWorldGameMode) || !Statics || !Begin || !Finish || !GetLocation || !Trigger || !Class)
    {
        Log("BossFight: " + TypeName + " nicht moeglich (Host " + std::to_string(World && Read<uintptr_t>(World + kWorldGameMode) != 0) + ", Klasse " +
            std::to_string(Class != 0) + ", Begin " + std::to_string(Begin != 0) + ", Finish " + std::to_string(Finish != 0) + ", Trigger " +
            std::to_string(Trigger != 0) + ")");
        return;
    }
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    uintptr_t Character = 0;
    for (int32_t i = 0; i < Count && !Character; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x30) != 0)
            continue;
        for (uintptr_t C = Read<uintptr_t>(Object + 0x10); C; C = Read<uintptr_t>(C + 0x40))
            if (NameOf(C) == "DDCharacter")
            {
                Character = Object;
                break;
            }
    }
    if (!Character)
    {
        Log("BossFight: keine Spielerfigur");
        return;
    }
    struct { double X, Y, Z; } Location{};
    Read<ProcessEventFn*>(Character)[kProcessEventIndex](Character, GetLocation, &Location);
    struct Transform
    {
        double Rotation[4];
        double Translation[3];
        double Pad0;
        double Scale[3];
        double Pad1;
    };
    static_assert(sizeof(Transform) == 0x60);
    Transform Where{};
    Where.Rotation[3] = 1.0;
    Where.Translation[0] = Location.X + Distance;
    Where.Translation[1] = Location.Y;
    Where.Translation[2] = Location.Z;
    Where.Scale[0] = Where.Scale[1] = Where.Scale[2] = 1.0;
    struct
    {
        uintptr_t WorldContext, ActorClass;
        Transform Spawn;
        uint8_t Collision, Pad[7];
        uintptr_t Owner;
        uint8_t ScaleMethod, Pad2[7];
        uintptr_t Result;
    } BeginParams{World, Class, Where, 1, {}, 0, 0, {}, 0};
    static_assert(sizeof(BeginParams) == 0x90);
    Read<ProcessEventFn*>(Statics)[kProcessEventIndex](Statics, Begin, &BeginParams);
    if (!BeginParams.Result)
    {
        Log("BossFight: SpawnPoint nicht entstanden");
        return;
    }
    struct
    {
        uintptr_t Actor, Pad;
        Transform Spawn;
        uint8_t ScaleMethod, Pad2[7];
        uintptr_t Result;
    } FinishParams{BeginParams.Result, 0, Where, 0, {}, 0};
    static_assert(sizeof(FinishParams) == 0x80);
    Read<ProcessEventFn*>(Statics)[kProcessEventIndex](Statics, Finish, &FinishParams);
    if (!Read<uintptr_t>(BeginParams.Result + 0x498))
        if (const uintptr_t SpawnBoss = FindObject("SpawnBoss", "BossfightSpawnPoint"))
            Read<ProcessEventFn*>(BeginParams.Result)[kProcessEventIndex](BeginParams.Result, SpawnBoss, nullptr);
    float Delay = 1.0f;
    Read<ProcessEventFn*>(BeginParams.Result)[kProcessEventIndex](BeginParams.Result, Trigger, &Delay);
    Log("BossFight: " + TypeName + " Spawnpunkt gesetzt, Kampf angestossen (SpawnedBoss " +
        (Read<uintptr_t>(BeginParams.Result + 0x498) ? NameOf(Read<uintptr_t>(Read<uintptr_t>(BeginParams.Result + 0x498) + 0x10)) : std::string("keiner")) + ")");
}

// Laedt ein Asset ueber seinen Pfad, etwa "/Game/.../BP_X.BP_X_C". Dumper-7 erfasst danach auch diese Klasse.
void LoadAssetCommand(const std::wstring& Path)
{
    const std::string Narrow(Path.begin(), Path.end());
    const uintptr_t Object = LoadObjectByPath(Narrow);
    Log(std::string("Laden: ") + (Object ? NameOf(Object) + " (" + NameOf(Read<uintptr_t>(Object + 0x10)) + ")" : "fehlgeschlagen") + " " + Narrow);
}
// Tutorial-Hinweise pausieren die ganze Welt ueber den PauserPlayerState, auch beim Client. Ein Hinweis, der beim
// Kartenwechsel offen bleibt, haelt jede weitere Runde an: Niemand steigt am Bus aus, die Uhr steht bei 00:00.
// Das Spiel hat dafuer einen eigenen Schalter, der Tick setzt ihn in jeder Instanz, sobald das Subsystem existiert.
namespace
{
constexpr uintptr_t kTutorialSkipFlag = 0xB8; // UTutorialStateSubsystem::SkipTutorialFlag
uintptr_t TutorialSubsystem = 0;
int32_t TutorialSubsystemIndex = -1;

uintptr_t FindTutorialSubsystem()
{
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    // Das Subsystem lebt mit der GameInstance. Solange der Platz im Objekt-Array dasselbe Objekt haelt, gilt der Treffer.
    if (TutorialSubsystem && TutorialSubsystemIndex >= 0)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (TutorialSubsystemIndex >> 16));
        if (Chunk && Read<uintptr_t>(Chunk + (TutorialSubsystemIndex & 0xFFFF) * 0x18 + 8) == TutorialSubsystem)
            return TutorialSubsystem;
    }
    TutorialSubsystem = 0;
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0)
            continue;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class; Class = Read<uintptr_t>(Class + 0x40))
            if (NameOf(Class) == "TutorialStateSubsystem")
            {
                TutorialSubsystem = Object;
                TutorialSubsystemIndex = i;
                return Object;
            }
    }
    return 0;
}
}

void SkipTutorials(bool Report)
{
    const uintptr_t Tutorials = FindTutorialSubsystem();
    if (!Tutorials)
    {
        if (Report)
            Log("Tutorial: Subsystem nicht gefunden");
        return;
    }
    // Einmal festhalten, ob der Schalter einen Neustart ueberlebt, also im Spielstand liegt.
    static bool FirstCheck = true;
    if (FirstCheck)
    {
        FirstCheck = false;
        Log(Read<bool>(Tutorials + kTutorialSkipFlag) ? "Tutorial: Schalter beim Start schon gesetzt" : "Tutorial: Schalter beim Start aus");
    }
    if (Read<bool>(Tutorials + kTutorialSkipFlag))
    {
        if (Report)
            Log("Tutorial: Hinweise sind abgeschaltet");
        return;
    }
    static const uintptr_t Function = FindObject("SetForceSkipTutorials", "TutorialStateSubsystem");
    if (!Function)
    {
        Log("Tutorial: SetForceSkipTutorials nicht gefunden");
        return;
    }
    struct { bool bSkip; } Params{true};
    Read<ProcessEventFn*>(Tutorials)[kProcessEventIndex](Tutorials, Function, &Params);
    Log(Read<bool>(Tutorials + kTutorialSkipFlag) ? "Tutorial: Hinweise abgeschaltet" : "Tutorial: Schalter nicht gesetzt");
}
}
