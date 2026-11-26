#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

// opt-ergebnis-werte: Werte der Rundenübersicht. Das Spiel führt sie im UDDGameStatsSubsystem je Tag (FMissionTimeIdentifier)
// und Typ (EGameStatType), einmal gesamt und einmal je Spieler. Beim Client fehlen die Treffer und Kills des Hosts, darum zeigt
// seine Übersicht Nullen. Der Host bucht, der Client übernimmt dessen Stand als Ganzes vor dem Öffnen der Übersicht.
// Reine Regeln, ohne Spiel getestet in tests/GameTests/RoundStatsTests.cpp.
namespace Game::RoundStats
{
inline constexpr int32_t GlobalPlayer = -1; // Gesamtwert, sonst Spielerindex des Spiels
inline constexpr int32_t MaxPlayer = 7;
inline constexpr uint8_t TypeCount = 11; // EGameStatType ohne __COUNT
// Tag und Teilmission -1 kommen vor (gesehen: Tageskennung 1.-1 beim Client), die Teilmission geht als int8 über den Draht.
inline constexpr int32_t MinDay = -1, MaxDay = 100000, MinSubMission = -1, MaxSubMission = 127;
inline constexpr size_t MaxEntries = 3000;

struct Entry
{
    int32_t Day = 0, SubMission = 0;
    uint8_t Type = 0;
    int32_t Player = GlobalPlayer;
    int64_t Value = 0;
    bool operator==(const Entry&) const = default;
};

inline bool Valid(const Entry& Value)
{
    return Value.Type < TypeCount && Value.Day >= MinDay && Value.Day <= MaxDay && Value.SubMission >= MinSubMission && Value.SubMission <= MaxSubMission &&
           Value.Player >= GlobalPlayer && Value.Player <= MaxPlayer;
}

inline auto Key(const Entry& Value)
{
    return std::make_tuple(Value.Player, Value.Day, Value.SubMission, Value.Type);
}

// Stand eines Subsystems: aktuelle Tageskennung (CurrentGameData.TimeIdentifier) und alle Werte. Die Übersicht zeigt den Tag der
// Tageskennung und vergleicht mit dem Vortag; ohne die des Hosts zeigt der Client Tag 0.
struct Snapshot
{
    int32_t Day = 0, SubMission = 0;
    std::vector<Entry> Entries;
    bool operator==(const Snapshot&) const = default;
};

constexpr bool ValidTime(int32_t Day, int32_t SubMission)
{
    return Day >= MinDay && Day <= MaxDay && SubMission >= MinSubMission && SubMission <= MaxSubMission;
}

// Gültig und jeder Schlüssel (Spieler, Tag, Teilmission, Typ) nur einmal.
inline bool Valid(const std::vector<Entry>& Values)
{
    if (Values.size() > MaxEntries)
        return false;
    std::map<decltype(Key(Entry{})), bool> Seen;
    for (const auto& Value : Values)
        if (!Valid(Value) || !Seen.emplace(Key(Value), true).second)
            return false;
    return true;
}

// Host: Mehr als MaxEntries passen nicht in eine Nachricht. Dann bleiben die jüngsten Tage, weil die Übersicht den letzten Tag
// und seinen Vortag vergleicht. Ungültige Einträge (etwa ein unbekannter Typ eines neuen Builds) fallen weg.
inline std::vector<Entry> Limit(std::vector<Entry> Values)
{
    std::erase_if(Values, [](const Entry& Value) { return !Valid(Value); });
    std::sort(Values.begin(), Values.end(), [](const Entry& A, const Entry& B) {
        return std::tie(B.Day, B.SubMission, A.Player, A.Type) < std::tie(A.Day, A.SubMission, B.Player, B.Type);
    });
    Values.erase(std::unique(Values.begin(), Values.end(), [](const Entry& A, const Entry& B) { return Key(A) == Key(B); }), Values.end());
    if (Values.size() > MaxEntries)
        Values.resize(MaxEntries);
    return Values;
}

// Client: Was zu schreiben ist, damit der lokale Stand dem des Hosts gleicht. Jeder Wert des Hosts, der lokal fehlt oder anders
// ist, und 0 für lokale Werte, die der Host nicht hat (etwa aus einem früheren Run oder eigener Zählung). Entfernen kann die Mod
// Einträge nicht, eine 0 zeigt die Übersicht wie einen fehlenden Wert. Hat der Host gekürzt (Limit, volle MaxEntries), bleiben
// lokale Werte vor seinem ältesten Tag stehen: Über diese Tage sagt das Bild nichts.
inline std::vector<Entry> PlanWrites(const std::vector<Entry>& Local, const std::vector<Entry>& Host)
{
    const bool Complete = Host.size() < MaxEntries;
    auto Oldest = std::make_pair(MaxDay + 1, 0);
    for (const auto& Value : Host)
        Oldest = std::min(Oldest, std::make_pair(Value.Day, Value.SubMission));
    std::map<decltype(Key(Entry{})), int64_t> Have;
    for (const auto& Value : Local)
        Have[Key(Value)] = Value.Value;
    std::vector<Entry> Writes;
    std::map<decltype(Key(Entry{})), bool> FromHost;
    for (const auto& Value : Host)
    {
        FromHost[Key(Value)] = true;
        const auto Found = Have.find(Key(Value));
        if (Found == Have.end() || Found->second != Value.Value)
            Writes.push_back(Value);
    }
    for (const auto& Value : Local)
        if (!FromHost.contains(Key(Value)) && Value.Value != 0 && (Complete || std::make_pair(Value.Day, Value.SubMission) >= Oldest))
            Writes.push_back({Value.Day, Value.SubMission, Value.Type, Value.Player, 0});
    return Writes;
}

// Client: Die Übersicht erst öffnen, wenn die Werte zu diesem Rundenende da sind. Der Host schickt sie vor dem Rundenende, auf
// demselben zuverlässigen Weg; kommen sie trotzdem nicht (etwa ein Host, der sie nicht lesen kann), öffnet sie nach MaxWaitMs
// mit den eigenen Werten.
inline constexpr uint64_t MaxWaitMs = 2000;
constexpr bool ClientMayOpenRoundEnd(uint64_t EndId, uint64_t StatsEndId, uint64_t WaitedMs)
{
    return !EndId || StatsEndId == EndId || WaitedMs >= MaxWaitMs;
}

// Rundenübersicht (URoundEndPanel::GenerateStats): Summen aller Tage statt des letzten Tags, wenn das Spiel die letzte Mission
// meldet oder (nur beim Client) der Host den Run in dieser Welt beendet hat. Beim Host bleibt es beim Spiel: Seine Niederlage
// erkennt GenerateStats selbst daran, dass alle Figuren tot sind.
constexpr bool PanelShowsRunTotals(bool FinalMission, bool ClientRunEnded)
{
    return FinalMission || ClientRunEnded;
}

// TSet/TMap des Spiels (TSparseArray mit Belegungsbits, Hash), 0x50 Byte. Wie ControllerSet in PatchPolicy.h, aber nur gelesen,
// darum Adressen statt Zeiger. Gesamtwerte: Element 0x20 Byte (FMissionTimeIdentifier, FGameStat). Je Spieler: Element 0x60
// Byte (int32 Spielerindex, bei +8 derselbe Container).
struct SetHeader
{
    uint64_t Data;
    int32_t Num;
    int32_t Max;
    uint32_t InlineBits[4];
    uint64_t SecondaryBits;
    int32_t NumBits;
    int32_t MaxBits;
    int32_t FirstFree;
    int32_t NumFree;
    int32_t InlineHash[2];
    uint64_t SecondaryHash;
    int32_t HashSize;
    int32_t Padding;
};
static_assert(sizeof(SetHeader) == 0x50 && offsetof(SetHeader, SecondaryBits) == 0x20 && offsetof(SetHeader, HashSize) == 0x48);
inline constexpr size_t StatElementSize = 0x20, PlayerElementSize = 0x60, PlayerContainerOffset = 8;
inline constexpr int32_t MaxSetNum = 4096;

// Kopf plausibel? Inline-Bits reichen für 128 Plätze, darüber braucht es den zweiten Speicher.
inline bool Plausible(const SetHeader& Set)
{
    return Set.Num >= 0 && Set.Num <= MaxSetNum && Set.Max >= Set.Num && Set.NumBits == Set.Num && Set.MaxBits >= Set.NumBits &&
           Set.NumFree >= 0 && Set.NumFree <= Set.Num && (!Set.Num || Set.Data) && (Set.SecondaryBits || Set.NumBits <= 128);
}

// Belegt laut Bits; Bits sind die Wörter aus InlineBits oder SecondaryBits.
inline bool Allocated(const uint32_t* Bits, size_t Words, int32_t Index)
{
    return Index >= 0 && size_t(Index / 32) < Words && (Bits[Index / 32] & (1u << (Index % 32)));
}

// Ein Element des Gesamtcontainers: Day und SubMission (je 4), Typ (1, dann 7 Füllbytes), Wert (8), Hash (8).
inline Entry DecodeStatElement(const uint8_t* Element, int32_t Player)
{
    Entry Value;
    Value.Day = static_cast<int32_t>(uint32_t(Element[0]) | uint32_t(Element[1]) << 8 | uint32_t(Element[2]) << 16 | uint32_t(Element[3]) << 24);
    Value.SubMission =
        static_cast<int32_t>(uint32_t(Element[4]) | uint32_t(Element[5]) << 8 | uint32_t(Element[6]) << 16 | uint32_t(Element[7]) << 24);
    Value.Type = Element[8];
    uint64_t Raw = 0;
    for (int i = 0; i < 8; ++i)
        Raw |= uint64_t(Element[16 + i]) << (8 * i);
    Value.Value = static_cast<int64_t>(Raw);
    Value.Player = Player;
    return Value;
}
}
