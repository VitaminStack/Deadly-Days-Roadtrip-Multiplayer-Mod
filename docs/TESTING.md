# Testen mit zwei Instanzen

Host und Client laufen auf einem PC unter einem Steam-Konto, ein zweiter PC ist für die Analyse nicht nötig. Beide Instanzen
bekommen ein eigenes `-UserDir`, der echte Spielstand bleibt also unberührt. Die Skripte stehen in [tools/](../tools/README.md).

> **Crash-Reporter:** Abstürze sind hier normal, und der `CrashReportClient` kann Berichte mit Spielstand an Pixelsplit schicken.
> `Start-Session.ps1` startet deshalb immer den Crash-Schutz und bricht ab, wenn er nicht läuft. Die Exe also nicht von Hand mit
> Spike starten, und `-unattended` niemals verwenden. Hintergrund im [Spike-Ergebnis](../research/spikes/a-connect/README.md#crash-reporter).

## Sitzung starten

Voraussetzung ist die Einrichtung aus [SETUP.md](SETUP.md). Die Skripte prüfen die Build-ID und brechen bei einem anderen Build ab.
`Start-Session.ps1` beendet alle laufenden Instanzen des Spiels, also auch ein normal gestartetes.

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1
```

Das dauert gut anderthalb Minuten und endet in der `MetaBase`, mit beiden Spielern in derselben Welt. Die letzte Ausgabe zeigt den Zustand beider
Instanzen, darunter `role`, `clients` und `players`. Mit `-Map /Game/Maps/DDDynamic_Map` geht es direkt in eine Mission, dort starten
Gegner aber nicht von selbst (`!wave` hilft).

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Send-Command.ps1 Host '!finish'
```

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Stop-Session.ps1
```

Nach Änderungen an `spike.cpp` zuerst `Stop-Session.ps1`, dann `Build-Spike.ps1`. Die Befehle stehen in der
[Spike-Tabelle](../research/spikes/a-connect/README.md#patches). Alles andere läuft als Konsolenbefehl, etwa `open` oder `servertravel`.

## Mission über das Spielmenü

Die Fenster sind 800 × 450 groß, der Host links, der Client rechts. Eingaben gehen an das Fenster im Vordergrund, darum vorher
`Focus-Window.ps1 Host`. Die Spike-Umleitung ist nach `Start-Session.ps1` scharf, sonst reist nur der Host.

1. Host-Fenster in den Vordergrund holen und rund 1,6 Sekunden `A` halten, bis die Figur im Ring steht.
2. Mission für Tag 1 in der Liste links anklicken, etwa bei (36, 124).
3. Fortsetzen bei (232, 241), Bestätigen bei (360, 240).
4. Einen Routenknoten anklicken. Die Lage ändert sich je Run.
5. Beide Instanzen landen in `DDDynamic_Map`, Gegner erscheinen von selbst.

Das Pausemenü hat „Zurück zum Hauptmenü“ bei (226, 152). Mit `!finish` endet die Mission beim Host, danach kommt die Routenkarte für Tag 2.
Dort stürzt der Client derzeit ab, das ist die offene Aufgabe zum [Inventar](../TODO.md#phase-1--analyse-und-spikes).

## Abstürze auswerten

```bash
py tools/spike/crashes.py
```

Das zeigt Fehlertext und Stack als RVAs. Manche Abstürze legen keinen Bericht an, dann steht die Adresse in der Windows-Ereignisanzeige unter
Windows-Protokolle → Anwendung, Ereignis 1000. Eine Adresse sieht man sich so an:

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Disassemble.ps1 0x59635E0 0x5963640
```

Nach dem Beenden gibt Windows `spike.dll` erst nach ein paar Sekunden frei, `Stop-Session.ps1` wartet deshalb 6 Sekunden.

## Ergebnisse festhalten

Damit niemand dieselbe Arbeit wiederholt, gehört jedes Ergebnis ins Repo, auch ein gescheiterter Versuch.

| Was | Wohin |
|---|---|
| Wissen über das Spiel | [GAME_ANALYSIS.md](GAME_ANALYSIS.md) |
| Neue Offsets und RVAs | [OFFSETS.md](OFFSETS.md) |
| Patch, Testablauf, gescheiterte Ansätze | README des Spikes |
| Erledigte oder neue Aufgaben | [TODO.md](../TODO.md) |
| Rohdaten | `research/local/`, wird nicht committet |
