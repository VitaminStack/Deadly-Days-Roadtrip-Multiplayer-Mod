#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

// Hält Mod-Runs aus Steam-Erfolgen, -Statistiken und -Bestenlisten heraus. Siehe src/Core/README.md.
namespace Core::SteamStats
{
// Richtet die Sperre synchron ein und blockiert den Start von Steam nicht:
// - Ist steam_api64.dll geladen, hängt sie sich an SteamInternal_FindOrCreateUserInterface. Jede Statistik-Schnittstelle
//   wird gesperrt, bevor das Spiel sie bekommt; eine schon vorhandene sofort.
// - Sonst hängt sie sich an LoadLibraryExW und richtet das beim Laden der DLL ein. Gelingt das dann nicht, scheitert
//   das Laden: Ohne Steam gibt es nichts zu schreiben.
// Gesperrt werden die schreibenden Methoden von ISteamUserStats: Erfolge, Statistiken, Bestenlisten-Einträge. Lesen
// bleibt erlaubt. Falsch heißt: Die Sperre steht nicht, die Mod darf nicht starten. Braucht Hooks::Initialize.
bool Block();

// Wie oft seit dem Start ein Schreibversuch abgewiesen wurde.
int BlockedCalls();

// Liest aus dem Code einer flachen Steam-Funktion (SteamAPI_ISteamUserStats_*) den Abstand des Methodenzeigers in der
// vtable: mov rax,[rcx] und dann jmp [rax+X] oder mov r10,[rax+X]; jmp r10. Sonst nichts.
std::optional<uint32_t> VtableOffsetFromThunk(const uint8_t* Code, size_t Size);

// Für Tests: dieselbe Logik mit austauschbarer Umgebung statt der echten steam_api64.dll.
namespace Testing
{
struct Environment
{
    // Export der Steam-DLL oder nullptr, wenn sie fehlt oder den Namen nicht hat.
    void* (*Export)(const char* Name);
    // Wie Hooks::Install.
    bool (*Install)(void* Target, void* Detour, void** Original);
};

// Wie beim Laden der DLL: Methodenplätze lesen und FindOrCreateUserInterface umleiten. Falsch bei jedem Fehler.
bool Attach(const Environment& Env);

// Was die Umleitung von FindOrCreateUserInterface dem Spiel zurückgibt: die gesperrte Schnittstelle, unverändert
// bei anderen Schnittstellen, nullptr, wenn das Sperren scheitert oder die Version unbekannt ist.
void* OnInterface(const char* Version, void* Interface);

// Setzt den Zustand zurück. Gesetzte Hooks bleiben, deshalb in Tests je Fall eigene Ziel-Funktionen nehmen.
void Reset();
}
}
