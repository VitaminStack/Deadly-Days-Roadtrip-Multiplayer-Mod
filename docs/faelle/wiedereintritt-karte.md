# wiedereintritt-karte: Client baut beim Wiederbeitritt keine Karte und fällt durch

## Reproduktion

Live-Test am 24.11.2026, zwei PCs, Build `26786272`: Der Client wird getrennt und tritt im selben Spielprozess der laufenden Mission wieder bei.
Er baut keine Karte und fällt durch, dreimal gleich. Ein Neustart des Spiels umgeht den Fehler.

## Belege

- [Belegt] „Werte vom Host“ kommen an, „Client übernimmt“ fehlt ([Live-Test 24.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)).
- [Belegt] `FreshValues` in `src/Game/MapPatches.cpp` nimmt die Werte nur bei neuer Id oder gleicher Welt.
  Die Id der laufenden Host-Karte ist schon als `UsedId` verbraucht, die Welt ist neu.
  `UsedId` wird beim Verbindungsende nicht zurückgesetzt.
- [Belegt] Aus der früheren Aufgabe `reconnect`: Nach einem Wiederbeitritt in den laufenden Run begann der Client mit Startinventar und Level 1 ([Live-Test 17.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-17112026-livetest-1711)). Hinweis im Spiel: `OnCoopReconnectRequiredChanged`.

## Hypothesen

- [Vermutung] `UsedId` beim Verbindungsende zurücksetzen behebt den Fall. Prüfidee: Unit-Test für `FreshValues` nach Trennung.

## Nächster Schritt

Unit-Test schreiben, der den Wiederbeitritt mit verbrauchter Id und neuer Welt abbildet, dann den Zustand beim Verbindungsende zurücksetzen.

## Abnahme

Der Client kann ohne Neustart derselben laufenden Mission wieder beitreten, baut die Karte des Hosts auf und hat Inventar und Level wie vor der Trennung.
Mit Unit-Test, belegt auf einem PC mit zwei Instanzen.
