# Tools

Skripte für den Alltag. Pfade kommen aus `config/Local.props`, nie fest im Skript.

## Test-Setup.ps1

Prüft die Einrichtung aus [docs/SETUP.md](../docs/SETUP.md): Git, Python, Visual Studio mit C++, `config/Local.props`, Spielordner und Build. Ändert nichts.

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

## spike/

Bauen und Testen des Spikes, die Anleitung steht in [docs/TESTING.md](../docs/TESTING.md). Laufzeitdateien landen in
`research/local/session/` und werden nicht committet. Die Python-Skripte brauchen nur die Standardbibliothek.

| Skript | Aufgabe |
|---|---|
| `Build-Spike.ps1` | Baut `spike.dll` |
| `Start-Session.ps1` | Setzt die Sperre, startet Crash-Schutz, Host und Client, verbindet sie und reist in eine Karte |
| `Send-Command.ps1` | Schickt einen Befehl an `Host` oder `Client` und zeigt das Log |
| `Stop-Session.ps1` | Beendet die Instanzen dieses Ordners und gibt die Sperre frei |
| `Focus-Window.ps1` | Holt ein Spielfenster nach vorn |
| `Disassemble.ps1` | Zeigt einen RVA-Bereich der Exe |
| `CrashGuard.ps1` | Crash-Schutz, wird von `Start-Session.ps1` gestartet |
| `netstate.py` | Welt, GameMode, GameState, Rolle, Spielerzahl |
| `objects.py` | Objektzählung, Rollen und Positionen der Figuren |
| `players.py` | `SpawnedPlayers` und `PlayerArray` beim Host |
| `inventory.py` | Vergleicht die Inventare von Host und Client |
| `pause_state.py` | Pausen, Zeitrequests und UI-Sitzungen je Instanz |
| `blueprint_refs.py` | Referenzen geladener Blueprints, kein Decompiler |
| `crashes.py` | Absturzberichte der Testinstanzen mit RVAs |
| `gamethread.py` | Wo der Game Thread bei einem Hänger steht |
| `callers.py` | Direkte Aufrufer einer Funktion in der Exe |
| `inject.py` | Lädt eine DLL in einen Prozess |
| `ddmem.py` | Gemeinsamer Speicherzugriff der Python-Skripte |
