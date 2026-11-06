# Tools

Skripte für den Alltag. Pfade kommen aus `config/Local.props`, nie fest im Skript.

## tasks.py

Reserviert Aufgaben aus [TODO.md](../TODO.md#wo-kann-ich-anfangen), damit niemand doppelt arbeitet, siehe [Aufgaben reservieren](../CONTRIBUTING.md#aufgaben-reservieren).

| Befehl | Aufgabe |
|---|---|
| `py tools/tasks.py list` | Offene Aufgaben aus `TODO.md` auf `origin/main` mit Status: frei, blockiert, Team, reserviert, Review |
| `py tools/tasks.py list --active` | Nur aktive Entwicklungsreservierungen/PRs, ohne ausstehende Bestätigungstests |
| `py tools/tasks.py claim <kennung> <name>` | Worktree, Branch `task/<kennung>`, Push und Draft-PR |
| `py tools/tasks.py claim <kennung> <name> host\|client` | Bei Aufgaben „zu zweit“ nur eine Rolle: Branch `task/<kennung>/<rolle>`, Worktree `.worktrees/<kennung>-<rolle>`, siehe [Test auf zwei PCs](../docs/TESTING.md#test-auf-zwei-pcs) |
| `py tools/tasks.py release <kennung> [<rolle>]` | Draft-PR schließen, Branch löschen, sauberen Worktree entfernen |
| `py tools/tasks.py show <kennung>` | Nur diese Aufgabe: Phase, Status, Braucht, Bearbeitung, Abnahme. Spart das Lesen der ganzen `TODO.md` |
| `py tools/tasks.py done <kennung> "<Ergebnis>"` | Auf `task/<kennung>`: Zeile alphabetisch nach „Erledigt“ verschieben, `task_status.py update` und `check` |

Erledigte, blockierte und Team-Aufgaben sind nicht reservierbar, auch ohne `gh`. Offene Aufgaben werden nicht allein wegen eines gemergten Teil-PRs abgeschlossen. Kurze Wartung ohne Eintrag in `TODO.md` ist weiterhin reservierbar.

### Bestätigungstests

**Bestätigungstest ausstehend** ist ein eigener Status: nicht „in Arbeit“, nicht frei zur
erneuten Entwicklungsreservierung und nicht erledigt. Die offene Aufgabe bleibt in ihrer
Phase, mit ursprünglicher Abnahme und Abhängigkeiten; abhängige Aufgaben und Fortschrittsbalken
warten weiter auf das tatsächliche Bestehen. In `TODO.md` steht der Status ausdrücklich im
Aufgabentext, nicht nur in einer PR-Beschreibung oder einem Branch.

```bash
py tools/tasks.py tests pending dodge-client-live --note "Diagnose integriert, zehn Dodges auf zwei PCs noch offen"
py tools/tasks.py tests
py tools/tasks.py tests plan
py tools/tasks.py tests plan dodge-client-live karte-tunnel
py tools/tasks.py tests update dodge-client-live --note "Termin verschoben; bisher keine neue Abnahme"
py tools/tasks.py tests update --all --note "Gemeinsamer Termin geplant; noch nicht bestanden"
```

`list` (Standard) und `plan` lesen `origin/main`. Mit `--local` sieht man vor dem Merge die
Änderungen im eigenen Worktree. `plan` bündelt verfügbare Tests mit vollständigen Anforderungen;
es startet kein Spiel und behauptet kein Ergebnis. Die Tests nacheinander in derselben
geschützten Sitzung spielen, die Einzelbefunde mit Build/Mod-Stand im Testbericht festhalten.
Keine zusätzliche Reservierung der getesteten Aufgaben: Änderungen an Testständen erfolgen
im eigenen reservierten Wartungs-Worktree und mit PR, nicht auf `main` oder in fremden Worktrees.

Nach **wirklich bestandener vollständiger Abnahme** können mehrere Tests mit einem gemeinsamen
Bericht abgeschlossen werden. Der Bericht muss für jede Kennung das Ergebnis belegen:

```bash
py tools/tasks.py tests pass dodge-client-live karte-tunnel --note "Beide Abnahmen bestanden, siehe [Bericht](docs/TESTING.md)"
```

Das Beispiel zeigt die Syntax, keinen tatsächlich bestandenen Test. `pass` verschiebt die
gewählten Aufgaben alphabetisch nach „Erledigt“ und erneuert den Fortschritt. Ein Teiltest,
eine Diagnose oder ein verschobener Termin bekommt nur `update`, niemals `pass`. Scheitert
eine Abnahme und braucht Code-Arbeit, setzt `tests reopen <kennung> --note "Befund"` die Aufgabe
wieder auf Entwicklung; ein alter gemergter PR bleibt dann bewusst „erst nachfragen“, bis die
weitere Arbeit mit dem Menschen geklärt ist. Es werden keine Branches oder PRs gelöscht/geöffnet.

**Schutz fremder Arbeit:** Offene PRs, Rollen-Reservierungen und unbestätigte Branches werden
bei `plan` und `--all` ausgelassen und ausdrücklich genannt. Bei expliziten Kennungen bricht
eine Kollision den ganzen Änderungsbatch unverändert ab. Die eigene Aufgabe darf man selbst
auf „Bestätigungstest ausstehend“ setzen. Ist der GitHub-PR-Status unbekannt, werden keine
Teststände geändert. Falsche/erledigte/doppelte Kennungen und leere Ergebnisse lehnt der Batch
vor dem Schreiben ab. Alle Änderungen bleiben lokal bis Commit und Merge.

## task_status.py

Lokale, netzwerkfreie Prüfung im aktuellen Checkout. `TODO.md` ist die einzige Aufgabenquelle, „Wo kann ich anfangen?“ und der Fortschritt im README sind daraus erzeugt.

| Befehl | Aufgabe |
|---|---|
| `py tools/task_status.py check` | Tabellen, Kennungen, „Braucht“ und erzeugte Abschnitte prüfen, bei Abweichung Fehlercode |
| `py tools/task_status.py update` | „Wo kann ich anfangen?“ in `TODO.md` sowie Zahlen und Balken im README neu erzeugen |

Geprüft wird: eindeutige Kennungen, bekannte Kennungen und keine Kreise unter „Braucht“, gültige Werte für Prio, Bearbeitung und Spielzeit, alphabetische Tabelle „Erledigt“, keine alten Checkboxen. Die CI und `Build-Mod.ps1 -Test` führen `check` aus. Der Fortschritt zählt je Phase die erledigten gegen alle Aufgaben, ohne Wartung und optionale Optimierungen.

## dumper7/

Baut Dumper-7 im getesteten Stand für UE 5.8, damit alle dasselbe SDK erzeugen. Hintergrund in [SDK-Erzeugung unter UE 5.8](../docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58).

```bash
powershell -ExecutionPolicy Bypass -File tools/dumper7/Build-Dumper7.ps1 -OutputDir C:/Dumper-7
```

- Baut aus dem Submodul `third_party/Dumper-7` (eigener Fork, upstream `2c1eae3` plus Fix) und lädt es bei Bedarf nach. Gebaut wird eine Kopie in `research/local/dumper7/`, das Submodul bleibt unverändert.
- `-OutputDir` ist der Ordner für das SDK, Standard `C:/Dumper-7`. Dumper-7 entlädt sich nach dem Dump selbst.
- Ergebnis ist `research/local/dumper7/out/Dumper-7.dll`. In den Host einer laufenden Mission laden: `py tools/spike/inject.py <host-pid> <dll>`.

## Test-Setup.ps1

Prüft die Einrichtung aus [docs/SETUP.md](../docs/SETUP.md): Git, Python, Visual Studio mit C++, `config/Local.props`, Spielordner und Build. Ändert nichts.

## Freeze-GameBuild.ps1

Isoliert die Mod-Entwicklung von Steam-Updates, siehe [gleiche Build-ID im Team](../docs/SETUP.md#gleiche-build-id-im-team).

```powershell
powershell -ExecutionPolicy Bypass -File tools/Freeze-GameBuild.ps1 -Activate
```

- Quelle bleibt `GameDir` aus `config/Local.props`, Standardziel ist `%LOCALAPPDATA%/DDCoop/builds/26187885`.
- `-Destination <Ordner>` wählt ein anderes Ziel außerhalb aller `steamapps`-Ordner. Keine Hardlinks, Symlinks oder Junctions.
- `-Activate` setzt `PinnedGameDir` in der gefundenen lokalen Konfiguration; in Worktrees ohne eigene `Local.props` ist das die gemeinsame Datei.
  Alle Skripte dieses Standes bevorzugen dann die Kopie. Bereits ältere Branches müssen die neue Startlogik erst übernehmen; nur der Konfigurationseintrag reicht dort nicht.
- Vorhandene Ziele werden geprüft und optional aktiviert, niemals überschrieben. Ohne `-Activate` wird nichts an der Konfiguration geändert.
- `game_build.py` kopiert und prüft den vollständigen Dateibeleg. Steam bleibt online und darf seine eigene Installation aktualisieren.
  Kein Download, keine Änderung an Steam-Einstellungen oder installierten Spieldateien.

## Build-Mod.ps1

Baut `DeadlyDaysCoop.sln` wie die CI nach `bin/<Konfiguration>/`. Mit `-Test` laufen danach `CoreTests.exe` und die Python-Tests.

```bash
powershell -ExecutionPolicy Bypass -File tools/Build-Mod.ps1 -Test
```

## Deploy-Mod.ps1

Kopiert `version.dll` und `DDCoop/DDCoop.dll` aus `bin/<Konfiguration>/` neben die Spiel-Exe. Vorher die Solution bauen.
Prüfen und Kopieren übernimmt derselbe [Installer](#installer) wie im Release-Paket, den Spielordner und die Build-Prüfung `config/Local.props` und `spike/Common.ps1`.

```bash
powershell -ExecutionPolicy Bypass -File tools/Deploy-Mod.ps1
powershell -ExecutionPolicy Bypass -File tools/Deploy-Mod.ps1 -Remove
```

- `-Configuration Debug` nimmt den Debug-Build, Standard ist `Release`.
- Bricht ab, wenn das Spiel läuft, der Build nicht passt oder eine fremde `version.dll` dort liegt. Diese bleibt unberührt.
- `-Remove` entfernt nur die beiden DLLs. Logs und `ddcoop.ini` bleiben in `DDCoop/`.

> **Nach dem Testen entfernen.** Solange die Mod im Spielordner liegt, lädt auch jeder normale Start über Steam sie, ohne Crash-Schutz.
> Getestet wird deshalb nur über `spike/Start-Session.ps1`, siehe [Testen](../docs/TESTING.md#mod-im-spiel-testen).
> Am einfachsten mit `Start-Session.ps1 -Mod`: Das baut und installiert die Mod, `Stop-Session.ps1` entfernt sie wieder.

## installer/

Installer für Spieler, liegt unverändert im Release-Paket. Anleitung für Spieler in [docs/INSTALL.md](../docs/INSTALL.md).

| Datei | Aufgabe |
|---|---|
| `Install-DDCoop.ps1` | Sucht das Spiel in den Steam-Bibliotheken (`-GameDir` überschreibt), prüft Build über das Steam-Manifest, laufendes Spiel und fremde `version.dll`, kopiert Mod und Loader. `-Uninstall` entfernt beide DLLs, `-Purge` dazu den Ordner `DDCoop` |
| `Install.cmd`, `Uninstall.cmd` | Doppelklick-Starter, `-ExecutionPolicy Bypass` nur für diesen Aufruf |
| `LIESMICH.txt` | Kurzanleitung im Paket |

`-Source`, `-SteamPath`, `-SkipBuildCheck` und `-GameProcess` sind für `Deploy-Mod.ps1` und die Tests in `tests/test_installer.py`.

## New-ReleasePackage.ps1

Baut `Release` und packt das Paket für Spieler nach `dist/DDCoop-<Version>.zip`: Loader, Mod, Installer, `LIESMICH.txt`, Lizenzen
(eigene und MinHook) und `SHA256SUMS.txt`. Keine PDBs, keine Spieldateien.

```bash
powershell -ExecutionPolicy Bypass -File tools/New-ReleasePackage.ps1 -Version <version>
```

- Ohne `-Version` heißt das Paket `dev-<commit>`, die Versionsvergabe gehört zur Aufgabe `release`.
- `-Bin <Ordner>` nimmt fertige DLLs statt neu zu bauen, `-OutputDir` wählt ein anderes Ziel als `dist/`.

## spike/

Bauen und Testen des Spikes, die Anleitung steht in [docs/TESTING.md](../docs/TESTING.md). Laufzeitdateien landen in
`research/local/session/` und werden nicht committet. Die Python-Skripte brauchen nur die Standardbibliothek.

| Skript | Aufgabe |
|---|---|
| `Build-Spike.ps1` | Baut `spike.dll` |
| `Start-Session.ps1` | Setzt die Sperre, startet Crash-Schutz, Host und Client parallel, verbindet sie und reist in eine Karte. Wartet auf Zustände statt fest zu schlafen und nennt die Zeit je Schritt (MetaBase etwa 30 bis 40 s). `-Wait <s>` wartet auf eine fremde Sitzung statt abzubrechen. `-Build` baut den Spike vorher neu, `-Mod` baut und installiert zusätzlich die Mod. `-Role Host` oder `-Role Client -Peer <SteamID>` startet nur eine Seite für den Test auf zwei PCs. `-Mod -Tunnel Mod` verbindet über den Transport der Mod statt über den Spike, `-Mod -Tunnel Lobby` über die Steam-Lobby der Mod, erstellt und betreten über das Overlay. `-Brief`: nur Zeit je Schritt und eine Zeile je Instanz. `-NetProfile Internet` simuliert Latenz im Tunnel der Mod, siehe [Latenz simulieren](../docs/TESTING.md#latenz-simulieren). `-Mod -Tunnel Lobby -NoSpike` startet nur mit der Mod wie im Release, siehe [Nur Mod, ohne Spike](../docs/TESTING.md#nur-mod-ohne-spike) |
| `Restart-Instance.ps1` | Startet nur den Client neu und verbindet ihn wieder, der Host läuft weiter (etwa 20 s statt eines vollen Neustarts). `-Brief` für kurze Ausgabe |
| `Overlay.ps1` | Bedient das Overlay der Mod ohne Maus: `status`, `create`, `join [<id>]`, `leave`, `invite`. `-Json` für Skripte |
| `Watch-Net.ps1` | Zeigt Änderungen an Overlay, Prozess und Mod-Log (Sitzung, Transport, Lobby, Welt) beider Instanzen mit Zeit, `-Until <Muster>` endet beim ersten Treffer |
| `Test-NetEnd.ps1` | Prüft Sitzungsende ohne Handarbeit: Host und Client verlassen, Client und Host hart beendet, Neubeitritt. Zeit bis zur Meldung, Bericht unter `research/local/` |
| `Test-Desync.ps1` | Eigene laufende Tag-1-Sitzung: 600 s fünf aktive Diagnosefelder gleich, dann kontrollierte Client-Inventarabweichung durch `desync_difference.py` mit Rücksetzung; beide Overlaymodelle müssen Alarm und Erholung zeigen. `-Seconds` nur für kurze Vorproben. [Ablauf und Grenzen](../docs/DESYNC.md) |
| `Guided-Test.ps1` | Läuft beim Agenten im Hintergrund, der Mensch spielt nur. Wartet bei Spielschritten auf `Guided-Answer.ps1` und hält Dauer, Mod-Logs, Overlay, Abstürze und mit `-Shots` Bilder fest. Schrittdateien in `spike/guided/`, siehe [Testen](../docs/TESTING.md#spielzeit-sparen) |
| `Guided-Answer.ps1` | Beantwortet den wartenden Spielschritt nach der Rückmeldung des Menschen im Chat: `ok`, `"<Abweichung>"`, `skip`, `quit`, `status` |
| `Test-TwoPc.ps1` | Test auf zwei PCs über Steam-P2P, je PC eine Rolle: verbinden, Run bis in die Mission an Tag 1, messen, Tunnel-Bericht |
| `tunnel_report.py` | Fasst die Tunnel-Zeilen eines Spike-Logs zusammen: Ping, Pakete, Qualität, Route, Abbrüche; SteamIDs und IPs maskiert |
| `Send-Command.ps1` | Schickt einen Befehl an `Host` oder `Client` und zeigt das Log |
| `Stop-Session.ps1` | Beendet die Instanzen dieses Ordners, gibt die Sperre frei und entfernt eine mit `-Mod` installierte Mod |
| `Get-Status.ps1` | Prozesse, Netzzustand, letzte Log- und Mod-Logzeilen, Abstürze seit Sitzungsstart. `-Inventory` vergleicht die Inventare. `-Brief`: je eine Zeile für Instanzen, Inventar und Abstürze |
| `Wait-Log.ps1` | Wartet auf eine neue passende Zeile im Spike-Log statt einer festen Pause |
| `Game-Input.ps1` | Bild aus einem Spielfenster, Farbe eines Punkts (`pixel`), Klick oder Taste, ohne Bildschirmaufnahme des Desktops |
| `Start-Run.ps1` | Startet aus der `MetaBase` einen Run bis zur Routenkarte: Position lesen, in einem Zug in den Ring, Karte am Banner erkennen, bis zu drei Versuche. Ein frischer Spielstand, der ohne Routenkarte direkt in die Mission fährt, zählt auch. `-Brief`: eine Zeile |
| `Travel-Day.ps1` | Fährt einen Tag über die Routenkarte, wartet auf die neue Karte, prüft beide Instanzen und beendet ihn. `-Brief`: eine Zeile je Instanz und Inventar |
| `Run-Route.ps1` | Fester Routenlauf mit Seed `20795` über mehrere Tage, nach jedem Wechsel `check_round.py`, Bilder während und nach dem Laden unter `research/local/session/runs/`. Mit `-Start -NetProfile Internet` in einer Lobby-Sitzung mit simulierter Latenz, mit `-Start -NoSpike -Days 1 -StayLast` nur mit der Mod |
| `check_round.py` | Prüft nach einem Kartenwechsel World, `PlayerArray`, PlayerStates, Figuren am Bus, gleiche Positionen bei Host und Client, NetGUIDs und `NewActor failed` |
| `Focus-Window.ps1` | Holt ein Spielfenster nach vorn |
| `Disassemble.ps1` | Zeigt einen RVA-Bereich der Exe |
| `CrashGuard.ps1` | Crash-Schutz, wird von `Start-Session.ps1` gestartet |
| `netstate.py` | Welt, GameMode, GameState, Rolle, Spielerzahl |
| `objects.py` | Objektzählung, Rollen und Positionen der Figuren |
| `enemy_sync.py` | Spawn und Tod der Gegner beider Seiten: Tode je Seite, Zuordnung über die Position, Verzögerung beim Client, `py tools/spike/enemy_sync.py <s> <host> <client>` |
| `players.py` | `SpawnedPlayers` und `PlayerArray` beim Host |
| `inventory.py` | Vergleicht die Inventare von Host und Client |
| `fuel.py` | Liest den Tank, `--fill` füllt ihn beim Host fast voll, damit ein Kanister das echte Rundenende auslöst |
| `guidcache.py` | NetGUID-Cache einer Instanz mit Pfad, Zustand und Flags, filterbar nach Pfad |
| `pause_state.py` | Pausen, Zeitrequests und UI-Sitzungen je Instanz |
| `blueprint_refs.py` | Referenzen geladener Blueprints, kein Decompiler |
| `crashes.py` | Absturzberichte der Testinstanzen mit RVAs, filterbar mit `--latest`, `--role` und `--session` |
| `gamethread.py` | Wo der Game Thread bei einem Hänger steht |
| `callers.py` | Direkte Aufrufer einer Funktion in der Exe |
| `inject.py` | Lädt eine DLL in einen Prozess |
| `ddmem.py` | Gemeinsamer Speicherzugriff der Python-Skripte |
