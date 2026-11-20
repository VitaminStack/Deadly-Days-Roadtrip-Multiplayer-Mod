#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

// Code- und Datenadressen je Spiel-Build. Der Quelltext nennt alle RVAs des Referenz-Builds, des einzigen unterstützten Builds
// (Nutzerentscheidung vom 21.11.2026: nur noch der neueste Spiel-Build). Während eines Umstiegs kann eine von tools/rva_map.py
// erzeugte Tabelle RvaMap<build>.inc einen weiteren Build umrechnen (in Table eintragen). Fehlt eine Adresse, ist das Ergebnis 0:
// Die Byte-Prüfungen der Patches schlagen dann fehl und keine Patch-Liste wird halb gesetzt.
// constexpr, damit SdkChecks.cpp die Werte beim Bauen gegen das SDK des Builds prüfen kann.
namespace Game::AddressMap
{
inline constexpr uint32_t ReferenceBuild = 26786272;

struct Entry
{
    uint32_t Reference;
    uint32_t Mapped;
};

// Tabelle eines Builds, aufsteigend nach Reference. Leer für den Referenz-Build und unbekannte Builds.
// Seit spiel-update-26786272 gibt es nur den Referenz-Build und darum keine Tabelle.
constexpr std::span<const Entry> Table(uint32_t)
{
    return {};
}

// Unterstützt die Mod Adressen dieses Builds überhaupt (Referenz oder eigene Tabelle)?
constexpr bool Known(uint32_t Build)
{
    return Build == ReferenceBuild || !Table(Build).empty();
}

// RVA im Build zur Referenz-RVA. Im Referenz-Build unverändert, sonst 0 ohne Tabelleneintrag.
constexpr uint32_t Map(uint32_t Build, uint32_t ReferenceRva)
{
    if (Build == ReferenceBuild)
        return ReferenceRva;
    const auto Entries = Table(Build);
    const auto Found = std::ranges::lower_bound(Entries, ReferenceRva, {}, &Entry::Reference);
    return Found != Entries.end() && Found->Reference == ReferenceRva ? Found->Mapped : 0;
}

constexpr bool Sorted(std::span<const Entry> Entries)
{
    for (size_t Index = 1; Index < Entries.size(); ++Index)
        if (Entries[Index - 1].Reference >= Entries[Index].Reference)
            return false;
    return true;
}
}
