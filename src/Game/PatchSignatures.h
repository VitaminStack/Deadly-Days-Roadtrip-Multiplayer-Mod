#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>

// Byte-Signaturen der Patch-Adressen (patch-signaturen). Die Tabelle erzeugt tools/patch_signatures.py, das auch prüft,
// dass jedes Muster in der eingefrorenen Exe genau einmal vorkommt. Zur Laufzeit prüft die Mod das Muster nur an der
// erwarteten Stelle: Passt es nicht, liefert Memory::Rva für diese Adresse 0, und ein Patch, der sie in seinen Sites
// nennt, wird allein ausgelassen (Patches::InstallAll).
namespace Game::Signatures
{
inline constexpr uint32_t AllBuilds = 0;

struct Entry
{
    uint32_t Reference;  // RVA im Referenz-Build
    uint32_t Build;      // 0: gilt für alle Builds ohne eigene Zeile
    const char* Pattern; // "48 8B ?? 05", ?? beliebig
    uint32_t Offset;     // Lage der Adresse im Muster
    const char* Name;
};

inline constexpr Entry Table[] = {
#include "Game/PatchSignatures.inc"
};

constexpr bool Sorted(std::span<const Entry> Entries)
{
    for (size_t Index = 1; Index < Entries.size(); ++Index)
    {
        const Entry& A = Entries[Index - 1];
        const Entry& B = Entries[Index];
        if (A.Reference > B.Reference || (A.Reference == B.Reference && A.Build >= B.Build))
            return false;
    }
    return true;
}
static_assert(Sorted(Table), "PatchSignatures.inc muss nach RVA und Build sortiert sein, ohne doppelte Zeilen");

// Zeile für Referenz-RVA und Build: die eigene des Builds, sonst die für alle Builds. -1 ohne Zeile.
constexpr int Find(std::span<const Entry> Entries, uint32_t Reference, uint32_t Build)
{
    const auto First = std::ranges::lower_bound(Entries, Reference, {}, &Entry::Reference);
    int Shared = -1;
    for (auto It = First; It != Entries.end() && It->Reference == Reference; ++It)
    {
        if (Build != AllBuilds && It->Build == Build)
            return static_cast<int>(It - Entries.begin());
        if (It->Build == AllBuilds)
            Shared = static_cast<int>(It - Entries.begin());
    }
    return Shared;
}

// Passen die Bytes ab dem Musteranfang? Ungültiger Mustertext und zu wenige Bytes passen nie.
bool Matches(std::string_view Pattern, std::span<const uint8_t> Bytes);

// Laufzeit: Darf die Adresse benutzt werden? Ohne Tabellenzeile immer, sonst muss das Muster im laufenden Spiel an
// Address - Offset passen. Je Zeile einmal geprüft und gemerkt, ein Fehlschlag wird einmal protokolliert.
bool Usable(uint32_t Reference, uintptr_t Address);

// Alle Zeilen des aktiven Builds jetzt prüfen und merken. Install ruft das vor dem ersten Patch auf, weil Hooks und
// Code-Höhlen die geprüften Bytes danach überschreiben.
void VerifyAll();
}
