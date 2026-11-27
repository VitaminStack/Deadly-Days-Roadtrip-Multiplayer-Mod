# busplatz-ohne-boden: Client fällt am Bus durch die Karte, weil der Boden dort noch keine Kollision hat

Stand 27.11.2026, ein PC mit zwei Instanzen, Build `26786272`. Die Ursache des Live-Falls bleibt eine Vermutung: Auf einem PC hatte
der Boden am Bus beim Busplatz des Clients in jeder Zone schon Kollision. Belegt ist der Mechanismus. Kommt der Boden spät, fällt die
Figur durch und bleibt unter der Karte, auch wenn die Kollision danach kommt. Dagegen hält die Mod die eigene Figur des Clients am
Busort, bis dort Boden ist, höchstens 5 s. Lokal, ohne Nachricht.
Herkunft: [spawn-nach-kartenaufbau](spawn-nach-kartenaufbau.md). Dort wurde die Vermutung „Host setzt die Figuren zu früh an den Bus“ widerlegt.

## Reproduktion

Live, zwei PCs: Im Labyrinth-Event fällt der Client nach dem Zonenstart durch die Karte, sein Bild ist schwarz bis auf die eigene Figur.
Auf einem PC mit zwei Instanzen ist das ohne Testschalter nicht nachgestellt.

Nachstellen mit spätem Boden: Wie in [spawn-nach-kartenaufbau](spawn-nach-kartenaufbau.md#reproduktion), dazu
`DDCOOP_TEST_CLIENT_FLOOR_DELAY_MS=1500` ([Testen](../TESTING.md#gezielt-in-eine-karte)). Mit `DDCOOP_TEST_CLIENT_BUS_HOLD_OFF=1`
hält die Mod die Figur nicht, dann fällt sie durch.

## Belege

- [Belegt] Die Mod prüft den Boden am Busplatz des Clients mit `CharacterMovementComponent.K2_ComputeFloorDist`. Das ist der Sweep
  der Bewegung mit Kanal und Antworten der Kapsel, von 150 über dem Busort bis 3000 darunter. Die Zeilen `Busboden: …` stehen im
  Client-Log, beim Busplatz und nach der Freiräumung, dann je Frame bis die Figur 3 Frames auf Boden geht, höchstens 3 s. Nach den
  ersten 10 Frames wird nur bei Änderung von Modus oder Boden geloggt. Felder kommen über `Reflection::Property` und die Parameter der
  UFunction (`Reflection::Parameter`).
- [Belegt] Ohne Testschalter hatte der Boden in jeder gemessenen Zone schon beim Busplatz Kollision:

  | Lauf | Karte | Boden am Busplatz | Figur beim Busplatz | danach |
  |---|---|---|---|---|
  | Zone 1 | `MGP_LabyrinthEvent` | Z 0, `Road_tile_24x24_base27` von `BP_Labyrinth_Entrance_C` | Modus 3, Z 50, Tempo Z −254 | Frame 2 (+125 ms) Modus 1, Z 90 |
  | Zone 2 | `MGP_Gated_Toscana` | Z 10, `Road_tile_24x24_pedest` von `BP_Street_Narrow_2_Curve_C` | Modus 1, Z 50 | Frame 3 (+188 ms) Z 100 |
  | Zone 3 | `MGP_LabyrinthEvent` | Z 0, wie Zone 1 | Modus 3, Z 50 | Frame 2 (+109 ms) Modus 1, Z 90 |
  | Zweite Sitzung, Zone 1 und 3 | `MGP_LabyrinthEvent` | Z 0, wie Zone 1 | Modus 3, Z 50 | Modus 1, Z 90 |
  | Zweite Sitzung, Zone 2 | normale Karte | Boden | Modus 3, Z 50 | Modus 1 |
  | Wiederbeitritt kurz nach dem Zonenstart, zweimal | `MGP_LabyrinthEvent` | Z 0, wie Zone 1 | Modus 0, Z 50 | Frame 4 (+156 ms) Modus 1, Z 90 |

  Die Kapsel hat die halbe Höhe 88. Bei Z 50 steckt sie 38 im Boden, die Bewegung schiebt sie im nächsten Frame auf Z 90.
- [Belegt, Testschalter] Kommt der Boden 1,5 s spät (Kollision der Kachel unter dem Bus aus und wieder an) und hält die Mod nicht,
  fällt die Figur: nach 281 ms Z −3, nach 1,5 s Z −1137. Als die Kollision zurückkam, war sie schon darunter und fiel weiter, nach 3 s
  Z −4632. Schon wenige hundert Millisekunden ohne Boden reichen also.
- [Belegt, Testschalter] Mit Halten (Standard) stand die Figur in 4 von 4 Zonenstarts mit 1,5 s spätem Boden danach auf dem Boden:
  3-mal `MGP_LabyrinthEvent` (Z 90), einmal `MGP_Gated_Modern` (Z 100, dort lagen zwei Komponenten unter dem Bus). Im Log:
  `kein Boden am Bus …, eigene Figur wird gehalten`, Modus 0 bei Z 50, dann `Boden am Bus nach 1500 ms da, eigene Figur losgelassen`,
  im nächsten Frame Modus 1. Der Host sah die Client-Figur dabei in Modus 0 am Bus und danach in Modus 1 bei Z 90.
- [Belegt] Join, Travel und Rundenende liefen in allen Läufen. Reconnect (`Test-Rejoin.ps1`) lief ohne Testschalter zweimal
  (Tabelle oben) und mit spätem Boden zweimal im Labyrinth, jeweils Karte übernommen und Inventar gleich. Bei den zweien mit spätem
  Boden fiel die Figur aber am Startpunkt, siehe den nächsten Punkt. `Test-Rejoin.ps1` meldete einen der Läufe ohne Schalter als
  Fehler, weil es nach `übernimmt \d` sucht und den negativen Seed übersah.
- [Belegt, Randbefund für `busplatz-vor-figur`] Bei diesen beiden Wiederbeitritten war die Erzeugung des Clients fertig (Profil schon
  geladen, `übernimmt … nach 0 ms`), bevor seine eigene Figur da war. Ein Busplatz des Clients kam dann gar nicht, auch keine Zeile
  `Busplatz … (Client)`. Die Figur fiel am Startpunkt `(3467, -399)` endlos (nach 17 s Z −56408). Das Halten greift dort nicht, weil
  es erst am Busplatz beginnt. Der Code dieses Falls ist unverändert; auf main nicht gezielt nachgestellt.
- [Belegt, ungeklärt] Einmal blieb ein Wiederbeitritt hängen, nach 30 min in einer Labyrinth-Zone: Der Client stand in der
  Missionswelt mit 0 Spielern, ohne GameState und ohne Kartenerzeugung. Die Bodenprüfung lief dabei nicht, denn ohne Erzeugung gibt es
  keinen Busplatz. Ein Wiederbeitritt mit main nach demselben Ablauf (zwei Rundenenden) lief einmal durch. Ursache offen.

## Hypothesen

- [Vermutung, offen] Live hat der Boden unter dem Bus beim Busplatz des Clients noch keine Kollision, etwa weil Kacheln langsamer
  laden. Auf einem PC nie gesehen. Entscheidbar im Live-Log: `Busboden: Busplatz, … kein Boden` oder `… wird gehalten`.
- [Vermutung, offen] Die Figur startet beim Busplatz 38 tief im Boden (Z 50 bei halber Höhe 88). Die Bewegung schiebt sie heraus,
  solange der Boden fest ist. Ob das live je scheitert, ist nicht gemessen.

## Nächster Schritt

Bestätigungstest auf zwei PCs: im Labyrinth-Event die Zeilen `Busboden: …` im Client-Log lesen. Steht dort `kein Boden am Bus` und
danach `Boden am Bus nach … ms da`, ist die Vermutung live belegt und das Halten hat gegriffen. Steht dort Boden und die Figur fällt
trotzdem, ist sie widerlegt.

## Abnahme

Erfüllt auf einem PC mit Testschalter: Die Client-Figur steht nach jedem Zonenstart auf dem Boden, auch wenn der Boden am Bus beim
Busplatz des Clients 1,5 s spät kommt. Join, Reconnect und Travel liefen. Beim Reconnect fiel die Figur in 2 von 4 Läufen am
Startpunkt, ohne Busplatz des Clients; das ist der Fall von [busplatz-vor-figur](busplatz-vor-figur.md), nicht dieser. Offen ist der
Bestätigungstest auf zwei PCs.
