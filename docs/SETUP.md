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

1. Repo mit Submodulen klonen: `git clone --recurse-submodules <url>`. Wer schon ohne geklont hat: `git submodule update --init`.
2. `config/Local.props.example` nach `config/Local.props` kopieren und `GameDir` eintragen. Die Datei bleibt lokal. Worktrees unter `.worktrees/` nutzen die aus dem Hauptordner.
3. Spiel auf die [unterstützte Version](GAME_ANALYSIS.md#unterstützte-spielversion) bringen und eine [eingefrorene Entwicklungskopie](#gleiche-build-id-im-team) aktivieren.
4. Einrichtung prüfen. Das Skript ändert nichts und nennt, was fehlt:

   ```bash
   powershell -ExecutionPolicy Bypass -File tools/Test-Setup.ps1
   ```

5. `DeadlyDaysCoop.sln` in Visual Studio öffnen und `Release|x64` bauen, oder in der „Developer PowerShell für VS“:

   ```bash
   msbuild DeadlyDaysCoop.sln /m /p:Configuration=Release /p:Platform=x64
   ```

6. Tests ausführen, wie in [tests/README.md](../tests/README.md) beschrieben. Dieselben Tests laufen bei jedem Pull Request in GitHub Actions.
7. Im Spiel testen mit [docs/TESTING.md](TESTING.md#mod-im-spiel-testen): Die Mod kommt mit `tools/Deploy-Mod.ps1` ins Spiel, die Sitzung startet `tools/spike/Start-Session.ps1`. Danach die Mod mit `-Remove` wieder entfernen. Spieler installieren stattdessen das Release-Paket, siehe [Installation für Spieler](INSTALL.md).

Dann eine Aufgabe nehmen und reservieren, siehe [Wo kann ich anfangen?](../TODO.md#wo-kann-ich-anfangen) und [Aufgaben reservieren](../CONTRIBUTING.md#aufgaben-reservieren).

## Gleiche Build-ID im Team

**Lösung:** Nicht Steam anhalten, sondern eine geprüfte Entwicklungskopie außerhalb seiner Bibliotheken verwenden. Steam darf seine
Installation aktualisieren; die Mod-Entwicklung bleibt auf Build `26187885`, bis das Team die Unterstützung eines anderen Builds freigibt.
Diese Trennung ist auf einem Windows-PC mit laufendem Steam, zwei verbundenen Instanzen und geladener Mod geprüft.
Belege und Dateireferenz stehen in [Gemeinsame Build-ID](GAME_ANALYSIS.md#gemeinsame-build-id-build-id).

### Auf jedem Entwicklungs-PC einmal einrichten

Voraussetzung: eigene lizenzierte Installation des unterstützten Builds, kein laufendes Spiel, ungefähr 1,9 GB freier Platz.
`GameDir` in `config/Local.props` zeigt weiterhin auf die Steam-Installation. Im Repo-Ordner ausführen:

```powershell
powershell -ExecutionPolicy Bypass -File tools/Freeze-GameBuild.ps1 -Activate
powershell -ExecutionPolicy Bypass -File tools/Test-Setup.ps1
```

Standardziel: `%LOCALAPPDATA%/DDCoop/builds/26187885`. Mit `-Destination <Ordner>` lässt sich ein anderes Ziel außerhalb jeder
`steamapps`-Struktur wählen. Das Skript kopiert echte Dateien, keine Hardlinks, und akzeptiert keine Junctions oder Symlinks im Zielweg.
Ein vorhandenes Ziel wird nur geprüft und gegebenenfalls aktiviert, **nie überschrieben oder repariert**.
Ohne `-Activate` bleibt die Konfiguration unverändert. Das Skript verändert keine Steam-Einstellungen und lädt nichts herunter.

Beim Erstellen müssen die sechs bekannten Exe-/Container-Hashes passen. Danach werden alle kopierten Dateien per SHA-256 in
`.ddcoop-build.json` erfasst und gegen die Quelle sowie die Kopie geprüft. Ändert sich die Quelle währenddessen, wird keine Kopie veröffentlicht.
Ausgenommen sind nur die eigenen Laufzeitdaten `DDSurvivors/Saved`, `DDSurvivors/Binaries/Win64/DDCoop` und die Mod-Proxydatei `version.dll`
neben der Shipping-Exe. Diese werden nicht aus der Quelle übernommen; Logs und Mod-Deploy dürfen dort später schreiben.

### Danach entwickeln und Updates getrennt bearbeiten

`PinnedGameDir` hat Vorrang vor `GameDir`. Start, Deploy, Status, Disassembly und Crash-Schutz nutzen damit dieselbe Entwicklungskopie.
Vor Start und Deploy wird der vollständige Dateibeleg geprüft; beschädigte, fehlende und zusätzliche unveränderliche Dateien führen zum Abbruch.
Es gibt keinen automatischen Rückfall auf die inzwischen vielleicht aktualisierte Steam-Installation. Vor der Spike-Injektion wird auch der
tatsächliche Prozesspfad geprüft. Die Mod-Versionserkennung im Core bleibt zusätzlich aktiv.

Steam bleibt normal angemeldet. Getestet wird wie bisher ausschließlich mit `Start-Session.ps1`, zum Beispiel `-Build -Mod`;
`Stop-Session.ps1` entfernt die Mod wieder aus der Entwicklungskopie. Niemals die eingefrorenen Dateien per Steam-Dateiprüfung aktualisieren,
keinen Bibliotheksordner oder Link darauf einrichten und nicht manuell mit neueren Dateien vermischen. Die eigene Kopie zusätzlich privat sichern.

Für einen neuen Spiel-Build wird `spiel-update` separat reserviert. Auf dessen Branch werden Offsets, Hooks, SDK und Hash-/Build-Referenzen
angepasst und eine **neue** Entwicklungskopie getestet. Das bestehende Ziel wird nicht automatisch ersetzt. Erst nach Freigabe wechselt das Team.
Ohne `PinnedGameDir` ist der bisherige Steam-Startweg weiter verfügbar, aber nicht vor automatischen Steam-Updates isoliert.

**Grenzen:** Das ist eine Dateitrennung, kein DRM- oder Steam-Bypass und kein Schutz gegen absichtliche lokale Bearbeitung des Belegs.
Jeder braucht seine eigene berechtigte Installation. Wer den alten Build noch nicht besitzt, braucht einen gesondert geprüften Beschaffungsweg;
der bisher ungetestete Depot-Download wird nicht vorausgesetzt. Ein zukünftiger Steam-/Backendwechsel kann alte Versionen am Starten hindern und
braucht dann eigene Untersuchung; er ersetzt aber nicht still die eingefrorenen Dateien. Zwei PCs mit verschiedenen Konten wurden hier nicht getestet.
Für die Team-Rückmeldung reichen Build-ID, Anzahl geprüfter Dateien und Starttest; keine Spieldateien, persönlichen Pfade oder Steam-IDs posten.

## SDK

Solution, Tests und Spike brauchen kein SDK, Mod und Spike kommen mit den [Offsets](OFFSETS.md) aus. Das SDK hilft beim Nachschlagen von Klassen. Ist es eingetragen, prüft der Build von `src/Game` die Werte aus `Offsets.h` gegen das SDK, das Log meldet dann „Offsets beim Bauen gegen das SDK geprüft“. Die CI baut ohne.

Es wird aus dem Spiel erzeugt und liegt außerhalb des Repos, zum Beispiel unter `C:\Dumper-7\<Version>\CppSDK`. Den Pfad trägt man als `Dumper7SdkRoot` in `config/Local.props` ein.
Alle im Team müssen ein SDK **desselben Spiel-Builds** verwenden.

> **Wichtig:** Dumper-7 braucht für UE 5.8 einen Fix und das SDK eine kleine Nacharbeit,
> siehe [SDK-Erzeugung unter UE 5.8](GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58).
> Der getestete Stand mit Fix liegt als Submodul unter `third_party/Dumper-7`. Gebaut wird er mit [`tools/dumper7/Build-Dumper7.ps1`](../tools/README.md#dumper7), kein eigener Patch nötig.

Das Repo ist öffentlich, darum wird das SDK nicht darüber verteilt. Alle erzeugen es selbst aus der eigenen Spielkopie.

## Logs und Spielstand

- Jede Spielinstanz schreibt `DDSurvivors/Binaries/Win64/DDCoop/logs/ddcoop-<pid>.log`. Bei Bug-Reports bitte die Logs beider Spieler anhängen.
- Der echte Spielstand liegt unter `%LOCALAPPDATA%\DDSurvivors\Saved\SaveGames`. Die Testskripte nutzen ein eigenes `-UserDir` und berühren ihn nicht.
- Mit geladener Mod speichert das Spiel nur in `SaveGames_DDCoop` daneben, auch allein gespielt. Beim ersten Start kopiert die Mod die normalen `.sav`-Dateien einmal dorthin, die Originale bleiben unverändert. Ohne Mod gilt wieder der normale Spielstand. Der Mod-Ordner liegt außerhalb von `SaveGames` und wird darum nicht über Steam Cloud abgeglichen.
