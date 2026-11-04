#pragma once

#include <cstdint>
#include <span>

#include "Game/CodeCave.h"

// Die Spielfehler-Fixes aus dem Spike als geprüfte Patches. Ohne sie läuft keine Mission zu zweit, siehe src/Game/README.md.
namespace Game::Patches
{
// Für den Spike über den Export DDCoopPatchState: Pending, solange Install noch nicht entschieden hat.
enum class State : int
{
    Pending = 0,
    Active = 1,   // alle Patches gesetzt
    Inactive = 2, // keiner gesetzt, etwa weil eine Prüfung scheiterte
    Partial = 3,  // Setzen brach mittendrin ab, ein Teil steht
};

State CurrentState();

// Prüft alle Stellen und setzt dann alle Patches, oder keinen. Einmal beim Start, braucht Core::Hooks::Initialize.
bool Install();

// Game-Thread-Tick: Teile, die Spielobjekte brauchen (Klassen-Defaultobjekte, Funktionsnamen), und die Abgleiche
// je Sekunde. Wird in Game::Start registriert.
void Tick();

// Wird nach jedem ProcessEvent aufgerufen, solange der Patch steht. Für den Spike, der darüber Rundenphasen beobachtet.
using EventObserver = void (*)(uintptr_t Object, uintptr_t Function, void* Params);
bool AddEventObserver(EventObserver Observer);

struct Patch
{
    const char* Name;
    // Prüft Bytes und vtable-Einträge, ändert nichts.
    bool (*Check)();
    // Setzt den Patch. Code-Höhlen kommen in den gemeinsamen Writer.
    bool (*Apply)(Code::Writer& Code);
};

enum class InstallResult
{
    Installed,
    CheckFailed,
    ApplyFailed,
};

// Prüft erst alle Patches, setzt dann alle in Reihenfolge. Scheitert eine Prüfung, bleibt alles unverändert.
InstallResult InstallAll(std::span<const Patch> List, Code::Writer& Code);
}
