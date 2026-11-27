# spawn-nach-kartenaufbau: Host setzt Figuren an den Bus, bevor der Client seine Karte gebaut hat

Widerlegt mit Messung am 27.11.2026, ein PC mit zwei Instanzen, Build `26786272`. Der Busplatz des Hosts bewegt die Figur des
Clients nicht. Der Client setzt seine eigene Figur in seiner eigenen Kartenerzeugung an den Bus. Das galt auch, als der Host-Busplatz
10 s vor dem Ende der Client-Erzeugung kam. Kein Fix. Warum der Client live im Labyrinth fiel, ist weiter offen.

## Reproduktion

Labyrinth-Event, zwei PCs: Der Client fällt nach dem Zonenstart durch die Karte.
Herkunft: [Labyrinth-Event beim Client](../archive/GAME_ANALYSIS.md#labyrinth-event-beim-client-labyrinth-event-client), Aufgabe `labyrinth-event-client`.

Nachstellen auf einem PC: `DDCOOP_TEST_MAP_PROFILE` auf das Labyrinth-Profil (`/Game/Map_Generation/Blueprints/MapEvents/MGP_LabyrinthEvent.MGP_LabyrinthEvent`),
`DDCOOP_TEST_MAP_PROFILE_EVERY=2` und für einen langsamen Client `DDCOOP_TEST_CLIENT_MAP_DELAY_MS` ([Testen](../TESTING.md#gezielt-in-eine-karte)).
Dann `Start-Session.ps1 -Build -Mod -Godmode -Tunnel Lobby -Brief` und Start über den Routenknoten ohne Eingaben (`!notutorial`,
`!shop go 3`, Knopf und Knoten per `!shop call`/`callat`). Nicht mit `-Map` direkt reisen: Dann fallen beide Figuren, egal wie lange
der Client baut. Rundenende und nächster Knoten wie unter „Rundenende ohne echte Eingaben“ in [Testen](../TESTING.md#gezielt-in-eine-karte).

## Belege

- [Belegt] Im Labyrinth lagen live 0,29 s zwischen dem Senden der Kartenwerte und dem Busplatz des Hosts, auf normalen Karten 1,2 bis 3,4 s.
- [Belegt] Der Host übernimmt die fallende Client-Position.
- [Belegt, Messung] Die Mod schreibt seit dieser Aufgabe `Spawn-Messung: …`-Zeilen. Beim Client stehen Ort und Bewegungsmodus der
  eigenen Figur darin, beim Host die der Client-Figur, jeweils in den ersten 30 s jeder Missionswelt und nur bei Änderung. Dazu
  kommt das Ende der Client-Erzeugung und der Ort der eigenen Figur am Busplatz des Clients. Die Busplatz-Zeile nennt die Rolle.
  Beide Instanzen eines PCs schreiben mit derselben Uhr.
- [Belegt] Der Client hat einen eigenen Busplatz. Seine Kartenerzeugung läuft durch `BusSpawnPositionGenerator` und die Freiräumung,
  im Log `Busplatz: 1 geschont, 1 Spielerfiguren ergänzt (Client)`. Erst dabei springt die eigene Figur an den Bus, kurz vor
  `Client-Erzeugung fertig`. Beim Freiräumen steht sie schon am Bus.
- [Belegt] Der Busplatz des Hosts bewegt die Client-Figur weder beim Client noch beim Host. Beim Host bleibt sie nach
  `Busplatz … (Host)` am Startpunkt, bis der Client seinen Ort meldet. Das passt zu `AcceptClientPositions`.
- [Belegt] Der Startpunkt `(3467, -399, ~950)` hat in diesen Karten keinen Boden. Bis zum eigenen Busplatz fällt die Figur dort im
  Modus 3 (bei 10 s Verzögerung bis Z −32600) und landet danach trotzdem am Bus.
- [Belegt] Zeiten, Uhr des Logs, Abstand = Ende der Client-Erzeugung minus Busplatz des Hosts:

  | Lauf | Karte | Busplatz Host | Client-Erzeugung fertig | Abstand | Client-Figur danach |
  |---|---|---|---|---|---|
  | ohne Verzögerung | `MGP_LabyrinthEvent` | 55,232 | 55,654 (609 ms) | 0,42 s | Modus 1, Z 90 am Bus |
  | 10 s Verzögerung | `MGP_LabyrinthEvent` | 28,879 | 39,153 (609 ms) | 10,27 s | Modus 1, Z 90 am Bus |
  | 10 s Verzögerung, nach Rundenende | `MGP_Forest_Campgrounds` | 00,642 | 07,785 (1719 ms) | 7,14 s | Modus 1, Z 100 am Bus |
  | 10 s Verzögerung, nach Rundenende | `MGP_LabyrinthEvent` | 48,797 | 59,104 (703 ms) | 10,31 s | Modus 1, Z 90 am Bus |
  | ohne Verzögerung, Endstand des PRs | `MGP_LabyrinthEvent` | 51,270 | 52,206 (875 ms) | 0,94 s | Modus 1, Z 90 am Bus |

  In Klammern steht die Dauer der Erzeugung. Im letzten Lauf meldet der Client `Busplatz des Clients, eigene Figur bei (-250, 9350, 50)`.
  Die Figur stand also beim Freiräumen schon am Bus. `objects.py` zeigte danach beide Figuren bei Host und Client auf `(-250, 9350, 90)`.
- [Belegt] Join, Travel und Rundenende liefen mit Messung und Testschalter in allen Läufen. Reconnect wurde nicht gefahren; die
  Messung liest nur, und ohne die Variable wartet der Client wie bisher.
- [Belegt, Randbefund] Einmal meldete der Host die Client-Figur kurz bei `(950, -1450, 100)`, bevor der Ort des Clients am Bus ankam.
  Woher dieser Ort kommt, ist nicht geklärt.

## Hypothesen

- [Widerlegt] Der Host setzt die Figuren an den Bus, bevor beim Client die Kartenerzeugung fertig ist, und der Client fällt darum. Der
  Host-Busplatz wirkt nicht auf die Client-Figur. Auch 10 s Vorsprung ließen den Client stehen.
- Die geplante Lösung (Figur bis `ClientGenerationActive()` falsch festhalten, dann den Host-Ort übernehmen) trifft damit nichts.
  Sie ist nicht gebaut. Der gemessene Abstand von 0,29 s live sagt über die Client-Figur nichts.
- [Vermutung, offen] Die umgekehrte Reihenfolge: Der Busplatz des Clients läuft, bevor seine eigene Figur da ist. Dann bliebe sie am
  Startpunkt ohne Boden und fiele. Das Labyrinth erzeugt am schnellsten (0,6 bis 0,9 s, normale Karte 1,7 s). Auf einem PC war die
  eigene Figur jedes Mal vor dem eigenen Busplatz da. Der Testschalter kann diese Reihenfolge nicht erzwingen, er verschiebt nur den
  Start. Eigene Aufgabe: [busplatz-vor-figur](busplatz-vor-figur.md).
- [Vermutung, offen] Der Boden unter dem Bus hat beim Busplatz des Clients noch keine Kollision. Die Figur steht dort 10 bis 280 ms im
  Modus 3, bevor sie geht. Nicht gezielt geprüft. Eigene Aufgabe: [busplatz-ohne-boden](busplatz-ohne-boden.md).

## Nächster Schritt

Im Live-Test auf zwei PCs (Bestätigungstest dieser Aufgabe) im Labyrinth das Client-Log lesen:
`Spawn-Messung: eigene Figur gefunden`, `Busplatz … (Client)` mit der Zeile `Busplatz des Clients, eigene Figur bei …` oder
`… ohne eigene Figur`, dann der Sprung an den Bus und `Client-Erzeugung fertig`.

- Fehlt die Figur am Busplatz oder steht sie dort noch am Startpunkt, gilt die erste offene Vermutung.
- Steht sie am Bus und fällt danach, gilt die zweite.

Die beiden Vermutungen bearbeiten die Aufgaben `busplatz-vor-figur` und `busplatz-ohne-boden`.

## Abnahme

Erfüllt durch Widerlegung: Die Vermutung ist mit Messung widerlegt, die Aufgabe schließt mit diesem Beleg. Offen bleibt nur der
Bestätigungstest auf zwei PCs mit den Messzeilen oben.
