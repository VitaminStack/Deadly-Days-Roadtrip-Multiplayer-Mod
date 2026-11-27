# spawn-nach-kartenaufbau: Host setzt Figuren an den Bus, bevor der Client seine Karte gebaut hat

Die Ursache ist eine Vermutung. Die Aufgabe prüft sie selbst und schließt auch ab, wenn die Messung sie widerlegt.

## Reproduktion

Labyrinth-Event, zwei PCs: Der Client fällt nach dem Zonenstart durch die Karte.
Herkunft: [Labyrinth-Event beim Client](../archive/GAME_ANALYSIS.md#labyrinth-event-beim-client-labyrinth-event-client), Aufgabe `labyrinth-event-client`.

## Belege

- [Belegt] Im Labyrinth lagen live 0,29 s zwischen dem Senden der Kartenwerte und dem Busplatz, auf normalen Karten 1,2 bis 3,4 s.
- [Belegt] Der Host übernimmt die fallende Client-Position.

## Hypothesen

- [Vermutung] Beim Start einer Zone setzt der Host die Figuren an den Bus, bevor beim Client die eigene Kartenerzeugung fertig ist.
- Lösungsidee: Der Client hält seine eigene Figur lokal fest, bis `ClientGenerationActive()` (`src/Game/MapPatches.cpp`) falsch wird.
  Dann übernimmt er einmal den replizierten Host-Ort. Keine neue Nachricht, der Host bucht den Spawn weiter allein.
- Rückfall: eine Wartezeit beim Host vor dem Busplatz.

## Nächster Schritt

Zeitpunkte messen: Busplatz beim Host gegen das Ende der Kartenerzeugung beim Client (`ClientGenerationActive()`), zuerst auf einem PC mit zwei Instanzen im Labyrinth-Event.
[Vermutung] Ein PC stellt den Fall nach, wenn der Kartenaufbau des Clients künstlich verzögert wird; belegt ist das nicht.
Stellt ein PC den Fall nicht nach, dieselben Logzeilen in einem Run auf zwei PCs auswerten.

## Abnahme

Der Client steht im Labyrinth-Event und auf allen anderen Karten nach dem Zonenstart auf dem Boden, auch mit langsamem Kartenaufbau beim Client.
Join, Reconnect und Travel laufen unverändert.
Oder: Die Vermutung ist mit Messung widerlegt; dann schließt die Aufgabe mit diesem Beleg.
