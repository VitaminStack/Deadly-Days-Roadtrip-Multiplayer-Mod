#pragma once

namespace Game
{
// Die Mod ruft Start einmal nach bestandener Versionsprüfung auf. Registriert die Adapter-Ticks und startet den
// Game-Thread-Takt, kehrt sofort zurück.
void Start();
}
