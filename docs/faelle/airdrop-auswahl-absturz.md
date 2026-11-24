# airdrop-auswahl-absturz: Client stürzt ab, wenn sich eine Airdrop-Auswahl ohne Inventar öffnet

## Reproduktion

Live-Test am 24.11.2026, zwei PCs, Build `26786272`: Beim Client öffnet sich eine Airdrop-Auswahl ohne Inventar am Panel.
Das war die erste Auswahl auf der Karte nach dem zweiten Händler-Halt. Auf einem PC mit zwei Instanzen ist der Fall noch nicht nachgestellt.

## Belege

- [Belegt] Client-Log: Der Client beginnt die lokale Bearbeitung mit drei Belohnungen („UpdateIsReward lokal“ je Belohnung). Im selben Takt
  zweimal „SetupInventory ohne Inventar ausgelassen“ am selben Panel, Aufrufer `0x1527C28` (`ProcessEvent`, also ein Blueprint)
  und `0x5A7007C`. 0,3 s später `EXCEPTION_ACCESS_VIOLATION` beim Lesen von `0x108` bei `0x596BF0F`, Aufrufliste `0x59660D5`,
  `0x5A7008C`, weiter unten `0x1527C28` ([Live-Test 24.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)).
- [Belegt] `0x5A7007C` liegt in `InventoryPanel::ForceUpdateFromComponent` (`0x5A70050`, Exec-Thunk `0x570E5B0`, Name aus der
  nativen Registrierungstabelle der Exe). Die Funktion ruft `SetupInventory(Panel)`, lädt danach selbst `[Panel+0x370]` und gibt es
  an `0x59660C0` weiter. Diese addiert `0x100` und liest in `0x596BE90` `[+8]`. Mit Inventar null ist das genau das Lesen bei `0x108`.
- [Belegt] Der Schutz aus `airdrop-dodge-absturz` (`SetupInventoryHook` in `src/Game/RewardPatches.cpp`) ließ nur den inneren Aufruf aus.
  Der Aufrufer las danach selbst das fehlende Inventar.
- [Belegt] Die übrigen nativen Aufrufer von `SetupInventory` lesen `+0x370` danach nicht ungeprüft (Disassembly, Build `26786272`).
- [Belegt] Weder der Händler-Halt noch die erste Auswahl einer Karte reichen allein: Nach dem ersten Händler-Halt lief die erste
  Auswahl der nächsten Karte ohne Ausfall. Vorher kam im ganzen Client-Log kein „ohne Inventar“.

## Fix

`ForceUpdateFromComponent` bekommt einen Hook am Anfang. Fehlt Panel oder Inventar, entfällt der ganze Aufruf (`PanelMayRebuild` in
`src/Game/PatchPolicy.h`); dasselbe gilt wie bisher für `SetupInventory`. Ein späterer Aufruf mit Inventar baut das Panel auf.
Das Log nennt je Panel einmal Panel, Objektname, Klasse, offen oder geschlossen und den Aufrufer. Es meldet
„baut Panel … jetzt mit Inventar auf“, sobald dasselbe Panel später durchkommt (`PanelsWithoutInventory`). Tests in
`tests/GameTests/PatchTests.cpp`.

## Hypothesen

- Offen: warum das Panel ohne Inventar kam. **[Vermutung]** Ein Blueprint-Ereignis, das die lokale Bearbeitung auslöst
  (`UpdateIsReward`), ruft `SetupInventory` und `ForceUpdateFromComponent` an einem Inventar-Panel, dessen Inventar noch nicht
  gesetzt oder schon gelöscht ist, etwa einem Panel aus dem Händler-Halt. Die neue Logzeile mit Objektname und Zustand soll das klären.

## Nächster Schritt

Auf einem PC mit zwei Instanzen nachstellen: Händler-Event (`DDCOOP_TEST_MAP_PROFILE` mit `MGP_MerchantEvent`, `..._EVERY=2`), beim
Client etwas kaufen, weiterreisen und auf der nächsten Karte die erste Airdrop-Auswahl beim Client öffnen. Erwartet: kein Absturz;
erscheint „ohne Inventar ausgelassen“, dann Objektname und Zustand festhalten und prüfen, ob die Auswahl normal aufgebaut wird.

## Abnahme

Eine Airdrop-Auswahl ohne Inventar stürzt beim Client nicht mehr ab, und die Auswahl wird danach mit Inventar normal aufgebaut.
Belegt auf einem PC mit zwei Instanzen; dabei geklärt, warum das Panel ohne Inventar kam.
