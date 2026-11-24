# lobby-rauswurf: Host trennt einen gesunden Client wegen „nicht mehr in der Lobby“

## Reproduktion

Live-Test am 24.11.2026, zwei PCs, Build `26786272`: Der Host trennt den Client mitten im Run mit Grund 1005 („Mitspieler ist nicht mehr in der Lobby“).
Nicht gezielt nachstellbar, solange die Logs keine Lobby-Ereignisse zeigen.

## Belege

- [Belegt] Die Verbindung war gesund: Ping 64 ms, Abgleich 3 s vorher in Ordnung ([Live-Test 24.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)).
- [Belegt] Der Host trennt ein Konto, das länger als `RevokeAfterMs` (10 s) in seiner Steam-Lobby fehlt (`Session::SyncLobbyMembers` in `src/Net/Session.cpp`, aus `lobby-auth`).
- [Belegt] Der Client verließ die Lobby laut Log nicht. Keines der Logs zeigt Mitgliederzahl oder Lobby-Ereignis.

## Hypothesen

- [Vermutung] Die Mitgliederliste der Steam-Lobby ist beim Host kurz leer oder veraltet. Prüfidee: Mitgliederzahl und Lobby-Ereignisse loggen.

## Nächster Schritt

Logzeilen zu Mitgliederzahl und Lobby-Ereignissen ergänzen, dann im nächsten Live-Test belegen.

## Abnahme

Die Ursache ist belegt, und ein verbundener Client wird nicht mehr wegen einer kurz leeren Mitgliederliste getrennt.
Die Sperre gegen Fremde bleibt erhalten.
