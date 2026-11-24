# Glossar

Begriffe der deutschen Doku und ihre Bezeichner im Spiel oder Code. Jeder Bezeichner ist im Code unter `src/` belegt.

| Begriff | Bezeichner | Bedeutung |
|---|---|---|
| Airdrop | `Airdrop*` (`src/Game/AirdropAdapter.cpp`) | Kiste mit Belohnungsauswahl während der Mission |
| Bestätigungstest | – (Prozess) | Abnahme, die noch zu zweit oder im Spiel geprüft werden muss, siehe [Prozess](PROZESS.md) |
| Bus | `BP_SpawnAndLeaveBus` | Start- und Abfahrtspunkt der Mission |
| Dodge | `GA_Dash` | Ausweichsprung der Figur |
| Edit | `InventoryEdit` | Inventarbearbeitung des Clients, die der Host übernimmt oder ablehnt |
| Händler | `ShopPanelItem` | Kauf und Verkauf im Händler-Event |
| Itemsack | Itemsack (Kommentare, `DevAdapter`) | Sack mit Items, zählt wie ein Airdrop für alle ([E-03](ENTSCHEIDUNGEN.md#e-03-itemsack-zählt-wie-airdrop-für-alle)) |
| Kontrollkanal | `ControlProtocol` (`src/Net`) | Eigene Nachrichten zwischen Host und Client neben der nativen Replikation |
| Labyrinth | Labyrinth (`src/Game/MapPatches.cpp`), Karte `MGP_LabyrinthEvent` | Karten-Event auf der Route |
| Lager | Stash (`UpdateIsInStash`) | Inventar-Bildschirm nach dem Rundenende, mit Ablage außerhalb des Rucksacks |
| MetaBase | `MetaBase` | Basis zwischen den Runs, Figurenwahl |
| P2 | `RemoteP2*` | Zweiter Spieler in der nativen Coop-Logik, bei der Mod der entfernte Spieler |
| Rezept, Werkbank | `Recipe*` | Aufwerten und Kombinieren von Items |
| Routenkarte | – (nur Kommentare) | Auswahl der nächsten Karte |
| Rucksack | `Backpack` (`DesyncSnapshot.h`) | Rasterinventar der Figur |
| Rundenende | `TriggerEndRound`, `FinishMission` | Ende einer Mission, entscheidet der Host |
| Rundenübersicht | `RoundEndPanel`, `WP_Round_Stats_C` | Wertung nach einer Mission oder dem Run |
| Sensenmann | `UReaperComponent`, `AReaperSpawner` | Gegner, der nach einer festen Missionszeit (`MinutesUntilReaper`) erscheint |
| Spike | `research/spikes/a-connect` | Frühe Machbarkeits-DLL, wird aus `main` entfernt |
| Tank | `UFuelMeter`, `FuelCurrent`, `FuelMax` | Treibstoff des Busses als Gruppenwert |
| Warnanzeige | `Telegraph` | Anzeige eines Gegnerangriffs (Kreis, Sprint-Strich) |
