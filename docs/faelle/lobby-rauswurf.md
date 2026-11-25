# lobby-rauswurf: Host trennt einen gesunden Client wegen „nicht mehr in der Lobby“

## Reproduktion

Live-Test am 24.11.2026, zwei PCs, Build `26786272`: Der Host trennt den Client mitten im Run mit Grund 1005 („Mitspieler ist nicht mehr in der Lobby“).
Nicht gezielt nachstellbar. Auf einem PC laufen beide Instanzen unter einem Konto über 127.0.0.1, dort prüft der Host die Lobby nicht.

## Belege

- [Belegt] Die Verbindung war gesund: Ping 64 ms, Abgleich 3 s vorher in Ordnung ([Live-Test 24.11.](../archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-24112026-livetest-2411)).
- [Belegt] Der Client verließ die Lobby laut Log nicht. Keines der Logs zeigt Mitgliederzahl oder Lobby-Ereignis.
- [Belegt, Code vor dem Fix] Der Host trennte ein Konto, das länger als `RevokeAfterMs` (10 s) **am Stück** in der zuletzt gemeldeten Liste fehlte.
  Lücken unter 10 s trennten niemanden. Eine kurz leere Liste allein erklärt den Rauswurf also nicht. Die Liste kam je Frame vom Game Thread
  (`Session::SyncLobbyMembers`), die Frist lief auf dem Netzwerkthread (`Transport::CheckMembership`) ohne Rücksicht darauf, ob neue Listen kamen.
- [Belegt, Code vor dem Fix] Eine leere Liste oder eine ohne das eigene Konto galt wie eine gültige Liste ohne den Mitspieler.
  `SteamLobbyApi::Members` ist leer, wenn das eigene Konto nicht in der Lobby geführt wird, und überspringt SteamIDs 0 ohne Hinweis.
- [Belegt, Code vor dem Fix] Der Client prüfte seine eigene Mitgliedschaft nie und trat nach einem Verlust nicht neu bei.
- [Belegt, Host-Log 24.11.] In den 10 s vor der Trennung um 14:18:11,9 (Uhr des Hosts) schreibt der Game Thread durchgehend, Abstand höchstens
  rund 5 s, Abgleich „OK“ noch um 14:18:09. Ein stehender Game Thread scheidet für diesen Fall also aus. Das Client-Log zeigt bis zur Trennung
  nur normalen Verkehr.
- [Belegt, ein PC, ein Konto, 25.11.] Verlässt das gemeinsame Konto die Lobby, meldet echtes Steam dem Host eine leere Liste („Mitgliederliste
  leer statt gültig“), `GetNumLobbyMembers` blieb dabei bei 1. Die Mitgliederzahl muss also nicht sinken, wenn die Liste unbrauchbar wird.

## Hypothesen

Drei Wege führen im alten Code zum Rauswurf. Für den 24.11. bleiben (a) und (c), welcher es war, ist offen:

- **(a) [Vermutung] Host ohne gültige Liste ≥ 10 s.** Steam liefert dem Host eine leere Liste oder eine ohne sein eigenes Konto, etwa während
  seine Verbindung zu den Steam-Servern neu aufgebaut wird.
- **(b) Veraltete Liste, für den 24.11. ausgeschlossen.** Eine einzelne Liste ohne den Mitspieler, danach steht der Game Thread (Kartenwechsel,
  Laden), und die alte Liste altert auf dem Netzwerkthread in die 10 s hinein. Möglich im Code, beim Rauswurf am 24.11. lief der Game Thread aber.
- **(c) [Vermutung] Steam nimmt den Client aus der Lobby.** Etwa nach einem kurzen Abriss seiner Verbindung zu den Steam-Servern; P2P über das
  Relay bleibt gesund, die Lobby-Mitgliedschaft kommt aber nicht von selbst zurück.

## Fix (PR `task/lobby-rauswurf`)

- `src/Net/LobbyMembership.cpp`: `ClassifyLobbyList` (gültig nur mit eigenem Konto und vollständig) und `Judge`. Die Frist läuft nur über
  **neue, gültige** Listen, braucht mindestens 3 Fehltreffer hintereinander und rechnet je Liste höchstens 1 s an. Eine unbrauchbare Liste
  hält die Frist an, ohne sie zurückzusetzen. Behebt (a) und (b).
- Neue Verbindungen bei unbrauchbarer Liste warten und werden nach `AuthorizeWaitMs` abgelehnt: Die Sperre gegen Fremde bleibt.
- `Session::RejoinLobby`: Fehlt beim Client das eigene Konto 2 s, tritt er derselben Lobby neu bei (höchstens alle 5 s). Soll (c) beheben,
  bevor der Host nach 10 s trennt. Auf echtem Steam ungeprüft.
- Logzeilen nur bei Änderungen, ohne IDs, siehe unten.

Geprüft in `tests/NetTests/LobbyMembershipTests.cpp`, `AuthorizationTests.cpp` und `SessionTests.cpp` mit nachgebautem Steam: leere Liste und
Liste ohne eigenes Konto lange, einzelne alte Liste, Fremder bei unbrauchbarer Liste, dauerhaftes Fehlen trennt weiter, Host ohne Liste in der
Sitzung, Neubeitritt des Clients, kein Neubeitritt nach Verlassen oder bei Ablehnung.

## Nächster Schritt

Live-Test auf zwei PCs (`py tools/tasks.py tests`), Logs beider Seiten mit `Collect-Logs.ps1` sammeln und nach `Lobby (` und
`Transport: Mitspieler` / `Transport: Lobby-Liste` durchsuchen:

| Weg | Host-Log | Client-Log |
|---|---|---|
| (a) | `Lobby (Host): Mitgliederliste leer` oder `… eigenes Konto fehlt statt gültig`, `Transport: Lobby-Liste unbrauchbar (…), Frist für den Mitspieler steht`; die Mitgliederzahl kann gleich bleiben | nichts Auffälliges |
| (b) | `Transport: Mitspieler fehlt in der gültigen Lobby-Liste`, kurz darauf `Mitspieler wieder in der Lobby-Liste (…), hat … ms angerechnet gefehlt` mit kleinem Wert, meist um einen Kartenwechsel | nichts Auffälliges |
| (c) | `Lobby (Host): 1 statt 2 Mitglieder`, `Transport: Mitspieler fehlt in der gültigen Lobby-Liste`, dann `2 statt 1` und `wieder in der Lobby-Liste` unter 10000 ms | `Lobby (Client): eigenes Konto fehlt in der Lobby`, `tritt derselben Lobby neu bei`, `derselben Lobby neu beigetreten`, `eigenes Konto wieder in der Lobby nach … ms` |

Trennt der Host trotzdem, steht davor `Transport: Mitspieler fehlt seit … ms in … gültigen Lobby-Listen hintereinander (zuletzt … Mitglieder,
Liste vor … ms, letzter Empfang vor … ms), trenne mit Grund 1005`. Beim Client zeigt dann `Neubeitritt abgelehnt (Antwort …)`, warum (c) nicht half.

## Abnahme

Die Ursache ist belegt, und ein verbundener Client wird nicht mehr wegen einer kurz leeren oder veralteten Mitgliederliste getrennt.
Die Sperre gegen Fremde bleibt erhalten.
