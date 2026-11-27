# Checkliste Live-Test zu zweit (zwei PCs)

Stand 22.11.2026, Spiel-Build `26786272`, Mod-Version **16**. Die Liste bündelt alle offenen Bestätigungstests
(`py tools/tasks.py tests plan`) und die Zeilen unter [Live-Bestätigung ausstehend](PROZESS.md#live-bestätigung-ausstehend) in
der Reihenfolge, in der sie in einem Run vorkommen. Je Punkt steht, was der Tester tut, was er sehen oder hören muss und welche
Logzeile den Punkt belegt. Abgehakt wird erst nach der Auswertung der Logs; bis dahin bleibt die Aufgabe in `tests plan`.

Allgemeiner Ablauf: [Test auf zwei PCs](TESTING.md#test-auf-zwei-pcs). SteamIDs, Lobby-IDs und Logs nie ins Repo.

**Grundregel:** Fällt etwas auf, sofort **F9** drücken (Spiel- oder Overlay-Fenster im Vordergrund, kurzer Ton). Das schreibt
`MARKE n` mit dem eigenen Zustand ins eigene Log, nicht in das der Gegenseite. Darum „F9“ im Chat rufen, dann drückt auch die
andere Person, und beide notieren ihre Markennummer mit einem Satz. Die Markennummer ist der Bezug, nicht die Uhrzeit: Das Log
schreibt UTC. Bilder nur, wenn sich etwas nicht in Worten sagen lässt. F9 ist im Spiel nach bisherigem Stand frei; löst es dort
etwas aus, bitte melden.

**Werte vergleichen:** Wo unten „gleich“ steht, drücken beide F9, und zwar außerhalb einer offenen Bearbeitung (Airdrop-Auswahl,
Lager, Station). Während einer Bearbeitung zeigt der Client seine lokale Vorschau, und die Taktzeilen beider Seiten liegen bis zu
60 s auseinander.

## Vor dem Termin, beide

- [ ] Im sauberen Hauptordner `git switch main`, `git pull`, dann `git rev-parse --short HEAD` vergleichen: gleich.
- [ ] Steam zeigt Build `26786272`. Entwicklungskopie umstellen, einmal je PC:
      `powershell -ExecutionPolicy Bypass -File tools/Freeze-GameBuild.ps1 -Activate`. Endet mit „eingefrorener Build 26786272“.
- [ ] Keine andere Testsitzung auf dem PC, Chat außerhalb des Spiels steht, Rollen für Run 1 festgelegt.

## Start (Run 1 mit Spike)

- [ ] **Host:** `powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Build -Mod -Role Host -Tunnel Lobby -Brief`
- [ ] **Client:** vor dem Start einmal `$env:DDCOOP_TEST_SKIP_SCRAP_CHECK = "1"` (für Abschnitt Lager), dann
      `powershell -ExecutionPolicy Bypass -File tools/spike/Start-Session.ps1 -Build -Mod -Role Client -Tunnel Lobby -Peer <Lobby-ID> -Brief`
- [ ] **Beide:** Overlay (F8) zeigt Mod-Version 16 und bestätigten Handshake. Log: `Mod gestartet, Mod-Version 16, Spiel-Build 26786272`.

## MetaBase

| Tester tut | Muss sehen oder hören | Log belegt | Aufgabe |
|---|---|---|---|
| Jeder wählt eine andere Figur | Beide sehen die Wahl des anderen | keine Zeile `Takt: Charakterwahl` | `charakter-wahl`, `opt-charakter-takt` |
| Host startet den Run per Klick aufs Startgebiet | Beide reisen, Startausrüstung beider Figuren im Run | `Zustand (Weltwechsel)` auf beiden Seiten | `charakter-wahl` |

## Mission und Kampf (Tag 1)

| Tester tut | Muss sehen oder hören | Log belegt | Aufgabe |
|---|---|---|---|
| Client läuft und sprintet | Laufanimation der Client-Figur passt beim Host zum Tempo; Ausdauerbalken beim Client wie beim Host | – | `opt-laufanimation-client`, `opt-ausdauerbalken` |
| Beide laufen und zielen | Beim Client schauen beide Figuren in Lauf- und Zielrichtung | – | `opt-blickrichtung-client` |
| Beide schießen und laden nach | Client hört Schuss- und Nachladesound **beider** Figuren | – | `opt-waffen-sound`, `update-26786272-run` |
| Gegner treffen | Client sieht Schadenszahlen eigener und fremder Treffer, Blut-Treffereffekte, Lebensleisten nach Schaden | `Schadenszahlen Client: empfangen …` | `opt-schadenszahlen`, `opt-blut`, `update-26786272-run` |
| Gegner greifen an | Client hört Treffer- und Angriffssounds etwa so oft wie der Host, ohne Doppel | – | `opt-gegner-sound` |
| Bedrohungsstufe steigt | Client hört je Stufe einen Ton, Zahl unter der Stoppuhr wie beim Host | – | `opt-bedrohung-sound` |
| Client sammelt Benzin, Schrott, Items | Je Objekt ein Sound; Schrottanzeige springt nicht | beide F9: `Schrott=` gleich | `opt-item-sound`, `opt-schrott-anzeige` |
| Client plündert ein Auto, nimmt Power-ups und Itemsäcke | Schrott und Benzin wie beim Host | beide F9: `Tank=` und `Schrott=` gleich; Zähler `wieder gezeigt` | `opt-loot-aufnahme-client` |
| Level-up, danach Level-up-Kiste im Endbildschirm | Kein Schrott-Dauersound beim Client, Kiste ohne Soundfehler | – | `opt-levelup-sound` |
| Ein abgeschaltetes Item (graues Symbol) | Beim Client gleich abgeschaltet wie beim Host | – | `update-26786272-run` |

## Airdrop

| Tester tut | Muss sehen oder hören | Log belegt | Aufgabe |
|---|---|---|---|
| Airdrop landet | Client hört den Landesound einmal, passend zum Aufschlag | – | `opt-airdrop-sound` |
| Client legt die Belohnung auf ein gleiches Item (Aufwertung), bestätigt | Kein Klon, verbrauchte Items weg | **keine** Zeile `erneut aufgenommen, wieder entfernt` | `opt-airdrop-edit-ungueltig` |
| Client zieht den Roboterarm und eine weitere Waffe in **einer** Bearbeitung in den Rucksack | Beide Seiten zeigen beide Waffen an der Figur | Host: `Inventar lokal: Edit … übernommen, … vollständig=true` | `client-roboterarm` |
| Eine Kiste bleibt am Rundenende liegen | Sie wird wie solo zu Beginn der nächsten Zone neu abgeworfen, danach kommen die weiteren Airdrops | Host: `Reward-Queue …` und `RewardQueue des … weitergeschaltet` | `opt-airdrop-uebertrag` |

## Rundenende und Lager

| Tester tut | Muss sehen oder hören | Log belegt | Aufgabe |
|---|---|---|---|
| Tank voll, beide am Bus | Ladebalken der Abfahrt beim Client wie beim Host | `Busbalken: Abfahrt beim Client abgeleitet`; `Tank=` aktuell gleich Maximum | `opt-busbalken` |
| Client repariert ein kaputtes Item, legt es in den Rucksack, bestätigt | Am nächsten Tag ausgerüstet | – | `lager-handel-client` |
| Client wertet auf und verkauft im Lager | Schrott beider Seiten gleich | nach dem Bestätigen beide F9: `Schrott=` gleich | `update-26786272-run` |
| **Ablehnung:** Client legt eine Reparatur an, nicht bestätigen. Host: `Send-Command.ps1 Host '!scrap -9999'`. Client bestätigt | Knopf geht aus, Ausgangsstand vollständig, neu bearbeiten und bestätigen geht, Host reist nicht allein weiter | Client: `Inventar lokal: Host hat Edit … abgelehnt: …`, `Lager: Edit abgelehnt, Bereit zurückgenommen` | `lager-edit-ablehnung` |

Kaputtes Item für die Ablehnung: Host gibt vorher `Send-Command.ps1 Host '!give ItemData_Pistol broken'`. Der Schalter
`DDCOOP_TEST_SKIP_SCRAP_CHECK` lässt nur die **erste** fehlgeschlagene Vorprüfung des Clients in der Sitzung aus; danach prüft er
wieder selbst. Den Ablauf also einmal und ohne vorherigen Fehlversuch spielen.

## Händler und Werkbank (Tag 2)

| Tester tut | Muss sehen oder hören | Log belegt | Aufgabe |
|---|---|---|---|
| Client kauft beim Händler | Item im Rucksack, Schrott gleich | nach dem Verlassen beide F9: `Schrott=` gleich | `update-26786272-run` |
| Client wertet über das **Rezept-Panel der Werkbank** auf | Aufwertung bleibt, Host bucht sie | Host: `Inventar lokal: Edit … übernommen` | `opt-werkbank-station`, `update-26786272-run` |

## Labyrinth (wenn es auf der Route liegt)

| Tester tut | Muss sehen oder hören | Log belegt | Aufgabe |
|---|---|---|---|
| Beide betreten das Labyrinth | Client fällt nicht durch den Boden, Runde endet normal | Fällt er: F9, Host-Log und Client-Log sichern | `labyrinth-event-client` (zwei Läufe nötig) |

## Boss

Boss über die Routenkarte erreichen, nicht per Befehl.

| Tester tut | Muss sehen oder hören | Log belegt | Aufgabe |
|---|---|---|---|
| Client betritt die Boss-Karte | Kein Absturz, Boss-Leiste folgt den HP bis zum Tod | genau eine Zeile `Boss-Leiste: lokal angelegt` je Eintritt | `boss-karte-absturz`, `opt-endboss-leiste` |
| Host öffnet die Boss-Tafel (Pause), Client öffnet sein eigenes Pausemenü, auch im Vollbild | Hinweis beim Client, solange der Host die Zeit anhält; nach Schließen spielt der Client weiter | `Zeitanfragen=0` in der nächsten `Zustand`-Zeile beider Seiten | `boss-client-einfrieren`, `opt-boss-tafel-client` |
| Phasen des Bosses (BlobBall, Lazy, HQ), Client tritt wenn möglich mitten im Kampf bei | Phasenleiste beim Client wie beim Host | – | `opt-boss-phasenleiste` |

## Run-Ende

| Tester tut | Muss sehen oder hören | Log belegt | Aufgabe |
|---|---|---|---|
| Run 1 bis zum Endboss durchspielen | Kein Absturz, Desync-Melder „OK“ | keine `danach abweichend` | `integration-run`, `update-26786272-run` |
| Sieg | Ergebnisbildschirm beim Client | – | `run-ende` |

## Run 2: Rollentausch und nur Mod

- [ ] **Beide:** `Stop-Session.ps1`, Rollen tauschen, beide ohne `DDCOOP_TEST_SKIP_SCRAP_CHECK` und mit `-NoSpike` neu starten.

| Tester tut | Muss sehen oder hören | Log belegt | Aufgabe |
|---|---|---|---|
| Früherer Client hostet einen neuen Run | Level 1 und Startinventar bei beiden, MetaBase-Items passen | `Zustand` zeigt neue Rolle; beim neuen Client `XP-Anzeige: Host-Level 1`. Das Startinventar steht nicht im Log, darum ein Bild des Rucksacks beider Seiten | `client-runstand-abbruch`, `opt-metabase-inventar` |
| Mehrere Tage nur mit Mod | Kein Absturz | – | `update-26786272-run` |
| Absichtlich verlieren | Niederlage beim Client nativ, neuer Run in derselben Sitzung geht | – | `run-ende` |
| Ein eigener offener Run des Clients | lässt sich danach fortsetzen | – | `client-runstand-abbruch` |

## Nach dem Test, beide

- [ ] `powershell -ExecutionPolicy Bypass -File tools/spike/Stop-Session.ps1`
- [ ] Nach einem Absturz oder vor dem Neustart für Run 2 ist nichts zu sichern: Logs und Absturzberichte bleiben liegen.
- [ ] `powershell -ExecutionPolicy Bypass -File tools/spike/Collect-Logs.ps1` packt Mod-Logs, Absturzberichte und Absturzereignisse
      der letzten 12 Stunden in ein Zip unter `research/local/livetest`. **Beide Zips** schicken, auch das des Hosts.
- [ ] Liste der Marken (Nummer, ein Satz) dazulegen. Die Logs schreiben UTC; die Uhren beider PCs können um etwa eine Sekunde
      abweichen.

## Zeilen, nach denen die Auswertung sucht

| Zeile | Seite | Bedeutung |
|---|---|---|
| `Mod gestartet, Mod-Version N, Spiel-Build B` | beide | Stand der Instanz |
| `Zustand (Weltwechsel\|Takt): Rolle=… Karte=… Spieler=… Zeitfaktor=… Pause=… Zeitanfragen=… Schrott=… Tank=… Ping=… Verbindungen=… Kontrolle-abgelehnt=…` | beide | Zustand bei jedem Kartenwechsel und alle 60 s; beide Logs nebeneinander zeigen, wo Tank, Schrott oder Zeitanfragen auseinanderlaufen. `-` heißt unbekannt, etwa vor dem Laden der Mission |
| `MARKE n: Tester hat F9 gedrückt, …` | je Seite | Zeitpunkt eines Fundes mit dem Zustand dieser Seite |
| `XP-Anzeige: Host-Level N` | Client | Level des Hosts, auch zu Beginn eines neuen Runs |
| `Busbalken: Abfahrt beim Client abgeleitet` | Client | Busbalken gestartet |
| `Boss-Leiste: lokal angelegt` | Client | Boss-Leiste je Eintritt |
| `Inventar lokal: Host hat Edit … abgelehnt: <Grund>` | Client | Ablehnung mit Grund |
| `Lager: Schrott reicht nach dem Stand des Hosts … nicht` | Client | Vorprüfung des Clients |
| `Inventar lokal: Edit … übernommen, … vollständig=true` | Host | Bearbeitung des Clients gebucht |
| `teilweise übernommen` | Client | Host hat nur einen Teil geschafft |
| `danach abweichend: … (ist/soll)` | beide | Item passt nach Übernahme oder Rücknahme nicht |
| `erneut aufgenommen, wieder entfernt` | Client | Klon einer aufgewerteten Belohnung |
| `Takt: … hielt den Frame … ms auf` | beide | Ruckler, mit früheren Läufen vergleichen |
| `Spielthread: seit … s kein Takt der Mod, Probe n/3, im Abschnitt …` | beide | Game Thread steht; Abschnitt der Mod oder „außerhalb der Mod-Ticks“, Stapel als Modul+RVA; mit `Spielthread: Takt der Mod läuft wieder` war es nur ein langer Abschnitt |
| `Kontrollnachrichten stauen sich` | beide | Trennung, weil das Spiel den Kontrollkanal nicht abholt, meist Folge eines stehenden Game Threads |
