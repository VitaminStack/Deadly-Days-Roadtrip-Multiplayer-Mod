#pragma once

#include <span>

// Sperren, ohne die die Mod nicht laufen darf: Nichts aus einem Mod-Betrieb soll beim Hersteller landen.
// Siehe src/Core/README.md.
namespace Core::Protection
{
struct Step
{
    const char* Name;
    bool (*Install)();
};

// Installiert die Schritte der Reihe nach. Beim ersten Fehlschlag wird er geloggt, die übrigen laufen nicht mehr,
// und das Ergebnis ist falsch. Ohne Schritte ist nichts zu schützen, das Ergebnis ist wahr.
bool InstallAll(std::span<const Step> Steps);

// Alle Sperren der Mod: Crash-Reporter, Telemetrie und Berichte, Steam-Erfolge und -Bestenlisten. Braucht Hooks::Initialize.
// Falsch heißt: Die Mod darf nicht starten.
bool Install();
}
