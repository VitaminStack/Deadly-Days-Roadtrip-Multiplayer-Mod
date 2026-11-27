# busplatz-vor-figur: Client fällt durch die Karte, weil seine Erzeugung vor seiner eigenen Figur läuft

Stand 27.11.2026, ein PC mit zwei Instanzen, Build `26786272`. Beim Wiederbeitritt ist die Vermutung belegt und behoben. Fehlt die
eigene Figur beim Start der Kartenerzeugung des Clients, fällt sein ganzer Busplatz aus: Figur und Bus bleiben fern vom Busort, und
die Freiräumung fehlt. Die Figur fällt am Startpunkt endlos. Die Mod lässt den Client darum mit seiner Erzeugung auf die eigene Figur
warten, höchstens 10 s. Lokal, ohne Nachricht, keine neue `ModVersion`. Beim Zonenstart über zwei PCs bleibt die Ursache des Live-Falls
eine Vermutung; das Warten deckt ihn mit ab.
Herkunft: [spawn-nach-kartenaufbau](spawn-nach-kartenaufbau.md). Dort wurde die Vermutung „Host setzt die Figuren zu früh an den Bus“ widerlegt.

## Reproduktion

Live, zwei PCs: Im Labyrinth-Event fällt der Client nach dem Zonenstart durch die Karte, sein Bild ist schwarz bis auf die eigene Figur.

Auf einem PC mit zwei Instanzen, beim Wiederbeitritt: Sitzung wie in [spawn-nach-kartenaufbau](spawn-nach-kartenaufbau.md#reproduktion),
Zone betreten, mindestens 15 s warten, dann `Test-Rejoin.ps1 -Rounds 5`. Ohne die Behebung mit
`DDCOOP_TEST_CLIENT_FIGURE_WAIT_OFF=1` ([Testen](../TESTING.md#gezielt-in-eine-karte)).

## Belege

- [Belegt] Den Busplatz seiner Erzeugung setzt der Client nur mit seiner eigenen Figur. Die Schonliste der Freiräumung enthält dann
  genau sie, im Log `Busort: Freiräumung des Clients, 1 geschont: BP_DD_Character_C (…)`. Dabei springen Figur und Bus an den Busort.
- [Belegt] Ohne eigene Figur beim Start läuft die Freiräumung des Clients gar nicht, auch keine Zeile `Busplatz … (Client)`. Der Bus
  steht dann woanders, im Labyrinth bei `(2350, 450, 0)` statt `(0, 9600, 0)`, und dort ohne Boden.
- [Belegt] Die Figur steht am Busplatz um `(−250, −250, 50)` neben dem Bus. Gemessen im Labyrinth (Bus `(0, 9600, 0)`), in
  `MGP_City_LittleItaly` (Bus `(19400, 9800, 0)`) und `MGP_Forest_Campgrounds` (Bus `(5000, 29000, 0)`); wo gemessen, mit Gierwinkel 60.
- [Belegt] Ein „gemerkter Busort“, an den die Mod die späte Figur setzen könnte, gibt es im gemessenen Fall nicht. Ohne Busplatz ist der
  Bus nicht versetzt, und eine eigene Figur, deren Ort man nehmen könnte, gab es nicht. Ein erster Versuch, die Figur nach dem Ende der
  Erzeugung an den Bus zu setzen, setzte sie im echten Fall an den unversetzten Bus. Dort fiel sie nach 5 s Halten. Er ist verworfen.
- [Belegt] Der Startpunkt `(3467, -399)` hat im Labyrinth keinen Boden, auf `MGP_Forest_Campgrounds` schon (Z 100). Ohne Busplatz
  fiele die Figur also nicht überall. Fern vom Bus bliebe sie trotzdem.
- [Belegt] Beim Wiederbeitritt sind die Werte des Hosts schon vor der eigenen Figur da („übernimmt … nach 0 ms“). Die Figur steht
  500 bis 860 ms nach dem Aufruf von `StartMapGeneration` in der PlayerArray. Ohne Warten kam sie in 7 von 13 Wiederbeitritten zu spät:
  1 von 2 mit den Messzeilen allein, 0 von 3 und 1 von 3 mit dem verworfenen Versuch, 5 von 5 im Gegentest.
- [Belegt] Messung auf diesem Build, Gegentest mit dem Aus-Schalter:

  | Lauf | Karte | Wiederbeitritte | Wartezeile | Busplatz des Clients mit Figur | Figur danach |
  |---|---|---|---|---|---|
  | mit Warten | `MGP_LabyrinthEvent` | 5 | 5, gewartet 532 bis 859 ms | 5 | Modus 1, Z 90 am Bus |
  | mit Warten | `MGP_Forest_Campgrounds` | 5 | 5, gewartet 656 bis 766 ms | 5 | Modus 1, Z 100 am Bus |
  | `…_FIGURE_WAIT_OFF=1` | `MGP_LabyrinthEvent` | 5 | – | 0, `Client-Erzeugung fertig ohne Busplatz des Clients` | fällt am Startpunkt, nach 7 s unter Z −16 000 |

- [Belegt] Beim Zonenstart nach einer Reise war die eigene Figur in jeder gemessenen Zone vor dem Start der Erzeugung da (wartet,
  0 bis 2 s auf die Host-Karte). Die Wartezeile kam dort nie. Join, Reise, Rundenende und Reconnect liefen in allen Läufen, beim Reconnect
  waren Karte übernommen und Inventar gleich.

## Hypothesen

- [Vermutung, offen] Live kommt beim Zonenstart über das Relay die eigene Figur nach den Werten des Hosts an, wie beim Wiederbeitritt.
  Ohne Warten fiele dann der Busplatz des Clients aus, und die Figur fiele im Labyrinth durch die Karte. Das Warten deckt das ab.
  Entscheidbar im Live-Log: `Karte: Client wartet auf seine eigene Figur` beim Zonenstart.
- [Offen] Kommt die eigene Figur in 10 s nicht, erzeugt der Client ohne sie, im Log `Karte: nach … ms keine eigene Figur …`. Dann
  fehlt sein Busplatz weiter. Auf einem PC nie gesehen.

## Nächster Schritt

Bestätigungstest auf zwei PCs: im Labyrinth-Event einen Zonenstart und einen Wiederbeitritt fahren und das Client-Log lesen. Steht beim
Zonenstart `Karte: Client wartet auf seine eigene Figur`, ist die Vermutung live belegt. Nach jeder Erzeugung muss
`Busplatz … (Client)` mit `Busplatz des Clients, eigene Figur bei …` kommen und die Figur am Bus stehen.

## Abnahme

Erfüllt auf einem PC: Die eigene Figur des Clients kommt nach jedem Zonenstart und Wiederbeitritt an den Bus und steht auf dem Boden,
auch wenn sie erst nach den Werten des Hosts eintrifft (10 von 10 Wiederbeitritten, Gegentest 5 von 5 gefallen). Join, Reconnect und
Travel liefen unverändert. Offen ist der Bestätigungstest auf zwei PCs.
