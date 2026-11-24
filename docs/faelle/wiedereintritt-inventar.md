# wiedereintritt-inventar: Client verliert beim Wiederbeitritt sein Inventar

## Reproduktion

Tritt der Client während eines Runs wieder bei, ob ohne Neustart im selben Prozess oder per Hot Join nach einem Neustart, beginnt er mit dem
Startinventar. Auf einem PC: Sitzung mit `Start-Session.ps1 -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map -Godmode -Brief`, dann
`tools/spike/Test-Rejoin.ps1`; vorher braucht der Client ein Inventar, das von der Startausrüstung abweicht.

## Belege

- [Belegt, Spiel] Am 24.11.2026 auf einem PC mit zwei Instanzen (`wiedereintritt-karte`): Der Host legt bei jedem Wiederbeitritt einen neuen
  PlayerState an, im Inventarvergleich (`inventory.py`) wechselt der Client von 258 zu 259, 260 und so weiter. Im Test hatte der Client nur die
  Startausrüstung, darum waren beide Seiten gleich; ein erspieltes Inventar ist damit nicht geprüft.
- [Belegt] Live-Test 17.11.2026: Nach dem Wiederbeitritt per Einladung begann der Client mit Startinventar (2 Items, 4 Felder statt 46 und 54)
  ([Live-Test 17.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-17112026-livetest-1711)). Der eigene Run-Stand des Clients ist nicht
  beteiligt, das Inventar kommt vom frischen PlayerState des Hosts.
- [Belegt] Live-Test 24.11.2026: Hot Join nach Absturz und Neustart des Clients, Karte und Figuren da
  ([Live-Test 24.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)). Vermutung: dieselbe Ursache für das Inventar.
- [Belegt, Code] Im Code gibt es keine Wiederherstellung. Der Run-Stand des Hosts enthält die Inventare beider Spieler
  (`Inventory_Player0`, `Inventory_Player1`).

## Hypothesen

- [Vermutung] Der Host merkt sich beim Verlassen das Inventar des Clients (Items, Felder, Rüstung, Schrott) und legt es beim nächsten Beitritt
  desselben Kontos in den neuen PlayerState, über denselben Weg, mit dem er heute Inventar-Edits des Clients übernimmt. Der Host bleibt
  einzige Autorität.

## Nächster Schritt

Prüfliste in [ARCHITECTURE.md](../ARCHITECTURE.md#verbindliches-architekturprinzip-lokal-zuerst-minimal-synchronisieren) beantworten, dann
Speichern und Zurückgeben als Policy mit Unit-Test.

## Abnahme

Der Host hebt das Inventar eines Clients, der die Sitzung verlässt, samt Rüstung und Schrott auf und gibt es ihm beim Wiederbeitritt oder Hot Join
zurück. Mit Unit-Test, belegt auf einem PC mit zwei Instanzen.
