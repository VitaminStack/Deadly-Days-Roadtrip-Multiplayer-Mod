# busplatz-vor-figur: Client fällt durch die Karte, weil sein Busplatz vor seiner eigenen Figur kommt

Die Ursache ist eine Vermutung. Die Aufgabe prüft sie selbst und schließt auch ab, wenn die Messung sie widerlegt.
Herkunft: [spawn-nach-kartenaufbau](spawn-nach-kartenaufbau.md). Dort wurde die Vermutung „Host setzt die Figuren zu früh an den Bus“ widerlegt.

## Reproduktion

Live, zwei PCs: Im Labyrinth-Event fällt der Client nach dem Zonenstart durch die Karte, sein Bild ist schwarz bis auf die eigene Figur.
Auf einem PC mit zwei Instanzen ist das nicht nachgestellt. Rezept und Testschalter stehen in der Fallakte von `spawn-nach-kartenaufbau`.

## Belege

- [Belegt, ein PC] An den Bus setzt der Client seine eigene Figur selbst, in seiner eigenen Kartenerzeugung. Im Log heißt die Zeile
  `Busplatz … (Client)`, kurz vor `Spawn-Messung: Client-Erzeugung fertig`. Der Busplatz des Hosts bewegt sie nicht.
- [Belegt, ein PC] Der Startpunkt `(3467, -399, ~950)` hat im Labyrinth und auf normalen Karten keinen Boden. Bis zum eigenen Busplatz
  fällt die Figur dort im Modus 3.
- [Belegt, ein PC] Die eigene Figur war in allen 5 Läufen vor dem eigenen Busplatz da. Wo gemessen, kam sie 70 bis 100 ms nach
  `Karte: Client wartet auf die Host-Karte`. Das Labyrinth erzeugt am schnellsten (0,6 bis 0,9 s, normale Karte 1,7 s).
- [Belegt] Die Mod schreibt `Spawn-Messung: Busplatz des Clients, eigene Figur bei (…)` oder `… ohne eigene Figur`. Beim Freiräumen
  steht die Figur schon am Bus, wenn der Generator sie hatte.

## Hypothesen

- [Vermutung] Live kommt die eigene Figur des Clients erst nach seinem Busplatz an, etwa durch Latenz über das Relay oder weil die
  Werte des Hosts schon vor der Welt da sind und die Erzeugung „nach 0 ms“ beginnt. Dann bleibt sie am Startpunkt und fällt.
  Prüfidee: Im Client-Log eines Live-Laufs steht dann `… ohne eigene Figur`, oder die Figur steht beim Busplatz noch am Startpunkt.
- Lösungsidee, nur bei Bestätigung: Beim Client die eigene Figur einmal an den Bus setzen, wenn sie nach dem eigenen Busplatz
  erscheint, lokal und ohne neue Nachricht. Den Ort könnte etwa der eigene Bus oder der replizierte Ort der Host-Figur liefern.

## Nächster Schritt

Die Reihenfolge auf einem PC erzwingen, etwa mit einer Testhilfe, die den Busplatz des Clients vor dem Eintreffen der eigenen Figur
laufen lässt. Gelingt das nicht, die Zeilen `Spawn-Messung` aus einem Live-Lauf mit Labyrinth auswerten. Das ist der
Bestätigungstest von `spawn-nach-kartenaufbau`.

## Abnahme

Die eigene Figur des Clients kommt nach jedem Zonenstart an den Bus und steht auf dem Boden, auch wenn sie erst nach seinem Busplatz
eintrifft. Join, Reconnect und Travel laufen unverändert. Oder: Die Vermutung ist mit Messung widerlegt.
