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

### Mod-Overlay prüfen

Nach einem Build mit dem Paket `overlay` zeigt **F8** im aktiven Spielfenster das Statusfenster; erneutes F8 oder dessen Schließen verbirgt es. Pro Prozess darf nur dessen eigenes Overlay erscheinen. Beim Wechsel zu einer anderen Anwendung und beim Minimieren muss es verschwinden; nach Rückkehr sichtbar bleiben, wenn es zuvor eingeschaltet war. Fenster verschieben und DPI/Lesbarkeit sowie Tab-Bedienung prüfen. Keine fremde Testsitzung dafür bedienen.

Ohne Netzwerkbackend müssen die Lobby-Knöpfe deaktiviert bleiben und der Status „Netzwerk noch nicht angebunden“ lauten. Fenster- oder randlosen Modus verwenden, kein exklusives Vollbild. Das Overlay pausiert das Spiel nicht. Optionen in `ddcoop.ini`, API und Testgrenzen stehen in [src/UI](../src/UI/README.md). Die [sichtbare Host-/Client-Abnahme vom 03.11.2026](../src/UI/README.md#sichtbare-spielabnahme-03112026) ist bestanden; die CI prüft weiterhin nur Modell und unsichtbaren Fenster-Lebenszyklus, keine Spielgrafik.

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

Ohne Dateinamen landen Bilder unter `research/local/session/shots/`. `Game-Input.ps1 Host pixel <x> <y>` gibt die Farbe eines Punkts als `RRGGBB` aus.
Für den Ablauf über die Route gibt es drei Skripte. `Start-Run.ps1 [-Seed <n>]` geht aus der `MetaBase` bis zur Routenkarte und legt
`research/local/session/route.png` ab. Es liest die Position des Hosts, läuft in einem Zug in den Ring, erkennt die Routenkarte am grünen Banner
und versucht es bis zu dreimal. `Travel-Day.ps1 <x> <y>` klickt den Knoten, prüft nach der Ankunft
Prozesse, Netzzustand und Inventare, beendet den Tag mit `!finish`, bestätigt das Lager und legt die nächste `route.png` ab. Mit `-Stay` bleibt es
in der Mission. Stürzt eine Instanz ab, endet es mit Exitcode 2 und dem neuesten Bericht. Tutorial-Hinweise schaltet der Spike auf beiden Seiten selbst ab.

### Fester Routenlauf

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Run-Route.ps1 -Start
```

Das startet die Sitzung neu (`-Start`), geht mit Seed `20795` auf die Route und fährt die Knoten (62, 213), (160, 213), (255, 213), (352, 112), (448, 230).
`-Days`, `-Seed` und `-Nodes` ändern den Lauf, `-StayLast` bleibt am letzten Tag in der Mission. Nach jedem Wechsel prüft `check_round.py`:
Welt, `PlayerArray` und PlayerStates (je 2), Figuren am Bus, Pause in `WorldSettings`, `MapTile`s, statische NetGUIDs der Spielerklassen
gegen den Host und neue `NewActor failed` im Log. Jede Zeile endet mit `sauber` oder `ABWEICHUNG` und den Gründen. Ergebnisse und Bilder
(3, 8 und 15 s nach dem Klick, dann nach der Prüfung) liegen unter `research/local/session/runs/<Zeit>/`, die Prüfungen in `checks.jsonl`.
Exitcode 0 heißt alles sauber, 1 Abweichung, 2 Instanz beendet. Eine einzelne Prüfung geht auch von Hand:

```bash
py tools/spike/check_round.py <host-pid> <client-pid>
```

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
Über die echte Route laden Tag 1 bis 4 sauber, an Tag 5 stürzt der Client ab, siehe [Pausierte Welt und Busplatz](GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung). Eine `servertravel`-Schleife ersetzt die echte Route dabei nicht.

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
