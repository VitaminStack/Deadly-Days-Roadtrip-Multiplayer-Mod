# Bis zu vier Spieler

Ist-Zustand für bis zu vier Spieler (Host und drei Clients) auf Build `26786272`, Aufgabe `vier-spieler`.
Jeder Spieler hat sein eigenes Inventar, auch im wählbaren Koop-Modus „Geteiltes Inventar“ ([Inventar](themen/INVENTAR_UND_HANDEL.md#überblick)); ein geteiltes Inventar wie im lokalen Koop ist `coop-inventar-geteilt`.

**Markierung:** **Belegt** heißt durch Code, Test oder Messung gedeckt, **Vermutung** heißt abgeleitet und nicht geprüft.

## Kurzfassung

- Der Host nimmt bis zu drei Clients an (`TransportOptions::MaxClients = 3`, Lobby `Session::kMaxMembers = 4`); jeder weitere bekommt `Busy` (Belegt, `TransportTests`).
- Das Spiel kennt nativ nur die Koop-Plätze P1 und P2. Der erste Client bleibt P2, der dritte und vierte Spieler sind Netzwerkspieler ohne Koop-Platz (Belegt für die Typen, siehe [Spielseite](#spielseite)).
- Der Host ordnet jeden Peer des Kontrollkanals genau einem `APlayerController` zu, über den UDP-Port des Tunnel-Sockets (Belegt, siehe [Zuordnung](#zuordnung-peer-zu-controller)).
- Jede Sync-Familie mit Zustand führt ihn beim Host je Peer; beim Client bleibt genau ein Peer, der Host (Belegt, siehe [Sync je Peer](#sync-je-peer)).

## Spielseite

| Bereich | Befund | Markierung | Umsetzung |
|---|---|---|---|
| Koop-Plätze | `ECoopInventoryLocationType::P1Backpack`/`P2Backpack`, `CoopP1Field`/`CoopP2Field`, `MaxLocalPlayers = 2` | Belegt | `ULocalCoopSubsystem::ActivePlayers` behält nur die Schlüssel 0 und 1; P2 ist der zuerst gebundene Remote, solange er da ist (`ChooseP2`) |
| `GetPlayerController(Index)` | Index 0 immer, Index 1 nur mit Koop oder `+0x18A` | Belegt für 0 und 1 | Für P3 und P4 nicht genutzt; Dev-Aktionen und Inventare gehen über den Controller des Peers |
| Spielerindex ohne `LocalPlayer` | Die Patches „Spielerindex ohne LocalPlayer“ und „Run-Ende ohne LocalPlayer“ geben jedem Remote-Controller Index 1 | Belegt | P3 und P4 gelten im Spielcode als P2; welche Anzeige das trifft, ist offen (Vermutung: Farbe und Porträt) |
| Kamera, Bewegungsgrenze, Busplatz, `SpawnedPlayers` | Laufen über `PlayerArray` bis 16 Einträge | Belegt | Keine Arbeit |
| Tank, XP, Schrott | Gruppenwerte am GameState bzw. beim Host | Belegt | Gehen an alle Clients |
| Reward-Queue | Je Figur (`URewardQueueComponent`) | Belegt | `AdvanceQueues` schaltet die Queues aller Figuren weiter |
| Sensenmann | `UReaperComponent::Players` behält abgebaute Figuren | Belegt | Die Wache beobachtet jede Remote-Figur |
| Figurenwahl | `APlayerState::PlayerId` repliziert und ist auf allen Seiten gleich | Belegt | `0x800` trägt die Wahl je `PlayerId`, jede Remote-Figur bekommt ihre |
| Gegner-Spawn | `MaxSpawnedEnemies` gilt für alle zusammen | Belegt | Balance mit vier Spielern ist nicht Aufgabe der Mod |

## Zuordnung Peer zu Controller

Jeder Link des Host-Tunnels hat einen eigenen UDP-Socket auf `127.0.0.1` mit freiem Port, von dem er die Pakete an den Listen-Server (`7777`) schickt.
Die `UIpConnection` des Hosts hält die Gegenadresse in `RemoteAddr` (`TSharedPtr<FInternetAddr>` an `+0x170`), die `sockaddr_in` liegt im `FInternetAddrBSD` ab `+0x8` ([OFFSETS](OFFSETS.md)).
Messung über den Tunnel der Mod mit zwei, drei und vier Instanzen auf einem PC: Der Port dort war genau der des Link-Sockets im Host-Prozess, jeder Peer wurde genau einem Remote-Controller zugeordnet, auch nach der Reise in die Mission (Belegt). Dass es der richtige ist, folgt aus dem Aufbau: Die Pakete dieses Controllers kommen nur vom Socket dieses Links.

`Net` darf `Game` nicht einbinden. Darum liefert `Transport::PeerGamePort` je Peer den Port, `Sync` gibt `Game::SetPeerLinks` die Liste einmal je Takt vor allen Familien, und `Game/RemotePlayers.h` beantwortet `PeerController`, `ControllerPeer` und `RemoteControllers`.
Zugeordnet wird nur über einen eindeutigen Port (`MatchPeers` in `RemoteP2Policy.h`); ohne lesbaren Port nur dann, wenn genau ein Link und genau ein Remote-Controller da sind.
Sonst bleibt ein Peer ohne Controller, bis er eindeutig ist (Reise, erste Replikation). Jede Änderung der Zuordnung steht einmal im Log („Mehrere Spieler: …“).

## Sync je Peer

| Familie | Ort | Beim Host |
|---|---|---|
| Karte `0x400`/`0x401` | `World/MapSync.cpp` | Gleiche Karte an jeden Peer, gesendeter Stand je Peer |
| Fortschritt `0x100`–`0x102` | `ProgressSync.cpp` | Abo, Nonce und Bild je Peer |
| Desync `0x200` | `DesyncSync.cpp`, `DesyncState.h` | Probe je Peer, Ergebnis zusammengefasst, bis vier Spieler im Bild |
| Inventar `0x300`–`0x309` | `Inventory/RewardSync.cpp`, `Game/RewardPatches.cpp` | Stand je Peer; Edits gehen auf das Inventar des Controllers dieses Peers |
| Pause `0x700` | `Players/PauseSync.cpp` | Halten je Peer, Pause solange einer hält |
| Figur `0x800` | `Players/CharacterSync.cpp` | Wahl je Peer über dessen `PlayerId`, jeder Client bekommt die Wahl aller anderen |
| Dodge `0x500` | `Sync.cpp`, `MovementPatches.cpp` | Eingaben je Controller, denn jeder Client zählt seine Schlüssel selbst |
| Entwickler `0x600` | `Sync.cpp`, `DevAdapter.cpp` | Ziel ist die Figur des Absenders; ohne Zuordnung nur Gegner und Tank |
| Schadenszahlen | `Sync.cpp` | Revision je Peer, weil der Client Lücken prüft |
| Warnanzeigen, Nachladen, Bedrohung | `Sync.cpp` | An jeden Peer, ohne Zustand |

### Airdrop-Auswahl und Lager

Jeder Client wählt aus denselben Choices, der Host legt jede Wahl neu in das Inventar dieses Clients (`ClaimForClient`).
An der Auswahl nimmt jeder verbundene Client teil, dem ein Controller zugeordnet ist; auf einen Client ohne Figur wartet niemand.
Der Host hält sein Bestätigen, bis jeder dieser Clients gewählt hat, und ein Client löst seines erst aus, wenn der Host und alle anderen bereit sind.
Am Rundenende hält der Host, solange ein verbundener Client im Lager dieses Rundenendes nicht bereit ist. Die Bits je Empfänger stehen in [Mehrere Clients](CONTROL_PROTOCOL.md#mehrere-clients-mod-kompatibilität-17).
Wiedereintritt (`RejoinPolicy.h`) führt je Steam-Konto einen Eintrag; wer in `Ledger::Connected` fehlt, gilt als getrennt und bekommt beim Wiederbeitritt sein Inventar zurück.

## Test mit mehreren Instanzen

`Start-Session.ps1 -Mod -Tunnel Lobby -Clients <2|3>` startet bis zu drei Clients auf einem PC, jeder mit eigenem Testordner und Tunnel-Port (`-ddcoop_clientport`, 7779 und 7780).
Alle Instanzen laufen unter einem Steam-Konto über den Loopback-Testmodus (`-ddcoop_localtest`); jeder Loopback-Link bekommt ein eigenes Pseudokonto.
Die weiteren Clients heißen in den Skripten `Client2` und `Client3` (`Game-Input.ps1`, `Get-Status.ps1`). `Run-Route.ps1` prüft mit `check_round.py --players` die Spielerzahl der Sitzung und bestätigt das Lager bei allen Instanzen.

Geprüft am 25.11.2026 auf einem PC mit Host und drei Clients (`Run-Route.ps1 -Days 2 -Godmode`): MetaBase, Tag 1, Rundenende, Lager, Tag 2, ohne Abweichung und ohne Absturz.
Der Host wartete im Lager auf jeden der drei Clients und holte sein Bestätigen erst nach dem letzten Bereit nach.

## Grenzen

- P3 und P4 zeigt das Spiel als P2 (Farbe, Porträt); nicht geprüft.
- Mit drei und vier Spielern nur auf einem PC geprüft, nicht über das Internet.
- Die Airdrop-Wahl mit mehreren Clients ist nur durch Tests gedeckt. Im Spiel ist sie nicht geprüft, denn der Routenlauf öffnet keine Kiste.
