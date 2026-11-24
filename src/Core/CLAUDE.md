# Core

Grundfunktionen für alle Module: Logging, Config, Hooks, Game-Thread-Dispatcher, Versionsprüfung, Sperren für Berichte und Statistiken. Darf nichts einbinden, kein SDK.

## Muster

- Includes über den Modulordner, etwa `#include "Core/Log.h"`.
- Schnittstellen hier nutzen alle Module: Änderungen in kurzen eigenen Pull Requests.

## Wo was liegt

- `Log`, `Config`, `Version`: Logging, Konfiguration, Build-Prüfung
- `Hooks`, `Signature`: Hook-Framework und Bytemuster-Suche im Image
- `Protection`: Sperren, ohne die die Mod nicht laufen darf (nichts aus dem Mod-Betrieb an den Hersteller)
- `Dispatcher`: Arbeit auf den Game Thread reichen
- `CrashReports`, `ReportHosts`, `SteamStats`: Berichte, Hosts und Erfolge sperren

## Tests

`tests/CoreTests`, lokal `bin\Release\CoreTests.exe`.

## Mehr

[README.md](README.md).
