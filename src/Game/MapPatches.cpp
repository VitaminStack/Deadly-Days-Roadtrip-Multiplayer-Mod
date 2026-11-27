// Beide: gleiche Karte und Vegetation bei Host und Client (missionsstart). Aus Spike MapGen.cpp und Pcg.cpp, die Werte
// kommen hier über den Kontrollkanal (Sync/World/MapSync.cpp) statt über eine Datei.
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/MapAdapter.h"
#include "Game/MapPolicy.h"
#include "Game/PatchPolicy.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/Reflection.h"
#include "Game/PatchRuntime.h"
#include "Game/RewardAdapter.h"
#include "Game/RoundStatsAdapter.h"
#include "Game/RouteMapAdapter.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t StartMapGeneration = 0x59F7220;
constexpr uintptr_t CreateChildActor = 0x288B6E0;
constexpr uintptr_t IsChildActorReplicated = 0x289DD30;
constexpr uintptr_t SetupPcgVolume = 0x59F61F0; // UPCGVolumeGenerator, Vtable 0x956D518 Slot 85: Volume spawnen, Graph wählen

constexpr uintptr_t ObjectOuter = 0x20;
constexpr uintptr_t ObjectClass = 0x10;
constexpr uintptr_t ActorRepBits = 0x5B; // Bit 3 bReplicates
constexpr uintptr_t ActorRole = 0x168;
constexpr uintptr_t ComponentOwner = 0xA8;
constexpr uint8_t RoleSimulatedProxy = 1, RoleAuthority = 3;

constexpr uintptr_t GeneratorProfile = 0x2B8;
constexpr uintptr_t GeneratorSeed = 0x2D8;
constexpr uintptr_t GeneratorMaxTile = 0x2E8;
constexpr uintptr_t GeneratorTags = 0x300; // TArray<FGameplayTag>, je Tag ein FName (8 Byte)
constexpr uintptr_t GeneratorState = 0x320;
constexpr uintptr_t ProfileMaxSizeInTiles = 0x44; // UMapGenerationProfile, FIntVector2, nur für den Testschalter
constexpr uintptr_t StateCurrentSubGenerator = 0x80, StateQueuedSubGeneratorsNum = 0x68, StateQueuedPostGeneratorsNum = 0x78;

constexpr uintptr_t PcgUseGraphFromBiome = 0x28;
constexpr uintptr_t PcgBiomeGraph = 0x50; // TSoftObjectPtr: FWeakObjectPtr, dann FSoftObjectPath (Paket, Asset)
constexpr uintptr_t PcgBiomeGraphPackage = 0x58, PcgBiomeGraphAsset = 0x60;

constexpr uintptr_t VolumePcgComponent = 0x2B0;
constexpr uintptr_t PcgGenerated = 0x6F8;
constexpr uintptr_t IsmStaticMesh = 0x5E8;
constexpr uintptr_t IsmInstances = 0x6A8; // TArray<FInstancedStaticMeshInstanceData>, je 0x80 Byte FMatrix
constexpr uintptr_t IsmRandomSeed = 0x734;

// ABossMapTileParcel und UBossSelectorComponent, im Spiel gemessen (docs/OFFSETS.md, endboss-spawn)
constexpr uintptr_t ParcelBossSelector = 0x378;
constexpr uintptr_t ParcelGameState = 0x3D8;
constexpr uintptr_t ParcelFightStarted = 0x3E8;
constexpr uintptr_t SelectorSpawnPoint = 0xF0; // BossfightSpawnPoint

using StartMapGenerationFn = void(__fastcall*)(uintptr_t);
using CreateChildActorFn = void(__fastcall*)(uintptr_t, uintptr_t);
using SetupPcgVolumeFn = void(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
StartMapGenerationFn OriginalStartMapGeneration = nullptr;
CreateChildActorFn OriginalCreateChildActor = nullptr;
SetupPcgVolumeFn OriginalSetupPcgVolume = nullptr;

struct FString
{
    const wchar_t* Data;
    int32_t Num;
    int32_t Max;
};

struct
{
    // Host
    std::optional<Map::HostMap> Published;
    uintptr_t PublishedWorld = 0;
    // Client
    std::optional<Map::HostMap> Received;
    ULONGLONG ReceivedAt = 0;
    ClientMapLedger Used; // jede Erzeugung des Hosts nur einmal übernehmen, außer bei Neuversuch oder Wiederbeitritt
    uintptr_t UsedWorld = 0;
    uintptr_t PendingGenerator = 0, PendingWorld = 0;
    ULONGLONG PendingSince = 0;
    uintptr_t Generator = 0, GeneratorWorld = 0; // laufende eigene Erzeugung, nur dann Child Actors unterdrücken
    ULONGLONG GeneratorSince = 0;
    int Suppressed = 0;
    // Rundenende (rundenende-client). Host: eigenes Ende in EndedWorld. Client: letztes Ende des Hosts und das zuletzt
    // nachgezogene.
    std::optional<Map::RoundEnd> Ended;
    uintptr_t EndedWorld = 0;
    std::optional<Map::RoundEnd> HostEnd;
    uintptr_t FollowedWorld = 0; // Welt, in der der Client Rundenübersicht und Lager geöffnet hat, 0 ohne
    uintptr_t LossPanelWorld = 0; // run-ende, Client: Welt, in der die Niederlage noch angezeigt werden muss, 0 ohne
    int LossPanelAttempts = 0;
    bool LossPanelShown = false;
    bool RunWon = false;
    bool WinContinueLogged = false;
    // opt-ergebnis-werte, Client: letzte Werte des Hosts und seit wann auf die Werte zu HostEnd gewartet wird.
    std::optional<Map::RoundStatsImage> HostStats;
    uintptr_t RunEndedWorld = 0; // Welt, in der der Host den Run beendet hat, 0 ohne
    uint64_t StatsWaitEnd = 0;
    ULONGLONG StatsWaitSince = 0;
    // Beide: PCG-Volume, dessen Instanzen noch einen festen Seed brauchen
    uintptr_t PcgWorld = 0;
    ULONGLONG PcgSince = 0;
    // Host (endboss-spawn): Welt, deren Boss-Parzelle den Missionsstart noch nachholen muss, 0 ohne
    uintptr_t BossWorld = 0;
    ULONGLONG BossSince = 0;
} S;

template <typename T> T ReadOr(uintptr_t Address, T Fallback = {})
{
    return Memory::Read<T>(Address).value_or(Fallback);
}

template <typename T> void Write(uintptr_t Address, T Value)
{
    Memory::WriteBytes(Address, &Value, sizeof(T));
}

uint64_t RandomId()
{
    std::random_device Random;
    return ((uint64_t(Random()) << 32) | Random()) | 1;
}

// FName als Text samt Nummer, der Index gilt nur je Prozess.
std::string NameText(uintptr_t Address)
{
    const int32_t Number = ReadOr<int32_t>(Address + 4);
    return Memory::NameFromIndex(ReadOr<int32_t>(Address, -1)) + (Number > 0 ? "_" + std::to_string(Number - 1) : "");
}

FString View(const std::wstring& Text)
{
    return {Text.c_str(), static_cast<int32_t>(Text.size() + 1), static_cast<int32_t>(Text.size() + 1)};
}

uint64_t MakeName(const std::string& Text)
{
    const uintptr_t Library = Objects::FindNamed("Default__KismetStringLibrary");
    static const uintptr_t Function = Objects::FindObject("Conv_StringToName", "KismetStringLibrary");
    if (!Library || !Function)
        return 0;
    const std::wstring Wide(Text.begin(), Text.end());
    struct
    {
        FString InString;
        uint64_t ReturnValue;
    } Params{View(Wide), 0};
    Objects::ProcessEvent(Library, Function, &Params);
    return Params.ReturnValue;
}

// Wie der Starter des Spiels per LoadSynchronous, nur über die Blueprint-Bibliothek: Pfad, Soft-Verweis, Laden.
uintptr_t LoadByPath(const std::string& Path)
{
    return Objects::LoadAsset(Path);
}

// Beim Client ist meist nur ein Teil der Profile geladen.
uintptr_t FindProfile(const std::string& Path)
{
    const size_t Dot = Path.rfind('.');
    if (Dot == std::string::npos)
        return 0;
    uintptr_t Profile = Objects::FindNamed(Path.substr(Dot + 1));
    if (!Profile)
    {
        Profile = LoadByPath(Path);
        Core::Log::Info("Karte: Profil {} {}", Profile ? "geladen:" : "nicht ladbar:", Path);
    }
    return Profile && Memory::NameOf(ReadOr<uintptr_t>(Profile + ObjectClass)) == "MapGenerationProfile" ? Profile : 0;
}

// Hängt einen Tag per BlueprintGameplayTagLibrary an, so legt das Spiel den Speicher des Arrays selbst an.
bool AddTag(uintptr_t Container, const std::string& Tag)
{
    const uintptr_t Library = Objects::FindNamed("Default__BlueprintGameplayTagLibrary");
    static const uintptr_t Add = Objects::FindObject("AddGameplayTag", "BlueprintGameplayTagLibrary");
    const uint64_t Name = MakeName(Tag);
    if (!Library || !Add || !Name)
        return false;
    // Der Container wandert durch die Parameter: hinein kopieren, aufrufen, das Ergebnis zurück.
    struct
    {
        uint8_t TagContainer[0x20];
        uint64_t Tag;
    } Params{};
    if (!Memory::ReadBytes(Container, Params.TagContainer, sizeof(Params.TagContainer)))
        return false;
    Params.Tag = Name;
    Objects::ProcessEvent(Library, Add, &Params);
    return Memory::WriteBytes(Container, Params.TagContainer, sizeof(Params.TagContainer));
}

// ---- Host ----

void PublishHostMap(uintptr_t Generator)
{
    Map::HostMap Value;
    Value.Id = RandomId();
    Value.Seed = ReadOr<int32_t>(Generator + GeneratorSeed);
    Value.MaxTileX = ReadOr<int32_t>(Generator + GeneratorMaxTile);
    Value.MaxTileY = ReadOr<int32_t>(Generator + GeneratorMaxTile + 4);
    const uintptr_t Profile = ReadOr<uintptr_t>(Generator + GeneratorProfile);
    const uintptr_t Package = Profile ? ReadOr<uintptr_t>(Profile + ObjectOuter) : 0;
    if (Package)
        Value.Profile = Memory::NameOf(Package) + "." + Memory::NameOf(Profile);
    const uintptr_t Tags = ReadOr<uintptr_t>(Generator + GeneratorTags);
    const int32_t Count = ReadOr<int32_t>(Generator + GeneratorTags + 8);
    for (int32_t Index = 0; Tags && Index < Count && Index < 32; ++Index)
        Value.Tags.push_back(Memory::NameFromIndex(ReadOr<int32_t>(Tags + 8 * Index, -1)));
    std::string TagText;
    for (const auto& Tag : Value.Tags)
        TagText += " " + Tag;
    Core::Log::Info("Karte: Host erzeugt {} {}{}", Value.Seed, Value.Profile, TagText);
    S.Published = std::move(Value);
    S.PublishedWorld = CurrentWorld();
    S.BossWorld = CurrentWorld();
    S.BossSince = GetTickCount64();
}

// endboss-spawn: Die Boss-Parzelle entsteht mit der Karte und bindet sich erst dann an GameState.OnMissionStarted. Erst ihr
// Handler setzt über einen Timer BossSelector.BossfightSpawnPoint, ohne den die Startzone keinen Kampf beginnt. Beim Host zu
// zweit reist das Spiel per servertravel statt über den eigenen Ladeablauf (TravelPatches.cpp), die Mission läuft dann schon,
// wenn die Karte fertig ist, und die Parzelle verpasst den Start. Darum den Handler einmal nachholen, nur wenn er fehlt.
void CatchUpBossMission()
{
    if (!S.BossWorld)
        return;
    if (S.BossWorld != CurrentWorld() || GetTickCount64() - S.BossSince > 120000)
    {
        S.BossWorld = 0;
        return;
    }
    const uintptr_t Parcel = Objects::FindInstance("BossMapTileParcel");
    const uintptr_t State = Parcel ? ReadOr<uintptr_t>(Parcel + ParcelGameState) : 0;
    static const uintptr_t IsStarted = Objects::FindObject("IsMissionStarted", "DDMissionGameState");
    static const uintptr_t OnStarted = Objects::FindObject("OnMissionStarted", "BossMapTileParcel");
    if (!State || !IsStarted || !OnStarted)
        return;
    struct
    {
        bool ReturnValue;
        uint8_t Pad[7];
    } Started{};
    Objects::ProcessEvent(State, IsStarted, &Started);
    if (!Started.ReturnValue)
        return; // Die Mission startet noch, dann kommt der Handler nativ.
    S.BossWorld = 0;
    const uintptr_t Selector = ReadOr<uintptr_t>(Parcel + ParcelBossSelector);
    if (!Selector || ReadOr<uintptr_t>(Selector + SelectorSpawnPoint) || ReadOr<uint8_t>(Parcel + ParcelFightStarted))
        return;
    Core::Log::Info("Boss: Parzelle hat den Missionsstart verpasst, Host holt ihn nach");
    uint8_t NoParams[16]{};
    Objects::ProcessEvent(Parcel, OnStarted, NoParams);
}

void PublishHostGraph(uintptr_t PcgGenerator)
{
    if (!S.Published || S.PublishedWorld != CurrentWorld())
        return;
    if (!ReadOr<uint32_t>(PcgGenerator + PcgBiomeGraphPackage))
    {
        S.Published->GraphState = Map::Graph::None;
        Core::Log::Info("Karte: Host ohne PCG-Graph");
        return;
    }
    S.Published->GraphState = Map::Graph::Set;
    S.Published->GraphPackage = NameText(PcgGenerator + PcgBiomeGraphPackage);
    S.Published->GraphAsset = NameText(PcgGenerator + PcgBiomeGraphAsset);
    Core::Log::Info("Karte: Host-Graph {}.{}", S.Published->GraphPackage, S.Published->GraphAsset);
}

// ---- Client ----

// Neue Werte, oder dieselben noch einmal, wenn das Spiel die Erzeugung in derselben Welt neu versucht
// (StartMapGenerationWithRetryAttempts, TryToRegenerate) oder der Client nach einer Trennung derselben Mission wieder beitritt.
// Sonst wartete der Client 60 s und baute eine eigene Karte.
bool FreshValues(uintptr_t Generator)
{
    return S.Received && S.Used.Fresh(S.Received->Id, CurrentWorld() == S.UsedWorld && Generator == S.Generator);
}

// Falsch, wenn die Werte unbrauchbar sind. Dann erzeugt der Client wie ohne Patch, statt endlos zu warten.
bool ApplyHostMap(uintptr_t Generator, const Map::HostMap& Map)
{
    const uintptr_t Profile = FindProfile(Map.Profile);
    if (!Profile)
    {
        Core::Log::Error("Karte: Profil des Hosts {} fehlt, Client erzeugt eine eigene Karte", Map.Profile);
        return false;
    }
    Write<uintptr_t>(Generator + GeneratorProfile, Profile);
    Write<int32_t>(Generator + GeneratorSeed, Map.Seed);
    Write<int32_t>(Generator + GeneratorMaxTile, Map.MaxTileX);
    Write<int32_t>(Generator + GeneratorMaxTile + 4, Map.MaxTileY);
    // Der Client hat ohne Mission keine Tags, also nur anhängen, nichts ersetzen.
    if (ReadOr<int32_t>(Generator + GeneratorTags + 8) == 0)
        for (const std::string& Tag : Map.Tags)
            if (!AddTag(Generator + GeneratorTags, Tag))
                Core::Log::Warn("Karte: Tag {} nicht gesetzt", Tag);
    return true;
}

void StartClientGeneration(uintptr_t Generator)
{
    S.Generator = Generator;
    S.GeneratorWorld = CurrentWorld();
    S.GeneratorSince = GetTickCount64();
    S.Suppressed = 0;
    OriginalStartMapGeneration(Generator);
}

// Testhilfe (spawn-nach-kartenaufbau): Mit DDCOOP_TEST_CLIENT_MAP_DELAY_MS=<ms> beginnt der Client seine Kartenerzeugung
// frühestens so lange nach dem Aufruf des Spiels, wie bei einem langsamen Client. Ohne die Variable 0, nichts ändert sich.
ULONGLONG TestClientMapDelay()
{
    static const ULONGLONG Delay = []
    {
        char Value[16]{};
        const DWORD Length = GetEnvironmentVariableA("DDCOOP_TEST_CLIENT_MAP_DELAY_MS", Value, sizeof(Value));
        return Length > 0 && Length < sizeof(Value) ? static_cast<ULONGLONG>(std::max(0, atoi(Value))) : 0ull;
    }();
    return Delay;
}

// Falsch, solange der Client warten soll.
bool TryStartClient(uintptr_t Generator, ULONGLONG Since)
{
    const ULONGLONG Now = GetTickCount64();
    if (Now - Since < TestClientMapDelay())
        return false;
    const bool Fresh = FreshValues(Generator);
    const bool GraphKnown = Fresh && S.Received->GraphState != Map::Graph::Unknown;
    switch (ClientMapStart(Fresh, GraphKnown, Now - Since, Fresh ? Now - S.ReceivedAt : 0))
    {
    case MapStart::Wait:
        return false;
    case MapStart::Own:
        Core::Log::Error("Karte: nach {} s keine Werte vom Host, Client erzeugt eine eigene Karte", HostMapWaitMs / 1000);
        break;
    case MapStart::HostValues:
        S.Used.Use(S.Received->Id);
        S.UsedWorld = CurrentWorld();
        if (ApplyHostMap(Generator, *S.Received))
            Core::Log::Info("Karte: Client übernimmt {} {} nach {} ms", S.Received->Seed, S.Received->Profile, Now - Since);
        break;
    }
    StartClientGeneration(Generator);
    return true;
}

void ApplyHostGraph(uintptr_t PcgGenerator)
{
    if (!S.Received || S.Received->Id != S.Used.UsedId || S.Received->GraphState == Map::Graph::Unknown)
    {
        Core::Log::Warn("Karte: PCG-Graph des Hosts fehlt noch, Vegetation fehlt beim Client");
        return;
    }
    if (S.Received->GraphState == Map::Graph::None)
        return;
    const uint64_t Package = MakeName(S.Received->GraphPackage);
    const uint64_t Asset = MakeName(S.Received->GraphAsset);
    if (!Package || !Asset)
    {
        Core::Log::Error("Karte: Namen für den PCG-Graphen nicht erzeugt");
        return;
    }
    // Der Generator hat den Verweis eben geleert, FWeakObjectPtr und SubPathString bleiben leer.
    Write<uint64_t>(PcgGenerator + PcgBiomeGraph, 0);
    Write<uint64_t>(PcgGenerator + PcgBiomeGraphPackage, Package);
    Write<uint64_t>(PcgGenerator + PcgBiomeGraphAsset, Asset);
    Core::Log::Info("Karte: Client übernimmt PCG-Graph {}.{}", S.Received->GraphPackage, S.Received->GraphAsset);
}

// Die Erzeugung läuft über mehrere Frames, fertig ist sie ohne aktiven oder wartenden Sub-Generator.
bool ClientGenerationActive()
{
    if (!S.Generator || CurrentWorld() != S.GeneratorWorld)
        return false;
    const uintptr_t State = ReadOr<uintptr_t>(S.Generator + GeneratorState);
    return State && (ReadOr<uintptr_t>(State + StateCurrentSubGenerator) || ReadOr<int32_t>(State + StateQueuedSubGeneratorsNum) > 0 ||
                     ReadOr<int32_t>(State + StateQueuedPostGeneratorsNum) > 0);
}

// ---- Spawn-Messung (spawn-nach-kartenaufbau) ----

// Beim Client die eigene Figur, beim Host die des Clients: in den ersten Sekunden jeder Missionswelt Ort und
// Bewegungsmodus bei Änderung ins Log, dazu das Ende der eigenen Kartenerzeugung des Clients und wo die Figur beim Busplatz
// stand. Beide Instanzen eines PCs teilen die Uhr des Logs. Nur lesend.
constexpr Reflection::FieldSpec ActorRemoteRoleField{"/Script/Engine", "Actor", "RemoteRole", "ByteProperty", 1};
constexpr Reflection::FieldSpec CharacterMovementField{"/Script/Engine", "Character", "CharacterMovement", "ObjectProperty", 8};
constexpr Reflection::FieldSpec MovementModeField{"/Script/Engine", "CharacterMovementComponent", "MovementMode", "ByteProperty", 1};
constexpr uint8_t RoleAutonomousProxy = 2;
constexpr ULONGLONG SpawnTraceMs = 30000;

struct
{
    uintptr_t World = 0;
    ULONGLONG Since = 0, HeightLoggedAt = 0;
    uintptr_t Figure = 0;
    bool GenerationDone = false, Missing = false, Known = false;
    uint8_t Mode = 0xFF;
    double X = 0, Y = 0, Z = 0, LoggedZ = 0;
} Trace;

struct Position
{
    double X = 0, Y = 0, Z = 0;
};

std::optional<Position> ActorPosition(uintptr_t Actor)
{
    static const uintptr_t Function = Objects::FindObject("K2_GetActorLocation", "Actor");
    if (!Actor || !Function)
        return std::nullopt;
    Position Value;
    Objects::ProcessEvent(Actor, Function, &Value);
    return Value;
}

uintptr_t FindTracedFigure(NetRole Role)
{
    const auto RemoteRole = Reflection::Property(ActorRemoteRoleField);
    const uintptr_t World = CurrentWorld();
    const uintptr_t Level = World ? ReadOr<uintptr_t>(World + Offsets::UWorld::PersistentLevel) : 0;
    if (!RemoteRole || !Level)
        return 0;
    uintptr_t Found = 0;
    Objects::ForEachLevelActor(Level, [&](uintptr_t Object)
    {
        if (Found || Memory::NameOf(ReadOr<uintptr_t>(Object + ObjectClass)) != "BP_DD_Character_C")
            return;
        const uint8_t Own = ReadOr<uint8_t>(Object + (Role == NetRole::Client ? ActorRole : RemoteRole->Offset));
        if (Own == RoleAutonomousProxy)
            Found = Object;
    });
    return Found;
}

// Welt mit Kartenerzeugung: beim Client angefragt oder begonnen, beim Host veröffentlicht. MetaBase und Hauptmenü fallen weg.
bool MissionWorld(NetRole Role, uintptr_t World)
{
    if (Role == NetRole::Client)
        return (S.PendingGenerator && S.PendingWorld == World) || S.GeneratorWorld == World;
    return S.Published && S.PublishedWorld == World;
}

const char* GenerationText(NetRole Role, uintptr_t World)
{
    if (Role != NetRole::Client)
        return "Host";
    if (S.PendingGenerator && S.PendingWorld == World)
        return "wartet";
    if (S.GeneratorWorld != World)
        return "nicht begonnen";
    return ClientGenerationActive() ? "läuft" : "fertig";
}

void TraceSpawn(NetRole Role)
{
    if (Role == NetRole::Standalone)
        return;
    const uintptr_t World = CurrentWorld();
    const ULONGLONG Now = GetTickCount64();
    if (World != Trace.World)
    {
        Trace = {};
        Trace.World = World;
        Trace.Since = Now;
    }
    const ULONGLONG Age = Now - Trace.Since;
    if (!World || Trace.Missing || !MissionWorld(Role, World))
        return;
    if (Age > SpawnTraceMs)
    {
        if (!Trace.Figure)
            Core::Log::Warn("Spawn-Messung: keine {} in den ersten {} s der Welt", Role == NetRole::Client ? "eigene Figur" : "Client-Figur",
                            SpawnTraceMs / 1000);
        Trace.Missing = true; // auch mit Figur: Messung für diese Welt beendet
        return;
    }
    if (Role == NetRole::Client && !Trace.GenerationDone && S.GeneratorWorld == World && !ClientGenerationActive())
    {
        Trace.GenerationDone = true;
        Core::Log::Info("Spawn-Messung: Client-Erzeugung fertig, Welt seit {} ms, Dauer {} ms", Age, Now - S.GeneratorSince);
    }
    const auto Movement = Reflection::Property(CharacterMovementField);
    const auto ModeField = Reflection::Property(MovementModeField);
    if (!Movement || !ModeField)
        return;
    if (!Trace.Figure || Memory::NameOf(ReadOr<uintptr_t>(Trace.Figure + ObjectClass)) != "BP_DD_Character_C")
    {
        Trace.Figure = FindTracedFigure(Role);
        Trace.Known = false;
        if (!Trace.Figure)
            return;
    }
    const uintptr_t Component = ReadOr<uintptr_t>(Trace.Figure + Movement->Offset);
    const auto At = ActorPosition(Trace.Figure);
    if (!Component || !At)
        return;
    const uint8_t Mode = ReadOr<uint8_t>(Component + ModeField->Offset);
    // Im freien Fall ändert sich nur Z, ein Sprung ist eine Versetzung in der Ebene.
    const double Planar = Trace.Known ? std::hypot(At->X - Trace.X, At->Y - Trace.Y) : 0;
    const bool Height = std::abs(At->Z - Trace.LoggedZ) > 200 && Now - Trace.HeightLoggedAt >= 1000;
    const char* Reason = !Trace.Known ? "gefunden" : Mode != Trace.Mode ? "Modus" : Planar > 300 ? "Sprung" : Height ? "Höhe" : nullptr;
    Trace.X = At->X, Trace.Y = At->Y, Trace.Z = At->Z;
    Trace.Mode = Mode;
    Trace.Known = true;
    if (!Reason)
        return;
    Core::Log::Info("Spawn-Messung: {} {} Modus {} bei ({:.0f}, {:.0f}, {:.0f}), Welt seit {} ms, Erzeugung {}",
                    Role == NetRole::Client ? "eigene Figur" : "Client-Figur", Reason, Mode, At->X, At->Y, At->Z, Age,
                    GenerationText(Role, World));
    Trace.LoggedZ = At->Z;
    Trace.HeightLoggedAt = Now;
}

// ---- Hooks ----

// Testhilfe: Mit der Umgebungsvariable DDCOOP_TEST_MAP_PROFILE (Pfad "/Game/.../MGP_X.MGP_X") erzeugt der Host jede Missionskarte
// mit diesem Profil statt mit dem des Routenknotens, etwa um gezielt in ein Karten-Event zu kommen. Der Client bekommt das
// Profil wie sonst über den Kontrollkanal. Ohne die Variable ändert sich nichts.
void ApplyTestProfile(uintptr_t Generator)
{
    static const std::string Path = []
    {
        char Value[260]{};
        const DWORD Length = GetEnvironmentVariableA("DDCOOP_TEST_MAP_PROFILE", Value, sizeof(Value));
        return Length > 0 && Length < sizeof(Value) ? std::string(Value, Length) : std::string{};
    }();
    if (Path.empty() || !ReadOr<uintptr_t>(Generator + GeneratorProfile))
        return;
    // DDCOOP_TEST_MAP_PROFILE_EVERY=<n>: nur jede n-te Missionskarte des Prozesses ab der ersten, dazwischen das Profil des
    // Routenknotens. Mit 2 wechseln Karten-Event und normale Karte, so lässt sich die Reise aus dem Event in eine normale Karte
    // mehrmals in einer Sitzung nachstellen (karte-nach-labyrinth-absturz).
    static const int Every = []
    {
        char Value[16]{};
        const DWORD Length = GetEnvironmentVariableA("DDCOOP_TEST_MAP_PROFILE_EVERY", Value, sizeof(Value));
        return Length > 0 && Length < sizeof(Value) ? atoi(Value) : 1;
    }();
    static int Maps = 0;
    if (Every > 1 && Maps++ % Every != 0)
        return;
    const uintptr_t Profile = FindProfile(Path);
    Core::Log::Warn("Karte: Testschalter DDCOOP_TEST_MAP_PROFILE, Profil {} {}", Path, Profile ? "gesetzt" : "nicht ladbar");
    if (!Profile)
        return;
    Write<uintptr_t>(Generator + GeneratorProfile, Profile);
    // Die Größe kommt sonst vom Routenknoten. Event-Profile wie das Labyrinth (13 x 9) brauchen ihre eigene, sonst setzt der
    // Generator nur Ersatzkacheln (labyrinth-event-client).
    const int32_t SizeX = ReadOr<int32_t>(Profile + ProfileMaxSizeInTiles), SizeY = ReadOr<int32_t>(Profile + ProfileMaxSizeInTiles + 4);
    if (SizeX <= 0 || SizeY <= 0)
        return;
    Core::Log::Warn("Karte: Testschalter setzt die Größe des Profils, {} x {} statt {} x {}", SizeX, SizeY,
                    ReadOr<int32_t>(Generator + GeneratorMaxTile), ReadOr<int32_t>(Generator + GeneratorMaxTile + 4));
    Write<int32_t>(Generator + GeneratorMaxTile, SizeX);
    Write<int32_t>(Generator + GeneratorMaxTile + 4, SizeY);
}

void __fastcall StartMapGenerationHook(uintptr_t Generator)
{
    if (CurrentRole() != NetRole::Client)
    {
        ApplyTestProfile(Generator);
        OriginalStartMapGeneration(Generator);
        PublishHostMap(Generator);
        return;
    }
    if (TryStartClient(Generator, GetTickCount64()))
        return;
    S.PendingGenerator = Generator;
    S.PendingWorld = CurrentWorld();
    S.PendingSince = GetTickCount64();
    if (TestClientMapDelay())
        Core::Log::Warn("Karte: Testschalter DDCOOP_TEST_CLIENT_MAP_DELAY_MS, Client erzeugt frühestens nach {} ms", TestClientMapDelay());
    Core::Log::Info("Karte: Client wartet auf die Host-Karte");
}

// Die Engine erzeugt replizierte Child Actors nur, wenn der Besitzer Autorität hat. Beim Client sind die erzeugten
// Parzellen lokal und damit Authority, also käme jedes Kartenobjekt doppelt. Für diesen Aufruf gilt der Besitzer deshalb
// als Proxy, dann nimmt die Engine ihren eigenen Ausstieg samt Aufräumen.
void __fastcall CreateChildActorHook(uintptr_t Component, uintptr_t Customizer)
{
    const uintptr_t Owner = ReadOr<uintptr_t>(Component + ComponentOwner);
    const bool Suppress = Owner && ClientGenerationActive() && ReadOr<uint8_t>(Owner + ActorRole) == RoleAuthority &&
                          !(ReadOr<uint8_t>(Owner + ActorRepBits) & 0x08) &&
                          reinterpret_cast<bool(__fastcall*)(uintptr_t)>(At(IsChildActorReplicated))(Component);
    if (!Suppress)
    {
        OriginalCreateChildActor(Component, Customizer);
        return;
    }
    Write<uint8_t>(Owner + ActorRole, RoleSimulatedProxy);
    OriginalCreateChildActor(Component, Customizer);
    Write<uint8_t>(Owner + ActorRole, RoleAuthority);
    if (++S.Suppressed % 500 == 1)
        Core::Log::Info("Karte: replizierte Child Actors unterdrückt: {}", S.Suppressed);
}

// Der PCG-Generator nimmt den Graphen aus dem Biom der Route in der GameInstance, und die ist beim Client leer.
void __fastcall SetupPcgVolumeHook(uintptr_t Generator, uintptr_t State, uintptr_t Third, uintptr_t Fourth)
{
    OriginalSetupPcgVolume(Generator, State, Third, Fourth);
    const NetRole Role = CurrentRole();
    if (Role == NetRole::Standalone) // allein bleibt die Vegetation wie ohne Mod
        return;
    S.PcgWorld = CurrentWorld();
    S.PcgSince = GetTickCount64();
    if (!ReadOr<uint8_t>(Generator + PcgUseGraphFromBiome))
        return;
    if (Role == NetRole::Host)
        PublishHostGraph(Generator);
    else if (!ReadOr<uint32_t>(Generator + PcgBiomeGraphPackage))
        ApplyHostGraph(Generator);
}

// ---- PCG-Seeds ----

// Gleiche Instanzen ergeben auf beiden Seiten denselben Wert. Die Reihenfolge der Instanzen spielt keine Rolle.
int32_t SeedFor(uintptr_t Component)
{
    const uintptr_t Data = ReadOr<uintptr_t>(Component + IsmInstances);
    const int32_t Num = ReadOr<int32_t>(Component + IsmInstances + 8);
    int64_t Sum[3]{};
    for (int32_t Index = 0; Data && Index < Num; ++Index)
        for (int Axis = 0; Axis < 3; ++Axis)
            Sum[Axis] += static_cast<int64_t>(ReadOr<double>(Data + 0x80 * Index + 0x60 + 8 * Axis));
    uint32_t Hash = 2166136261u;
    const auto Mix = [&Hash](uint64_t Value) {
        for (int Byte = 0; Byte < 8; ++Byte)
            Hash = (Hash ^ static_cast<uint8_t>(Value >> (8 * Byte))) * 16777619u;
    };
    for (const char Letter : Memory::NameOf(ReadOr<uintptr_t>(Component + IsmStaticMesh)))
        Mix(static_cast<uint8_t>(Letter));
    Mix(static_cast<uint64_t>(Num));
    for (const int64_t Value : Sum)
        Mix(static_cast<uint64_t>(Value));
    return static_cast<int32_t>(Hash | 1);
}

// Neuer Seed wirkt erst mit neuem Render-Zustand. Mesh weg und wieder setzen erzwingt ihn über die Blueprint-Funktion.
void Refresh(uintptr_t Component, uintptr_t SetStaticMesh)
{
    const uintptr_t Mesh = ReadOr<uintptr_t>(Component + IsmStaticMesh);
    struct
    {
        uintptr_t NewMesh;
        bool ReturnValue;
        uint8_t Pad[7];
    } Params{};
    Objects::ProcessEvent(Component, SetStaticMesh, &Params);
    Params = {Mesh, false, {}};
    Objects::ProcessEvent(Component, SetStaticMesh, &Params);
}

// Die Engine würfelt InstancingRandomSeed jeder ISM-Komponente beim Erzeugen, und das Material wählt damit die Baumart je
// Instanz. Beide Seiten setzen ihn deshalb nach der PCG-Erzeugung aus Mesh und Instanzen. Nur nach SetupPcgVolume.
void SeedPcgInstances()
{
    if (!S.PcgWorld)
        return;
    if (S.PcgWorld != CurrentWorld() || GetTickCount64() - S.PcgSince > 120000)
    {
        S.PcgWorld = 0;
        return;
    }
    const uintptr_t Volume = Objects::FindInstance("BP_PCGVolume_C");
    const uintptr_t Pcg = Volume ? ReadOr<uintptr_t>(Volume + VolumePcgComponent) : 0;
    static const uintptr_t SetStaticMesh = Objects::FindObject("SetStaticMesh", "StaticMeshComponent");
    if (!Pcg || !ReadOr<uint8_t>(Pcg + PcgGenerated) || !SetStaticMesh)
        return;
    S.PcgWorld = 0;
    int Seeded = 0;
    for (int32_t Index = 0; Index < Objects::Count(); ++Index)
    {
        const uintptr_t Object = Objects::At(Index);
        // Besitzer und Instanzen erst lesen, wenn die Klasse passt. Kleinere Objekte enden vor diesen Feldern.
        if (!Object || !Objects::IsA(Object, "InstancedStaticMeshComponent") || ReadOr<uintptr_t>(Object + ComponentOwner) != Volume ||
            ReadOr<int32_t>(Object + IsmInstances + 8) <= 0)
            continue;
        Write<int32_t>(Object + IsmRandomSeed, SeedFor(Object));
        Refresh(Object, SetStaticMesh);
        ++Seeded;
    }
    Core::Log::Info("Karte: Seeds für {} ISM-Komponenten gesetzt", Seeded);
}

bool CheckMap()
{
    return Code::Expect(At(StartMapGeneration), {0x40, 0x55, 0x53, 0x56, 0x57}, "in StartMapGeneration") &&
           Code::Expect(At(CreateChildActor), {0x40, 0x55, 0x53, 0x57, 0x41, 0x55}, "in CreateChildActor") &&
           Code::Expect(At(SetupPcgVolume), {0x48, 0x89, 0x5C, 0x24, 0x18, 0x55}, "in UPCGVolumeGenerator Slot 85");
}

bool ApplyMap(Code::Writer&)
{
    return Core::Hooks::Install(reinterpret_cast<void*>(At(StartMapGeneration)), reinterpret_cast<void*>(&StartMapGenerationHook),
                                reinterpret_cast<void**>(&OriginalStartMapGeneration)) &&
           Core::Hooks::Install(reinterpret_cast<void*>(At(CreateChildActor)), reinterpret_cast<void*>(&CreateChildActorHook),
                                reinterpret_cast<void**>(&OriginalCreateChildActor)) &&
           Core::Hooks::Install(reinterpret_cast<void*>(At(SetupPcgVolume)), reinterpret_cast<void*>(&SetupPcgVolumeHook),
                                reinterpret_cast<void**>(&OriginalSetupPcgVolume));
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t MapSites[] = {CreateChildActor, IsChildActorReplicated, SetupPcgVolume, StartMapGeneration};
const Patch List[] = {
    {"Gleiche Karte und Vegetation beim Client", &CheckMap, &ApplyMap, MapSites},
    RoundStatsPatch(),
    RoundStampsPatch(),
};
}

std::span<const Patch> MapPatches()
{
    return List;
}

void NoteBusPlacement()
{
    if (CurrentRole() != NetRole::Client)
        return;
    const uintptr_t World = CurrentWorld();
    const auto At = ActorPosition(FindTracedFigure(NetRole::Client));
    const ULONGLONG Age = Trace.World == World ? GetTickCount64() - Trace.Since : 0;
    if (!At)
        Core::Log::Warn("Spawn-Messung: Busplatz des Clients ohne eigene Figur, Welt seit {} ms, Erzeugung {}", Age,
                        GenerationText(NetRole::Client, World));
    else
        Core::Log::Info("Spawn-Messung: Busplatz des Clients, eigene Figur bei ({:.0f}, {:.0f}, {:.0f}), Welt seit {} ms", At->X,
                        At->Y, At->Z, Age);
}

void NoteHostRoundEnd(bool Lost)
{
    const uintptr_t World = CurrentWorld();
    if (S.Ended && S.EndedWorld == World)
        return;
    Map::RoundEnd End;
    End.MapId = S.Published && S.PublishedWorld == World ? S.Published->Id : 0;
    End.Id = RandomId();
    End.Outcome = Lost ? Map::RunOutcome::Lost : Map::RunOutcome::None;
    S.Ended = End;
    S.EndedWorld = World;
    if (End.MapId)
        Core::Log::Info("Rundenende: Host beendet die Runde, Client wird benachrichtigt");
    else
        Core::Log::Warn("Rundenende: Host beendet die Runde ohne eigene Kartenwerte, der Client bleibt in der Mission");
}

void NoteHostRunWon()
{
    if (S.Ended && S.EndedWorld == CurrentWorld())
        S.Ended->Outcome = Map::RunOutcome::Won;
}

// run-ende, Client: Offene Rundenübersicht auf das Run-Ende umschalten. Einmal je Sekunde, höchstens fünfmal.
void TickLossPanel(bool Second)
{
    if (!S.LossPanelWorld)
        return;
    if (S.LossPanelWorld != CurrentWorld())
    {
        S.LossPanelWorld = 0;
        S.LossPanelShown = false;
        return;
    }
    if (!Second)
        return;
    if (S.LossPanelShown)
    {
        // opt-ergebnis-werte: Beim Sieg bleibt es bei „Fortsetzen“ ins Lager, auch wenn das Spiel nach der Animation die Knöpfe des
        // Run-Endes zeigt (Tageskennung des Hosts, siehe RoundStatsAdapter.h).
        if (!S.RunWon)
            KeepRunLossButtonsHidden();
        else if (KeepRunWinContinueOnly() && !S.WinContinueLogged)
        {
            S.WinContinueLogged = true;
            Core::Log::Info("Run-Ende: Client behält am Sieg „Fortsetzen“, Rückkehr und Endlosmodus entscheidet der Host");
        }
        return;
    }
    if (!ShouldRetryLossPanel(S.LossPanelAttempts, 5))
    {
        Core::Log::Warn("Run-Ende: Keine Rundenübersicht zum Umschalten gefunden, Client wartet auf die Reise");
        S.LossPanelWorld = 0;
        return;
    }
    ++S.LossPanelAttempts;
    if (!ShowRunEndPanel(S.RunWon))
        return;
    Core::Log::Info("Run-Ende: Client zeigt den Ergebnisbildschirm ({})", S.RunWon ? "Sieg" : "Niederlage");
    // Niederlage: Knöpfe ausgeblendet. Sieg: nur „Fortsetzen“. Beides hält der Sekundentakt, bis die Welt wechselt.
    S.LossPanelShown = true;
    S.WinContinueLogged = false;
}

// Client: Das Rundenende des Hosts einmal lokal nachziehen, sobald es zur gespielten Karte passt.
void FollowRoundEnd()
{
    if (!S.HostEnd)
        return;
    const uint64_t ClientMapId = S.UsedWorld == CurrentWorld() ? S.Used.UsedId : 0;
    if (!ClientMayFollowRoundEnd(S.HostEnd->MapId, ClientMapId, S.Used.Followed(S.HostEnd->Id)))
        return;
    // opt-ergebnis-werte: Die Übersicht liest beim Öffnen die Werte des Subsystems, darum vorher die des Hosts übernehmen.
    const ULONGLONG Now = GetTickCount64();
    if (S.StatsWaitEnd != S.HostEnd->Id)
    {
        S.StatsWaitEnd = S.HostEnd->Id;
        S.StatsWaitSince = Now;
    }
    const uint64_t StatsEnd = S.HostStats ? S.HostStats->EndId : 0;
    if (!RoundStats::ClientMayOpenRoundEnd(S.HostEnd->Id, StatsEnd, Now - S.StatsWaitSince))
        return;
    // Gehalten wird der Stand bis nach FollowHostRoundEnd: Dort bucht das Spiel am Tagesende selbst, danach setzt der Hook ihn erneut.
    std::optional<RoundStats::Snapshot> HostStats;
    if (StatsEnd != S.HostEnd->Id)
        Core::Log::Warn("Rundenwerte: keine Werte des Hosts nach {} ms, Client zeigt die eigenen", Now - S.StatsWaitSince);
    else if (!S.HostStats->Known)
        Core::Log::Warn("Rundenwerte: Host konnte seine Werte nicht lesen, Client zeigt die eigenen");
    else if (const auto Written = RoundStats::ApplyHost(S.HostStats->Values))
    {
        Core::Log::Info("Rundenwerte: Tag {}.{} und {} Werte des Hosts übernommen, {} davon geschrieben", S.HostStats->Values.Day,
                        S.HostStats->Values.SubMission, S.HostStats->Values.Entries.size(), *Written);
        HostStats = S.HostStats->Values;
    }
    else
        Core::Log::Warn("Rundenwerte: eigene Werte nicht lesbar, Client zeigt sie unverändert");
    S.Used.Follow(S.HostEnd->Id);
    RoundStats::NoteRoundEndFollowed(S.HostEnd->Outcome == Map::RunOutcome::Lost); // opt-rundenstempel
    S.FollowedWorld = 0;
    // run-ende: Der Bildschirm kommt aus dem nativen Rundenende, das Umschalten folgt im nächsten Sekundentakt, wenn die Übersicht
    // offen ist. Eine Niederlage hat kein Lager, darum bleibt FollowedWorld dort leer und Bestätigen unberührt. Ein Sieg behält das
    // Lager, falls der Host den Endlosmodus startet.
    const Map::RunOutcome Outcome = S.HostEnd->Outcome;
    // Am Run-Ende zeigt die Übersicht wie beim Host die Summen aller Tage, bis der Client die Welt verlässt (TickMap).
    S.RunEndedWorld = Outcome != Map::RunOutcome::None ? CurrentWorld() : 0;
    RoundStats::NoteRunEnded(S.RunEndedWorld);
    RoundStats::HoldForRoundChange(HostStats ? &*HostStats : nullptr);
    // opt-routenkarte: Route des Hosts vor dem lokalen FinishMission, das sie wie beim Host weiterführt und die Karte öffnet.
    ::Game::RouteMap::ApplyBeforeRoundEnd(S.HostEnd->MapId, S.HostEnd->Id, Outcome == Map::RunOutcome::None);
    const bool Followed = FollowHostRoundEnd();
    RoundStats::HoldForRoundChange(nullptr);
    if (const int Reapplied = RoundStats::TakeReapplied())
        Core::Log::Info("Rundenwerte: nach der eigenen Tagesend-Buchung {}-mal erneut gesetzt", Reapplied);
    else if (HostStats && Followed)
        Core::Log::Warn("Rundenwerte: keine eigene Tagesend-Buchung gesehen, Werte nur vorher gesetzt");
    if (!Followed)
    {
        Core::Log::Warn("Rundenende: Client ohne Missions-GameState, folgt nur der Reise");
        return;
    }
    if (Outcome != Map::RunOutcome::Lost)
        S.FollowedWorld = CurrentWorld();
    if (Outcome != Map::RunOutcome::None)
    {
        S.LossPanelWorld = CurrentWorld();
        S.LossPanelAttempts = 0;
        S.LossPanelShown = false;
        S.RunWon = Outcome == Map::RunOutcome::Won;
    }
    Core::Log::Info("Rundenende: Host hat die Runde beendet, Client öffnet Rundenübersicht und Lager selbst");
}

void TickMap(NetRole Role, bool Second)
{
    // Merker des Run-Endes in jeder Rolle löschen, sobald die Welt wechselt: Eine neue Welt kann dieselbe Adresse bekommen.
    if (S.RunEndedWorld && (S.RunEndedWorld != CurrentWorld() || Role != NetRole::Client))
    {
        S.RunEndedWorld = 0;
        RoundStats::NoteRunEnded(0);
    }
    if (Role == NetRole::Client)
    {
        FollowRoundEnd();
        TickLossPanel(Second);
    }
    else if (S.Ended && S.EndedWorld != CurrentWorld())
        S.Ended.reset();
    // bestaetigen-rundenende: Bestätigen im Lager wartet auf alle, solange beide in diesem Rundenende sind.
    if (Role == NetRole::Client)
        Rewards::SetRoundEnd(S.FollowedWorld && S.FollowedWorld == CurrentWorld() ? S.Used.FollowedEnd : 0);
    else
        Rewards::SetRoundEnd(Role == NetRole::Host && S.Ended && S.Ended->Outcome != Map::RunOutcome::Lost && S.EndedWorld == CurrentWorld()
                                 ? S.Ended->Id
                                 : 0);
    if (Role != NetRole::Client)
    {
        if (S.Published && S.PublishedWorld != CurrentWorld())
            S.Published.reset();
        S.PendingGenerator = 0;
    }
    else if (S.PendingGenerator)
    {
        if (CurrentWorld() != S.PendingWorld)
        {
            S.PendingGenerator = 0;
            Core::Log::Info("Karte: Kartenwechsel, wartende Erzeugung verworfen");
        }
        else if (TryStartClient(S.PendingGenerator, S.PendingSince))
            S.PendingGenerator = 0;
    }
    TraceSpawn(Role);
    if (Second)
        SeedPcgInstances();
    if (Second && Role == NetRole::Host)
        CatchUpBossMission();
}
}

namespace Game::Map
{
std::optional<HostMap> CurrentHostMap()
{
    using namespace Patches;
    if (!S.Published || S.PublishedWorld != CurrentWorld())
        return std::nullopt;
    return S.Published;
}

std::optional<RoundEnd> CurrentRoundEnd()
{
    using namespace Patches;
    if (!S.Ended || S.EndedWorld != CurrentWorld())
        return std::nullopt;
    return S.Ended;
}

void SetHostRoundEnd(std::optional<RoundEnd> Value)
{
    using namespace Patches;
    S.HostEnd = Value;
}

std::optional<RoundStatsImage> CurrentRoundStats()
{
    const auto End = CurrentRoundEnd();
    if (!End)
        return std::nullopt;
    RoundStatsImage Image{End->Id};
    auto Values = RoundStats::ReadLocal();
    static uint64_t Logged = 0; // einmal je Rundenende
    if (Values && RoundStats::ValidTime(Values->Day, Values->SubMission))
    {
        const size_t Read = Values->Entries.size();
        Image.Known = true;
        Image.Values = {Values->Day, Values->SubMission, RoundStats::Limit(std::move(Values->Entries))};
        // Was nicht ins Bild passt oder ungültig ist, sieht der Client nicht.
        if (Logged != End->Id && Read != Image.Values.Entries.size())
            Core::Log::Warn("Rundenwerte: {} von {} eigenen Werten nicht übertragbar (ungültig oder über {})",
                            Read - Image.Values.Entries.size(), Read, RoundStats::MaxEntries);
    }
    else if (Logged != End->Id && Values)
        Core::Log::Warn("Rundenwerte: eigene Tageskennung {}.{} nicht übertragbar", Values->Day, Values->SubMission);
    Logged = End->Id;
    return Image;
}

void SetHostRoundStats(std::optional<RoundStatsImage> Value)
{
    using namespace Patches;
    S.HostStats = std::move(Value);
}

void NoteConnectionChanged()
{
    using namespace Patches;
    S.Used.ConnectionChanged(S.UsedWorld && S.UsedWorld == CurrentWorld());
}

void SetHostMap(std::optional<HostMap> Value)
{
    using namespace Patches;
    if (Value && (!S.Received || S.Received->Id != Value->Id))
    {
        Core::Log::Info("Karte: Werte vom Host {} {}", Value->Seed, Value->Profile);
        S.ReceivedAt = GetTickCount64();
    }
    S.Received = std::move(Value);
}
}
