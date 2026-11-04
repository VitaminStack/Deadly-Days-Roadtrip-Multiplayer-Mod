# Tests

Tests für Code ohne SDK-Abhängigkeit. Sie laufen bei jedem Pull Request in GitHub Actions, ohne Spiel und ohne SDK. Code, der das SDK braucht, lässt sich dort nicht bauen, weil das SDK nicht im Repo liegt.

| Was | Wo | Lokal starten |
|---|---|---|
| C++-Tests mit doctest | `tests/CoreTests` | `bin\Release\CoreTests.exe` nach dem Bauen der Solution |
| Overlay-Modell und nativer Fenster-Lebenszyklus | `tests/UITests` | `bin\Release\UITests.exe`, auch Teil von `Build-Mod.ps1 -Test` und CI; ohne Spiel/SDK |
| Transport-Tunnel mit nachgebautem Steam | `tests/NetTests` | `bin\Release\NetTests.exe`, auch Teil von `Build-Mod.ps1 -Test` und CI; ohne Spiel/Steam |
| Python-Tests | `tests/test_*.py` | `py -B -m unittest discover -s tests` |

`test_module_boundaries.py` prüft die [Modulgrenzen](../docs/ARCHITECTURE.md#module) an den Includes. Ein neuer Ordner unter `src/` braucht dort einen Eintrag.

Neue Module bekommen ein eigenes Testprojekt nach dem Muster von `CoreTests`, zuerst `Net` für Serialisierung, Protokoll und Handshake.
