# Entwicklungsumgebung einrichten

Die Solution existiert noch nicht (Phase 0 in [TODO.md](../TODO.md)). Diese Anleitung wird mit dem Projekt ergänzt.

## Benötigt

| Werkzeug | Wofür |
|---|---|
| Deadly Days: Roadtrip (Steam) | Testen; jeder braucht eine eigene Kopie |
| Visual Studio 2022 oder neuer mit „Desktopentwicklung mit C++“ | Bauen (x64, Release) |
| Git | Versionsverwaltung |
| Dumper-7 | SDK aus dem laufenden Spiel erzeugen |
| Steamworks SDK 1.64 | Header für Steam-Funktionen (kostenloser Download für Steam-Partner-Konten, nicht im Repo) |
| FModel oder repak | Paks und Configs auslesen (nur für Analyse) |
| Steam-Konto je Testspieler | Online-Tests |

## Einrichtung

1. Repo klonen.
2. `config/Local.props.example` nach `config/Local.props` kopieren und die eigenen Pfade eintragen. Die Datei bleibt lokal.
3. SDK für die [unterstützte Version](GAME_ANALYSIS.md#unterstützte-spielversion) bereitstellen (siehe unten).
4. Solution öffnen, `Release|x64` bauen.
5. Mod mit dem Deploy-Skript aus `tools/` ins Spiel kopieren.

## SDK

Das SDK wird aus dem Spiel erzeugt und liegt außerhalb des Repos, zum Beispiel unter `C:\Dumper-7\<Version>\CppSDK`.
Alle im Team müssen ein SDK **desselben Spiel-Builds** verwenden.

> **Wichtig:** Dumper-7 braucht für UE 5.8 einen Fix und das SDK eine kleine Nacharbeit,
> siehe [SDK-Erzeugung unter UE 5.8](GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58).

Das Repo ist öffentlich, darum wird das SDK nicht darüber verteilt. Alle erzeugen es selbst aus der eigenen Spielkopie.

## Testen

- Mod-Logs liegen später neben der Mod-DLL unter `logs/`. Bei Bug-Reports bitte Logs beider Spieler anhängen.
- Ob zwei Instanzen auf einem PC laufen, ist noch nicht geklärt. Bis dahin sind zwei PCs einzuplanen.
- Für Tests einen Zweit-Spielstand verwenden oder den eigenen vorher sichern. Der Speicherort wird hier ergänzt, sobald er geklärt ist.
