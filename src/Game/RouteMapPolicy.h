#pragma once

#include <cstdint>
#include <vector>

// opt-routenkarte: Routenkarte des Hosts beim Client. Die Run-Info der GameInstance (FCurrentRunInfo) repliziert nicht, und
// UBusPanel::OpenPanel befüllt die Karte nur mit ROLE_Authority. Der Host meldet die Teile seiner Run-Info, aus denen das Spiel
// die Karte baut. Der Client erzeugt damit seine eigene, gleiche Missionsstruktur und setzt seine Route lokal darauf. Reine Logik,
// ohne Spiel testbar (tests/GameTests/RouteMapTests.cpp).
namespace Game::RouteMap
{
// FRouteMissionStructureIdentifier: Gruppe (Tag in der Missionsstruktur) und Route in deren PossibleRoutes.
struct Ident
{
    int32_t Group = -1;
    int32_t Route = -1;
    bool operator==(const Ident&) const = default;
};

// Grenzen für die Übertragung und das Anwenden. Gemessen: 8 Gruppen, je höchstens 3 Routen, eine Wahl je Tag.
inline constexpr int32_t MaxGroups = 64, MaxRoutes = 16, MaxChosen = 64, MaxMissionIndex = 64;

// Run-Info des Hosts vor seinem Rundenende, gebunden an die Karte (Game::Map::HostMap::Id), auf der er sie gelesen hat.
struct HostRoute
{
    uint64_t MapId = 0;           // nie 0
    int32_t Seed = 0;             // CurrentRunSeed
    int32_t ThreatIncrease = 0;   // PermanentThreatLevelIncrease
    Ident Current;                // CurrentRoute.RouteMissionStructureIdentifier
    int32_t MissionIndex = -1;    // CurrentRoute.CurrentMissionIndex
    int32_t CachedMissionIndex = -1; // CurrentRoute.CachedCurrentMissionIndex
    bool EndOfChain = false;      // CurrentRoute.IsEndOfMissionChain
    Ident Previous;               // PreviousRoute, Group -1 ohne
    std::vector<int32_t> Chosen;  // ChosenMissionIndexes
    bool operator==(const HostRoute&) const = default;
};

constexpr bool IdentValid(const Ident& Value)
{
    return Value.Group >= 0 && Value.Group < MaxGroups && Value.Route >= 0 && Value.Route < MaxRoutes;
}

// Keine oder eine gültige Kennung: Gruppe -1 heißt ohne (Route dann auch -1).
constexpr bool OptionalIdentValid(const Ident& Value)
{
    return (Value.Group == -1 && Value.Route == -1) || IdentValid(Value);
}

inline bool Valid(const HostRoute& Value)
{
    if (!Value.MapId || !IdentValid(Value.Current) || !OptionalIdentValid(Value.Previous))
        return false;
    if (Value.MissionIndex < -1 || Value.MissionIndex > MaxMissionIndex || Value.CachedMissionIndex < -1 ||
        Value.CachedMissionIndex > MaxMissionIndex)
        return false;
    if (Value.ThreatIncrease < 0 || Value.ThreatIncrease > 100000 || Value.Chosen.size() > MaxChosen)
        return false;
    for (const int32_t Index : Value.Chosen)
        if (Index < 0 || Index > 2 * MaxRoutes) // Knotenposition, gemessen 0, 2, 4
            return false;
    return true;
}

// Client: Die Route des Hosts gilt nur für das Tagesende der Karte, auf der er sie gelesen hat. Am Run-Ende (Sieg oder
// Niederlage) gibt es keine Routenkarte; mit der vollen Route würde das lokale FinishMission des Clients dort selbst den Sieg
// buchen. Jedes Rundenende nur einmal.
constexpr bool ClientMayApplyRoute(uint64_t RouteMapId, uint64_t EndMapId, bool DayEnd, bool AlreadyApplied)
{
    return RouteMapId != 0 && RouteMapId == EndMapId && DayEnd && !AlreadyApplied;
}

// Gibt es die Kennung in der eigenen Missionsstruktur des Clients? Routes[Gruppe] ist die Zahl der PossibleRoutes je Gruppe.
inline bool IdentInStructure(const Ident& Value, const std::vector<int32_t>& Routes)
{
    return IdentValid(Value) && Value.Group < static_cast<int32_t>(Routes.size()) && Value.Route < Routes[Value.Group];
}

// Client: OpenPanel sieht während des Aufrufs die Rolle Authority, damit das Spiel SetChoices aufruft. Nur, wenn die Route des
// Hosts in dieser Welt übernommen ist; sonst bliebe die Karte so leer wie bisher.
constexpr bool PanelNeedsAuthority(bool IsNetClient, bool RouteApplied)
{
    return IsNetClient && RouteApplied;
}

// Client: Mit der Welt endet die Routenkarte. Danach steht wieder die eigene Run-Info in der GameInstance, damit sie nie in
// einen eigenen Stand oder in die nächste Mission gerät.
constexpr bool ShouldRestore(uintptr_t AppliedWorld, uintptr_t World)
{
    return AppliedWorld != 0 && AppliedWorld != World;
}

// Client: Wahl über einen Knoten der Karte (LoadMapByInformation setzt Route und Wahl, dann reist es). Die Route wählt der Host.
constexpr bool ClientMayChooseRoute(bool IsNetClient)
{
    return !IsNetClient;
}
}
