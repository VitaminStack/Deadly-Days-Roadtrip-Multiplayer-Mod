# airdrop-auswahl-absturz: Client stürzt ab, wenn sich eine Airdrop-Auswahl ohne Inventar öffnet

## Reproduktion

Live-Test am 24.11.2026, zwei PCs, Build `26786272`: Beim Client öffnet sich eine Airdrop-Auswahl ohne Inventar am Panel.
Auf einem PC mit zwei Instanzen ist der Fall noch nicht nachgestellt.

## Belege

- [Belegt] Client-Log: zweimal „SetupInventory ohne Inventar ausgelassen“ (Aufrufer `0x1527C28`, `0x5A7007C`).
  0,3 s später `EXCEPTION_ACCESS_VIOLATION` beim Lesen von `0x108` bei `0x5A7008C`, direkt hinter dem ausgelassenen Aufruf ([Live-Test 24.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)).
- [Belegt] Der Schutz aus `airdrop-dodge-absturz` (`SetupInventoryHook` in `src/Game/RewardPatches.cpp`) lässt nur diesen einen Aufruf aus.
  Der Aufrufer liest danach selbst das fehlende Inventar.

## Hypothesen

- Offen: warum das Panel ohne Inventar kam. Die Abnahme verlangt, das zu klären.

## Nächster Schritt

Auf einem PC mit zwei Instanzen nachstellen und klären, wer das Panel ohne Inventar öffnet.

## Abnahme

Eine Airdrop-Auswahl ohne Inventar stürzt beim Client nicht mehr ab, und die Auswahl wird danach mit Inventar normal aufgebaut.
Belegt auf einem PC mit zwei Instanzen; dabei geklärt, warum das Panel ohne Inventar kam.
