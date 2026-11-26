// opt-ergebnis-werte: Werte der Rundenübersicht. Das UDDGameStatsSubsystem führt sie in zwei Containern direkt hinter
// CurrentGameData: gesamt eine TMultiMap<FMissionTimeIdentifier, FGameStat>, je Spieler eine TMap<int32, gleicher Container>. Die
// Übersicht zeigt den Tag aus CurrentGameData.TimeIdentifier. Der Host liest seinen Stand, der Client setzt seinen darauf, bevor er
// das Rundenende des Hosts nachzieht, und noch einmal nach seiner eigenen Tagesend-Buchung (OnRoundChanged -> StopTracking).
// Geschrieben wird über die nativen Helfer des Spiels zum Suchen und Anlegen, nicht über IncrementGameStat: Das meldet jede
// Änderung an Erfolge, Ziele und Item-Effekte weiter.
#include "Game/RoundStatsAdapter.h"

#include <atomic>
#include <cstring>
#include <utility>

#include "Core/Hooks.h"
#include "Core/Log.h"
#include "Game/CodeCave.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

namespace Game::RoundStats
{
namespace
{
const Snapshot* HeldHost = nullptr; // nur Game Thread, siehe HoldForRoundChange
int Reapplied = 0;
uintptr_t RunEndedWorld = 0; // Client: Welt, in der der Host den Run beendet hat, siehe NoteRunEnded
}
}

namespace Game::Patches
{
namespace
{
constexpr uintptr_t StatIncrement = 0x58F45A0;    // Container suchen oder anlegen und erhöhen (Typ 0 und 4: Maximum), ohne Meldung
constexpr uintptr_t StatFind = 0x58EBFD0;         // Container: FGameStat* zu Typ und Tag, 0 ohne Eintrag
constexpr uintptr_t PlayerFindOrAdd = 0x58D56F0;  // TMap<int32, Container>::FindOrAdd, liefert den Container
constexpr uintptr_t IncrementImpl = 0x58F4420;    // UDDGameStatsSubsystem::IncrementGameStat
constexpr uintptr_t IncrementPlayer = 0x58F4520;  // dessen Teil je Spieler
constexpr uintptr_t GetGameStatImpl = 0x58EF260;  // UDDGameStatsSubsystem::GetGameStat
constexpr uintptr_t RoundChanged = 0x58FC4B0;     // UDDGameStatsSubsystem::OnRoundChanged(Reason): StopTracking, dann Sieg und Ratables
constexpr uintptr_t RoundChangedThunk = 0x56C6B4B; // Aufruf im Exec-Thunk, dem einzigen Aufrufer
constexpr uintptr_t WasFinalMission = 0x590D930;   // UDDGameStatsSubsystem::WasFinalMission
constexpr uintptr_t GenerateStats = 0x5AE3510;     // URoundEndPanel::GenerateStats
// In GenerateStats: Summen aller Tage, wenn WasFinalMission oder alle Figuren tot sind, sonst der Tag aus TimeIdentifier.
constexpr uintptr_t GenerateStatsFinalCall = 0x5AE367D;

bool CheckRoundStats()
{
    return Code::Expect(At(StatIncrement), {0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x10, 0x4D, 0x89, 0x43, 0x18}, "Statistik erhöhen") &&
           Code::Expect(At(StatFind), {0x4C, 0x89, 0x44, 0x24, 0x18, 0x53, 0x48, 0x83, 0xEC, 0x50, 0x0F, 0xB6, 0xDA}, "Statistik suchen") &&
           Code::Expect(At(PlayerFindOrAdd), {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x8B, 0x41, 0x08}, "Statistik je Spieler anlegen") &&
           Code::Expect(At(RoundChanged), {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x0F, 0xB6, 0xDA, 0x48, 0x8B, 0xF9},
                        "OnRoundChanged der Statistik") &&
           ExpectBranch(At(IncrementImpl, 0x27), CallRel32, StatIncrement, "Gesamtwert in IncrementGameStat") &&
           ExpectBranch(At(IncrementPlayer, 0x4C), CallRel32, PlayerFindOrAdd, "Spieler-Container in IncrementGameStat") &&
           ExpectBranch(At(IncrementPlayer, 0x60), CallRel32, StatIncrement, "Spielerwert in IncrementGameStat") &&
           ExpectBranch(At(GetGameStatImpl, 0x0B), CallRel32, StatFind, "Suche in GetGameStat") &&
           ExpectBranch(At(RoundChangedThunk), CallRel32, RoundChanged, "OnRoundChanged im Exec-Thunk") &&
           Code::Expect(At(WasFinalMission), {0x48, 0x83, 0xEC, 0x38, 0x48, 0x8D, 0x44, 0x24, 0x48}, "WasFinalMission") &&
           Code::Expect(At(GenerateStats), {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18}, "GenerateStats") &&
           Code::Expect(At(GenerateStatsFinalCall, 5), {0x84, 0xC0, 0x75}, "Prüfung nach WasFinalMission in GenerateStats") &&
           ExpectBranch(At(GenerateStatsFinalCall), CallRel32, WasFinalMission, "WasFinalMission in GenerateStats");
}

std::atomic<bool> RoundStatsReady = false;
using RoundChangedFn = void(__fastcall*)(uintptr_t, uint8_t);
RoundChangedFn OriginalRoundChanged = nullptr;

// Beim Client bucht das Spiel am Tagesende selbst (Dauer, DPS, Leben) unter seiner Tageskennung. Gilt gerade ein Stand des Hosts,
// wird er danach noch einmal gesetzt, bevor die Übersicht ihn liest. Sonst unverändert.
void __fastcall RoundChangedHook(uintptr_t Subsystem, uint8_t Reason)
{
    OriginalRoundChanged(Subsystem, Reason);
    if (RoundStats::HeldHost && RoundStats::ApplyHost(*RoundStats::HeldHost))
        ++RoundStats::Reapplied;
}

// Nur der Aufruf in GenerateStats: Beim Client entscheidet der Host über das Run-Ende. Seine Übersicht zeigt dann die Summen aller
// Tage (bei der Niederlage, weil alle Figuren tot sind), die des Clients sähe nur den letzten Tag, weil dort nicht alle Figuren tot
// gemeldet sind. Ratables und der Exec-Thunk rufen WasFinalMission unverändert.
using WasFinalFn = bool(__fastcall*)(uintptr_t);
bool __fastcall WasFinalForPanel(uintptr_t Subsystem)
{
    const bool Final = reinterpret_cast<WasFinalFn>(At(WasFinalMission))(Subsystem);
    return RoundStats::PanelShowsRunTotals(
        Final, CurrentRole() == NetRole::Client && RoundStats::RunEndedWorld && RoundStats::RunEndedWorld == CurrentWorld());
}

bool ApplyRoundStats(Code::Writer& Code)
{
    // Der CALL bleibt, nur sein Ziel ändert sich. So stimmt die Rücksprungadresse.
    uint8_t* Stub = Code.EmitAbsoluteJump(reinterpret_cast<uintptr_t>(&WasFinalForPanel));
    const auto Delta = Code::Rel32(At(GenerateStatsFinalCall) + 5, reinterpret_cast<uintptr_t>(Stub));
    if (Code.Failed() || !Delta)
        return false;
    if (!Core::Hooks::Install(reinterpret_cast<void*>(At(RoundChanged)), reinterpret_cast<void*>(&RoundChangedHook),
                              reinterpret_cast<void**>(&OriginalRoundChanged)) ||
        !Code::WriteCode(At(GenerateStatsFinalCall) + 1, &*Delta, sizeof(*Delta)))
        return false;
    RoundStatsReady = true;
    return true;
}

// Je Patch die benutzten Adressen der Signaturtabelle, erzeugt von tools/patch_signatures.py sites.
constexpr uintptr_t RoundStatsSites[] = {StatIncrement, StatFind, PlayerFindOrAdd, IncrementImpl, IncrementPlayer, GetGameStatImpl, RoundChanged, RoundChangedThunk, WasFinalMission, GenerateStats, GenerateStatsFinalCall};
// Steht in der Liste Map (MapPatches.cpp), hier nur für die Werkzeuge der Signaturtabelle.
const Patch List[] = {
    {"Werte der Rundenübersicht vom Host", &CheckRoundStats, &ApplyRoundStats, RoundStatsSites},
};
}

Patch RoundStatsPatch()
{
    return List[0];
}

std::span<const uintptr_t> RoundStatsTickOptionalSites()
{
    return RoundStatsSites;
}
}

namespace Game::RoundStats
{
namespace
{
using namespace Game::Patches;
using IncrementFn = int64_t(__fastcall*)(uintptr_t Container, uint8_t Type, uint64_t Day, int64_t Increment);
using FindFn = uintptr_t(__fastcall*)(uintptr_t Container, uint8_t Type, uint64_t Day);
using PlayerFn = uintptr_t(__fastcall*)(uintptr_t Map, int32_t Player, const int32_t* Key);

constexpr Reflection::FieldSpec CurrentGameData{"/Script/DDSurvivors", "DDGameStatsSubsystem", "CurrentGameData", "StructProperty", 0xC8};
constexpr uintptr_t FGameStatValue = 8;

Objects::Ref Held; // nur Game Thread

// CurrentGameData des Subsystems; TimeIdentifier ist sein erstes Feld, der Gesamtcontainer folgt direkt dahinter, der Container je
// Spieler mit 0x50 Byte Abstand.
uintptr_t GameData()
{
    if (!RoundStatsReady)
        return 0;
    if (!Objects::IsLive(Held))
    {
        const uintptr_t Found = Objects::FindInstance("DDGameStatsSubsystem");
        Held = Found ? Objects::RefTo(Found) : Objects::Ref{};
    }
    const auto Field = Reflection::Property(CurrentGameData);
    return Held.Object && Field ? Held.Object + Field->Offset : 0;
}

uintptr_t GlobalContainer(uintptr_t Data)
{
    return Data ? Data + CurrentGameData.Size : 0;
}

std::optional<SetHeader> ReadHeader(uintptr_t Address)
{
    SetHeader Set{};
    if (!Address || !Memory::ReadBytes(Address, &Set, sizeof(Set)) || !Plausible(Set))
        return std::nullopt;
    return Set;
}

// Belegte Elemente eines Sets, je Element Stride Byte; Bits liegen inline oder im zweiten Speicher.
bool ReadElements(const SetHeader& Set, size_t Stride, std::vector<uint8_t>& Data, std::vector<uint32_t>& Bits)
{
    const size_t Words = (size_t(Set.Num) + 31) / 32;
    Data.resize(size_t(Set.Num) * Stride);
    Bits.assign(Set.InlineBits, Set.InlineBits + 4);
    if (Set.SecondaryBits)
    {
        Bits.resize(Words);
        if (Words && !Memory::ReadBytes(Set.SecondaryBits, Bits.data(), Words * sizeof(uint32_t)))
            return false;
    }
    return !Set.Num || Memory::ReadBytes(Set.Data, Data.data(), Data.size());
}

bool ReadContainer(const SetHeader& Set, int32_t Player, std::vector<Entry>& Out)
{
    std::vector<uint8_t> Data;
    std::vector<uint32_t> Bits;
    if (!ReadElements(Set, StatElementSize, Data, Bits))
        return false;
    for (int32_t Index = 0; Index < Set.Num; ++Index)
        if (Allocated(Bits.data(), Bits.size(), Index))
            Out.push_back(DecodeStatElement(Data.data() + size_t(Index) * StatElementSize, Player));
    return true;
}

uint64_t PackDay(int32_t Day, int32_t SubMission)
{
    return uint64_t(uint32_t(Day)) | uint64_t(uint32_t(SubMission)) << 32;
}
}

std::optional<Snapshot> ReadLocal()
{
    const uintptr_t Data = GameData();
    const uintptr_t Global = GlobalContainer(Data);
    const std::optional<uint64_t> Time = Data ? Memory::Read<uint64_t>(Data) : std::optional<uint64_t>{};
    const auto GlobalSet = ReadHeader(Global);
    const auto PlayerSet = ReadHeader(Global ? Global + sizeof(SetHeader) : 0);
    if (!Time || !GlobalSet || !PlayerSet)
        return std::nullopt;
    Snapshot Value{static_cast<int32_t>(uint32_t(*Time)), static_cast<int32_t>(uint32_t(*Time >> 32))};
    if (!ReadContainer(*GlobalSet, GlobalPlayer, Value.Entries))
        return std::nullopt;
    std::vector<uint8_t> Elements;
    std::vector<uint32_t> Bits;
    if (!ReadElements(*PlayerSet, PlayerElementSize, Elements, Bits))
        return std::nullopt;
    for (int32_t Index = 0; Index < PlayerSet->Num; ++Index)
    {
        if (!Allocated(Bits.data(), Bits.size(), Index))
            continue;
        const uint8_t* Element = Elements.data() + size_t(Index) * PlayerElementSize;
        int32_t Player = 0;
        SetHeader Inner{};
        std::memcpy(&Player, Element, sizeof(Player));
        std::memcpy(&Inner, Element + PlayerContainerOffset, sizeof(Inner));
        if (Player < 0 || Player > MaxPlayer || !Plausible(Inner) || !ReadContainer(Inner, Player, Value.Entries))
            return std::nullopt;
    }
    return Value;
}

std::optional<size_t> ApplyHost(const Snapshot& Host)
{
    auto Local = ReadLocal();
    if (Local && StaleRun(*Local, Host.Day, Host.SubMission))
    {
        // Wie beim Host zu Beginn eines Runs: leert CurrentGameData, beide Container und das Siegflag.
        static const uintptr_t RunStarted = Reflection::Function("/Script/DDSurvivors", "DDGameStatsSubsystem", "OnRunStarted");
        if (!RunStarted)
            return std::nullopt;
        Core::Log::Info("Rundenwerte: eigener Stand bis Tag {}.{} stammt aus einem früheren Run, Subsystem zurückgesetzt", Local->Day,
                        Local->SubMission);
        Objects::ProcessEvent(Held.Object, RunStarted, nullptr);
        Local = ReadLocal();
    }
    const uintptr_t Data = GameData();
    const uintptr_t Global = GlobalContainer(Data);
    if (!Local || !Global || !ValidTime(Host.Day, Host.SubMission))
        return std::nullopt;
    const auto Increment = reinterpret_cast<IncrementFn>(At(StatIncrement));
    const auto Find = reinterpret_cast<FindFn>(At(StatFind));
    const auto PlayerContainer = reinterpret_cast<PlayerFn>(At(PlayerFindOrAdd));
    if (!Increment || !Find || !PlayerContainer)
        return std::nullopt;
    const uint64_t Time = PackDay(Host.Day, Host.SubMission);
    if (!Memory::WriteBytes(Data, &Time, sizeof(Time)))
        return std::nullopt;
    size_t Written = 0;
    for (const Entry& Value : PlanWrites(Local->Entries, Host.Entries))
    {
        const uintptr_t Container = Value.Player == GlobalPlayer ? Global : PlayerContainer(Global + sizeof(SetHeader), Value.Player, &Value.Player);
        if (!Container)
            continue;
        // Mit 0 erhöhen legt einen fehlenden Eintrag an und ändert keinen vorhandenen, danach steht der Wert des Hosts im Eintrag.
        Increment(Container, Value.Type, PackDay(Value.Day, Value.SubMission), 0);
        const uintptr_t Stat = Find(Container, Value.Type, PackDay(Value.Day, Value.SubMission));
        if (Stat && Memory::WriteBytes(Stat + FGameStatValue, &Value.Value, sizeof(Value.Value)))
            ++Written;
    }
    return Written;
}

void HoldForRoundChange(const Snapshot* Host)
{
    HeldHost = Host;
}

void NoteRunEnded(uintptr_t World)
{
    RunEndedWorld = World;
}

int TakeReapplied()
{
    return std::exchange(Reapplied, 0);
}
}
