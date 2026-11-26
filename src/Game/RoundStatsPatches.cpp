// opt-ergebnis-werte: Werte der Rundenübersicht. Das UDDGameStatsSubsystem führt sie in zwei Containern direkt hinter
// CurrentGameData: gesamt eine TMultiMap<FMissionTimeIdentifier, FGameStat>, je Spieler eine TMap<int32, gleicher Container>.
// Die Übersicht (URoundEndPanel.GenerateStats) und die Wertung lesen beim Öffnen nur dort. Der Host liest seine Werte, der Client
// setzt seine auf diese, bevor er das Rundenende des Hosts nachzieht. Geschrieben wird über die nativen Helfer des Spiels zum
// Suchen und Anlegen, nicht über IncrementGameStat: Das meldet jede Änderung an Erfolge, Ziele und Item-Effekte weiter.
#include "Game/RoundStatsAdapter.h"

#include <atomic>
#include <cstring>

#include "Core/Log.h"
#include "Game/CodeCave.h"
#include "Game/Memory.h"
#include "Game/Objects.h"
#include "Game/PatchRuntime.h"
#include "Game/Reflection.h"

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

bool CheckRoundStats()
{
    return Code::Expect(At(StatIncrement), {0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x10, 0x4D, 0x89, 0x43, 0x18}, "Statistik erhöhen") &&
           Code::Expect(At(StatFind), {0x4C, 0x89, 0x44, 0x24, 0x18, 0x53, 0x48, 0x83, 0xEC, 0x50, 0x0F, 0xB6, 0xDA}, "Statistik suchen") &&
           Code::Expect(At(PlayerFindOrAdd), {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x8B, 0x41, 0x08}, "Statistik je Spieler anlegen") &&
           ExpectBranch(At(IncrementImpl, 0x27), CallRel32, StatIncrement, "Gesamtwert in IncrementGameStat") &&
           ExpectBranch(At(IncrementPlayer, 0x4C), CallRel32, PlayerFindOrAdd, "Spieler-Container in IncrementGameStat") &&
           ExpectBranch(At(IncrementPlayer, 0x60), CallRel32, StatIncrement, "Spielerwert in IncrementGameStat") &&
           ExpectBranch(At(GetGameStatImpl, 0x0B), CallRel32, StatFind, "Suche in GetGameStat");
}

std::atomic<bool> RoundStatsReady = false;

bool ApplyRoundStats(Code::Writer&)
{
    RoundStatsReady = true;
    return true;
}

constexpr uintptr_t RoundStatsSiteList[] = {StatIncrement, StatFind, PlayerFindOrAdd, IncrementImpl, IncrementPlayer, GetGameStatImpl};
}

Patch RoundStatsPatch()
{
    return {"Werte der Rundenübersicht vom Host", &CheckRoundStats, &ApplyRoundStats, RoundStatsSiteList};
}

std::span<const uintptr_t> RoundStatsSites()
{
    return RoundStatsSiteList;
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

// Adresse des Gesamtcontainers: direkt hinter CurrentGameData; der Container je Spieler folgt mit 0x50 Byte Abstand.
uintptr_t GlobalContainer()
{
    if (!RoundStatsReady)
        return 0;
    if (!Objects::IsLive(Held))
    {
        const uintptr_t Found = Objects::FindInstance("DDGameStatsSubsystem");
        Held = Found ? Objects::RefTo(Found) : Objects::Ref{};
    }
    const auto Field = Reflection::Property(CurrentGameData);
    return Held.Object && Field ? Held.Object + Field->Offset + Field->Size : 0;
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

uint64_t PackDay(const Entry& Value)
{
    return uint64_t(uint32_t(Value.Day)) | uint64_t(uint32_t(Value.SubMission)) << 32;
}
}

std::optional<std::vector<Entry>> ReadLocal()
{
    const uintptr_t Global = GlobalContainer();
    const auto GlobalSet = ReadHeader(Global);
    const auto PlayerSet = ReadHeader(Global ? Global + sizeof(SetHeader) : 0);
    if (!GlobalSet || !PlayerSet)
        return std::nullopt;
    std::vector<Entry> Values;
    if (!ReadContainer(*GlobalSet, GlobalPlayer, Values))
        return std::nullopt;
    std::vector<uint8_t> Data;
    std::vector<uint32_t> Bits;
    if (!ReadElements(*PlayerSet, PlayerElementSize, Data, Bits))
        return std::nullopt;
    for (int32_t Index = 0; Index < PlayerSet->Num; ++Index)
    {
        if (!Allocated(Bits.data(), Bits.size(), Index))
            continue;
        const uint8_t* Element = Data.data() + size_t(Index) * PlayerElementSize;
        int32_t Player = 0;
        SetHeader Inner{};
        std::memcpy(&Player, Element, sizeof(Player));
        std::memcpy(&Inner, Element + PlayerContainerOffset, sizeof(Inner));
        if (Player < 0 || Player > MaxPlayer || !Plausible(Inner) || !ReadContainer(Inner, Player, Values))
            return std::nullopt;
    }
    return Values;
}

std::optional<size_t> ApplyHost(const std::vector<Entry>& Host)
{
    const auto Local = ReadLocal();
    const uintptr_t Global = GlobalContainer();
    if (!Local || !Global)
        return std::nullopt;
    const auto Increment = reinterpret_cast<IncrementFn>(At(StatIncrement));
    const auto Find = reinterpret_cast<FindFn>(At(StatFind));
    const auto PlayerContainer = reinterpret_cast<PlayerFn>(At(PlayerFindOrAdd));
    if (!Increment || !Find || !PlayerContainer)
        return std::nullopt;
    size_t Written = 0;
    for (const Entry& Value : PlanWrites(*Local, Host))
    {
        const uintptr_t Container = Value.Player == GlobalPlayer ? Global : PlayerContainer(Global + sizeof(SetHeader), Value.Player, &Value.Player);
        if (!Container)
            continue;
        // Mit 0 erhöhen legt einen fehlenden Eintrag an und ändert keinen vorhandenen, danach steht der Wert des Hosts im Eintrag.
        Increment(Container, Value.Type, PackDay(Value), 0);
        const uintptr_t Stat = Find(Container, Value.Type, PackDay(Value));
        if (Stat && Memory::WriteBytes(Stat + FGameStatValue, &Value.Value, sizeof(Value.Value)))
            ++Written;
    }
    return Written;
}
}
