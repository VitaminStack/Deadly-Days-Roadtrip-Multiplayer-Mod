# Mehr als zwei Spieler (Analyse)

Stand 07.11.2026, `main` bei `340ada8`, Mod-Version 7, Build `26187885`. Nur Lesen von Doku und Quellcode, kein Spieltest,
kein Code geändert. Bezug: Aufgabe `mehr-spieler` (blockiert, braucht `release`). Der Bericht soll Phase 5 nicht aufhalten,
sondern zeigen, was 3 und 4 Spieler kosten würden und was sich schon jetzt günstig vorbereiten lässt.

**Markierung:** **Belegt** heißt, durch eine genannte Stelle in `docs/` oder `src/` gedeckt. **Vermutung** heißt,
abgeleitet, aber nicht gemessen oder nicht im Code nachgelesen.

## Kurzfassung

- Die **Spielseite** kennt nativ genau zwei Koop-Plätze (P1, P2). Ein dritter und vierter Spieler kann daher **nicht** in die
  native Koop-Logik eingetragen werden. Er wäre ein reiner Netzwerkspieler mit eigener Figur, ohne Koop-Platz.
- Die **Systeme, die über die Spielerliste laufen**, tragen schon mehr als zwei: gemeinsame Kamera, Bewegungsgrenze, Busplatz,
  `SpawnedPlayers`-Abgleich, Tank am GameState, Bewegungs- und Dodge-Reparatur.
- Auf der **Mod-Seite** ist „genau ein Client“ an etwa einem Dutzend Stellen fest eingebaut. Die Stellen sind gut zu finden. Der
  Transport ist schon listenbasiert und sperrt nur an einer Stelle. Teuer sind Airdrop-Auswahl, Bereit-Logik und
  Inventar-Edits, denn dort hängt der Zustand an genau einem Client.
- Der **Kontrollkanal** braucht keinen Spielerindex im Umschlag, weil der Host den Absender über das Verbindungstoken kennt. Die
  Bedeutung einiger Flags („Client bereit“) muss sich ändern. Das ist eine neue Mod-Version.
- **Empfehlung:** Bis `release` nichts umbauen. In neuem Code Zustand **je Peer** halten statt als einzelnes Feld, und
  `Peers.size() == 1` nicht weiter kopieren. Danach zuerst 3 Spieler als „Netzwerkspieler ohne Koop-Platz“ angehen. 4 Spieler
  sind derselbe Weg, aber erst nach einem Last- und Balancetest.

## Spielseite: was das Spiel nativ kann

| Bereich | Befund | Markierung | Folge für 3 bis 4 Spieler |
|---|---|---|---|
| Koop-Plätze | `ECoopInventoryLocationType::P1Backpack`/`P2Backpack`, `CoopP1Field`/`CoopP2Field` ([Lokale Coop-Schnittstellen](GAME_ANALYSIS.md#lokale-coop-schnittstellen)) | Belegt | Lager- und Rucksack-UI des Spiels kennt nur zwei Rucksäcke |
| Geräte und lokale Spieler | `AssignPhysicalDevice` erlaubt Index 0 und 1, `DraftAssignPhysicalDevice` -1 bis 1 ([Eingaben](GAME_ANALYSIS.md#eingaben-getrennt-eingaben)); `ApplyDeviceAssignmentDraftAndJoin` setzt `MaxLocalPlayers=2` | Belegt | Kein natives P3. `bUseSplitscreen=False`, es gibt keinen Split-Screen-Koop ([Konfiguration](GAME_ANALYSIS.md#konfiguration)) |
| `GetPlayerController(Index)` | Index 0 immer, Index 1 nur bei `bCoopEnabled` oder `+0x18A` ([Coop-Ablauf](GAME_ANALYSIS.md#coop-ablauf)) | Belegt für 0 und 1 | Was Index 2 und 3 liefern, ist offen. Vermutung: nichts Brauchbares, weil Index 1 schon als Sonderfall geprüft wird |
| UI-Bedienung (`CoopInputRouterSubsystem`) | `RequestUIAuthorityTransfer(PlayerIndex)`, `FCoopUISession.AuthorityPlayerIndex` | Belegt | Der Router ist lokal und repliziert nicht. Die Mod nutzt ihn für Remote-Spieler ohnehin nicht. Für P3 also kein Verlust |
| `CoopInventoryCoordinatorComponent` | Am `ADDMissionGameState`, repliziert nicht, `ClaimReward(PlayerIndex, Choice)`, `AwaitingClaims` | Belegt | Die native Reward-Sitzung wartet auf P1/P2. Ob sie mehr Teilnehmer zulässt, ist offen (Vermutung: nein, wegen fester P1/P2-Felder) |
| Reward-Queue | `URewardQueueComponent` hängt an jeder Figur (`+0x980`) ([Airdrop](GAME_ANALYSIS.md#airdrop-auswahl-für-beide-spieler-airdrop-auswahl)) | Belegt | Je Figur, also nicht auf zwei begrenzt. Die Queue eines Remote-Spielers blieb aber hängen, die Mod lenkt deshalb auf die Host-Figur um |
| Gemeinsame Kamera | `AllPlayerFramingCamera` rahmt alle aus `SpawnedPlayers` ([Kamera](GAME_ANALYSIS.md#kamera-und-sichtbereich)) | Belegt | Läuft über eine Liste. Mit vier Figuren wird das Bild enger oder der Zoom größer. Ob `ZoomOutDistance` reicht, ist offen (Vermutung) |
| Bewegungsgrenze | `UInViewportMovementRestrictor` bremst bei mehr als einem Spieler (`PlayerArray.Num > 1`) ([Restrictor](GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz)) | Belegt | Funktioniert für jede Anzahl. Vier Spieler behindern sich stärker (Vermutung) |
| Spawnplätze am Bus | `BusSpawnPositionGenerator`, die Mod ergänzt die Schonliste um die Figuren **aller** PlayerStates ([Busplatz](GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung)) | Belegt | Die Schonliste ist schon listenbasiert. Ob der Generator für mehr als zwei Figuren freie Plätze hat, ist offen (Vermutung: Figuren stehen übereinander, schadet nach dem Fix nicht) |
| Gegner-Spawn | Zufälliges Ziel aus `SpawnedPlayers`, `MaxSpawnedEnemies` für alle zusammen ([Gegner-Spawning](GAME_ANALYSIS.md#gegner-spawning-und-pooling)) | Belegt | Weniger Gegner je Spieler. Balance für 3 bis 4 Spieler ist im Spiel nicht vorgesehen (Vermutung) |
| Tank | `UFuelMeter` am GameState ist der Gruppenwert ([Tank](GAME_ANALYSIS.md#tank-als-gruppenwert-tank-gemeinsam)) | Belegt | Keine Arbeit |
| XP und Level | Level und XP waren bei Host und Client gleich ([XP-Bälle](GAME_ANALYSIS.md#xp-bälle-beim-client-xp-baelle)). Der Host-Stand wird gespiegelt | Belegt für 2 | Gruppenwert, derselbe Spiegel an alle Clients reicht (Vermutung: der Spiegel ist nicht peer-abhängig) |
| HUD, Porträts, Rundenstatistik | Rundenstatistik und Lager zeigen beim Host beide Spieler ([Remote-P2](GAME_ANALYSIS.md#remote-p2-geprüfte-anbindung-und-grenze-der-lokalen-ui)) | Belegt für 2 | Für P3/P4 gibt es keinen nativen Platz. Lebensleisten über den Figuren hängen an der Figur und sollten für jede Anzahl laufen (Vermutung) |
| Figurenwahl | Ohne `LocalPlayer` nimmt der Spawn die Standardfigur | Belegt | Gilt dann für alle Remote-Spieler |
| Netzlast | Etwa 47 KB/s bei rund 85 Gegnern für einen Client (Loopback, [Gate G1](ARCHITECTURE.md#synchronisierung-zwei-wege)) | Belegt für 1 Client | Der Upload des Hosts wächst etwa linear, bei drei Clients etwa 140 KB/s (Vermutung, ungemessen) |
| `GameSession.MaxPlayers` | Nicht untersucht | Offen | Vermutung: Engine-Standard, kein Hindernis. Vor der Umsetzung lesend prüfen |

**Folgerung Spielseite:** P1 und P2 bleiben die nativen Koop-Plätze. P3 und P4 wären **Netzwerkspieler ohne Koop-Platz**: Sie
haben Figur, Kamera, Bewegungsgrenze, Tank und XP, aber keine native Rucksack-, Lager- oder Statistik-Darstellung beim Host.
Weil die Mod Airdrop-Wahl und Inventar schon heute über eigene Edits statt über native `ClaimReward` abwickelt, ist das weniger
schlimm, als es klingt. Die Folgen betreffen vor allem die Host-Anzeige.

## Mod-Seite: wo „genau ein Client“ fest steckt

| Bereich | Fundstelle | Markierung | Aufwand | Risiko |
|---|---|---|---|---|
| Transport | `src/Net/Transport.cpp` `AcceptIncoming`: bei vorhandenem Link `Busy` „es gibt schon einen Mitspieler“. Sonst schon Listen: `Links`, `Peers`, je Client ein eigener UDP-Socket, Lobby-Zulassung über die Mitgliederliste | Belegt | klein | klein: die Sperre wird zur Obergrenze. Trennen, Timeout und Zulassung je Link sind schon da |
| Lobby | `src/Net/Session.h` `kMaxMembers = 4`, Freundes-Lobby | Belegt | keiner | Die Lobby nimmt heute schon vier auf, der Transport lehnt ab ([Live-Test](GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-06112026-livetest-befunde), „Mehr als ein Mitspieler geht nicht“) |
| Remote-P2-Bindung | `src/Game/RemoteP2Patches.cpp` `CurrentRoster`: nur mit `PlayerArray.Num` 1 oder 2, sonst leer. `RemoteP2Policy.h` `ActivePlayersMap::Valid` erlaubt nur Schlüssel 0 und 1 | Belegt | mittel | **hoch:** Ein dritter Spieler lässt heute das Roster leer werden. Damit fällt auch die P2-Bindung des zweiten Spielers weg. Für P3 muss das Roster „P2 = erster Remote-Spieler, Rest ohne Koop-Platz“ lernen, mit einer stabilen Regel, wer P2 ist |
| Spielerindex ohne LocalPlayer | `src/Game/PlayerPatches.cpp` `ApplyPlayerIndex`: Ohne `LocalPlayer` gibt der Index-Helfer immer 1 zurück | Belegt | mittel | **mittel bis hoch:** P3 und P4 gälten im Spielcode als P2. Welche Spielpfade den Index nutzen (Anzeige, Reward, Statistik), ist nicht vollständig bekannt (Vermutung). Braucht eine Untersuchung der Aufrufer |
| Lokale Geräte | `src/Game/LocalDevicePolicy.h` lehnt im Netzwerkspiel Index ≥ 1 ab | Belegt | keiner | Passt auch für P3 und P4 |
| Sync-Ticks | `src/Sync/ProgressSync.cpp`, `src/Sync/DesyncSync.cpp`, `src/Sync/Inventory/RewardSync.cpp`, `src/Sync/World/MapSync.cpp`: je `Peer = Peers.size() == 1 ? Peers[0] : 0`. Bei zwei Clients ist der Peer 0, **alle vier Ticks schalten ab** | Belegt | klein bis mittel je Datei | mittel: Der Zustand (Epoche, Nonce, gesendet, letzte Antwort) hängt an genau einem Peer und muss in eine Tabelle je Peer |
| Karte und Rundenende | `MapSync.cpp`: Host sendet dieselbe Karte, Client übernimmt | Belegt | klein | klein: an alle Peers senden, Rundenende-Id je Peer bestätigen |
| Fortschritt (XP, Schrott) | `ProgressSync.cpp`: `Mirror.Bind(Peer, ...)`, Abo eines Clients | Belegt | klein bis mittel | klein: gleicher Gruppenwert für alle, Abo je Peer |
| Desync-Melder | `DesyncSync.cpp` je ein Peer; `src/Sync/DesyncState.h` lehnt mehr als 2 Spieler im Bild ab | Belegt | mittel | klein: Diagnose, kein Spielzustand. Prüfung je Peer, Grenze auf 4 |
| Airdrop-Auswahl | `src/Game/RewardPatches.cpp`: `Host.ClientPick`, `Host.ClientReady`, `Host.ReadyRevision`, `RemoteReady`, `RewardCharacter` lenkt nur den gebundenen P2 auf die Host-Figur um (etwa 25 Stellen mit diesen Feldern) | Belegt | **groß** | **hoch:** Wahl, Bereit und Ablage je Client, Abschluss erst, wenn alle gewählt und bestätigt haben. Kiste von P3 geöffnet: Umlenkung muss für jede Remote-Figur gelten |
| Bereit-Logik | `src/Game/PatchPolicy.h` `HostHoldsRoundEndConfirm(..., ClientIn, ClientReady)` und `ClientMayReleaseRoundEnd(..., HostReady, ClientReadyEcho)`, `End.ClientIn`/`End.ClientReady` in `RewardPatches.cpp`. Aufgaben `bestaetigen-alle`, `bestaetigen-rundenende` | Belegt | mittel | mittel: aus „Client bereit“ wird „alle verbundenen Clients bereit“. Wer mitten im Lager geht, muss aus der Menge fallen. Für einen Client ist das schon gelöst („Client meldet sich nicht mehr aus dem Lager“) |
| Inventar-Edits | `RewardPatches.cpp`: `InventoryOfController(Remote)` und `CachedInventory(Cache, Remote)` nur für den gebundenen P2; `ClientPending` je Client schon getrennt, weil jeder Client nur seinen eigenen hat | Belegt | mittel | mittel: Edit eines Peers auf das Inventar **seines** Controllers anwenden. Dafür braucht der Host eine Zuordnung Peer → Controller, die es heute nicht gibt (siehe unten) |
| Kamera und Restrictor | `src/Game/CameraPatches.cpp` liest die `PlayerArray` bis 16 Einträge; der Restrictor-Schutz ist anzahlunabhängig | Belegt | keiner | klein |
| Busplatz, `SpawnedPlayers` | `PlayerPatches.cpp`: Schonliste und Mengenabgleich laufen über alle PlayerStates (bis 16) | Belegt | keiner | klein |
| Bewegung und Dodge | `src/Game/MovementPatches.cpp` je Figur (`AutonomousProxy`-Prüfung) | Belegt | keiner | klein |
| Reise | `TravelPatches.cpp` räumt Zeitanfragen global auf | Belegt | keiner | Vermutung: Reise mit vier Verbindungen dauert länger, Timeouts prüfen |
| Spielstand (`coop-spielstand`) | `LocalCoopRunPersistData` kennt P1/P2 ([Coop-Ablauf](GAME_ANALYSIS.md#coop-ablauf)) | Vermutung | groß | Inventare von P3/P4 bräuchten einen eigenen Speicherweg der Mod |

### Zuordnung Peer → Spieler

Heute gibt es genau einen Remote-Controller, die Zuordnung ist deshalb implizit. Bei mehreren Clients muss der Host wissen,
welches Verbindungstoken zu welchem `APlayerController` gehört. Das ist die eigentliche neue Grundlage.

- **Belegt:** Das Token ist lokal und wird nie wiederverwendet. Der Client darf keinen Spielerindex behaupten
  ([Kontrollkanal](CONTROL_PROTOCOL.md#benutzung-und-lebensdauer)). Der Tunnel gibt jedem Client einen eigenen UDP-Socket zum
  Spiel.
- **Vermutung, Weg:** Die Zuordnung entsteht beim Host über den lokalen UDP-Port der Tunnelverbindung, der bei der
  `IpConnection` des Controllers ankommt (`Controller+0x348`). Damit braucht der Kontrollkanal keine neue Kennung, nur eine
  Abfrage im Modul `Game`, das den Port zu einem Controller findet. Vor der Umsetzung lesend prüfen, ob die Remote-Adresse der
  `IpConnection` genau diesen Port zeigt.

## Kontrollkanal

- **Belegt:** Der Umschlag (`Run`, `Epoch`, `Revision`, `Type`, Nutzlast) hat keinen Spielerindex. Jede Nachricht kommt mit dem
  Token des Absenders, und der Host sendet je Peer (`SendControl(peer, ...)`). Für Client → Host braucht es also **keinen**
  Index im Format, das bleibt so.
- **Bedeutung ändert sich:** `0x300` Bit 3 („Host hat das Bereit des Clients“) und `0x307` Bit 1 meinen heute den einen Client.
  Mit mehreren Clients braucht der Empfänger zwei Aussagen: „dein Bereit ist angekommen“ und „alle sind bereit“. Am billigsten
  wird Bit 3 auf „dein Bereit“ festgelegt und ein neues Bit kommt für „alle anderen bereit“ dazu. Wer damit einen Client mit
  Version 7 verbindet, sähe nie „alle bereit“, also **Mod-Version 8** (Belegt aus der Versionsregel in
  [Kontrollkanal](CONTROL_PROTOCOL.md#handshake)).
- **Neu, nur falls Anzeige nötig:** Wenn Clients die anderen Mitspieler mit Platz anzeigen sollen (Porträt, „wartet auf P3“),
  sendet der Host eine Spielerliste mit vom **Host vergebenen** Plätzen. Das ist eine neue Nachricht, keine Änderung am Umschlag.
- **Desync-Bild:** Grenze „Spieler höchstens 2“ ([Diagnosefamilie](CONTROL_PROTOCOL.md#diagnosefamilie-mod-kompatibilität-2), `DesyncState.h`) auf 4 anheben. Ändert die Annahme
  eines Bildes, also mit derselben Versionsänderung.
- **Hello:** Bleibt unverändert. Der Host lehnt den fünften Spieler über den Transport ab, nicht über das Protokoll.

## Reihenfolge

Jeder Schritt hat eine eigene Abnahme und lässt zwei Spieler unverändert laufen.

1. **Grundlage, ohne dritten Spieler** (mittel): Zustand in den vier Sync-Ticks auf eine Tabelle je Peer umstellen, Zuordnung
   Peer → Controller beim Host einführen, `CurrentRoster` mit fester P2-Regel (erster Remote-Spieler). Abnahme: ein normaler
   Zweierlauf wie heute, Desync-Melder ruhig.
2. **Dritter Spieler zulassen** (klein): Transport-Obergrenze auf 3. Abnahme: drei Instanzen, alle in der Mission, Kamera, Bus,
   Bewegung, Tank, XP und Karte bei allen gleich. Airdrop und Lager dürfen hier noch nur für P2 laufen.
3. **Airdrop und Lager für alle** (groß): Wahl, Bereit und Edit je Peer, „alle bereit“ als Menge, Mod-Version 8. Abnahme:
   Level-up-Airdrop und Lager am Tagesende mit drei Spielern, jeder Rucksack richtig, Ausstieg eines Clients mitten im Lager.
4. **Spielerindex klären** (mittel): Aufrufer des Index-Helfers prüfen, P3 nicht mehr als P2 ausgeben. Vorher reicht „gilt als
   P2“, wenn die Prüfung keine Spielwirkung findet.
5. **Vier Spieler** (klein, wenn 1 bis 4 stehen): Obergrenze 4, Lasttest mit drei Clients über `-NetProfile Internet`,
   Kamera und Bewegungsgrenze mit vier Figuren ansehen. Danach Balance entscheiden (Team), die Mod ändert das Spiel dabei nicht.
6. **Spielstand** (groß): nur, falls `coop-spielstand` bis dahin steht und P3/P4 gespeichert werden sollen.

Grob geschätzt sind 3 Spieler zwei bis drei Aufgaben der Größe „viel“ (Schritte 1 bis 3), der Sprung auf 4 danach wenig
(Vermutung).

## Was schon jetzt beim Weiterbauen günstig ist

Ohne Phase 5 aufzuhalten und ohne bestehende Pfade umzubauen:

- In **neuem** Sync-Code Zustand als `std::map<uint64_t Peer, ...>` oder ähnliche Tabelle führen, auch wenn heute nur ein Eintrag
  existiert. `Peers.size() == 1 ? Peers[0] : 0` nicht weiter kopieren.
- Host-Nachrichten, die für alle Clients gleich sind (Karte, Fortschritt), über eine Schleife über `ControlPeers()` senden.
- Bereit-Zustände als „Menge der bereiten Peers“ modellieren, nicht als einzelnes `bool ClientReady`. Das kostet beim Schreiben
  kaum mehr.
- Neue Adapter, die den Remote-Controller brauchen, über eine Funktion beziehen, die später einen Peer bekommen kann, statt
  `BoundRemoteP2` direkt zu rufen.
- Keine neuen Prüfungen „höchstens 2 Spieler“ einbauen. Wo eine Grenze nötig ist, die der Lobby nehmen (`kMaxMembers`).
- In Doku und Logs „Client“ statt „P2“ schreiben, wenn der Netzwerkspieler gemeint ist und nicht der native Koop-Platz.

Nicht jetzt: die Transport-Sperre lockern, `CurrentRoster` ändern oder die Bereit-Logik umbauen. Das gefährdet den Zweierlauf
ohne Nutzen vor `release`.

## Empfehlung

**3 bis 4 Spieler sind machbar, aber nicht nativ.** Das Spiel liefert für P3 und P4 Figur, Kamera, Grenze und Gruppenwerte,
aber keinen Koop-Platz. Der größte Teil der Arbeit liegt in der Mod: Zuordnung Peer → Controller, Zustand je Peer und die
Airdrop- und Lagerlogik für eine Menge von Clients. Die Aufgabe `mehr-spieler` bleibt hinter `release` blockiert. Bis dahin
gelten nur die Hinweise im vorigen Abschnitt. Danach mit Schritt 1 beginnen und 3 Spieler abnehmen, bevor 4 freigegeben werden.
Vor der Umsetzung sind lesend zu prüfen: `GameSession.MaxPlayers`, `GetPlayerController(2..3)`, die Aufrufer des Index-Helfers
und die Remote-Adresse der `IpConnection` für die Zuordnung.
