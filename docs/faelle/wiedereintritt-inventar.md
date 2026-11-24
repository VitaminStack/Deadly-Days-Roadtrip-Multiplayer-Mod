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

- [Belegt, SDK] Die Astronauten-Rüstung kommt aus der Startausrüstung der Figur (`UPlayerCharacterDefinition.EquippedItemDatas`), liegt also
  als Item im Inventar. Der Schrott liegt nicht beim Spieler, sondern im gemeinsamen Konto der Mission (`ADDMissionGameState.ScrapWallet`,
  im Run-Stand nur `ScrapWallet_Player0`); ihn bucht schon heute nur der Host. Es gibt also keinen Schrott des Clients zum Aufheben.
- [Belegt, Spiel] Der Peer am Kontrollkanal ist ein Zähler je Verbindung, nicht die Steam-Identität: Nach dem Wiederbeitritt galt der Client
  zuerst als neues Konto. Jetzt nennt `Net::Transport::PeerAccount` die SteamID hinter dem Peer. Auf einem PC verbindet der Client im lokalen
  Testmodus über 127.0.0.1 ohne Steam-Identität; solche Verbindungen zählen als festes Testkonto (`Transport::LoopbackAccount`).
- [Belegt, Spiel] Auf einem PC löst Steam die Lobby beim Verlassen auf, der Host erstellt eine neue; dazwischen gibt es keinen Transport,
  die Mission läuft weiter. Das Bild bleibt darum über Transportlücken erhalten und wird nur außerhalb der Mission verworfen.
- [Belegt, Spiel] Am 25.11.2026 auf einem PC mit zwei Instanzen (`Start-Session.ps1 -Mod -Godmode -Tunnel Lobby -Map
  /Game/Maps/DDDynamic_Map -Brief`, `DDCOOP_TEST_REJOIN_SEED=1`), Vergleich mit `inventory.py`, Host- und Client-Sicht:
  - Vorher: Client 3 Items (Pistole im Rucksack, zwei im Lager, eine davon kaputt), 10 Felder; Startinventar wäre 1 Item, 8 Felder.
  - Wiederbeitritt im selben Prozess (`Test-Rejoin.ps1`): neuer PlayerState, 10 s nach dem Beitritt „Inventar des Clients
    wiederhergestellt, 3 Items und 10 Felder statt 1 und 8 (2 Felder neu, 0 abgebaut), vollständig=true“; danach auf beiden Seiten dieselben
    Items, Zustände und Felder wie vorher.
  - Hot Join nach Neustart des Clients (`Restart-Instance.ps1`): ebenso, wieder vollständig; Desync-Melder danach „OK“ für alle Felder.
  - Reise ohne Trennung (`servertravel`): neues Inventar übernommen, keine Rückgabe, Inventar unverändert. Kein Absturz.
- [Vermutung] Die Astronauten-Rüstung kommt mit dem Anzug-Item zurück, im Spiel nur mit der Standardfigur geprüft.

## Prüfliste ([ARCHITECTURE.md](../ARCHITECTURE.md#verbindliches-architekturprinzip-lokal-zuerst-minimal-synchronisieren))

- **Was bleibt lokal?** Alles beim Client. Er bekommt das Inventar über die native Replikation wie jede Vergabe des Hosts.
- **Was wird geteilt?** Nichts Neues, keine Kontrollnachricht, `ModVersion` bleibt. Seltenheiten gehen wie bisher über den vorhandenen Abgleich.
- **Wer entscheidet?** Allein der Host. Er liest das Inventar des Clients aus seinem eigenen, replizierten Stand.
- **Welcher Weg wird wiederverwendet?** `CreateAndAddItem` wie bei der Airdrop-Wahl, `InventoryEdit::Plan`/`Apply` wie bei einem Edit des
  Clients (Items heraus, Felder angleichen, ablegen), danach `RefreshEffects`. Der native Run-Stand ist keine Quelle: Er wird nur beim
  Rundenwechsel geschrieben und wäre mitten in der Mission veraltet.
- **Schlüssel:** Steam-Identität des Peers am Kontrollkanal, nicht der PlayerState-Zeiger. Sie erscheint nie im Log.
- **Ablehnung, Wiederholung, Neuaufbau:** `Game::Rejoin::Ledger` in `src/Game/RejoinPolicy.h`, getestet in
  `tests/GameTests/RejoinPolicyTests.cpp`: Rückgabe genau einmal je neuem Inventar und nur nach beobachteter Trennung (Reise, neuer Tag
  und kurzer Ausfall des Kontrollkanals geben nichts); Bild eingefroren von der Trennung bis zur Rückgabe; anderes Konto bekommt das
  Startinventar; keine Rückgabe während Airdrop-Auswahl oder Lager; ein Fehlschlag versucht dasselbe Inventar nie zweimal, das Bild bleibt für
  den nächsten Beitritt; Sieg, Niederlage und Rückkehr in MetaBase oder Hauptmenü verwerfen alles. `PeerAccount` ist in
  `tests/NetTests/TransportTests.cpp` geprüft (neuer Peer, gleiches Konto; Testkonto über 127.0.0.1).

## Umsetzung

`TickRejoin` in `src/Game/RewardPatches.cpp`, je Takt aus `src/Sync/Inventory/RewardSync.cpp` mit dem Konto des Peers (0 ohne). Alle
500 ms liest der Host in einer Mission das Inventar des Clients (Items mit Daten, Seltenheit, kaputt, Rucksack, Papierkorb, Drehung, Pivot,
Feldern; Rucksackfelder). Nach der Trennung bleibt das Bild stehen. Erscheint für dasselbe Konto ein neues Inventar, wartet der Host 10 s und
bis es sich 3 s nicht geändert hat (Startausrüstung, Charakterwahl), legt dann alle gemerkten Items an und ersetzt mit einem Plan das
Startinventar. Fehlt ein Item beim Host, bleibt das Startinventar unverändert. Sieg und Niederlage (`URunPersistencyManager.OnRoundChanged`)
und der Host in MetaBase oder Hauptmenü verwerfen die Bilder. Testschalter `DDCOOP_TEST_REJOIN_SEED=1`: Der Host gibt dem Client einmal je Prozess nach 15 s zwei Items ins Lager
und zwei Rucksackfelder, damit sich sein Inventar ohne Eingaben vom Startinventar unterscheidet.

Nicht enthalten: Powerups (`PowerUpInventory_Player1`), Glücksschutz und die Kistenstufen des Inventars (`levelUpCrateLevels`), Modifikatoren
und Community-Namen einzelner Items. Vermutung: Powerups gehen beim Wiederbeitritt ebenso verloren, nicht geprüft.

## Offen

- Auf zwei PCs mit echter Steam-Identität nicht geprüft (Zeile unter „Live-Bestätigung ausstehend“ sinnvoll).
- Live-Test 24.11.2026: Nach dem Hot Join zeigte die Schrottanzeige im Airdrop-Panel des Clients bis zum Rundenende 140000 statt rund 230. Der
  Schrott selbst gehört dem Host (siehe oben); die Anzeige ist nicht untersucht.
- Powerups, Modifikatoren, Figur Astronaut im Spiel (siehe Umsetzung).

## Abnahme

Der Host hebt das Inventar eines Clients, der die Sitzung verlässt, samt Rüstung und Schrott auf und gibt es ihm beim Wiederbeitritt oder Hot Join
zurück. Mit Unit-Test, belegt auf einem PC mit zwei Instanzen.
