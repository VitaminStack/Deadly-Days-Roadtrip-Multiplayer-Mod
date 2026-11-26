// opt-routenkarte, siehe RouteMapAdapter.h und RouteMapPolicy.h. Die Run-Info (FCurrentRunInfo) der GameInstance repliziert nicht.
// Die Missionsstruktur baut das Spiel aus dem Run-Seed, und CurrentRoute ist eine Kopie eines ihrer Einträge. Darum braucht der
// Client nur Seed, Wahl, Kennungen und Fortschritt der Route; Biome, Knoten und Ereignisse erzeugt er selbst. UBusPanel::OpenPanel
// befüllt die Karte nur mit ROLE_Authority des eigenen Controllers; beim Client bekommt es die Rolle für diesen einen Aufruf.
#include "Game/RouteMapAdapter.h"

#include <windows.h>

#include <cstring>
#include <optional>
#include <vector>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/CodeCave.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/Offsets.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

namespace Game::Patches
{
namespace
{
constexpr uintptr_t OpenPanel = 0x5ABA540;           // UBusPanel::OpenPanel: Run-Info lesen, mit Autorität SetChoices
constexpr uintptr_t OpenPanelRoleCheck = 0x5ABA627;  // darin: Controller über vtable +2C8h, cmp byte ptr [rax+168h], 3; jne
constexpr uintptr_t GetMissionStructure = 0x58EFD10; // UMissionGenerationSubsystem::GetMissionStructure(Out, Day): baut den Cache neu
constexpr uintptr_t DestructGroups = 0x56ADF20;      // FPossibleMissionGroup-Elemente abbauen (Data, Num)
constexpr uintptr_t DestructRoutes = 0x56ADFB0;      // FRoute-Elemente abbauen (Data, Num)
constexpr uintptr_t RouteAssign = 0x57487D0;         // FRoute::operator=
constexpr uintptr_t SetCurrentRoute = 0x59056C0;     // UDDMissionGameInstance: PreviousRoute = CurrentRoute, CurrentRoute = neu
constexpr uintptr_t MemoryRealloc = 0x1304360;       // FMemory::Realloc(Original, Bytes, Alignment)
constexpr uintptr_t MemoryFree = 0x12F85E0;          // FMemory::Free

constexpr uintptr_t ControllerVtableSlot = 0x2C8; // UBusPanel: besitzender PlayerController, wie in OpenPanel
constexpr uintptr_t ActorRole = 0x168;            // AActor::Role, docs/OFFSETS.md
constexpr uint8_t RoleAuthority = 3;
constexpr size_t RouteSize = 0x40, GroupSize = 0x20; // FRoute, FPossibleMissionGroup (gemessen, Feldgrenzen über die Reflexion)

using OpenPanelFn = void(__fastcall*)(uintptr_t);
using SetCurrentRouteFn = void(__fastcall*)(uintptr_t, const void*);
using GetMissionStructureFn = void*(__fastcall*)(uintptr_t, void*, int32_t);
using DestructFn = void(__fastcall*)(void*, int32_t);
using RouteAssignFn = void*(__fastcall*)(void*, const void*);
using ReallocFn = void*(__fastcall*)(void*, size_t, uint32_t);
using FreeFn = void(__fastcall*)(void*);
OpenPanelFn OriginalOpenPanel = nullptr;
SetCurrentRouteFn OriginalSetCurrentRoute = nullptr;
bool RouteMapReady = false;

struct ScriptArray
{
    uintptr_t Data = 0;
    int32_t Num = 0;
    int32_t Max = 0;
};
static_assert(sizeof(ScriptArray) == 0x10);

constexpr std::string_view Package = "/Script/DDSurvivors";
constexpr Reflection::FieldSpec RunInfoField{Package, "DDMissionGameInstance", "CurrentRunInfo", "StructProperty", 0xE8};
constexpr Reflection::FieldSpec SeedField{Package, "CurrentRunInfo", "CurrentRunSeed", "IntProperty", 4};
constexpr Reflection::FieldSpec ThreatField{Package, "CurrentRunInfo", "PermanentThreatLevelIncrease", "IntProperty", 4};
constexpr Reflection::FieldSpec CurrentField{Package, "CurrentRunInfo", "CurrentRoute", "StructProperty", RouteSize};
constexpr Reflection::FieldSpec PreviousField{Package, "CurrentRunInfo", "PreviousRoute", "StructProperty", RouteSize};
constexpr Reflection::FieldSpec ChosenField{Package, "CurrentRunInfo", "ChosenMissionIndexes", "ArrayProperty", 0x10};
constexpr Reflection::FieldSpec IdentField{Package, "Route", "RouteMissionStructureIdentifier", "StructProperty", 8};
constexpr Reflection::FieldSpec ChainField{Package, "Route", "MissionChain", "ArrayProperty", 0x10};
constexpr Reflection::FieldSpec IndexField{Package, "Route", "CurrentMissionIndex", "IntProperty", 4};
constexpr Reflection::FieldSpec CachedField{Package, "Route", "CachedCurrentMissionIndex", "IntProperty", 4};
constexpr Reflection::FieldSpec EndField{Package, "Route", "IsEndOfMissionChain", "BoolProperty", 1};
constexpr Reflection::FieldSpec StructureField{Package, "MissionGenerationSubsystem", "CachedMissionStructure", "ArrayProperty", 0x10};
constexpr Reflection::FieldSpec PossibleRoutesField{Package, "PossibleMissionGroup", "PossibleRoutes", "ArrayProperty", 0x10};

// Aufgelöste Abstände, einmal je Start. Ident: Gruppe +0, Route +4 (gemessen, je int32).
struct Layout
{
    int32_t RunInfo, Seed, Threat, Current, Previous, Chosen, Ident, Chain, Index, Cached, End, Structure, PossibleRoutes;
};

std::optional<Layout> ResolveLayout()
{
    // Gefundene Felder cacht die Reflexion selbst, fehlende meldet sie einmal. Ohne Ergebnis bleibt der Adapter aus.
    static std::optional<Layout> Cached;
    if (Cached)
        return Cached;
    const Reflection::FieldSpec* Specs[] = {&RunInfoField, &SeedField,  &ThreatField, &CurrentField, &PreviousField,  &ChosenField,         &IdentField,
                                            &ChainField,   &IndexField, &CachedField, &EndField,     &StructureField, &PossibleRoutesField};
    int32_t Offsets[std::size(Specs)]{};
    for (size_t Index = 0; Index < std::size(Specs); ++Index)
    {
        const auto Field = Reflection::Property(*Specs[Index]);
        if (!Field)
            return std::nullopt;
        Offsets[Index] = Field->Offset;
    }
    Cached = Layout{Offsets[0], Offsets[1], Offsets[2], Offsets[3], Offsets[4], Offsets[5], Offsets[6],
                    Offsets[7], Offsets[8], Offsets[9], Offsets[10], Offsets[11], Offsets[12]};
    return Cached;
}

template <typename T> T ReadOr(uintptr_t Address, T Fallback = {})
{
    return Memory::Read<T>(Address).value_or(Fallback);
}

template <typename T> void Write(uintptr_t Address, T Value)
{
    Memory::WriteBytes(Address, &Value, sizeof(T));
}

uintptr_t GameInstance()
{
    const uintptr_t World = CurrentWorld();
    const uintptr_t Instance = World ? Memory::ReadPointer(World + Offsets::UWorld::OwningGameInstance).value_or(0) : 0;
    return Instance && IsA(Instance, "DDMissionGameInstance") ? Instance : 0;
}

RouteMap::Ident ReadIdent(uintptr_t Route, const Layout& L)
{
    return {ReadOr<int32_t>(Route + L.Ident, -1), ReadOr<int32_t>(Route + L.Ident + 4, -1)};
}

std::vector<int32_t> ReadInts(uintptr_t Array)
{
    const auto Value = ReadOr<ScriptArray>(Array);
    std::vector<int32_t> Out;
    if (!Value.Data || Value.Num <= 0 || Value.Num > RouteMap::MaxChosen)
        return Out;
    Out.resize(Value.Num);
    if (!Memory::ReadBytes(Value.Data, Out.data(), Out.size() * sizeof(int32_t)))
        Out.clear();
    return Out;
}

// TArray<int32> mit dem Allokator des Spiels auf Values setzen. Max wächst nur.
bool WriteInts(uintptr_t Array, const std::vector<int32_t>& Values)
{
    auto Value = ReadOr<ScriptArray>(Array);
    const int32_t Num = static_cast<int32_t>(Values.size());
    if (Num > Value.Max)
    {
        void* Data = reinterpret_cast<ReallocFn>(At(MemoryRealloc))(reinterpret_cast<void*>(Value.Data), Values.size() * sizeof(int32_t),
                                                                    alignof(int32_t));
        if (!Data)
            return false;
        Value.Data = reinterpret_cast<uintptr_t>(Data);
        Value.Max = Num;
    }
    if (Num && !Memory::WriteBytes(Value.Data, Values.data(), Values.size() * sizeof(int32_t)))
        return false;
    Value.Num = Num;
    return Memory::WriteBytes(Array, &Value, sizeof(Value));
}

// Eigene Kopie eines FRoute im Speicher der Mod, die Inhalte (Arrays) liegen im Heap des Spiels. Leer heißt alles 0.
struct RouteCopy
{
    alignas(16) uint8_t Bytes[RouteSize]{};
    bool Held = false;
};

void Assign(void* To, const void* From)
{
    reinterpret_cast<RouteAssignFn>(At(RouteAssign))(To, From);
}

void Release(RouteCopy& Copy)
{
    if (Copy.Held)
        reinterpret_cast<DestructFn>(At(DestructRoutes))(Copy.Bytes, 1);
    std::memset(Copy.Bytes, 0, sizeof(Copy.Bytes));
    Copy.Held = false;
}

void Hold(RouteCopy& Copy, uintptr_t Route)
{
    Release(Copy);
    Assign(Copy.Bytes, reinterpret_cast<const void*>(Route));
    Copy.Held = true;
}

// Client: eigene Run-Info vor der Übernahme, wieder eingesetzt nach dem Weltwechsel.
struct Own
{
    int32_t Seed = 0, Threat = 0;
    std::vector<int32_t> Chosen;
    RouteCopy Current, Previous;
};

struct
{
    std::optional<RouteMap::HostRoute> Host; // Client: zuletzt vom Host
    uint64_t AppliedEnd = 0;                 // Client: zuletzt übernommenes Rundenende
    uintptr_t AppliedWorld = 0;              // Client: Welt mit übernommener Route, 0 ohne
    uintptr_t AppliedInstance = 0;
    Own Saved;
    int PanelFills = 0, BlockedChoices = 0;
} S; // nur Game Thread

void RestoreOwn(const char* Why)
{
    if (!S.AppliedWorld)
        return;
    const auto L = ResolveLayout();
    const uintptr_t Instance = GameInstance();
    if (L && Instance && Instance == S.AppliedInstance)
    {
        const uintptr_t Info = Instance + L->RunInfo;
        Write<int32_t>(Info + L->Seed, S.Saved.Seed);
        Write<int32_t>(Info + L->Threat, S.Saved.Threat);
        WriteInts(Info + L->Chosen, S.Saved.Chosen);
        Assign(reinterpret_cast<void*>(Info + L->Current), S.Saved.Current.Bytes);
        Assign(reinterpret_cast<void*>(Info + L->Previous), S.Saved.Previous.Bytes);
        Core::Log::Info("Routenkarte: eigene Run-Info zurückgestellt ({})", Why);
    }
    else
        Core::Log::Warn("Routenkarte: GameInstance gewechselt, eigene Run-Info nicht zurückgestellt ({})", Why);
    Release(S.Saved.Current);
    Release(S.Saved.Previous);
    S.Saved = {};
    S.AppliedWorld = 0;
    S.AppliedInstance = 0;
}

// Eintrag Ident der Missionsstruktur, 0 ohne.
uintptr_t StructureRoute(const ScriptArray& Groups, const RouteMap::Ident& Ident, const Layout& L)
{
    std::vector<int32_t> Routes;
    for (int32_t Group = 0; Group < Groups.Num && Group < RouteMap::MaxGroups; ++Group)
        Routes.push_back(ReadOr<ScriptArray>(Groups.Data + GroupSize * Group + L.PossibleRoutes).Num);
    if (!Groups.Data || !RouteMap::IdentInStructure(Ident, Routes))
        return 0;
    const auto Possible = ReadOr<ScriptArray>(Groups.Data + GroupSize * Ident.Group + L.PossibleRoutes);
    return Possible.Data ? Possible.Data + RouteSize * Ident.Route : 0;
}

bool ApplyHost(const RouteMap::HostRoute& Host)
{
    const auto L = ResolveLayout();
    const uintptr_t Instance = GameInstance();
    const uintptr_t Subsystem = Objects::FindInstance("MissionGenerationSubsystem");
    if (!L || !Instance || !Subsystem || Memory::ReadPointer(Subsystem + Offsets::UObject::Outer).value_or(0) != Instance)
    {
        Core::Log::Warn("Routenkarte: GameInstance oder Missionsstruktur fehlt, Client zeigt die Karte ohne Route");
        return false;
    }
    const uintptr_t Info = Instance + L->RunInfo;
    if (!S.AppliedWorld)
    {
        S.Saved.Seed = ReadOr<int32_t>(Info + L->Seed);
        S.Saved.Threat = ReadOr<int32_t>(Info + L->Threat);
        S.Saved.Chosen = ReadInts(Info + L->Chosen);
        Hold(S.Saved.Current, Info + L->Current);
        Hold(S.Saved.Previous, Info + L->Previous);
        S.AppliedInstance = Instance;
    }
    S.AppliedWorld = CurrentWorld();
    Write<int32_t>(Info + L->Seed, Host.Seed);
    Write<int32_t>(Info + L->Threat, Host.ThreatIncrease);
    if (!WriteInts(Info + L->Chosen, Host.Chosen))
    {
        RestoreOwn("Wahl nicht schreibbar");
        return false;
    }
    // Die Struktur entsteht aus Seed und Run-Info, wie beim Host. Der Rückgabewert ist eine Kopie, der Cache bleibt im Subsystem.
    ScriptArray Copy;
    reinterpret_cast<GetMissionStructureFn>(At(GetMissionStructure))(Subsystem, &Copy, -1);
    if (Copy.Data)
    {
        reinterpret_cast<DestructFn>(At(DestructGroups))(reinterpret_cast<void*>(Copy.Data), Copy.Num);
        reinterpret_cast<FreeFn>(At(MemoryFree))(reinterpret_cast<void*>(Copy.Data));
    }
    const auto Groups = ReadOr<ScriptArray>(Subsystem + L->Structure);
    const uintptr_t Current = StructureRoute(Groups, Host.Current, *L);
    const uintptr_t Previous = Host.Previous.Group >= 0 ? StructureRoute(Groups, Host.Previous, *L) : 0;
    if (!Current || (Host.Previous.Group >= 0 && !Previous))
    {
        Core::Log::Warn("Routenkarte: Route {}.{} des Hosts fehlt in der eigenen Missionsstruktur ({} Gruppen), Client zeigt sie nicht",
                        Host.Current.Group, Host.Current.Route, Groups.Num);
        RestoreOwn("Struktur passt nicht");
        return false;
    }
    if (Previous)
        Assign(reinterpret_cast<void*>(Info + L->Previous), reinterpret_cast<const void*>(Previous));
    const uintptr_t Route = Info + L->Current;
    Assign(reinterpret_cast<void*>(Route), reinterpret_cast<const void*>(Current));
    Write<int32_t>(Route + L->Index, Host.MissionIndex);
    Write<int32_t>(Route + L->Cached, Host.CachedMissionIndex);
    Write<uint8_t>(Route + L->End, Host.EndOfChain ? 1 : 0);
    Core::Log::Info("Routenkarte: Route {}.{} des Hosts übernommen (Seed {}, Tag {} von {}, Wahl {}, {} Gruppen)", Host.Current.Group,
                    Host.Current.Route, Host.Seed, Host.MissionIndex, ReadOr<ScriptArray>(Route + L->Chain).Num, Host.Chosen.size(),
                    Groups.Num);
    return true;
}

void __fastcall OpenPanelHook(uintptr_t Panel)
{
    const bool Client = CurrentRole() == NetRole::Client;
    if (!RouteMap::PanelNeedsAuthority(Client, S.AppliedWorld && S.AppliedWorld == CurrentWorld()))
        return OriginalOpenPanel(Panel);
    const uintptr_t Vtable = ReadOr<uintptr_t>(Panel);
    const auto Get = Vtable ? Memory::ReadPointer(Vtable + ControllerVtableSlot) : std::nullopt;
    const uintptr_t Controller = Get && *Get ? reinterpret_cast<uintptr_t(__fastcall*)(uintptr_t)>(*Get)(Panel) : 0;
    const auto Role = Controller ? Memory::Read<uint8_t>(Controller + ActorRole) : std::nullopt;
    if (!Role)
        return OriginalOpenPanel(Panel);
    Write<uint8_t>(Controller + ActorRole, RoleAuthority);
    OriginalOpenPanel(Panel);
    Write<uint8_t>(Controller + ActorRole, *Role);
    if (S.PanelFills++ < 20)
        Core::Log::Info("Routenkarte: Karte des Hosts beim Client befüllt");
}

// Ein Klick auf einen Knoten setzt die Route der GameInstance, danach sperrt ClientMayStartTravel die Reise. Die Route bleibt die
// des Hosts.
void __fastcall SetCurrentRouteHook(uintptr_t Instance, const void* Route)
{
    if (RouteMap::ClientMaySetRoute(CurrentRole() == NetRole::Client))
        return OriginalSetCurrentRoute(Instance, Route);
    if (S.BlockedChoices++ < 5)
        Core::Log::Info("Routenkarte: Wahl eines Knotens beim Client ignoriert, die Route wählt der Host");
}

bool CheckRouteMap()
{
    return Code::Expect(At(OpenPanel), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x18, 0x56, 0x57, 0x41, 0x56}, "UBusPanel::OpenPanel") &&
           Code::Expect(At(OpenPanelRoleCheck), {0x48, 0x8B, 0x07, 0x48, 0x8B, 0xCF, 0xFF, 0x90, 0xC8, 0x02, 0x00, 0x00, 0x80, 0xB8, 0x68, 0x01,
                                                 0x00, 0x00, 0x03, 0x0F, 0x85},
                        "Rollenprüfung in OpenPanel") &&
           Code::Expect(At(GetMissionStructure), {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x20, 0x48, 0x89, 0x50, 0x10, 0x55, 0x56, 0x57},
                        "GetMissionStructure") &&
           Code::Expect(At(DestructGroups), {0x85, 0xD2, 0x74, 0x3D, 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20},
                        "Missionsgruppen abbauen") &&
           Code::Expect(At(DestructRoutes), {0x85, 0xD2, 0x74, 0x4C, 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20}, "Routen abbauen") &&
           Code::Expect(At(RouteAssign), {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x48, 0x89, 0x7C, 0x24}, "FRoute::operator=") &&
           Code::Expect(At(SetCurrentRoute), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC}, "Route setzen") &&
           ExpectBranch(At(SetCurrentRoute, 0x50), CallRel32, RouteAssign, "operator= in Route setzen") &&
           ExpectBranch(At(DestructGroups, 0x1A), CallRel32, DestructRoutes, "Routen in Missionsgruppen abbauen") &&
           Code::Expect(At(MemoryRealloc), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC}, "FMemory::Realloc") &&
           Code::Expect(At(MemoryFree), {0x48, 0x85, 0xC9, 0x74, 0x2E, 0x53, 0x48, 0x83, 0xEC, 0x20}, "FMemory::Free");
}

bool ApplyRouteMap(Code::Writer&)
{
    RouteMapReady = Core::Hooks::Install(reinterpret_cast<void*>(At(OpenPanel)), reinterpret_cast<void*>(&OpenPanelHook),
                                         reinterpret_cast<void**>(&OriginalOpenPanel)) &&
                    Core::Hooks::Install(reinterpret_cast<void*>(At(SetCurrentRoute)), reinterpret_cast<void*>(&SetCurrentRouteHook),
                                         reinterpret_cast<void**>(&OriginalSetCurrentRoute));
    return RouteMapReady;
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t RouteMapSites[] = {OpenPanel, OpenPanelRoleCheck, GetMissionStructure, DestructGroups, DestructRoutes,
                                       RouteAssign, SetCurrentRoute, MemoryRealloc, MemoryFree};
const Patch List[] = {
    {"Routenkarte des Hosts beim Client", &CheckRouteMap, &ApplyRouteMap, RouteMapSites},
};
}

std::span<const Patch> RouteMapPatches()
{
    return List;
}
}

namespace Game::RouteMap
{
using namespace Game::Patches;

std::optional<HostRoute> ReadHostRoute()
{
    const auto L = ResolveLayout();
    const uintptr_t Instance = GameInstance();
    if (!RouteMapReady || !L || !Instance)
        return std::nullopt;
    const uintptr_t Info = Instance + L->RunInfo;
    const uintptr_t Current = Info + L->Current, Previous = Info + L->Previous;
    if (ReadOr<ScriptArray>(Current + L->Chain).Num <= 0)
        return std::nullopt; // vor dem ersten Knoten des Runs
    HostRoute Value;
    Value.Seed = ReadOr<int32_t>(Info + L->Seed);
    Value.ThreatIncrease = ReadOr<int32_t>(Info + L->Threat);
    Value.Current = ReadIdent(Current, *L);
    Value.MissionIndex = ReadOr<int32_t>(Current + L->Index, -1);
    Value.CachedMissionIndex = ReadOr<int32_t>(Current + L->Cached, -1);
    Value.EndOfChain = ReadOr<uint8_t>(Current + L->End) != 0;
    // Ohne Missionen steht in PreviousRoute keine gültige Kennung (gemessen: Zufallswerte).
    if (ReadOr<ScriptArray>(Previous + L->Chain).Num > 0)
        Value.Previous = ReadIdent(Previous, *L);
    Value.Chosen = ReadInts(Info + L->Chosen);
    Value.MapId = 1; // Platzhalter für Valid, die Sync-Schicht setzt die Karte
    if (!Valid(Value))
        return std::nullopt;
    Value.MapId = 0;
    return Value;
}

void SetHostRoute(std::optional<HostRoute> Value)
{
    S.Host = std::move(Value);
}

void ApplyBeforeRoundEnd(uint64_t EndMapId, uint64_t EndId, bool DayEnd)
{
    if (!RouteMapReady || CurrentRole() != NetRole::Client)
        return;
    const uint64_t RouteMapId = S.Host ? S.Host->MapId : 0;
    if (!ClientMayApplyRoute(RouteMapId, EndMapId, DayEnd, S.AppliedEnd == EndId))
    {
        if (DayEnd && S.AppliedEnd != EndId)
            Core::Log::Warn("Routenkarte: keine Route des Hosts zu dieser Karte, Client zeigt die Karte leer");
        return;
    }
    S.AppliedEnd = EndId;
    ApplyHost(*S.Host);
}

void Tick()
{
    if (ShouldRestore(S.AppliedWorld, CurrentWorld()))
        RestoreOwn("Weltwechsel");
}
}
