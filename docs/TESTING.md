# Testen mit zwei Instanzen

Host und Client laufen auf einem PC unter einem Steam-Konto, ein zweiter PC ist für die Analyse nicht nötig. Beide Instanzen
bekommen ein eigenes `-UserDir`, der echte Spielstand bleibt also unberührt. Die Skripte stehen in [tools/](../tools/README.md).
Nur was zwei Steam-Konten braucht, etwa Steam-P2P, läuft auf zwei PCs, siehe [Test auf zwei PCs](#test-auf-zwei-pcs).

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
Mit der Mod kommen die Spielfehler-Fixes von ihr, der Spike lässt seine aus (Spike-Log: `Patch: Spielfehler-Fixes kommen von der Mod`), siehe [Patches](../src/Game/README.md#patches).
`Stop-Session.ps1` entfernt die Mod danach wieder, sonst lädt auch ein normaler Spielstart sie ohne Crash-Schutz. Ohne `-Mod` nimmt
`Start-Session.ps1` eine fest installierte Mod vorher heraus und warnt: Mit ihren Patches ließe der Spike die Karte aus, und ohne Tunnel
der Mod fielen die Figuren durch den Boden. Wer die Mod danach wieder fest im Spiel haben will, führt `Deploy-Mod.ps1` aus.

### Tunnel der Mod prüfen

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Mod -Tunnel Mod
```

Statt des Spike-Tunnels verbindet dann der Transport der Mod aus `src/Net`. Der Host startet mit `-ddcoop_transport=host`, der Client
mit `-ddcoop_transport=loopback`, auf zwei PCs mit `-Role Client -Tunnel Mod -Peer <SteamID64 des Hosts>`. Sobald im Mod-Log des
Clients `Transport: verbunden` steht, öffnet der Spike `127.0.0.1:7778`. Die Statistik steht alle 30 s im Mod-Log, siehe
[Net](../src/Net/README.md#transport-transport).

### Steam-Lobby der Mod prüfen

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Mod -Tunnel Lobby
```

Beide Instanzen starten gleichzeitig ohne Lobby-Schalter, der Host mit `-ddcoop_localtest`: Nur damit nimmt er den Client
unter demselben Konto über 127.0.0.1 an, siehe [Zugriffskontrolle](../src/Net/README.md#zugriffskontrolle-lobby-auth). Sobald der Spike geladen ist, erstellt der Host die Lobby über
das Overlay der Mod und wird selbst Listen-Server in der MetaBase, der Client tritt ebenso mit der Lobby-ID bei (`Overlay.ps1`).
Die Abstürze beim Beitritt ohne Spike (Host `0x5901CE8`, Client `0x56B16D9`, 04.11.2026) verhindert inzwischen die Mod selbst,
siehe [Nur Mod, ohne Spike](#nur-mod-ohne-spike). Die Schalter `-ddcoop_lobby` und `-ddcoop_lobby_delay` der Mod gibt es weiter, das Skript braucht sie nicht.
Auf zwei PCs: Host mit `-Role Host -Tunnel Lobby`, die Ausgabe nennt die Lobby-ID, Client mit `-Role Client -Tunnel Lobby -Peer <Lobby-ID>`.
Die Lobby-ID gehört zu keinem Konto und verfällt mit der Lobby. Einladungen über Steam prüft man nur mit zwei Konten: Overlay
mit **F8**, „Einladen“, Freund wählen, beim Freund „Spiel beitreten“. Siehe [Net](../src/Net/README.md#steam-lobby-steam-lobby).

- **Einladung bei geschlossenem Spiel:** Client mit `-Role Client -Mod -Tunnel Lobby -NoSpike -SteamLaunch`, ohne `-Peer`. Steam startet
  immer seine eigene Installation, darum legt das Skript die Mod dorthin statt in `PinnedGameDir` (gleicher Build nötig), startet den
  Crash-Schutz und wartet bis `-JoinTimeoutSeconds` auf das Spiel, das Steam nach der angenommenen Einladung startet. Es meldet nur, ob
  `+connect_lobby` dabei war, und wartet auf „Start über Steam-Einladung“ im Mod-Log. Das Spiel läuft ohne eigenes `-UserDir`, die Mod
  speichert in `SaveGames_DDCoop`. `Stop-Session.ps1` nimmt die Mod wieder aus der Steam-Installation.
- **Konto außerhalb der Lobby:** `-Role Client -Mod -Tunnel Mod -NoSpike -Peer <SteamID64 des Hosts>` gegen einen Host mit `-Tunnel Lobby`.
  Ohne Spike verbindet danach niemand das Spiel, erwartet ist der Abbruch mit „nicht Mitglied seiner Lobby“ (Grund 1005).
- Ablauf für Beitritt und geordnetes Verlassen: `Guided-Test.ps1 -Steps tools/spike/guided/lobby-abnahme.txt` nach dem Start des Clients.

### Nur Mod, ohne Spike

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Mod -Tunnel Lobby -NoSpike -Brief
powershell -ExecutionPolicy Bypass -File tools/spike/Run-Route.ps1 -Start -NoSpike -Days 1 -StayLast
```

So läuft das Spiel wie im Release: nur `DDCoop.dll`, kein `spike.dll`. Crash-Schutz, eigene `UserDir` und Sperre bleiben gleich.
Das Skript prüft nach dem Hauptmenü und am Ende, dass `spike.dll` nicht geladen ist. Ohne Spike gibt es keine Konsolenbefehle,
also nur `-Tunnel Lobby` und die MetaBase als Ziel, kein `-Godmode`, kein fester Seed und kein `!finish`. `Run-Route.ps1` endet darum
in der ersten Mission. Ein frischer Test-Spielstand fährt nach „So wird gespielt“ ohne Routenkarte direkt los, sonst sucht das Skript den
Knoten für Tag 1 in der linken Spalte. `Send-SpikeCommand` bricht in so einer Sitzung mit einer klaren Meldung ab.

Erwartet ist heute Exitcode 1 mit „Figur nicht am Bus“ und verschiedenen Kachelzahlen: Die Karte vom Host gibt es nur im Spike,
siehe [Ohne Spike](../src/Game/README.md#ohne-spike). Abstürze enden wie sonst mit Exitcode 2.

### Mod-Overlay prüfen

Nach einem Build mit dem Paket `overlay` zeigt **F8** im aktiven Spielfenster das Statusfenster; erneutes F8 oder dessen Schließen verbirgt es. Pro Prozess darf nur dessen eigenes Overlay erscheinen. Beim Wechsel zu einer anderen Anwendung bleibt ein eingeschaltetes Overlay über dem Spielfenster sichtbar (seit `overlay-fokus`), beim Minimieren verschwindet es und kommt nach Wiederherstellen zurück. „Lobby-ID kopieren“ ist nur mit Lobby aktiv und legt die ID in die Zwischenablage. Fenster verschieben und DPI/Lesbarkeit sowie Tab-Bedienung prüfen. Keine fremde Testsitzung dafür bedienen.

Mit Steam bedienen die Lobby-Knöpfe die echte Lobby der Mod (`steam-lobby`), siehe [Steam-Lobby der Mod prüfen](#steam-lobby-der-mod-prüfen). Nur ohne Netzwerkbackend, etwa ohne laufendes Steam, bleiben sie deaktiviert und der Status lautet „Netzwerk noch nicht angebunden“. Fenster- oder randlosen Modus verwenden, kein exklusives Vollbild. Das Overlay pausiert das Spiel nicht. Optionen in `ddcoop.ini`, API und Testgrenzen stehen in [src/UI](../src/UI/README.md). Die [sichtbare Host-/Client-Abnahme vom 03.11.2026](../src/UI/README.md#sichtbare-spielabnahme-03112026) ist bestanden; die CI prüft weiterhin nur Modell und unsichtbaren Fenster-Lebenszyklus, keine Spielgrafik.

## Spielzeit sparen

Pro PC läuft nur eine Sitzung, Spielzeit ist der Engpass. Darum:

- **Bestätigungstests bündeln.** `py tools/tasks.py tests plan` zeigt verfügbare ausstehende
  Prüfungen mit ihren ursprünglichen Abnahmen und lässt aktive fremde Arbeit aus. In einer
  Sitzung nacheinander abarbeiten; je Kennung Build/Mod-Stand, Beobachtung und Beleg notieren.
  Teststände mit `tests update`, nur vollständig bestandene Abnahmen mit `tests pass` im
  eigenen Wartungs-Worktree aktualisieren. Das braucht keine Entwicklungsreservierung je Test
  und macht wartende Tests nicht zu „in Arbeit“. [Befehle](../tools/README.md#bestätigungstests).

- **Code und Spieltest trennen.** Code und `Build-Mod.ps1 -Test` laufen ohne Spiel und parallel. Spieltests sammeln und in einem Block
  fahren, die Sitzung dabei weiterverwenden statt sie je Prüfung neu zu starten.
- **Warten statt abbrechen.** `Start-Session.ps1 -Wait 1800` reiht sich hinter eine fremde Sitzung ein und startet, sobald sie endet.
- **Nur neu starten, was nötig ist.** `Restart-Instance.ps1` startet nur den Client neu (etwa 20 s). `-Build` nur, wenn sich der Spike geändert hat.
  Hält der Host direkt nach dem Beenden noch die alte Verbindung („Der Host hat schon einen Mitspieler“), erstellt er eine neue Lobby und
  der Client tritt ihr bei, das macht das Skript selbst.
- **Kurz ausgeben.** `Start-Session.ps1`, `Restart-Instance.ps1`, `Get-Status.ps1`, `Start-Run.ps1` und `Travel-Day.ps1` kennen `-Brief`:
  eine Zeile je Instanz (Karte, Spieler, Rolle), eine für Inventar und eine für Abstürze. Volle Ausgabe und Bilder nur ansehen, wenn
  etwas nicht stimmt. `Travel-Day.ps1` wartet auf die neue Karte mit beiden Spielern statt fest 35 s, meist reichen 10 s.
- **Skripte statt Klicks.** `Overlay.ps1` bedient das Overlay, `Watch-Net.ps1` zeigt Meldungen mit Zeit, `Test-NetEnd.ps1` prüft das
  Sitzungsende in rund einer Minute. Wer eine Prüfung zum zweiten Mal von Hand macht, schreibt dafür ein Skript.

| Start (05.11.2026, ein PC) | Vorher | Jetzt |
|---|---|---|
| `-Mod -Tunnel Lobby` bis MetaBase | 220 s (mit erstem Mod-Build) | 28 s |
| `-Mod` bis MetaBase | 100 s | 42 s |
| `-Map /Game/Maps/DDDynamic_Map` ohne Mod | – | 37 s |
| Client nach Absturz wieder verbunden | voller Neustart | 20 s |

### Testkanal der Mod

Jede Sitzung mit `-Mod` startet die Instanzen mit `-ddcoop_control` und vermerkt `Control` in `session.json`. Die Mod öffnet dann die
Named Pipe `\\.\pipe\ddcoop-ctl-<pid>` (nur lokal, nur der eigene Benutzer, nie im Release ohne den Schalter). Eine Zeile Anfrage, eine
Zeile JSON-Antwort, Protokoll in [`src/Net/ControlProtocol.h`](../src/Net/ControlProtocol.h):

| Anfrage | Wirkung |
|---|---|
| `state` | Welt, Paket, Rolle, Spieler, Clients, Sitzung (`state`, `session`, `lobby`, `members`, `ping`, `message`, `canCreate`, `canJoin`, `canInvite`, `canLeave`) |
| `lobby create`, `lobby join <id>`, `lobby invite`, `lobby leave` | Dasselbe wie die Overlay-Knöpfe (`Net::Session`) |
| `day` | Nur lesen: alle skalaren Reflexionsfelder des GameState, seiner Unterobjekte (Tank, Nacht, Gefahr, Wallets) und aller `DDObjective`. `py tools/spike/day_state.py <host-pid> <client-pid>` vergleicht beide Seiten, `Run-Route.ps1 -DayState [-DaySeconds 200]` nach jeder Ankunft, spät am Tag und auf der Routenkarte |
| `console <text>` | Konsolenbefehl des Spiels, etwa `open 127.0.0.1:7778` |
| `travel <Karte>` | `servertravel <Karte>?listen`, nur als Host |

In `tools/spike/Common.ps1` stehen `Invoke-ModControl` und `Send-ModControl` (roh), `Send-GameConsole` und `Send-GameTravel`
(Kanal oder Spike). `Get-NetState`, `Get-OverlayState`, `Invoke-OverlayAction`, `Join-HostLobby` und `New-OverlayLobby` nehmen in solchen
Sitzungen die Pipe statt `netstate.py`, Overlay-Fenster und Befehlsdatei und geben dieselben Felder zurück. Antwortet die Pipe nicht
(Spiel startet noch), liest `Get-NetState` wie bisher den Speicher. Die Sync-Anzeige für `Test-Desync.ps1` gibt es nur im Fenster
(`Get-OverlayState -Window`). Ohne Mod und beim Start über Steam (`-SteamLaunch`) bleiben die alten Wege.

Gemessen am 07.11.2026, ein PC, Build 26495302: `Start-Session.ps1 -Mod -Tunnel Lobby -NoSpike` 27 s bis zur MetaBase mit beiden Spielern
(vorher 28 s), `Restart-Instance.ps1` 16 s (vorher 20 s), `Test-NetEnd.ps1` 50 s mit allen Fällen bestanden (vorher rund eine Minute).
Spike-Befehle wie `!finish`, `!round`, `!god` und `!tunnel` kennt der Kanal nicht, sie bleiben Sache des Spikes.

### Mensch und Skript zusammen

Arbeitsteilung: Alles, was sich über Konsole, Befehle oder Skripte lesen oder steuern lässt, erledigt der Agent selbst, auch die Prüfungen
(Overlay, Logs, Netzzustand, Lobby, Reisen, Run-Start). Der Mensch übernimmt nur Spielaktionen: mit der Figur laufen, kämpfen,
Belohnungen wählen. Er springt dort ein, wo der Agent sonst erst eine Schnittstelle oder ein Skript bauen müsste.

**Der Mensch bedient keine Konsole.** Er schaut zu oder spielt, Rückmeldung gibt er im Chat. Starten, Schritte bestätigen, Prüfen und
Auswerten macht der Agent. Am 05.11.2026 wartete ein Tag-1-Test bei Schritt 3 auf Enter in der Konsole des Menschen. Seitdem bestätigt
der Agent die Spielschritte selbst:

1. Der Agent startet `Guided-Test.ps1` im Hintergrund und beobachtet die Ausgabe.
2. Bei einem Spielschritt gibt das Skript `WARTET [n/m] Aktion | Erwartet: …` aus. Der Agent sagt dem Menschen im Chat, was er spielen
   soll, und wertet währenddessen Logs und Zustand aus.
3. Meldet der Mensch im Chat, wie es lief, antwortet der Agent mit `Guided-Answer.ps1 ok`, `Guided-Answer.ps1 "<Abweichung>"`, `skip`
   oder `quit`. `Guided-Answer.ps1 status` zeigt den wartenden Schritt.

Endet eine Instanz oder kommt nach `-AnswerMinutes` (30) keine Antwort, bricht der Test ab und schreibt den Bericht.

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Guided-Test.ps1 -Steps tools/spike/guided/tag-1.txt -Start -Shots
```

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Guided-Answer.ps1 ok
```

Aufbau der Schrittdatei: eine Zeile je Schritt, `Aktion | Erwartung` für eine Spielaktion des Menschen, `> Skript.ps1 ...` für einen
automatischen Schritt aus `tools/spike`. `> Skript.ps1 ... | Muster` prüft automatisch und wiederholt bis zu 30 s, bis die Ausgabe zum
Muster passt. Das Skript hält je Schritt Dauer, neue Zeilen der Mod-Logs, Overlay- und Netzzustand, neue Abstürze und mit `-Shots` Bilder
beider Fenster fest und schreibt `research/local/guided/<Name>-<Zeit>/report.md`, den der Agent auswertet. Beispiele liegen in
`tools/spike/guided/`.

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
Prozesse, Netzzustand und Inventare, beendet den Tag mit `!finish`, klickt bei jeder lokalen Instanz Fortsetzen und Bestätigen im Lager
(`Complete-RoundEnd` in `Common.ps1`; der Host wartet seit `bestaetigen-rundenende` auf den Client) und legt die nächste `route.png` ab. Mit `-Stay` bleibt es
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

### Latenz simulieren

Auf einem PC läuft alles über Loopback mit 0 ms, über das Internet sind 40 bis 150 ms und Verluste normal. Der Tunnel der Mod kann
Ping, Schwankung und Paketverlust simulieren, Einzelheiten in [Netzsimulation](../src/Net/README.md#netzsimulation-latenz-test):

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Mod -Tunnel Lobby -NetProfile Internet -Brief
```

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Run-Route.ps1 -Start -NetProfile Internet -Days 1 -StayLast
```

| Profil | Ping | Schwankung | Verlust je Richtung |
|---|---|---|---|
| `Internet` | 120 ms | ±30 ms | 2 % |
| `Schlecht` | 250 ms | ±80 ms | 5 % |
| eigene Werte, etwa `80,10,1` | 80 ms | ±10 ms | 1 % |

Das Profil wirkt nur im Tunnel der Mod, also mit `-Mod -Tunnel Lobby` oder `-Tunnel Mod`. `Restart-Instance.ps1` übernimmt es aus der
Sitzung, `Get-Status.ps1 -Brief` zeigt je Instanz die letzte Messzeile aus dem Mod-Log.

**Wann mit Profil:** Abnahmen von Aufgaben, die Zustand zwischen Host und Client abgleichen (Bewegung, Kampf, Gegner, Eingaben,
Belohnungen, Kontrollnachrichten, Sitzungsende), laufen zusätzlich mit `-NetProfile Internet`. Wer Timing-Fehler sucht, nimmt `Schlecht`.
Ohne Profil bleiben Tests, die nur Start, Build oder Darstellung auf einer Seite prüfen, und alles, was Zeiten misst, die mit Latenz
nicht vergleichbar wären. Den Test über echtes Internet zwischen zwei Haushalten ersetzt das Profil nicht.

## Test auf zwei PCs

Manches lässt sich auf einem PC nicht prüfen: Steam lehnt P2P zum eigenen Konto ab, siehe [Steam-Tunnel](../research/spikes/a-connect/README.md#spike-e-steam-tunnel-tunnel).
Dafür laufen Host und Client auf zwei PCs mit zwei Steam-Konten, je PC nur eine Instanz. Solche Aufgaben haben in
[TODO.md](../TODO.md) die Bearbeitung **zu zweit**, etwa `tunnel-p2p`.
Solche Tests lassen sich in einem gemeinsamen Termin bündeln. Als Planungshilfe dienen die Hinweise unter
[Live-Sitzungen zu zweit](../TODO.md#live-sitzungen-zu-zweit).

### Reservieren zu zweit

Zwei Personen teilen sich die Aufgabe, je eine Rolle. Jede reserviert ihre Rolle auf ihrem PC:

```bash
py tools/tasks.py claim tunnel-p2p <eigener-name> host
```

```bash
py tools/tasks.py claim tunnel-p2p <eigener-name> client
```

Das legt je Rolle den Branch `task/<kennung>/<rolle>`, den Worktree `.worktrees/<kennung>-<rolle>` und einen Draft-PR an.
`tasks.py list` zeigt beide Rollen, etwa `zu zweit: host reserviert (anna), client frei`. Wer beide PCs selbst bedient,
reserviert ohne Rolle wie sonst auch. Eine Rolle gibt man mit `py tools/tasks.py release <kennung> <rolle>` frei.

Abschluss: Der **Host-PR schließt die Aufgabe ab**. Er bringt die Doku, die TODO-Zeile und beide Berichte. Der Client-PR
enthält nur Änderungen, die auf der Client-Seite nötig waren, und seinen Bericht in der Beschreibung. Hat der Client nichts
geändert, kommt sein Bericht als Kommentar in den Host-PR, danach gibt er seine Rolle mit `release` frei. Beide arbeiten
auf demselben Stand von `main`: vor dem Test `git rev-parse --short HEAD` vergleichen.

### Vorbereitung auf beiden PCs

1. Einrichtung nach [SETUP.md](SETUP.md), Spiel-Build `26495302`. `Start-Session.ps1` bricht bei einem anderen Build ab.
2. Zwei verschiedene Steam-Konten, beide besitzen das Spiel, Steam läuft und ist online. Freundschaft ist nicht nötig.
3. Kein Port muss freigegeben werden: Steam vermittelt die Verbindung und leitet sie notfalls über eigene Relays. Fragt die
   Windows-Firewall beim ersten Start nach dem Spiel, reicht „Abbrechen“, der Tunnel spricht mit dem Spiel nur über `127.0.0.1`.
4. Auf jedem PC läuft nur diese eine Sitzung, kein anderes Spiel und keine zweite Testsitzung.
5. Absprache über einen Chat außerhalb des Spiels, etwa Discord: Der Host-PC zeigt beim Start seine SteamID, der Client braucht sie für `-Peer`.
   Die SteamID nie ins Repo, in Commits oder in PR-Texte schreiben.

### Automatischer Ablauf

Erst den Host starten, dann den Client. Jede Seite wartet bis zu 15 Minuten auf die andere (`-JoinTimeoutSeconds`).

Auf dem Host-PC:

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Test-TwoPc.ps1 -Role Host
```

Auf dem Client-PC, mit der SteamID aus der Zeile `Host bereit. SteamID fuer -Peer ...`:

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Test-TwoPc.ps1 -Role Client -Peer <SteamID>
```

Danach läuft alles ohne Eingriff:

| Schritt | Host-PC | Client-PC |
|---|---|---|
| Start | Spiel mit Spike und Crash-Schutz, Listen-Server, `!tunnel host`, zeigt die SteamID | Spiel mit Spike und Crash-Schutz, `!tunnel join <SteamID>`, dann `open 127.0.0.1:7778` |
| Verbinden | wartet, bis der Client im Spiel ist, dann `servertravel` in die `MetaBase` | wartet, bis der Host in die `MetaBase` reist |
| Run | `Start-Run.ps1 -Seed 20795`, dann `Travel-Day.ps1 62 213 -Stay` in die Mission an Tag 1 | `!notutorial`, wartet auf `DDDynamic_Map` |
| Messen | 120 s in der Mission, prüft alle 10 s, ob die Instanz läuft | ebenso |
| Bericht | `research/local/session/tunnel-host.txt` | `research/local/session/tunnel-client.txt` |

Die Sitzung läuft danach weiter, man kann also selbst weiterspielen. `Stop-Session.ps1` beendet sie, `-Stop` macht das gleich
am Ende. Weitere Schalter: `-Build` wie bei `Start-Session.ps1`, `-Godmode`, `-MissionSeconds`, und für eine andere
Route `-Seed`, `-NodeX`, `-NodeY`. Exitcode 0 heißt Ping und Route stehen im Log und der Client hat die Host-Karte übernommen (`MapGen: Client uebernimmt`), 1 heißt nicht belegt oder Abbruch. `-Mod` bricht seit `missionsstart` ab: Mit der Mod kommt die Host-Karte über ihren Kontrollkanal, den der Spike-Tunnel nicht trägt. Zwei PCs mit Mod gehen über die Lobby im Overlay. Ohne Host-Karte hat der Client keinen Boden, siehe [Host-Karte über den Tunnel](../research/spikes/a-connect/README.md#host-karte-über-den-tunnel-karte-tunnel).

Der Bericht kommt aus `tools/spike/tunnel_report.py`: Verbindung, Ping (Minimum, Median, Maximum), Pakete und verworfene Pakete,
Qualität, die Routenzeilen von Steam (direkt oder über Relay) und Abbrüche. SteamIDs und IP-Adressen sind darin maskiert, er darf
so in den PR. Von Hand geht es auch: `py tools/spike/tunnel_report.py <spike-log> --role Host`.

### Mit der Mod über die Lobby

Für Abnahmen mit der Mod ist das der übliche Weg, `Test-TwoPc.ps1` bricht mit `-Mod` ab. Erst der Host, die Ausgabe nennt die Lobby-ID:

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Build -Mod -Role Host -Tunnel Lobby -Brief
```

Dann der Client mit dieser Lobby-ID, alternativ Einladung über das Overlay (**F8**, „Einladen“):

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Build -Mod -Role Client -Tunnel Lobby -Peer <Lobby-ID>
```

Die Lobby-ID gehört zu keinem Konto, verfällt mit der Lobby und kommt trotzdem nicht ins Repo. Beide brauchen denselben Stand von
`main` und denselben Build. Der Spike läuft auf beiden Builds; `-NoSpike` bleibt für Läufe wie im Release. Bei unvollständiger Portierung eines neuen Builds kommt
`-PartialPatches`; ohne Spike gibt es keine Konsolenbefehle wie `!finish`. Ein Rundenende geht dann über vollen Tank und Busflucht,
den Tank füllt `py tools/spike/fuel.py <host-pid> --fill`. Ablauf und Ergebnisse eines solchen Termins:
[Live-Test 07.11.](GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-07112026-livetest-0711).

### Von Hand oder in Teilen

`Start-Session.ps1` kennt dafür `-Role`. Ohne `-Role` oder mit `-Role Both` läuft alles wie bisher auf einem PC.

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Role Host -Tunnel P2p
```

```bash
powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Role Client -Tunnel P2p -Peer <SteamID>
```

Danach gehen `Send-Command.ps1`, `Wait-Log.ps1`, `Game-Input.ps1` und `Get-Status.ps1` nur für die eigene Rolle; für die andere
melden sie, dass sie auf diesem PC nicht läuft. Den Run steuert der Host mit `Start-Run.ps1` und `Travel-Day.ps1` wie gewohnt. Am Rundenende muss der Client am eigenen PC
Fortsetzen und Bestätigen klicken; `Travel-Day.ps1` wartet bis zu 120 s darauf, dass der Host die Routenkarte zeigt.
Ohne Tunnel verbindet `-Role Client -Peer <Adresse>:7777` direkt, dafür muss der Host-PC UDP 7777 erreichbar machen.

### Grenzen

- `Run-Route.ps1`, `check_round.py` und der Inventarvergleich lesen beide Instanzen aus dem Speicher und gehen nur auf einem PC.
  Auf zwei PCs vergleicht man Inventare und Zustand über Bildschirmfotos (`Game-Input.ps1 <Rolle> shot`) und die Logs.
- Stürzt eine Seite ab, wertet sie `crashes.py --session` auf ihrem eigenen PC aus und schickt das Ergebnis der anderen.

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
