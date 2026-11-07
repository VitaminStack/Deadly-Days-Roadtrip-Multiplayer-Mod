#pragma once

#include <cstdint>
#include <span>

// Code- und Datenadressen je Spiel-Build (update-basis). Der Quelltext nennt alle RVAs des Referenz-Builds; für jeden
// weiteren Build rechnet eine von tools/rva_map.py erzeugte Tabelle sie um. Fehlt eine Adresse, ist das Ergebnis 0:
// Die Byte-Prüfungen der Patches schlagen dann fehl und keine Patch-Liste wird halb gesetzt.
namespace Game::AddressMap
{
inline constexpr uint32_t ReferenceBuild = 26187885;

struct Entry
{
    uint32_t Reference;
    uint32_t Mapped;
};

// Tabelle eines Builds, aufsteigend nach Reference. Leer für den Referenz-Build und unbekannte Builds.
std::span<const Entry> Table(uint32_t Build);

// RVA im Build zur Referenz-RVA. Im Referenz-Build unverändert, sonst 0 ohne Tabelleneintrag.
uint32_t Map(uint32_t Build, uint32_t ReferenceRva);

// Unterstützt die Mod Adressen dieses Builds überhaupt (Referenz oder eigene Tabelle)?
bool Known(uint32_t Build);
}
