// Beide: Der Client erzeugt die Karte mit Seed, Profil, Groesse und Tags des Hosts statt mit den Standardwerten.
// Spike-Ersatz fuer den Kontrollkanal: Der Host schreibt die Werte in eine Datei, der Client auf demselben PC liest sie.
#include "Spike.h"

#include <fstream>

namespace Spike
{
namespace
{
constexpr uintptr_t kStartMapGeneration = 0x59EF6D0;
constexpr uintptr_t kGeneratorProfile = 0x2B8;
constexpr uintptr_t kGeneratorSeed = 0x2D8;
constexpr uintptr_t kGeneratorMaxTile = 0x2E8;
constexpr uintptr_t kGeneratorTags = 0x300;
constexpr DWORD kMaxValueAgeMs = 120000;

using StartMapGenerationFn = void(__fastcall*)(uintptr_t);
StartMapGenerationFn OriginalStartMapGeneration;

uintptr_t PendingGenerator = 0;
uintptr_t PendingWorld = 0;
// Generator, dessen Erzeugung der Client zuletzt gestartet hat. Nur waehrenddessen werden Child Actors unterdrueckt.
uintptr_t ClientGenerator = 0;
uintptr_t ClientGeneratorWorld = 0;

constexpr uintptr_t kGeneratorState = 0x320;
constexpr uintptr_t kStateQueuedSubGeneratorsNum = 0x68;
constexpr uintptr_t kStateQueuedPostGeneratorsNum = 0x78;
constexpr uintptr_t kStateCurrentSubGenerator = 0x80;

void StartClientGeneration(uintptr_t Generator)
{
    ClientGenerator = Generator;
    ClientGeneratorWorld = Read<uintptr_t>(Base + kGWorld);
    OriginalStartMapGeneration(Generator);
}

struct FHostMap
{
    int32_t Seed = 0;
    int32_t MaxTileX = 0;
    int32_t MaxTileY = 0;
    std::string Profile;
    std::vector<std::string> Tags;
};

std::wstring ValuesFile()
{
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    return std::wstring(Temp) + L"ddspike\\mapgen.txt";
}

bool IsClient()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    return Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
}

void PublishHostMap(uintptr_t Generator)
{
    const uintptr_t Profile = Read<uintptr_t>(Generator + kGeneratorProfile);
    const uintptr_t Package = Profile ? Read<uintptr_t>(Profile + 0x20) : 0;
    const std::string Name = Package ? NameOf(Package) + "." + NameOf(Profile) : "";
    // GenerationTags: TArray<FGameplayTag>, je Tag ein FName. Uebertragen wird der Name, der FName-Index gilt nur je Prozess.
    std::string Tags;
    const uintptr_t TagData = Read<uintptr_t>(Generator + kGeneratorTags);
    for (int32_t Index = 0; Index < Read<int32_t>(Generator + kGeneratorTags + 8); ++Index)
        Tags += ' ' + NameFromIndex(Read<int32_t>(TagData + 8 * Index));
    std::ofstream(ValuesFile()) << Read<int32_t>(Generator + kGeneratorSeed) << ' ' << Read<int32_t>(Generator + kGeneratorMaxTile) << ' '
                                << Read<int32_t>(Generator + kGeneratorMaxTile + 4) << ' ' << Name << Tags << '\n';
    Log("MapGen: Host-Karte " + std::to_string(Read<int32_t>(Generator + kGeneratorSeed)) + " " + Name + Tags);
}

// Nur Werte, die der Host bei diesem Kartenwechsel geschrieben hat, eine alte Datei passt zu einer anderen Karte.
bool ReadHostMap(FHostMap& Out)
{
    WIN32_FILE_ATTRIBUTE_DATA Info;
    if (!GetFileAttributesExW(ValuesFile().c_str(), GetFileExInfoStandard, &Info))
        return false;
    FILETIME Now;
    GetSystemTimeAsFileTime(&Now);
    const auto Ticks = [](const FILETIME& Time) { return (static_cast<uint64_t>(Time.dwHighDateTime) << 32) | Time.dwLowDateTime; };
    if ((Ticks(Now) - Ticks(Info.ftLastWriteTime)) / 10000 > kMaxValueAgeMs)
        return false;
    std::ifstream Input(ValuesFile());
    if (!(Input >> Out.Seed >> Out.MaxTileX >> Out.MaxTileY >> Out.Profile))
        return false;
    for (std::string Tag; Input >> Tag;)
        Out.Tags.push_back(Tag);
    return true;
}

struct FStringView
{
    const wchar_t* Data;
    int32_t Num;
    int32_t Max;
};
}

// Wie der Starter des Spiels per LoadSynchronous, nur ueber die Blueprint-Bibliothek: Pfad, Soft-Verweis, Laden.
uintptr_t LoadObjectByPath(const std::string& Path)
{
    const uintptr_t Library = FindObject("Default__KismetSystemLibrary", nullptr);
    const uintptr_t MakePath = FindObject("MakeSoftObjectPath", "KismetSystemLibrary");
    const uintptr_t ToReference = FindObject("Conv_SoftObjPathToSoftObjRef", "KismetSystemLibrary");
    const uintptr_t Load = FindObject("LoadAsset_Blocking", "KismetSystemLibrary");
    if (!Library || !MakePath || !ToReference || !Load)
        return 0;
    const auto ProcessEvent = Read<ProcessEventFn*>(Library)[kProcessEventIndex];

    const std::wstring Wide(Path.begin(), Path.end());
    struct { FStringView PathString; uint8_t ReturnValue[0x20]; } MakeParams{{Wide.c_str(), static_cast<int32_t>(Wide.size() + 1), static_cast<int32_t>(Wide.size() + 1)}, {}};
    ProcessEvent(Library, MakePath, &MakeParams);
    struct { uint8_t SoftObjectPath[0x20]; uint8_t ReturnValue[0x28]; } ReferenceParams{};
    memcpy(ReferenceParams.SoftObjectPath, MakeParams.ReturnValue, sizeof(MakeParams.ReturnValue));
    ProcessEvent(Library, ToReference, &ReferenceParams);
    struct { uint8_t Asset[0x28]; uintptr_t ReturnValue; } LoadParams{};
    memcpy(LoadParams.Asset, ReferenceParams.ReturnValue, sizeof(ReferenceParams.ReturnValue));
    ProcessEvent(Library, Load, &LoadParams);
    return LoadParams.ReturnValue;
}

namespace
{

// Haengt einen Tag per BlueprintGameplayTagLibrary an, so legt das Spiel den Speicher des Arrays selbst an.
bool AddTag(uintptr_t Container, const std::string& Tag)
{
    const uintptr_t Strings = FindObject("Default__KismetStringLibrary", nullptr);
    const uintptr_t ToName = FindObject("Conv_StringToName", "KismetStringLibrary");
    const uintptr_t Tags = FindObject("Default__BlueprintGameplayTagLibrary", nullptr);
    const uintptr_t Add = FindObject("AddGameplayTag", "BlueprintGameplayTagLibrary");
    if (!Strings || !ToName || !Tags || !Add)
        return false;
    const std::wstring Wide(Tag.begin(), Tag.end());
    struct { FStringView InString; uint64_t ReturnValue; } NameParams{{Wide.c_str(), static_cast<int32_t>(Wide.size() + 1), static_cast<int32_t>(Wide.size() + 1)}, 0};
    Read<ProcessEventFn*>(Strings)[kProcessEventIndex](Strings, ToName, &NameParams);
    // Der Container wandert durch die Parameter: hinein kopieren, aufrufen, das Ergebnis zurueck.
    struct { uint8_t TagContainer[0x20]; uint64_t Tag; } AddParams{};
    memcpy(AddParams.TagContainer, reinterpret_cast<void*>(Container), sizeof(AddParams.TagContainer));
    AddParams.Tag = NameParams.ReturnValue;
    Read<ProcessEventFn*>(Tags)[kProcessEventIndex](Tags, Add, &AddParams);
    memcpy(reinterpret_cast<void*>(Container), AddParams.TagContainer, sizeof(AddParams.TagContainer));
    return true;
}

// Pfad "/Game/.../MGP_X.MGP_X". Beim Client ist meist nur ein Teil der Profile geladen.
uintptr_t FindProfile(const std::string& Path)
{
    const std::string Name = Path.substr(Path.rfind('.') + 1);
    uintptr_t Profile = FindObject(Name.c_str(), nullptr);
    if (!Profile)
    {
        Profile = LoadObjectByPath(Path);
        Log(std::string("MapGen: Profil ") + (Profile ? "geladen: " : "nicht ladbar: ") + Path);
    }
    return Profile && NameOf(Read<uintptr_t>(Profile + 0x10)) == "MapGenerationProfile" ? Profile : 0;
}

enum class EApply
{
    NoValues,
    Applied,
    Failed
};

// Failed: Werte da, aber unbrauchbar. Dann erzeugt der Client wie ohne Patch, statt endlos zu warten.
EApply ApplyHostMap(uintptr_t Generator)
{
    FHostMap Map;
    if (!ReadHostMap(Map))
        return EApply::NoValues;
    const uintptr_t Profile = FindProfile(Map.Profile);
    if (!Profile)
        return EApply::Failed;
    *reinterpret_cast<uintptr_t*>(Generator + kGeneratorProfile) = Profile;
    *reinterpret_cast<int32_t*>(Generator + kGeneratorSeed) = Map.Seed;
    *reinterpret_cast<int32_t*>(Generator + kGeneratorMaxTile) = Map.MaxTileX;
    *reinterpret_cast<int32_t*>(Generator + kGeneratorMaxTile + 4) = Map.MaxTileY;
    // Der Client hat ohne Mission keine Tags, also nur anhaengen, nichts ersetzen.
    if (Read<int32_t>(Generator + kGeneratorTags + 8) == 0)
        for (const std::string& Tag : Map.Tags)
            if (!AddTag(Generator + kGeneratorTags, Tag))
                Log("MapGen: Tag " + Tag + " nicht gesetzt");
    Log("MapGen: Client uebernimmt " + std::to_string(Map.Seed) + " " + Map.Profile);
    return EApply::Applied;
}

// Der Client wartet mit der Erzeugung, bis die Werte des Hosts da sind. Der Host erzeugt wie bisher.
void __fastcall StartMapGenerationHook(uintptr_t Generator)
{
    if (!IsClient())
    {
        OriginalStartMapGeneration(Generator);
        PublishHostMap(Generator);
        return;
    }
    if (ApplyHostMap(Generator) != EApply::NoValues)
    {
        StartClientGeneration(Generator);
        return;
    }
    PendingGenerator = Generator;
    PendingWorld = Read<uintptr_t>(Base + kGWorld);
    Log("MapGen: Client wartet auf die Host-Karte");
}
}

void TickMapGen()
{
    if (!PendingGenerator)
        return;
    if (Read<uintptr_t>(Base + kGWorld) != PendingWorld)
    {
        PendingGenerator = 0;
        Log("MapGen: Kartenwechsel, wartende Erzeugung verworfen");
        return;
    }
    if (ApplyHostMap(PendingGenerator) == EApply::NoValues)
        return;
    const uintptr_t Generator = PendingGenerator;
    PendingGenerator = 0;
    StartClientGeneration(Generator);
}

namespace
{
constexpr uintptr_t kCreateChildActor = 0x288B510;
constexpr uintptr_t kIsChildActorReplicated = 0x289DB60;
constexpr uintptr_t kComponentOwner = 0xA8;
constexpr uintptr_t kActorRole = 0x168;
constexpr uint8_t kRoleAuthority = 3;
constexpr uint8_t kRoleSimulatedProxy = 1;

using CreateChildActorFn = void(__fastcall*)(uintptr_t, uintptr_t);
CreateChildActorFn OriginalCreateChildActor;
int SuppressedChildActors = 0;

// Die Erzeugung laeuft ueber mehrere Frames, fertig ist sie ohne aktiven oder wartenden Sub-Generator.
bool ClientGenerationActive()
{
    if (!ClientGenerator || Read<uintptr_t>(Base + kGWorld) != ClientGeneratorWorld)
        return false;
    const uintptr_t State = Read<uintptr_t>(ClientGenerator + kGeneratorState);
    return State && (Read<uintptr_t>(State + kStateCurrentSubGenerator) || Read<int32_t>(State + kStateQueuedSubGeneratorsNum) > 0 ||
                     Read<int32_t>(State + kStateQueuedPostGeneratorsNum) > 0);
}

// Die Engine erzeugt replizierte Child Actors nur, wenn der Besitzer Autoritaet hat. Beim Client sind die erzeugten
// Parzellen lokal und damit Authority, also kaeme jedes Kartenobjekt doppelt. Fuer diesen Aufruf gilt der Besitzer
// deshalb als Proxy, dann nimmt die Engine ihren eigenen Ausstieg samt Aufraeumen.
void __fastcall CreateChildActorHook(uintptr_t Component, uintptr_t Customizer)
{
    const uintptr_t Owner = Read<uintptr_t>(Component + kComponentOwner);
    const bool Suppress = Owner && ClientGenerationActive() && Read<uint8_t>(Owner + kActorRole) == kRoleAuthority &&
                          !(Read<uint8_t>(Owner + 0x5B) & 0x08) &&
                          reinterpret_cast<bool(__fastcall*)(uintptr_t)>(Base + kIsChildActorReplicated)(Component);
    if (!Suppress)
    {
        OriginalCreateChildActor(Component, Customizer);
        return;
    }
    *reinterpret_cast<uint8_t*>(Owner + kActorRole) = kRoleSimulatedProxy;
    OriginalCreateChildActor(Component, Customizer);
    *reinterpret_cast<uint8_t*>(Owner + kActorRole) = kRoleAuthority;
    if (++SuppressedChildActors % 50 == 1)
        Log("MapGen: replizierte Child Actors unterdrueckt: " + std::to_string(SuppressedChildActors));
}

bool PatchChildActors(CodeWriter& Code)
{
    const uintptr_t Site = Base + kCreateChildActor;
    if (!Expect(Site, {0x40, 0x55, 0x53, 0x57, 0x41, 0x55}, "in CreateChildActor"))
        return false;
    OriginalCreateChildActor = reinterpret_cast<CreateChildActorFn>(Code.Here());
    Code.Emit({0x40, 0x55, 0x53, 0x57, 0x41, 0x55});             // push rbp,rbx,rdi,r13
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 6);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&CreateChildActorHook)));
    return true;
}
}

// Mit den Patches der Mod kommt die Karte ueber ihren Kontrollkanal (src/Game/MapPatches.cpp), nicht ueber die Datei.
bool PatchMapGen(CodeWriter& Code)
{
    if (ModOwnsPatches)
    {
        Log("MapGen: Karte vom Host kommt von der Mod");
        return false;
    }
    if (!PatchChildActors(Code))
        return false;
    const uintptr_t Site = Base + kStartMapGeneration;
    if (!Expect(Site, {0x40, 0x55, 0x53, 0x56, 0x57}, "in StartMapGeneration"))
        return false;
    OriginalStartMapGeneration = reinterpret_cast<StartMapGenerationFn>(Code.Here());
    Code.Emit({0x40, 0x55, 0x53, 0x56, 0x57});                   // push rbp,rbx,rsi,rdi
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 5);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&StartMapGenerationHook)));
    return true;
}
}
