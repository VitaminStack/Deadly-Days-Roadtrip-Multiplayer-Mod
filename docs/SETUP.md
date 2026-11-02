# Entwicklungsumgebung einrichten

Die Solution der Mod existiert noch nicht (Phase 0 in [TODO.md](../TODO.md)). Gearbeitet wird bisher mit dem Spike, dafür reicht die Grundausstattung.

## Benötigt

**Für Analyse und Spike:**

| Werkzeug | Wofür |
|---|---|
| Deadly Days: Roadtrip (Steam) | Testen; jeder braucht eine eigene Kopie |
| Visual Studio 2022 oder neuer mit „Desktopentwicklung mit C++“ | Spike bauen, `dumpbin` für die Disassembly |
| Python 3 mit Launcher `py` | Testskripte, nur Standardbibliothek |
| Git | Versionsverwaltung |

**Nur bei Bedarf:**

| Werkzeug | Wofür |
|---|---|
| Dumper-7 | SDK zum Nachschlagen von Klassen, später Pflicht für `src/Game` |
| Steamworks SDK 1.64 | Header für Steam-Funktionen (kostenloser Download für Steam-Partner-Konten, nicht im Repo) |
| FModel oder repak | Paks und Configs auslesen |
| Steam-Konto je Testspieler | Online-Tests. Für Tests auf einem PC reicht ein Konto |

## Einrichtung

1. Repo klonen.
2. `config/Local.props.example` nach `config/Local.props` kopieren und `GameDir` eintragen. Die Datei bleibt lokal.
3. Spiel auf die [unterstützte Version](GAME_ANALYSIS.md#unterstützte-spielversion) bringen. Die Testskripte prüfen das.
4. Mit [docs/TESTING.md](TESTING.md) eine Testsitzung starten. Der Spike wird dabei automatisch gebaut.

## SDK

Der Spike braucht kein SDK, nur die [Offsets](OFFSETS.md). Das SDK hilft beim Nachschlagen von Klassen und wird ab der Mod selbst benötigt.

Es wird aus dem Spiel erzeugt und liegt außerhalb des Repos, zum Beispiel unter `C:\Dumper-7\<Version>\CppSDK`.
Alle im Team müssen ein SDK **desselben Spiel-Builds** verwenden.

> **Wichtig:** Dumper-7 braucht für UE 5.8 einen Fix und das SDK eine kleine Nacharbeit,
> siehe [SDK-Erzeugung unter UE 5.8](GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58).

Das Repo ist öffentlich, darum wird das SDK nicht darüber verteilt. Alle erzeugen es selbst aus der eigenen Spielkopie.

## Testen

Siehe [docs/TESTING.md](TESTING.md). Zwei Instanzen laufen auf einem PC.

- Mod-Logs liegen später neben der Mod-DLL unter `logs/`. Bei Bug-Reports bitte Logs beider Spieler anhängen.
- Der echte Spielstand liegt unter `%LOCALAPPDATA%\DDSurvivors\Saved\SaveGames`. Die Testskripte nutzen ein eigenes `-UserDir` und berühren ihn nicht.
