# Entwicklungsumgebung einrichten

Danach lässt sich die Mod bauen, testen und im Spiel starten. Das dauert wenige Minuten, ohne SDK.

## Benötigt

| Werkzeug | Wofür |
|---|---|
| Deadly Days: Roadtrip (Steam) | Testen, jeder braucht eine eigene Kopie |
| Visual Studio 2022 oder neuer mit „Desktopentwicklung mit C++“ | Mod und Spike bauen, `dumpbin` für die Disassembly |
| Python 3 mit Launcher `py` | Tests und Testskripte, nur Standardbibliothek |
| Git | Versionsverwaltung |

**Nur bei Bedarf:**

| Werkzeug | Wofür |
|---|---|
| Dumper-7 | SDK zum Nachschlagen von Klassen, Pflicht erst für `src/Game` |
| Steamworks SDK 1.64 | Header für Steam-Funktionen (kostenloser Download für Steam-Partner-Konten, nicht im Repo) |
| FModel oder repak | Paks und Configs auslesen |
| Steam-Konto je Testspieler | Online-Tests. Für Tests auf einem PC reicht ein Konto |

## Einrichtung

1. Repo klonen.
2. `config/Local.props.example` nach `config/Local.props` kopieren und `GameDir` eintragen. Die Datei bleibt lokal. Worktrees unter `.worktrees/` nutzen die aus dem Hauptordner.
3. Spiel auf die [unterstützte Version](GAME_ANALYSIS.md#unterstützte-spielversion) bringen.
4. Einrichtung prüfen. Das Skript ändert nichts und nennt, was fehlt:

   ```bash
   powershell -ExecutionPolicy Bypass -File tools/Test-Setup.ps1
   ```

5. `DeadlyDaysCoop.sln` in Visual Studio öffnen und `Release|x64` bauen, oder in der „Developer PowerShell für VS“:

   ```bash
   msbuild DeadlyDaysCoop.sln /m /p:Configuration=Release /p:Platform=x64
   ```

6. Tests ausführen, wie in [tests/README.md](../tests/README.md) beschrieben. Dieselben Tests laufen bei jedem Pull Request in GitHub Actions.
7. Im Spiel testen mit [docs/TESTING.md](TESTING.md#mod-im-spiel-testen): Die Mod kommt mit `tools/Deploy-Mod.ps1` ins Spiel, die Sitzung startet `tools/spike/Start-Session.ps1`. Danach die Mod mit `-Remove` wieder entfernen.

Dann eine Aufgabe nehmen und reservieren, siehe [Wo kann ich anfangen?](../TODO.md#wo-kann-ich-anfangen) und [Aufgaben reservieren](../CONTRIBUTING.md#aufgaben-reservieren).

## SDK

Solution, Tests und Spike brauchen kein SDK, der Spike kommt mit den [Offsets](OFFSETS.md) aus. Das SDK hilft beim Nachschlagen von Klassen und wird für `src/Game` benötigt.

Es wird aus dem Spiel erzeugt und liegt außerhalb des Repos, zum Beispiel unter `C:\Dumper-7\<Version>\CppSDK`. Den Pfad trägt man als `Dumper7SdkRoot` in `config/Local.props` ein.
Alle im Team müssen ein SDK **desselben Spiel-Builds** verwenden.

> **Wichtig:** Dumper-7 braucht für UE 5.8 einen Fix und das SDK eine kleine Nacharbeit,
> siehe [SDK-Erzeugung unter UE 5.8](GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58).
> Den getesteten Stand mit Fix baut jeder gleich mit [`tools/dumper7/Build-Dumper7.ps1`](../tools/README.md#dumper7), kein eigener Patch nötig.

Das Repo ist öffentlich, darum wird das SDK nicht darüber verteilt. Alle erzeugen es selbst aus der eigenen Spielkopie.

## Logs und Spielstand

- Jede Spielinstanz schreibt `DDSurvivors/Binaries/Win64/DDCoop/logs/ddcoop-<pid>.log`. Bei Bug-Reports bitte die Logs beider Spieler anhängen.
- Der echte Spielstand liegt unter `%LOCALAPPDATA%\DDSurvivors\Saved\SaveGames`. Die Testskripte nutzen ein eigenes `-UserDir` und berühren ihn nicht.
