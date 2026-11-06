# Tests

Tests für Code ohne SDK-Abhängigkeit. Sie laufen bei jedem Pull Request in GitHub Actions, ohne Spiel und ohne SDK. Code, der das SDK braucht, lässt sich dort nicht bauen, weil das SDK nicht im Repo liegt.

| Was | Wo | Lokal starten |
|---|---|---|
| C++-Tests mit doctest | `tests/CoreTests` | `bin\Release\CoreTests.exe` nach dem Bauen der Solution |
| Overlay-Modell und nativer Fenster-Lebenszyklus | `tests/UITests` | `bin\Release\UITests.exe`, auch Teil von `Build-Mod.ps1 -Test` und CI; ohne Spiel/SDK |
| Transport-Tunnel mit nachgebautem Steam | `tests/NetTests` | `bin\Release\NetTests.exe`, auch Teil von `Build-Mod.ps1 -Test` und CI; ohne Spiel/Steam |
| Python-Tests | `tests/test_*.py` | `py -B -m unittest discover -s tests` |

`test_module_boundaries.py` prüft die [Modulgrenzen](../docs/ARCHITECTURE.md#module) an den Includes. Ein neuer Ordner unter `src/` braucht dort einen Eintrag.

## CI

`.github/workflows/ci.yml` baut die Solution in Debug und Release und führt danach die C++-Tests aus. Python-Tests und die beiden `Run-*.ps1` hängen nicht von der Konfiguration ab und laufen einmal im Job `scripts`, parallel zum Build. Ändert ein PR nur `*.md` oder `docs/`, baut die CI nicht: Der Job `changes` erkennt das, und `build-and-test` überspringt alle Schritte und meldet Erfolg, damit die Pflichtprüfungen der Branch-Regel auch bei Doku-PRs vorliegen. Scheitert die Erkennung, wird gebaut; `.github/workflows/docs.yml` prüft `TODO.md`, README-Fortschritt und Fortschrittsbild bei jeder Änderung. Ein neuer Push auf denselben PR bricht den laufenden Lauf ab.

Der Build läuft über ccache (Cache je Konfiguration in `actions/cache`) und mit MultiToolTask, also je Quelldatei ein Compileraufruf und mehrere parallel. Dafür schaltet `/p:DdcoopCompilerCache=true` in `src/Common.props` auf `/Z7` und eine Objektdatei je Quelle; lokale Builds bleiben bei `/Zi`. Neue Projekte erben das über `Common.props`. Die Statistik am Ende des Build-Schritts zeigt Treffer und Fehlschläge.

Net-Tests mit echtem Transport sollen nicht auf feste Takte warten: `TransportOptions::TotalsIntervalMs` steht in den Tests auf 20 ms, und `NetTests` setzt die Windows-Zeitscheibe auf 1 ms. Neue Zeitlimits in Tests so kurz wählen, dass sie noch sicher greifen, und mit mehreren Läufen prüfen.

Neue Module bekommen ein eigenes Testprojekt nach dem Muster von `CoreTests`, zuerst `Net` für Serialisierung, Protokoll und Handshake.
