# busplatz-ohne-boden: Client fällt am Bus durch die Karte, weil der Boden dort noch keine Kollision hat

Die Ursache ist eine Vermutung. Die Aufgabe prüft sie selbst und schließt auch ab, wenn die Messung sie widerlegt.
Herkunft: [spawn-nach-kartenaufbau](spawn-nach-kartenaufbau.md). Dort wurde die Vermutung „Host setzt die Figuren zu früh an den Bus“ widerlegt.

## Reproduktion

Live, zwei PCs: Im Labyrinth-Event fällt der Client nach dem Zonenstart durch die Karte, sein Bild ist schwarz bis auf die eigene Figur.
Auf einem PC mit zwei Instanzen ist das nicht nachgestellt. Rezept und Testschalter stehen in der Fallakte von `spawn-nach-kartenaufbau`.

## Belege

- [Belegt, ein PC] Der Client setzt seine eigene Figur in seiner eigenen Kartenerzeugung an den Bus, im Log `Busplatz … (Client)`.
  Das geschieht vor `Spawn-Messung: Client-Erzeugung fertig`, also solange noch Generatoren laufen.
- [Belegt, ein PC] Nach dem Sprung an den Bus steht die Figur bei Z 50 noch im Modus 3. Erst 10 bis 280 ms später geht sie im
  Modus 1 auf Z 90 (Labyrinth) oder Z 100 (normale Karte). Das war auch so, als sie vorher 10 s mit voller Fallgeschwindigkeit gefallen
  war. In 5 von 5 Läufen fiel sie nicht durch.

## Hypothesen

- [Vermutung] Live hat der Boden unter dem Bus beim Busplatz des Clients noch keine Kollision, etwa weil Kacheln langsamer laden oder
  ein späterer Generator sie erst setzt. Die Figur fällt dann in diesen Millisekunden durch. Prüfidee: Im Client-Log eines Live-Laufs
  steht `Busplatz des Clients, eigene Figur bei` mit dem Ort des Busses, und danach fällt Z weiter, statt in Modus 1 zu wechseln.
- Lösungsidee, nur bei Bestätigung: Die eigene Figur des Clients nach seinem Busplatz bis zum Ende seiner Erzeugung lokal halten,
  etwa mit Bewegungsmodus `MOVE_None`, und dann einmal neu an den Bus setzen. Lokal, ohne neue Nachricht. Mit der Dodge-Reparatur
  in `src/Game/MovementPatches.cpp` abgleichen.

## Nächster Schritt

Messen, ob der Boden am Bus beim Busplatz des Clients schon Kollision hat, etwa per Linienprüfung nach unten am Busort in den ersten
Frames nach `Busplatz … (Client)`. Dann prüfen, ob sich ein späterer Boden auf einem PC erzwingen lässt. Sonst die Zeilen
`Spawn-Messung` aus einem Live-Lauf mit Labyrinth auswerten. Das ist der Bestätigungstest von `spawn-nach-kartenaufbau`.

## Abnahme

Die Client-Figur steht nach jedem Zonenstart auf dem Boden, auch wenn der Boden am Bus beim Busplatz des Clients spät kommt. Join,
Reconnect und Travel laufen unverändert. Oder: Die Vermutung ist mit Messung widerlegt.
