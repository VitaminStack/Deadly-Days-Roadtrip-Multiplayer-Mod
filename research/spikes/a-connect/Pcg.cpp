// Beide: Der Client erzeugt die PCG-Vegetation mit dem Graphen des Hosts. Der PCG-Generator nimmt den Graphen aus dem Biom
// der Route in der GameInstance, und die ist beim Client leer. Spike-Ersatz fuer den Kontrollkanal wie in MapGen.cpp:
// Der Host schreibt den gewaehlten Graphen in eine Datei, der Client auf demselben PC liest sie.
#include "Spike.h"

#include <fstream>

namespace Spike
{
namespace
{
// UPCGVolumeGenerator, Vtable 0x95626D8 Slot 85: spawnt das PCG-Volume und waehlt den Graphen. Slot 86 laedt ihn danach.
constexpr uintptr_t kSetupPcgVolume = 0x59EE6A0;
constexpr uintptr_t kUseGraphFromBiome = 0x28;
constexpr uintptr_t kBiomeGraph = 0x50;            // TSoftObjectPtr: FWeakObjectPtr, dann FSoftObjectPath
constexpr uintptr_t kBiomeGraphPackage = 0x58;
constexpr uintptr_t kBiomeGraphAsset = 0x60;
constexpr uintptr_t kObjectOuter = 0x20;
constexpr uintptr_t kGeneratorSeed = 0x2D8;
constexpr DWORD kMaxValueAgeMs = 120000;
constexpr DWORD kMaxWaitMs = 15000;

using SetupPcgVolumeFn = void(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
SetupPcgVolumeFn OriginalSetupPcgVolume;

std::wstring ValuesFile()
{
    wchar_t Temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, Temp);
    return std::wstring(Temp) + L"ddspike\\pcg.txt";
}

bool IsClient()
{
    const uintptr_t World = Read<uintptr_t>(Base + kGWorld);
    const uintptr_t Driver = World ? Read<uintptr_t>(World + kWorldNetDriver) : 0;
    return Driver && Read<uintptr_t>(Driver + kDriverServerConnection);
}

// FName als Text samt Nummer, der Index gilt nur je Prozess.
std::string NameText(uintptr_t Address)
{
    const int32_t Index = Read<int32_t>(Address);
    const int32_t Number = Read<int32_t>(Address + 4);
    return NameFromIndex(Index) + (Number > 0 ? "_" + std::to_string(Number - 1) : "");
}

uint64_t MakeName(const std::string& Text)
{
    const uintptr_t Strings = FindObject("Default__KismetStringLibrary", nullptr);
    const uintptr_t ToName = FindObject("Conv_StringToName", "KismetStringLibrary");
    if (!Strings || !ToName)
        return 0;
    struct FStringView { const wchar_t* Data; int32_t Num; int32_t Max; };
    const std::wstring Wide(Text.begin(), Text.end());
    struct { FStringView InString; uint64_t ReturnValue; } Params{{Wide.c_str(), static_cast<int32_t>(Wide.size() + 1), static_cast<int32_t>(Wide.size() + 1)}, 0};
    Read<ProcessEventFn*>(Strings)[kProcessEventIndex](Strings, ToName, &Params);
    return Params.ReturnValue;
}

int32_t MapSeedOf(uintptr_t PcgGenerator)
{
    const uintptr_t MapGenerator = Read<uintptr_t>(PcgGenerator + kObjectOuter);
    return MapGenerator ? Read<int32_t>(MapGenerator + kGeneratorSeed) : 0;
}

// Ein Leerzeichen trennt Paket und Asset, "-" heisst: Der Host hat keinen Graphen.
void PublishHostGraph(uintptr_t Generator)
{
    const bool HasGraph = Read<uint32_t>(Generator + kBiomeGraphPackage) != 0;
    const std::string Graph = HasGraph ? NameText(Generator + kBiomeGraphPackage) + " " + NameText(Generator + kBiomeGraphAsset) : "-";
    std::ofstream(ValuesFile()) << MapSeedOf(Generator) << ' ' << Graph << '\n';
    Log("Pcg: Host-Graph " + std::to_string(MapSeedOf(Generator)) + " " + Graph);
}

// Nur ein Wert zu dieser Karte zaehlt, eine alte Datei kann zur vorigen gehoeren.
bool ReadHostGraph(int32_t Seed, std::string& Package, std::string& Asset)
{
    WIN32_FILE_ATTRIBUTE_DATA Info;
    if (!GetFileAttributesExW(ValuesFile().c_str(), GetFileExInfoStandard, &Info))
        return false;
    FILETIME Now;
    GetSystemTimeAsFileTime(&Now);
    const auto Ticks = [](const FILETIME& Time) { return (static_cast<uint64_t>(Time.dwHighDateTime) << 32) | Time.dwLowDateTime; };
    if ((Ticks(Now) - Ticks(Info.ftLastWriteTime)) / 10000 > kMaxValueAgeMs)
        return false;
    int32_t FileSeed = 0;
    std::ifstream Input(ValuesFile());
    if (!(Input >> FileSeed >> Package) || FileSeed != Seed)
        return false;
    if (Package != "-" && !(Input >> Asset))
        return false;
    return true;
}

// Der Host setzt den Graphen ueblicherweise frueher, die Erzeugung des Clients startet erst nach der des Hosts. Faellt
// die Reihenfolge anders aus, wartet der Client hier kurz. Er haelt dabei den Spielthread an, fuer den Spike reicht das.
void ApplyHostGraph(uintptr_t Generator)
{
    const int32_t Seed = MapSeedOf(Generator);
    std::string Package, Asset;
    const DWORD Start = GetTickCount();
    while (!ReadHostGraph(Seed, Package, Asset))
    {
        if (GetTickCount() - Start > kMaxWaitMs)
        {
            Log("Pcg: kein Host-Graph fuer " + std::to_string(Seed) + ", Vegetation fehlt");
            return;
        }
        Sleep(20);
    }
    const DWORD Waited = GetTickCount() - Start;
    if (Package == "-")
    {
        Log("Pcg: Host hat keinen Graphen");
        return;
    }
    const uint64_t PackageName = MakeName(Package);
    const uint64_t AssetName = MakeName(Asset);
    if (!PackageName || !AssetName)
    {
        Log("Pcg: Namen nicht erzeugt");
        return;
    }
    // Der Generator hat den Verweis eben geleert, FWeakObjectPtr und SubPathString bleiben leer.
    *reinterpret_cast<uint64_t*>(Generator + kBiomeGraph) = 0;
    *reinterpret_cast<uint64_t*>(Generator + kBiomeGraphPackage) = PackageName;
    *reinterpret_cast<uint64_t*>(Generator + kBiomeGraphAsset) = AssetName;
    Log("Pcg: Client uebernimmt " + Package + "." + Asset + " nach " + std::to_string(Waited) + " ms");
}

void __fastcall SetupPcgVolumeHook(uintptr_t Generator, uintptr_t State, uintptr_t Third, uintptr_t Fourth)
{
    OriginalSetupPcgVolume(Generator, State, Third, Fourth);
    if (!Read<uint8_t>(Generator + kUseGraphFromBiome))
        return;
    if (!IsClient())
        PublishHostGraph(Generator);
    else if (Read<uint32_t>(Generator + kBiomeGraphPackage) == 0)
        ApplyHostGraph(Generator);
}
}

namespace
{
constexpr uintptr_t kVolumePcgComponent = 0x2B0;
constexpr uintptr_t kPcgGenerated = 0x6F8;
constexpr uintptr_t kComponentOwner = 0xA8;
constexpr uintptr_t kStaticMesh = 0x5E8;
constexpr uintptr_t kInstances = 0x6A8;            // TArray<FInstancedStaticMeshInstanceData>, je 0x80 Bytes FMatrix
constexpr uintptr_t kInstancingRandomSeed = 0x734;

uintptr_t SeededVolume = 0;

// Gleiche Instanzen ergeben auf beiden Seiten denselben Wert. Die Reihenfolge der Instanzen spielt keine Rolle.
int32_t SeedFor(uintptr_t Component)
{
    const uintptr_t Data = Read<uintptr_t>(Component + kInstances);
    const int32_t Num = Read<int32_t>(Component + kInstances + 8);
    int64_t Sum[3]{};
    for (int32_t Index = 0; Index < Num; ++Index)
        for (int Axis = 0; Axis < 3; ++Axis)
            Sum[Axis] += static_cast<int64_t>(Read<double>(Data + 0x80 * Index + 0x60 + 8 * Axis));
    uint32_t Hash = 2166136261u;
    const auto Mix = [&Hash](uint64_t Value) {
        for (int Byte = 0; Byte < 8; ++Byte)
            Hash = (Hash ^ static_cast<uint8_t>(Value >> (8 * Byte))) * 16777619u;
    };
    for (const char Letter : NameOf(Read<uintptr_t>(Component + kStaticMesh)))
        Mix(static_cast<uint8_t>(Letter));
    Mix(static_cast<uint64_t>(Num));
    for (const int64_t Value : Sum)
        Mix(static_cast<uint64_t>(Value));
    return static_cast<int32_t>(Hash | 1);
}

// Neuer Seed wirkt erst mit neuem Render-Zustand. Mesh weg und wieder setzen erzwingt ihn ueber die Blueprint-Funktion.
void Refresh(uintptr_t Component, uintptr_t SetStaticMesh)
{
    const uintptr_t Mesh = Read<uintptr_t>(Component + kStaticMesh);
    const auto ProcessEvent = Read<ProcessEventFn*>(Component)[kProcessEventIndex];
    struct { uintptr_t NewMesh; bool ReturnValue; uint8_t Pad[7]; } Params{};
    ProcessEvent(Component, SetStaticMesh, &Params);
    Params = {Mesh, false, {}};
    ProcessEvent(Component, SetStaticMesh, &Params);
}
}

// Die Engine wuerfelt InstancingRandomSeed jeder ISM-Komponente beim Erzeugen, und das Material waehlt damit die Baumart
// je Instanz. Beide Seiten setzen ihn deshalb nach der PCG-Erzeugung aus Mesh und Instanzen.
void TickPcg()
{
    const uintptr_t Volume = FindInstance("BP_PCGVolume_C");
    const uintptr_t Pcg = Volume ? Read<uintptr_t>(Volume + kVolumePcgComponent) : 0;
    if (!Pcg || !Read<uint8_t>(Pcg + kPcgGenerated) || Volume == SeededVolume)
        return;
    const uintptr_t SetStaticMesh = FindObject("SetStaticMesh", "StaticMeshComponent");
    if (!SetStaticMesh)
        return;
    SeededVolume = Volume;
    const uintptr_t Chunks = Read<uintptr_t>(Base + kGObjects);
    const int32_t Count = Read<int32_t>(Base + kGObjects + 8);
    int Seeded = 0;
    for (int32_t i = 0; i < Count; ++i)
    {
        const uintptr_t Chunk = Read<uintptr_t>(Chunks + 8 * (i >> 16));
        const uintptr_t Object = Chunk ? Read<uintptr_t>(Chunk + (i & 0xFFFF) * 0x18 + 8) : 0;
        if (!Object || (Read<uint32_t>(Object + 8) & 0x10) != 0 || Read<uintptr_t>(Object + kComponentOwner) != Volume)
            continue;
        bool IsInstanced = false;
        for (uintptr_t Class = Read<uintptr_t>(Object + 0x10); Class && !IsInstanced; Class = Read<uintptr_t>(Class + 0x40))
            IsInstanced = NameOf(Class) == "InstancedStaticMeshComponent";
        if (!IsInstanced || Read<int32_t>(Object + kInstances + 8) <= 0)
            continue;
        *reinterpret_cast<int32_t*>(Object + kInstancingRandomSeed) = SeedFor(Object);
        Refresh(Object, SetStaticMesh);
        ++Seeded;
    }
    Log("Pcg: Seeds fuer " + std::to_string(Seeded) + " ISM-Komponenten gesetzt");
}

bool PatchPcg(CodeWriter& Code)
{
    const uintptr_t Site = Base + kSetupPcgVolume;
    if (!Expect(Site, {0x48, 0x89, 0x5C, 0x24, 0x18, 0x55}, "in UPCGVolumeGenerator Slot 85"))
        return false;
    OriginalSetupPcgVolume = reinterpret_cast<SetupPcgVolumeFn>(Code.Here());
    Code.Emit({0x48, 0x89, 0x5C, 0x24, 0x18, 0x55});             // mov [rsp+18h],rbx; push rbp
    Code.Emit({0xE9});                                           // jmp zurueck
    Code.EmitRel(Site + 6);
    WriteJump(Site, Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&SetupPcgVolumeHook)));
    return true;
}
}
