# Spiel, SDK und Systeme, die die Mod respektiert

Gilt für Build `26786272`. Datierte Befunde und Messungen stehen im [Archiv](../archive/GAME_ANALYSIS.md), Abschnitte unten unter „Historie“.

## Überblick

- Unterstützt wird nur Spiel-Build `26786272` (Patch 1.0.3). Auf jedem anderen Build setzt die Mod keine Patches, siehe [Offsets](../OFFSETS.md).
- Das Spiel läuft auf Unreal Engine 5.8. Das SDK für Offsets und Klassen erzeugt Dumper-7 lokal; es gehört nie ins Repo.
- Die Mod sperrt im Coop Erfolge, Bestenlisten, Telemetrie und Absturzberichte an den Hersteller und schreibt eigene Spielstände, damit normale Spielstände unberührt bleiben.

## Code

| Datei | Aufgabe | Test |
|---|---|---|
| `src/Game/AddressMap.h`, `src/Game/Offsets.h`, `src/Game/SdkChecks.cpp` | RVAs und Feld-Offsets für `26786272`, Prüfung gegen das lokale SDK beim Bauen | `tests/GameTests/AddressMapTests.cpp` |
| `src/Game/PatchSignatures.inc`, `tools/patch_signatures.py`, `tools/check_patch_registry.py` | Prüfbytes jeder Patch-Stelle | `tests/test_patch_signatures.py`, `tests/test_patch_registry.py` |
| `src/Core/CrashReports.cpp`, `src/Core/ReportHosts.cpp`, `src/Game/TelemetryAdapter.cpp` | Absturzberichte und Telemetrie an den Hersteller sperren | `tests/CoreTests/CrashReportsTests.cpp`, `tests/CoreTests/ReportHostsTests.cpp`, `tests/GameTests/TelemetryAdapterTests.cpp` |
| `src/Core/SteamStats.cpp` | Erfolge und Bestenlisten im Coop sperren | `tests/CoreTests/SteamStatsTests.cpp` |
| `src/Game/SaveRedirect.cpp`, `src/Game/SavePaths.h` | Eigene Spielstände der Mod | `tests/GameTests/SavePathsTests.cpp` |
| `src/Game/TutorialAdapter.cpp` | Tutorial-Hinweise im Coop | `tests/GameTests/TutorialAdapterTests.cpp` |

## Historie

Wortgleiche Befunde, nur zum Nachschlagen, nicht als Ist-Zustand lesen:

- [Unterstützte Spielversion](../archive/GAME_ANALYSIS.md#unterstützte-spielversion)
- [Gemeinsame Build-ID (`build-id`)](../archive/GAME_ANALYSIS.md#gemeinsame-build-id-build-id)
- [SDK-Erzeugung unter UE 5.8](../archive/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58)
- [Technische Basis](../archive/GAME_ANALYSIS.md#technische-basis)
- [Systeme, die die Mod respektieren muss](../archive/GAME_ANALYSIS.md#systeme-die-die-mod-respektieren-muss)
- [Allgemeines](../archive/GAME_ANALYSIS.md#allgemeines)
- [Erfolge und Bestenlisten (`online-sperren`)](../archive/GAME_ANALYSIS.md#erfolge-und-bestenlisten-online-sperren)
- [Telemetrie und Berichte (`telemetrie-aus`)](../archive/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus)
- [Spielstände der Mod (`mod-saves`)](../archive/GAME_ANALYSIS.md#spielstände-der-mod-mod-saves)
