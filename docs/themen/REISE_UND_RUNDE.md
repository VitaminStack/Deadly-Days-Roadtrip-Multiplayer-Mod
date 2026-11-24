# Reise, Karte und Rundenablauf

Gilt für Build `26786272`. Datierte Befunde und Messungen stehen im [Archiv](../archive/GAME_ANALYSIS.md), Abschnitte unten unter „Historie“.

## Überblick

- Die lokale Reise des Spiels (`SeamlessTravelToLevel`) wird beim Host zu `servertravel`, damit der Client mitkommt. Allein und ins Hauptmenü reist das Spiel wie gewohnt.
- Beide erzeugen dieselbe Karte: Der Host meldet Seed, Profil, Größe, Tags und PCG-Graph seiner Erzeugung über den Kontrollkanal, die Erzeugung des Clients wartet darauf.
- Rundenende, Busabfahrt, Tank und Tagesablauf entscheidet der Host. Der Client zeigt Rundenübersicht, Lager und Routenkarte des Hosts mit Eingabesperre.
- Pausiert der Host die Welt, zeigt der Client einen Hinweis; das Pausemenü des Clients meldet sich beim Host.
- Beim Run-Ende (Sieg oder Niederlage) kehren beide in die MetaBase zurück; der Client übernimmt keinen fremden Run-Stand in seinen Spielstand.

## Code

| Datei | Aufgabe | Test |
|---|---|---|
| `src/Game/TravelPatches.cpp` | Host: Reise als `servertravel`, vorher Zeitanfragen abräumen | `tests/GameTests/PatchTests.cpp` |
| `src/Game/MapPatches.cpp`, `src/Game/MapPolicy.h`, `src/Sync/World/MapSync.cpp` | Gleiche Karte und Vegetation bei Host und Client | `tests/NetTests/MapTests.cpp` |
| `src/Game/PauseAdapter.cpp`, `src/Sync/Players/PauseSync.cpp` | Pausen und Pausemenü | `tests/NetTests/PauseTests.cpp` |
| `src/Game/ProgressAdapter.cpp`, `src/Sync/ProgressSync.cpp`, `src/Game/DayView.cpp` | Rundenphase, Tag und Fortschritt beim Client | `tests/NetTests/ProgressTests.cpp` |
| `src/Game/WidgetPatches.cpp` | Client: HUD wartet auf den GameState | – |
| `src/Game/WorldAdapter.cpp` | Zugriff auf Welt und GameState | `tests/GameTests/WorldAdapterTests.cpp` |

## Offene Fehler

- [wiedereintritt-karte](../faelle/wiedereintritt-karte.md): Client baut beim Wiederbeitritt keine Karte.
- Was im Spiel noch zu prüfen ist: [Spicker](../ERLEDIGT.md#bestätigungstests-ausstehend), etwa `labyrinth-event-client`.
- [spawn-nach-kartenaufbau](../faelle/spawn-nach-kartenaufbau.md): Spawn vor dem Kartenaufbau des Clients.

## Historie

Wortgleiche Befunde, nur zum Nachschlagen, nicht als Ist-Zustand lesen:

- [Karte und Seed](../archive/GAME_ANALYSIS.md#karte-und-seed)
- [Karte und replizierte Kartenobjekte](../archive/GAME_ANALYSIS.md#karte-und-replizierte-kartenobjekte)
- [Run-Ablauf](../archive/GAME_ANALYSIS.md#run-ablauf)
- [Wege zwischen den Karten](../archive/GAME_ANALYSIS.md#wege-zwischen-den-karten)
- [Niederlage (`niederlage`)](../archive/GAME_ANALYSIS.md#niederlage-niederlage)
- [Run-Ende bei Host und Client (`run-ende`)](../archive/GAME_ANALYSIS.md#run-ende-bei-host-und-client-run-ende)
- [Was beim Kartenwechsel passiert](../archive/GAME_ANALYSIS.md#was-beim-kartenwechsel-passiert)
- [Was beim Client laufen darf](../archive/GAME_ANALYSIS.md#was-beim-client-laufen-darf)
- [Rundenende über Tank und Bus (`rundenende-absturz`)](../archive/GAME_ANALYSIS.md#rundenende-über-tank-und-bus-rundenende-absturz)
- [Tank als Gruppenwert (`tank-gemeinsam`)](../archive/GAME_ANALYSIS.md#tank-als-gruppenwert-tank-gemeinsam)
- [Busabfahrt nur beim Host (`bus-ende`)](../archive/GAME_ANALYSIS.md#busabfahrt-nur-beim-host-bus-ende)
- [Tagesablauf: Tag, Uhr, Tank, Nacht und Ziele (`tagesablauf`)](../archive/GAME_ANALYSIS.md#tagesablauf-tag-uhr-tank-nacht-und-ziele-tagesablauf)
- [Routenkarte beim Client (`routenkarte-client`)](../archive/GAME_ANALYSIS.md#routenkarte-beim-client-routenkarte-client)
- [Rundenende: Befund und offene Phasenübertragung (`rundenende`)](../archive/GAME_ANALYSIS.md#rundenende-befund-und-offene-phasenübertragung-rundenende)
- [Labyrinth-Event beim Client (`labyrinth-event-client`)](../archive/GAME_ANALYSIS.md#labyrinth-event-beim-client-labyrinth-event-client)
- [Client-Absturz am Sensenmann (`karte-nach-labyrinth-absturz`)](../archive/GAME_ANALYSIS.md#client-absturz-am-sensenmann-karte-nach-labyrinth-absturz)
- [Run-Ablauf auf Build 26786272 (`update-26786272-run`)](../archive/GAME_ANALYSIS.md#run-ablauf-auf-build-26786272-update-26786272-run)
