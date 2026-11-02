# Core

Grundfunktionen für alle Module: Logging, Config, Hook-Framework, Game-Thread-Dispatcher und Versionsprüfung.
Kennt keine Spielklassen und bindet kein SDK ein. Includes laufen über den Modulordner, etwa `#include "Core/Log.h"`.

Die Schnittstellen hier nutzen alle Module. Änderungen daran gehen in kurzen eigenen Pull Requests, siehe [Gemeinsame Dateien](../../TODO.md#zentrale-arbeit).

## Logging: `Core/Log.h`

```cpp
Core::Log::Info("Lobby {} erstellt", LobbyId);
Core::Log::Warn("...");
Core::Log::Error("...");
Core::Log::Debug("...");
```

- Von jedem Thread aus aufrufbar, jede Zeile wird sofort geschrieben. Vor `Open` gehen Zeilen verloren.
- Zeitstempel sind UTC, dazu Thread-ID und Stufe.
- `Open` und `SetLevel` ruft nur der Start der Mod auf.
- Keine vollständigen lokalen Pfade, Steam-IDs oder IP-Adressen loggen, Logs landen in Bug-Reports.

## Config: `Core/Config.h`

Zeilen `schluessel=wert`, Kommentare mit `#` oder `;`, keine Abschnitte. Eine fehlende Datei oder ein fehlender oder unlesbarer Wert ergibt den übergebenen Standardwert, das ist kein Fehler.

```cpp
const auto Config = Core::Config::Load(ModDir / "ddcoop.ini");
const bool Enabled = Config.GetBool("enabled", true);
```

`GetString`, `GetBool` (`true`, `false`, `1`, `0`) und `GetInt` sind die ganze Schnittstelle.

## Game Thread: `Core/Dispatcher.h`

Unreal-Objekte dürfen nur auf dem Game Thread angefasst werden. Andere Threads geben Arbeit ab:

```cpp
Core::GameThread().Post([Value] { /* läuft auf dem Game Thread */ });
```

- `Post` darf jeder Thread aufrufen, die Aufgabe läuft später, ohne dass der Aufrufer wartet. Aufgaben laufen in Reihenfolge.
- Aufgaben kopieren Daten hinein. UObject-Zeiger können bis zur Ausführung ungültig werden.
- `Drain` ruft ein Hook auf dem Game Thread auf. Diesen Hook liefert `game-adapter`, bis dahin läuft keine Aufgabe.
- `IsDrainThread` ist für Prüfungen da und bleibt falsch, bis `Drain` einmal lief.
- Eine Ausnahme in einer Aufgabe wird geloggt, die übrigen Aufgaben laufen weiter.

## Versionsprüfung: `Core/Version.h`

`DetectSupportedBuild` liefert die Steam-Build-ID, wenn der Prozess das Spiel ist und der Fingerabdruck aus dem Exe-Header (`TimeDateStamp`, `SizeOfImage`) bekannt ist. Sonst liefert sie nichts, loggt den Grund, und die Mod bleibt aus. Die Liste steht in `Core/Version.cpp`.

Das Spiel schreibt die Build-ID nur ins Steam-Manifest, dessen Pfad die Mod nicht kennt. Der Fingerabdruck steht dagegen schon im Speicher. Neue Werte liefert `dumpbin /headers` auf die Exe, siehe [Spiel-Updates](../../CONTRIBUTING.md#spiel-updates).
