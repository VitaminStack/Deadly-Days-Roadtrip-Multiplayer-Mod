# Testen mit zwei Instanzen

Host und Client laufen auf einem PC unter einem Steam-Konto, ein zweiter PC ist für die Analyse nicht nötig. Beide Instanzen
bekommen ein eigenes `-UserDir`, der echte Spielstand bleibt also unberührt. Die Skripte stehen in [tools/](../tools/README.md).

> **Crash-Reporter:** Abstürze sind hier normal, und der `CrashReportClient` kann Berichte mit Spielstand an Pixelsplit schicken.
> `Start-Session.ps1` startet deshalb immer den Crash-Schutz und bricht ab, wenn er nicht läuft. Die Exe also nicht von Hand mit
> Spike starten, und `-unattended` niemals verwenden. Hintergrund im [Spike-Ergebnis](../research/spikes/a-connect/README.md#crash-reporter).
> Mod und Spike sperren den Reporter zusätzlich selbst, `CrashGuard` bleibt trotzdem die erste Schicht, weil beide erst nach dem Spielstart greifen.

## Mod im Spiel testen

Die Mod unter `src/` kommt mit [tools/Deploy-Mod.ps1](../tools/README.md#deploy-modps1) ins Spiel und startet dann in beiden Instanzen einer Sitzung.
Jede Instanz schreibt `DDSurvivors/Binaries/Win64/DDCoop/logs/ddcoop-<pid>.log`. Am einfachsten geht das in einem Schritt:

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Build -Mod
```

Das baut Spike und Mod, installiert die Mod und startet die Sitzung. `Get-Status.ps1` zeigt die Mod-Logs beider Instanzen.
`Stop-Session.ps1` entfernt die Mod danach wieder, sonst lädt auch ein normaler Spielstart sie ohne Crash-Schutz. Ohne `-Mod` gilt:
nach `Stop-Session.ps1` selbst `Deploy-Mod.ps1 -Remove` ausführen.

## Sitzung starten

Voraussetzung ist die Einrichtung aus [SETUP.md](SETUP.md). Die Skripte prüfen die Build-ID und brechen bei einem anderen Build ab.

**Pro PC läuft nur eine Sitzung**, auch über mehrere Klone und Worktrees hinweg. Sie belegt Port 7777, die Fenster, Maus und Tastatur.
`Start-Session.ps1` bricht deshalb ab, wenn schon eine Sitzung läuft, und nennt den Ordner, aus dem sie stammt. Auch ein normal gestartetes Spiel muss vorher beendet sein.
Die Skripte beenden nur die eigenen Instanzen. Mit `-Restart` ersetzt man die Sitzung aus dem eigenen Ordner. Nach einem Absturz gibt sich die Sperre von selbst frei.
In einem Git-Worktree nutzen die Skripte `config/Local.props` aus dem Hauptordner.

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

Nach Änderungen am Spike startet `Start-Session.ps1 -Build` neu: Es beendet die eigene Sitzung, baut `spike.dll` und startet wieder. Die Befehle stehen in der
[Spike-Tabelle](../research/spikes/a-connect/README.md#patches). Alles andere läuft als Konsolenbefehl, etwa `open` oder `servertravel`.

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Get-Status.ps1 -Inventory
```

Das zeigt beide Prozesse, den Netzzustand, die letzten Logzeilen und Abstürze seit dem Start. Statt fester Pausen wartet
`Wait-Log.ps1 Host '<Muster>' -TimeoutSeconds 60` auf eine neue passende Logzeile, Exitcode 2 heißt: Die Instanz ist vorher beendet.

## Mission über das Spielmenü

Die Fenster sind 800 × 450 groß, der Host links, der Client rechts. Die Spike-Umleitung ist nach `Start-Session.ps1` scharf, sonst reist nur der Host.
`Game-Input.ps1` bedient nur diese Fenster. Es holt das Fenster für Klick und Taste kurz nach vorn und liest Bilder nur aus dem Fenster, nie vom Desktop:

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Game-Input.ps1 Host shot
```

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Game-Input.ps1 Host click 398 417
```

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Game-Input.ps1 Host key A 1.6
```

Ohne Dateinamen landen Bilder unter `research/local/session/shots/`. Für den Ablauf über die Route gibt es zwei Skripte. `Start-Run.ps1 [-Seed <n>]` geht
aus der `MetaBase` bis zur Routenkarte und legt `research/local/session/route.png` ab. `Travel-Day.ps1 <x> <y>` klickt den Knoten, prüft nach der Ankunft
Prozesse, Netzzustand und Inventare, beendet den Tag mit `!finish`, bestätigt das Lager und legt die nächste `route.png` ab. Mit `-Stay` bleibt es
in der Mission. Stürzt eine Instanz ab, endet es mit Exitcode 2 und dem neuesten Bericht. Beide Skripte setzen ein `UserDir` ohne Tutorial-Dialoge voraus.

Von Hand geht es so:

1. Host-Fenster in den Vordergrund holen und rund 1,6 Sekunden `A` halten, bis die Figur im Ring steht.
2. Mission für Tag 1 in der Liste links anklicken, etwa bei (36, 124).
3. Fortsetzen bei (232, 241), Bestätigen bei (360, 240).
4. Einen Routenknoten anklicken. Die Lage ändert sich je Run.
5. Beide Instanzen landen in `DDDynamic_Map`, Gegner erscheinen von selbst.

In einem frischen `UserDir` kommen vorher Tutorial-Dialoge („Fortsetzen“ unter der Sprechblase) und die Seite „So wird gespielt“ (Knopf bei (398, 417)).
Danach öffnet sich direkt die Routenkarte, die Knoten für Tag 1 liegen in der linken Spalte, etwa bei (62, 113).
Für eine reproduzierbare Karte vor dem Öffnen der Routenkarte `Send-Command.ps1 Host 'DD_SetCurrentRunSeed <n>'` senden, siehe [Karte und Seed](GAME_ANALYSIS.md#karte-und-seed).

Das Pausemenü (`Esc`) hat „Zurück zum Hauptmenü“ bei (396, 266). Mit `!finish` endet die Mission beim Host, danach kommt die Routenkarte für Tag 2.
Über die echte Route erreichen beide Instanzen Tag 4, dort fehlen dem Client aber die PlayerStates, an Tag 5 stürzt er ab, siehe [Tageswechsel](GAME_ANALYSIS.md#tageswechsel-client-verliert-die-playerstates-tageswechsel-übergabestand). Eine `servertravel`-Schleife ersetzt die echte Route dabei nicht.

## Abstürze auswerten

```bash
py tools/spike/crashes.py
```

Das zeigt Fehlertext und Stack als RVAs. `--session --latest 1` zeigt nur den neuesten Bericht der laufenden Sitzung, `--role client` nur die des Clients. Manche Abstürze legen keinen Bericht an, dann steht die Adresse in der Windows-Ereignisanzeige unter
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
