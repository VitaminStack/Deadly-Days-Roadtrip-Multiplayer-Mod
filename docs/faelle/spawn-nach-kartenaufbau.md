# spawn-nach-kartenaufbau: Host setzt Figuren an den Bus, bevor der Client seine Karte gebaut hat

Diese Aufgabe gilt nur, wenn der Live-Test die Theorie bestätigt; sonst entfällt sie. Entscheidung: Team.

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

Theorie im nächsten Live-Test prüfen (siehe Bestätigungstest von `labyrinth-event-client`).

## Abnahme

Der Client steht im Labyrinth-Event und auf allen anderen Karten nach dem Zonenstart auf dem Boden, auch mit langsamem Kartenaufbau beim Client.
Join, Reconnect und Travel laufen unverändert.
