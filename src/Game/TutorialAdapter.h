#pragma once

#include <cstdint>
#include <optional>

#include "Game/WorldAdapter.h"

// Schaltet die Tutorial-Hinweise im Coop ab. Ein offener Hinweis pausiert die Welt für beide Spieler.
// Der Schalter des Spiels gilt nur bis zum nächsten Start, der Tick prüft ihn deshalb jede Sekunde.
namespace Game
{
enum class TutorialAction
{
    None,  // Allein oder Subsystem noch nicht da
    Skip,  // SetForceSkipTutorials(true) aufrufen
    Done   // Schalter steht schon
};

// Reine Entscheidung: nur als Host oder Client, sobald der Schalter lesbar ist und noch nicht steht.
TutorialAction DecideTutorials(NetRole Role, std::optional<int32_t> SkipFlag);

// Game-Thread-Tick, prüft höchstens einmal je Sekunde.
void TickTutorials();
}
