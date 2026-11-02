# Tools

Skripte für den Alltag. Pfade kommen aus `config/Local.props`, nie fest im Skript.

## tasks.py

Reserviert Aufgaben aus [TODO.md](../TODO.md#wo-kann-ich-anfangen), damit niemand doppelt arbeitet, siehe [Aufgaben reservieren](../CONTRIBUTING.md#aufgaben-reservieren).

| Befehl | Aufgabe |
|---|---|
| `py tools/tasks.py list` | Alle Kennungen mit Status aus Branches und Pull Requests |
| `py tools/tasks.py claim <kennung> <name>` | Worktree, Branch `task/<kennung>`, Push und Draft-PR |
| `py tools/tasks.py release <kennung>` | Draft-PR schließen, Branch löschen, sauberen Worktree entfernen |

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
