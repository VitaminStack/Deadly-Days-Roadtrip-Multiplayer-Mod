# Mehr als zwei Spieler

Ist-Zustand und Umbauplan für bis zu vier Spieler (Host und drei Clients) auf Build `26786272`.
Grundlage ist eine Durchsicht von `src/` und `tests/` und eine Speicherprobe mit zwei Instanzen auf einem PC.

**Markierung:** **Belegt** heißt durch Code oder Messung gedeckt, **Vermutung** heißt abgeleitet und nicht geprüft.

## Kurzfassung

- Heute lässt der Host genau einen Client zu: `Transport::AcceptIncoming` lehnt jeden weiteren mit `Busy` ab (Belegt).
- Die Steam-Lobby nimmt schon vier Mitglieder auf (`Session::kMaxMembers = 4`), der Transport ist sonst listenbasiert (Belegt).
- Fiele nur die Sperre, schalteten sich alle Sync-Familien mit Zustand ab, auch für den ersten Client (Belegt, siehe [Warum die Sperre zuletzt fällt](#warum-die-sperre-zuletzt-fällt)).
- Das Spiel kennt nativ nur die Koop-Plätze P1 und P2; P3 und P4 werden Netzwerkspieler ohne Koop-Platz (Belegt für die Typen, siehe [Spielseite](#spielseite)).
- Der Host kann jede Verbindung einem `APlayerController` zuordnen: über den UDP-Port des Tunnel-Sockets (Belegt, siehe [Zuordnung Verbindung zu Controller](#zuordnung-verbindung-zu-controller)).

## Spielseite

| Bereich | Befund | Markierung | Folge |
|---|---|---|---|
| Koop-Plätze | `ECoopInventoryLocationType::P1Backpack`/`P2Backpack`, `CoopP1Field`/`CoopP2Field`, `MaxLocalPlayers = 2` | Belegt | P3 und P4 bekommen keinen Eintrag in `ULocalCoopSubsystem::ActivePlayers`; die Mod bindet dort weiter nur Schlüssel 0 und 1 |
| `GetPlayerController(Index)` | Index 0 immer, Index 1 nur mit Koop oder `+0x18A` | Belegt für 0 und 1 | Für P3 und P4 nicht nutzen |
| Spielerindex ohne `LocalPlayer` | Die Patches „Spielerindex ohne LocalPlayer“ und „Run-Ende ohne LocalPlayer“ geben jedem Remote-Controller Index 1 | Belegt | P3 und P4 gelten im Spielcode als P2; welche Anzeige das trifft, ist offen (Vermutung: Farbe und Porträt) |
| Kamera, Bewegungsgrenze, Busplatz, `SpawnedPlayers` | Laufen über `PlayerArray` bis 16 Einträge | Belegt | Keine Arbeit |
| Tank, XP, Schrott | Gruppenwerte am GameState bzw. beim Host | Belegt | Keine Arbeit, nur an alle Clients senden |
| Reward-Queue | Je Figur (`URewardQueueComponent`) | Belegt | Die Mod schaltet heute nur zwei Queues weiter (`AdvanceQueues`, `Last[2]`) |
| Sensenmann | `UReaperComponent::Players` behält abgebaute Figuren | Belegt | Die Bereinigung muss jede Remote-Figur beobachten, nicht nur eine |
| Gegner-Spawn | `MaxSpawnedEnemies` gilt für alle zusammen | Belegt | Balance mit vier Spielern ist nicht Aufgabe der Mod |

## Zuordnung Verbindung zu Controller

Jeder Link des Host-Tunnels hat einen eigenen UDP-Socket auf `127.0.0.1` mit freiem Port, von dem er die Pakete an den Listen-Server (`7777`) schickt.
Die `UIpConnection` des Hosts hält die Gegenadresse in `RemoteAddr` (`TSharedPtr<FInternetAddr>` an `+0x170`), die `sockaddr_in` liegt im `FInternetAddrBSD` ab `+0x8`.
Messung mit zwei Instanzen über den Tunnel der Mod: Der Port dort war genau der des Link-Sockets im Host-Prozess (Belegt).
Ohne Tunnel steht dort der Spiel-Socket des Clients (Belegt), das hilft nur dem Testmodus ohne Kontrollkanal.
`Net` darf `Game` nicht einbinden, darum liefert `Transport` je Peer den Port und `Sync` fragt `Game` nach dem Controller zu diesem Port.
`APlayerState::SavedNetworkAddress` taugt nicht: Ohne Port sind alle Tunnel-Verbindungen gleich `127.0.0.1` (Vermutung aus dem Engine-Standard, nicht gemessen).

## Mod-Seite: Stellen mit genau einem Client

### Netz

| Ort | Annahme | Änderung |
|---|---|---|
| `Transport.cpp` `AcceptIncoming` | Zweiter Link bekommt `Busy` | Obergrenze `Session::kMaxMembers - 1` Clients, als letzter Schritt |
| `Transport.h` `ClientPort = 7778`, `Session.cpp` `ConnectToLocalPort` | Fester Port je Client-Instanz | Port aus der Kommandozeile, sonst 7778; nur für mehrere Instanzen auf einem PC nötig |
| `Transport.cpp` `PeerAccounts` | Alle Loopback-Verbindungen haben `LoopbackAccount = 1` | Eigenes Pseudokonto je Loopback-Link, sonst verschmilzt der Wiedereintritt mehrere Testclients |
| `Transport.cpp` `Reject` | Meldet `Listening`, obwohl andere Clients verbunden sind | Nur ohne verbundenen Link |
| `Transport.cpp` `EndMessage` | Text „schon einen Mitspieler“ | „Die Sitzung ist voll“ |

### Sync

Alle Familien mit Zustand wählen ihren Peer heute mit `Peers.size() == 1 ? Peers[0] : 0`; mit zwei Clients ist der Peer 0 und die Familie schweigt.
Beim Client ist genau ein Peer (der Host) richtig, die Änderung betrifft nur den Host.

| Familie | Ort | Änderung beim Host |
|---|---|---|
| Karte `0x400`/`0x401` | `World/MapSync.cpp` | Gleiche Karte an jeden Peer, gesendeter Stand je Peer |
| Fortschritt `0x100`–`0x102` | `ProgressSync.cpp` | Abo, Nonce und Bild je Peer |
| Desync `0x200` | `DesyncSync.cpp`, `DesyncState.h` (`Players <= 2`) | Prüfung je Peer, Grenze 4 |
| Inventar `0x300`–`0x309` | `Inventory/RewardSync.cpp` | Zustand je Peer, Edits auf das Inventar des Controllers dieses Peers |
| Pause `0x700` | `Players/PauseSync.cpp`, `PauseState.h` | `HostHold` je Peer, Pause solange einer hält |
| Figur `0x800` | `Players/CharacterSync.cpp`, `CharacterPatches.cpp` | Wahl je Controller, Weitergabe an alle Clients mit Spielerkennung, neues Format |
| Dodge `0x500` | `Sync.cpp`, `MovementPolicy.h` `DashInputBook` | Eingabe nach Controller und Schlüssel, denn jeder Client zählt seine Schlüssel selbst |
| Entwickler `0x600` | `Sync.cpp` `RunDevAction(Action, 1, …)` | Ziel ist der Controller des Absenders statt Index 1 |
| Warnanzeigen, Schadenszahlen, Nachladen, Bedrohung | `Sync.cpp` | Schon Schleifen über alle Peers; die Revision steigt aber je Send, jeder Client sähe Lücken; Revision je Peer |

### Spiel

| Ort | Annahme | Änderung |
|---|---|---|
| `RemoteP2Patches.cpp` `CurrentRoster` | `PlayerArray.Num` höchstens 2, ein `Remote` | Liste aller Remote-Controller; P2 bleibt der zuerst gebundene, solange er da ist |
| `RemoteP2Patches.cpp` POI-Filter, Sensenmann-Wache | Nur die Figur des einen Remote | Alle Remote-Figuren |
| `RewardPatches.cpp` Airdrop | `Host.ClientPick`, `ClientReady`, `ReadyRevision`, `RemoteReady` einfach; `ClaimForClient` sperrt nach der ersten Wahl | Wahl und Bereit je Peer, Abschluss erst, wenn alle verbundenen Clients gewählt haben |
| `RewardPatches.cpp` Lager am Rundenende | `End.ClientIn`/`ClientReady` einfach, `HostHoldsRoundEndConfirm` mit Einzelwerten | Stand je Peer, Host hält, solange ein anwesender Client nicht bereit ist |
| `RewardPatches.cpp` `RewardCharacter`, `ClientTradeRpc`, `ApplyClientEdit`, `ClientRarities` | Nur der als P2 gebundene Remote | Jeder Remote-Controller, Inventar über die Zuordnung |
| `RewardPatches.cpp` `AdvanceQueues` | `Controllers[2]` | Alle Controller |
| `RejoinPolicy.h` `Ledger::Tick`, `TickRejoin` | Ein `Current`-Konto | Menge der anwesenden Konten |
| `ProgressAdapter.cpp`, `DesyncAdapter.cpp`, `DevAdapter.cpp` | `PlayerArray.Num` höchstens 2 | Höchstens 4 |

Schon für jede Anzahl richtig (Belegt): Lobby, Mitgliederprüfung und Trennung je Link, Kamera, Bewegungsgrenze, `SpawnedPlayers`, Busplatz, geteilte Beute, Reise, Inventar-Bearbeitung beim Client, Schrott als Gruppenwert.

### Kontrollkanal

Der Umschlag bleibt, der Host kennt den Absender am Peer.
Mit mehreren Clients heißen „Client hat gewählt“ und „Host hat das Bereit des Clients“ (`0x300` Bit 0 und 3, `0x307` Bit 1) „dieser Client“; der Host füllt das Bild je Peer.
Das Format bleibt, die Bedeutung von „Host wartet“ wird „Host wartet auf alle“; das und das neue Format von `0x800` erhöhen `ModVersion`.

## Warum die Sperre zuletzt fällt

Mit zwei Clients liefert jede Familie Peer 0: `RewardSync` setzt dann `SetRemoteReady(false)`, bricht offene Edits ab und meldet dem Wiedereintritt eine Trennung, `MapSync` schickt keine Karte mehr (der Client baut nach 60 s eine eigene), und `CurrentRoster` wird leer, womit auch die P2-Bindung des ersten Clients fällt.
Darum zuerst alles auf Zustand je Peer umstellen, jeweils mit unverändertem Zweierlauf, und erst danach die Obergrenze anheben.

## Test mit mehreren Instanzen

`Start-Session.ps1` kennt nur eine Host- und eine Client-Instanz, die Sitzungssperre hält genau zwei Prozesse, jeder Client öffnet `127.0.0.1:7778`.
Für drei oder vier Instanzen auf einem PC braucht es einen Port je Client (Mod und Skript), ein Pseudokonto je Loopback-Link und `-Clients <n>` im Skript mit Fensterversatz.
Alle Instanzen laufen dabei unter einem Steam-Konto, also über den Loopback-Testmodus (`-ddcoop_localtest`).

## Reihenfolge

1. Mehrere Clients testbar machen: Client-Port, Pseudokonto je Loopback-Link, Port des Link-Sockets je Peer, `Start-Session.ps1 -Clients`.
2. Zuordnung Peer zu Controller und Roster mit Liste, P2-Regel unverändert für den ersten Client.
3. Sync-Familien auf Zustand je Peer, Revision je Peer.
4. Airdrop, Lager, Edits, Rarities, Wiedereintritt und Dodge je Peer, `ModVersion` erhöhen.
5. Grenzen 2 auf 4, Obergrenze im Transport auf drei Clients.
6. Lauf mit drei und vier Instanzen bis Tag 2.

Nach jedem Schritt laufen `NetTests`, `GameTests` und ein Zweierlauf.
