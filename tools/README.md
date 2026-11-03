# Tools

Skripte für den Alltag. Pfade kommen aus `config/Local.props`, nie fest im Skript.

## tasks.py

Reserviert Aufgaben aus [TODO.md](../TODO.md#wo-kann-ich-anfangen), damit niemand doppelt arbeitet, siehe [Aufgaben reservieren](../CONTRIBUTING.md#aufgaben-reservieren).

| Befehl | Aufgabe |
|---|---|
| `py tools/tasks.py list` | Alle Kennungen mit Status aus Branches und Pull Requests |
| `py tools/tasks.py claim <kennung> <name>` | Worktree, Branch `task/<kennung>`, Push und Draft-PR |
| `py tools/tasks.py release <kennung>` | Draft-PR schließen, Branch löschen, sauberen Worktree entfernen |

## dumper7/

Baut Dumper-7 im getesteten Stand für UE 5.8, damit alle dasselbe SDK erzeugen. Hintergrund in [SDK-Erzeugung unter UE 5.8](../docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58).

```bash
powershell -ExecutionPolicy Bypass -File tools/dumper7/Build-Dumper7.ps1 -OutputDir C:/Dumper-7
```

- Baut aus dem Submodul `third_party/Dumper-7` (upstream `2c1eae3` plus Fix) und lädt es bei Bedarf nach. Gebaut wird eine Kopie in `research/local/dumper7/`, das Submodul bleibt unverändert.
- `-OutputDir` ist der Ordner für das SDK, Standard `C:/Dumper-7`. Dumper-7 entlädt sich nach dem Dump selbst.
- Ergebnis ist `research/local/dumper7/out/Dumper-7.dll`. In den Host einer laufenden Mission laden: `py tools/spike/inject.py <host-pid> <dll>`.

## Test-Setup.ps1

Prüft die Einrichtung aus [docs/SETUP.md](../docs/SETUP.md): Git, Python, Visual Studio mit C++, `config/Local.props`, Spielordner und Build. Ändert nichts.

## Build-Mod.ps1

Baut `DeadlyDaysCoop.sln` wie die CI nach `bin/<Konfiguration>/`. Mit `-Test` laufen danach `CoreTests.exe` und die Python-Tests.

```bash
powershell -ExecutionPolicy Bypass -File tools/Build-Mod.ps1 -Test
```

## Deploy-Mod.ps1

Kopiert `version.dll` und `DDCoop/DDCoop.dll` aus `bin/<Konfiguration>/` neben die Spiel-Exe. Vorher die Solution bauen.

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

## spike/

Bauen und Testen des Spikes, die Anleitung steht in [docs/TESTING.md](../docs/TESTING.md). Laufzeitdateien landen in
`research/local/session/` und werden nicht committet. Die Python-Skripte brauchen nur die Standardbibliothek.

| Skript | Aufgabe |
|---|---|
| `Build-Spike.ps1` | Baut `spike.dll` |
| `Start-Session.ps1` | Setzt die Sperre, startet Crash-Schutz, Host und Client, verbindet sie und reist in eine Karte. `-Build` baut den Spike vorher neu, `-Mod` baut und installiert zusätzlich die Mod |
| `Send-Command.ps1` | Schickt einen Befehl an `Host` oder `Client` und zeigt das Log |
| `Stop-Session.ps1` | Beendet die Instanzen dieses Ordners, gibt die Sperre frei und entfernt eine mit `-Mod` installierte Mod |
| `Get-Status.ps1` | Prozesse, Netzzustand, letzte Log- und Mod-Logzeilen, Abstürze seit Sitzungsstart. `-Inventory` vergleicht die Inventare |
| `Wait-Log.ps1` | Wartet auf eine neue passende Zeile im Spike-Log statt einer festen Pause |
| `Game-Input.ps1` | Bild aus einem Spielfenster, Klick oder Taste, ohne Bildschirmaufnahme des Desktops |
| `Start-Run.ps1` | Startet aus der `MetaBase` einen Run bis zur Routenkarte |
| `Travel-Day.ps1` | Fährt einen Tag über die Routenkarte, prüft beide Instanzen und beendet ihn |
| `Focus-Window.ps1` | Holt ein Spielfenster nach vorn |
| `Disassemble.ps1` | Zeigt einen RVA-Bereich der Exe |
| `CrashGuard.ps1` | Crash-Schutz, wird von `Start-Session.ps1` gestartet |
| `netstate.py` | Welt, GameMode, GameState, Rolle, Spielerzahl |
| `objects.py` | Objektzählung, Rollen und Positionen der Figuren |
| `players.py` | `SpawnedPlayers` und `PlayerArray` beim Host |
| `inventory.py` | Vergleicht die Inventare von Host und Client |
| `pause_state.py` | Pausen, Zeitrequests und UI-Sitzungen je Instanz |
| `blueprint_refs.py` | Referenzen geladener Blueprints, kein Decompiler |
| `crashes.py` | Absturzberichte der Testinstanzen mit RVAs, filterbar mit `--latest`, `--role` und `--session` |
| `gamethread.py` | Wo der Game Thread bei einem Hänger steht |
| `callers.py` | Direkte Aufrufer einer Funktion in der Exe |
| `inject.py` | Lädt eine DLL in einen Prozess |
| `ddmem.py` | Gemeinsamer Speicherzugriff der Python-Skripte |
