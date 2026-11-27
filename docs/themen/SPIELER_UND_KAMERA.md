# Spieler, Bewegung und Kamera

Gilt für Build `26786272`. Datierte Befunde und Messungen stehen im [Archiv](../archive/GAME_ANALYSIS.md), Abschnitte unten unter „Historie“.

## Überblick

- Das Spiel kennt nur lokales Coop mit zwei `LocalPlayer`. Die Mod trägt den entfernten Spieler beim Host als P2 in die native Coop-Logik ein, ohne zweiten `LocalPlayer`.
- Der Host spawnt den entfernten Spieler, hält ihn in `SpawnedPlayers`, übernimmt seine Position und verliert ihn weder am Busplatz noch beim Run-Ende.
- An den Bus setzt der Client seine eigene Figur selbst, in seiner eigenen Kartenerzeugung; der Busplatz des Hosts bewegt sie nicht. Die Zeilen `Spawn-Messung` im Log zeigen das in den ersten 30 s jeder Missionswelt. Fehlt am Busort noch der Boden (Bodenprüfung der Bewegung, `K2_ComputeFloorDist`), hält die Mod die eigene Figur dort, bis er da ist, höchstens 5 s; im Log `Busboden: …`.
- Lokale Eingabegeräte bleiben im Netzwerkspiel beim eigenen Spieler; ohne Netzwerk bleibt das Spiel unverändert.
- Beide sehen eine gemeinsame Kamera. Der Client nimmt die Kamera-Spieler aus der `PlayerArray` statt aus dem GameMode.
- Nach dem Dodge (`GA_Dash`) repariert die Mod beim Client Reibung und Schwerkraft; die Figur des Clients dodgt bei HP 0 nicht.
- Lauftempo und Beschleunigung: Das Spiel setzt `MaxWalkSpeed` (gleich dem Attribut `MovementSpeed`) und `MaxAcceleration` (3,5-faches Attribut) nur beim Host. Beim Client übernimmt die Mod beide Werte bei jeder Änderung des replizierten Attributs, für die eigene und für fremde Figuren. Upgrades, Items, Trefferbremse und Kriechen im Tod bucht weiter nur der Host, es gibt keine eigene Nachricht. Lesewerkzeug: `tools/spike/movement_speed.py`.- Die Figurenwahl der MetaBase geht in beide Richtungen; der Host spielt mit der Definition des Clients.

## Code

| Datei | Aufgabe | Test |
|---|---|---|
| `src/Game/PlayerPatches.cpp` | Host: Spawn, Spielerindex, `SpawnedPlayers`, Position, Busplatz, Run-Ende | `tests/GameTests/PatchTests.cpp` |
| `src/Game/BusFloorPatches.cpp`, `src/Game/BusFloorPolicy.h` | Client: Boden am eigenen Busplatz prüfen, Figur ohne Boden am Busort halten | `tests/GameTests/BusFloorPolicyTests.cpp` |
| `src/Game/RemoteP2Patches.cpp`, `src/Game/RemoteP2Policy.h` | Host: entfernter Spieler als P2 | `tests/GameTests/PatchTests.cpp` |
| `src/Game/LocalDevicePatches.cpp`, `src/Game/LocalDevicePolicy.h` | Lokale Geräte bleiben bei Spieler 0 | `tests/GameTests/PatchTests.cpp` |
| `src/Game/CameraPatches.cpp` | Gemeinsame Kamera, Bewegungs-Restrictor ohne GameState | `tests/GameTests/PatchTests.cpp` (nur Restrictor) |
| `src/Game/MovementPatches.cpp`, `src/Game/MovementPolicy.h`, `src/Game/DashAdapter.h` | Dodge-Reparaturen, Lauftempo und Beschleunigung aus dem Attribut, fremde Figur folgt repliziertem Ort | `tests/GameTests/PatchTests.cpp`, `tests/NetTests/MovementTests.cpp` |
| `src/Game/CharacterPatches.cpp`, `src/Sync/Players/CharacterSync.cpp` | Figurenwahl in beide Richtungen | – |
| `src/Game/ClientPatches.cpp` | Client: Stellen, die Host-Objekte erwarten | `tests/GameTests/PatchTests.cpp` |

## Offene Fehler

- Client fällt live im Labyrinth-Event durch die Karte, Ursache offen. Die Vermutung „Host setzt die Figuren zu früh an den Bus“ ist widerlegt, siehe [spawn-nach-kartenaufbau](../faelle/spawn-nach-kartenaufbau.md). Offen sind zwei Vermutungen: [busplatz-vor-figur](../faelle/busplatz-vor-figur.md) (Busplatz des Clients vor seiner eigenen Figur) und [busplatz-ohne-boden](../faelle/busplatz-ohne-boden.md) (Boden am Bus noch ohne Kollision; auf einem PC nie gesehen, mit Testschalter nachgestellt, die Mod hält die Figur jetzt bis zum Boden).

## Historie

Wortgleiche Befunde, nur zum Nachschlagen, nicht als Ist-Zustand lesen:

- [Kamera und Sichtbereich](../archive/GAME_ANALYSIS.md#kamera-und-sichtbereich)
- [Gemeinsame Kamera in der Mod (`kamera-code`)](../archive/GAME_ANALYSIS.md#gemeinsame-kamera-in-der-mod-kamera-code)
- [ViewTarget beim Client (`kamera-viewtarget`)](../archive/GAME_ANALYSIS.md#viewtarget-beim-client-kamera-viewtarget)
- [Dodge beim Client (`bewegung`)](../archive/GAME_ANALYSIS.md#dodge-beim-client-bewegung)
- [Dodge des Clients beim Host (`ausweichen-client`)](../archive/GAME_ANALYSIS.md#dodge-des-clients-beim-host-ausweichen-client)
- [Dodge im Zwei-PC-Test (`dodge-client-live`)](../archive/GAME_ANALYSIS.md#dodge-im-zwei-pc-test-dodge-client-live)
- [Tod des Clients (`tod-client`)](../archive/GAME_ANALYSIS.md#tod-des-clients-tod-client)
- [Client-Absturz im Restrictor (`tick-absturz`)](../archive/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz)
- [Lokales Coop](../archive/GAME_ANALYSIS.md#lokales-coop)
- [Blueprint-Stichprobe (`blueprint-logik`)](../archive/GAME_ANALYSIS.md#blueprint-stichprobe-blueprint-logik)
- [Lokale Coop-Schnittstellen](../archive/GAME_ANALYSIS.md#lokale-coop-schnittstellen)
- [Remote-P2: geprüfte Anbindung und Grenze der lokalen UI](../archive/GAME_ANALYSIS.md#remote-p2-geprüfte-anbindung-und-grenze-der-lokalen-ui)
- [Eingaben getrennt (`eingaben`)](../archive/GAME_ANALYSIS.md#eingaben-getrennt-eingaben)
- [Coop-Ablauf](../archive/GAME_ANALYSIS.md#coop-ablauf)
- [Charakterwahl in der MetaBase (`charakter-wahl`)](../archive/GAME_ANALYSIS.md#charakterwahl-in-der-metabase-charakter-wahl)
- [Host-Absturz beim Rundenstart (`host-absturz-start`)](../archive/GAME_ANALYSIS.md#host-absturz-beim-rundenstart-host-absturz-start)
