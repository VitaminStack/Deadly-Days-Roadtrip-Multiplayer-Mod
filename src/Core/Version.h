#pragma once

#include <cstdint>
#include <optional>

// Prüft, ob die Mod in genau dem Spiel-Build läuft, für den ihre Offsets gelten.
// Unbekannter Build bedeutet: Mod bleibt aus, siehe docs/ARCHITECTURE.md.
namespace Core
{
// Felder aus dem PE-Header der Exe. Sie stehen im Speicher, ohne die 179 MB große Datei zu lesen.
struct BuildFingerprint
{
    uint32_t TimeDateStamp = 0;
    uint32_t SizeOfImage = 0;

    bool operator==(const BuildFingerprint&) const = default;
};

std::optional<BuildFingerprint> ReadFingerprint(const void* ImageBase);

// Steam-Build-ID zum Fingerabdruck, oder nichts bei einem unbekannten Build.
std::optional<uint32_t> FindSupportedBuild(const BuildFingerprint& Fingerprint);

// Prüft den laufenden Prozess. Liefert nichts, wenn es nicht das Spiel oder ein unbekannter Build ist.
std::optional<uint32_t> DetectSupportedBuild();
}
