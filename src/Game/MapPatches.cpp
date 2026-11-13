// Beide: gleiche Karte und Vegetation bei Host und Client (missionsstart). Aus Spike MapGen.cpp und Pcg.cpp, die Werte
// kommen hier über den Kontrollkanal (Sync/World/MapSync.cpp) statt über eine Datei.
#include <windows.h>

#include <random>
#include <string>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/MapAdapter.h"
#include "Game/MapPolicy.h"
#include "Game/PatchPolicy.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/PatchRuntime.h"
#include "Game/RewardAdapter.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t StartMapGeneration = 0x59F44A0;
constexpr uintptr_t CreateChildActor = 0x288B710;
constexpr uintptr_t IsChildActorReplicated = 0x289DD60;
constexpr uintptr_t SetupPcgVolume = 0x59F3470; // UPCGVolumeGenerator, Vtable 0x9566B18 Slot 85: Volume spawnen, Graph wählen

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
    uint64_t UsedId = 0; // jede Erzeugung des Hosts nur einmal übernehmen, außer bei einem neuen Versuch in derselben Welt
    uintptr_t UsedWorld = 0;
    uintptr_t PendingGenerator = 0, PendingWorld = 0;
    ULONGLONG PendingSince = 0;
    uintptr_t Generator = 0, GeneratorWorld = 0; // laufende eigene Erzeugung, nur dann Child Actors unterdrücken
    int Suppressed = 0;
    // Rundenende (rundenende-client). Host: eigenes Ende in EndedWorld. Client: letztes Ende des Hosts und das zuletzt
    // nachgezogene.
    std::optional<Map::RoundEnd> Ended;
    uintptr_t EndedWorld = 0;
    std::optional<Map::RoundEnd> HostEnd;
    uint64_t FollowedEnd = 0;
    uintptr_t FollowedWorld = 0; // Welt, in der der Client Rundenübersicht und Lager geöffnet hat, 0 ohne
    uintptr_t LossPanelWorld = 0; // run-ende, Client: Welt, in der die Niederlage noch angezeigt werden muss, 0 ohne
    int LossPanelAttempts = 0;
    bool LossPanelShown = false;
    bool RunWon = false;
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
// (StartMapGenerationWithRetryAttempts, TryToRegenerate). Sonst wartete der Client 60 s und baute eine eigene Karte.
bool FreshValues(uintptr_t Generator)
{
    return S.Received && (S.Received->Id != S.UsedId || (CurrentWorld() == S.UsedWorld && Generator == S.Generator));
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
    S.Suppressed = 0;
    OriginalStartMapGeneration(Generator);
}

// Falsch, solange der Client warten soll.
bool TryStartClient(uintptr_t Generator, ULONGLONG Since)
{
    const ULONGLONG Now = GetTickCount64();
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
        S.UsedId = S.Received->Id;
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
    if (!S.Received || S.Received->Id != S.UsedId || S.Received->GraphState == Map::Graph::Unknown)
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

// ---- Hooks ----

void __fastcall StartMapGenerationHook(uintptr_t Generator)
{
    if (CurrentRole() != NetRole::Client)
    {
        OriginalStartMapGeneration(Generator);
        PublishHostMap(Generator);
        return;
    }
    if (TryStartClient(Generator, GetTickCount64()))
        return;
    S.PendingGenerator = Generator;
    S.PendingWorld = CurrentWorld();
    S.PendingSince = GetTickCount64();
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
};
}

std::span<const Patch> MapPatches()
{
    return List;
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
        KeepRunLossButtonsHidden();
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
    // Beim Sieg bleiben die Knöpfe nativ, nur die Niederlage hält sie ausgeblendet.
    if (S.RunWon)
        S.LossPanelWorld = 0;
    else
        S.LossPanelShown = true;
}

// Client: Das Rundenende des Hosts einmal lokal nachziehen, sobald es zur gespielten Karte passt.
void FollowRoundEnd()
{
    if (!S.HostEnd)
        return;
    const uint64_t ClientMapId = S.UsedWorld == CurrentWorld() ? S.UsedId : 0;
    if (!ClientMayFollowRoundEnd(S.HostEnd->MapId, ClientMapId, S.FollowedEnd == S.HostEnd->Id))
        return;
    S.FollowedEnd = S.HostEnd->Id;
    S.FollowedWorld = 0;
    // run-ende: Der Bildschirm kommt aus dem nativen Rundenende, das Umschalten folgt im nächsten Sekundentakt, wenn die Übersicht
    // offen ist. Eine Niederlage hat kein Lager, darum bleibt FollowedWorld dort leer und Bestätigen unberührt. Ein Sieg behält das
    // Lager, falls der Host den Endlosmodus startet.
    const Map::RunOutcome Outcome = S.HostEnd->Outcome;
    if (!FollowHostRoundEnd())
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
    if (Role == NetRole::Client)
    {
        FollowRoundEnd();
        TickLossPanel(Second);
    }
    else if (S.Ended && S.EndedWorld != CurrentWorld())
        S.Ended.reset();
    // bestaetigen-rundenende: Bestätigen im Lager wartet auf alle, solange beide in diesem Rundenende sind.
    if (Role == NetRole::Client)
        Rewards::SetRoundEnd(S.FollowedWorld && S.FollowedWorld == CurrentWorld() ? S.FollowedEnd : 0);
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
