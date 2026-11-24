# wiedereintritt-karte: Client baut beim Wiederbeitritt keine Karte und fällt durch

## Reproduktion

Live-Test am 24.11.2026, zwei PCs, Build `26786272`: Der Client wird getrennt und tritt im selben Spielprozess der laufenden Mission wieder bei.
Er baut keine Karte und fällt durch, dreimal gleich. Ein Neustart des Spiels umgeht den Fehler.

Auf einem PC ohne echte Eingaben: Sitzung mit `Start-Session.ps1 -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map -Godmode -Brief`, dann
`tools/spike/Test-Rejoin.ps1 [-Rounds 3]`. Das Skript lässt den Client über den Testkanal die Lobby verlassen, wartet auf das Hauptmenü, tritt
im selben Prozess wieder bei und wartet auf die Kartenzeile des Clients. Vorher und nachher zeigt es Zustand, Inventarvergleich und ein Bild
des Client-Fensters (PrintWindow).

## Belege

- [Belegt] „Werte vom Host“ kommen an, „Client übernimmt“ fehlt ([Live-Test 24.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)).
- [Belegt, Code] `FreshValues` in `src/Game/MapPatches.cpp` nahm die Werte nur bei neuer Id oder bei einem Neuversuch in derselben Welt
  mit demselben Generator. Der Host schickt nach dem Wiederbeitritt dieselbe Id seiner laufenden Karte, die beim Client schon als `UsedId`
  verbraucht war, die Welt ist neu. `UsedId` blieb beim Verbindungsende stehen. `MapSync.cpp` leert beim Peer-Wechsel nur die empfangenen
  Werte (`SetHostMap(nullopt)`), was die Meldung „Werte vom Host“ beim Wiederbeitritt erklärt.
- [Belegt, Spiel] Nachgestellt am 24.11.2026 auf einem PC mit zwei Instanzen, Stand vor dem Fix: Client-Log „Karte: Client wartet auf die
  Host-Karte“, 0,3 s später „Werte vom Host“, dann 60 s nichts und „nach 60 s keine Werte vom Host, Client erzeugt eine eigene Karte“.
- [Belegt, Spiel] Mit dem Fix in drei Sitzungen 12 von 12 Wiederbeitritten (die letzten drei mit der Ausfallregel) ohne Neustart: „Karte: Client übernimmt“ nach 0 bis 422 ms, Client
  wieder in `DDDynamic_Map` mit 2 Spielern, im Bild steht die Figur auf der Karte des Hosts. Kein Absturz.
- [Belegt, Spiel] Level: Host per `DD_CheatAddExperience` auf Level 8. Die Level-Anzeige des Clients zeigt vor und nach 5 Wiederbeitritten 8
  und hält es über 30 s. Einmal (Sitzung ohne Godmode, Figuren kurz vor dem Tod) zeigte sie nach dem Wiederbeitritt 1; nicht wieder
  aufgetreten. Vermutung: der schon bekannte Fall, dass das Spiel zwischendurch das lokale Level ins HUD schreibt (Level-Anzeige im
  [Live-Test 24.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)).
- [Belegt, Spiel] Inventar: Der Host legt bei jedem Wiederbeitritt einen neuen PlayerState an (im Inventarvergleich 258, 259, 260 …). Im Test
  hatte der Client nur die Startausrüstung (Pistole, 8 Felder), vor und nach dem Wiederbeitritt bei beiden gleich. Ein erspieltes Inventar
  ginge wie am 17.11. verloren ([Live-Test 17.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-17112026-livetest-1711)); dafür
  gibt es im Code keine Wiederherstellung. Das ist eine andere Ursache als die Karte.

## Fix

`ClientMapLedger` in `src/Game/MapPolicy.h` hält beim Client die verbaute Karten-Id und das nachgezogene Rundenende. Beim Peer-Wechsel ruft
`MapSync.cpp` `Game::Map::NoteConnectionChanged()`. Entsteht die Verbindung, während der Client nicht in der Welt der verbauten Karte steht
(Hauptmenü, Laden), gilt dieselbe Id einmal wieder als frisch. Ohne Trennung, und bei einem kurzen Ausfall des Kontrollkanals in der Mission,
bleibt eine verbrauchte Id verbraucht, damit der Client nach einer Reise nicht die alte Karte baut. Das Rundenende gilt je Übernahme: Nach einem Wiederbeitritt mit neu
gebauter Karte öffnet dasselbe Ende des Hosts die Rundenübersicht wieder; ein Wechsel ohne neue Karte öffnet sie nicht doppelt.
Tests: `tests/GameTests/PatchTests.cpp`, „Wiederbeitritt übernimmt dieselbe Host-Karte noch einmal“ und „Rundenende gilt je verbauter Karte“.

## Offen

- Inventar des Clients nach dem Wiederbeitritt wiederherstellen: eigene Aufgabe [wiedereintritt-inventar](wiedereintritt-inventar.md).
- Wiederbeitritt während der Rundenübersicht des Hosts ist nur im Unit-Test geprüft, nicht im Spiel.

## Abnahme

Der Client kann ohne Neustart derselben laufenden Mission wieder beitreten, baut die Karte des Hosts auf und hat das Level wie vor der Trennung.
Das Inventar ist [wiedereintritt-inventar](wiedereintritt-inventar.md) (Nutzerentscheidung vom 24.11.2026).
Mit Unit-Test, belegt auf einem PC mit zwei Instanzen.
