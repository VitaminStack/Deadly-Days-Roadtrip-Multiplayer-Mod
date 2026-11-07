#pragma once

#include <cstddef>
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
    Partial = 3,  // Setzen brach mittendrin ab, oder Teilinstallation (DDCOOP_PARTIAL_PATCHES) ließ Listen aus
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

// Host: Der Remote-Spieler steht als P2 in ActivePlayers des Coop-Subsystems, ohne zweiten LocalPlayer. Nur Game Thread.
bool RemoteP2Bound();

// Host: Ab jetzt zusätzlich den nativen Koop-Modus für den Remote-Spieler einschalten und seinen Beitritt melden. Standard
// ist aus, weil Lager und Airdrop-Auswahl im Koop-Modus auf eine lokale Eingabe von P2 warten. Für airdrop-auswahl und
// inventar-ui, sobald sie diese Eingaben übers Netz liefern. Zurückschalten ist nicht vorgesehen.
void AllowRemoteCoop();

struct Patch
{
    const char* Name;
    // Prüft Bytes und vtable-Einträge, ändert nichts.
    bool (*Check)();
    // Setzt den Patch. Code-Höhlen kommen in den gemeinsamen Writer.
    bool (*Apply)(Code::Writer& Code);
    // Referenz-RVAs aller Stellen, die Check, Apply und die Hooks des Patches benutzen (patch-signaturen). Passt die
    // Signatur einer davon nicht, wird nur dieser Patch ausgelassen. Leer: keine Angabe, es gilt allein Check.
    std::span<const uintptr_t> Sites{};
};

enum class InstallResult
{
    Installed,
    CheckFailed,
    ApplyFailed,
};

// Darf die Stelle mit dieser Referenz-RVA benutzt werden? Standard: Memory::Rva liefert nicht 0. Tests setzen eigene ein.
using SiteCheck = bool (*)(uintptr_t ReferenceRva);
bool SiteUsable(uintptr_t ReferenceRva);

// Prüft erst alle Patches, setzt dann alle in Reihenfolge. Scheitert eine Prüfung, bleibt alles unverändert.
// Patches mit einer unbrauchbaren Stelle in Sites werden vorher ausgelassen und in Skipped gezählt, der Rest geht weiter.
InstallResult InstallAll(std::span<const Patch> List, Code::Writer& Code, SiteCheck Usable = SiteUsable, size_t* Skipped = nullptr);

// Eine Patch-Datei mit ihrem Tick. Für die Teilinstallation zählt die Liste als Einheit.
struct PatchList
{
    const char* Name;
    std::span<const Patch> Patches;
};

struct ListsResult
{
    size_t Installed = 0; // gesetzte Listen
    size_t Skipped = 0;   // Listen mit gescheiterter Prüfung, unverändert
    size_t SkippedPatches = 0; // einzelne Patches mit unpassender Signatur, siehe Patch::Sites
    bool ApplyFailed = false;
};

// Teilinstallation für die Portierung auf einen neuen Build (update-basis): jede Liste für sich ganz oder gar nicht.
// Enabled[i] wird wahr, wenn Lists[i] steht. Bricht das Setzen ab, hört die Funktion sofort auf.
ListsResult InstallLists(std::span<const PatchList> Lists, Code::Writer& Code, std::span<bool> Enabled);

// Entwicklerschalter DDCOOP_PARTIAL_PATCHES=1 gilt nur außerhalb des Referenz-Builds.
bool PartialInstallAllowed(bool SwitchOn, uint32_t Build);
}
