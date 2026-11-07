#pragma once

// Trennt die Spielstände der Mod vom normalen Spielstand (mod-saves). Ohne Spiel-Offsets: Die Datei-APIs von Windows
// leiten jeden Zugriff unter <Saved>\SaveGames nach <Saved>\SaveGames_DDCoop um, solange die Mod geladen ist.
namespace Game::SaveRedirect
{
// Vor allem anderen nach Core::Hooks::Initialize aufrufen. Falsch, wenn nicht jede Umleitung sitzt; dann ist keine
// aktiv und die Mod darf nicht starten. CopyNormalSaves kopiert beim ersten Start die normalen .sav-Dateien.
bool Install(bool CopyNormalSaves);

// Anzahl umgeleiteter Dateiaufrufe seit Install, für Diagnose.
long long RedirectedCalls();
}
