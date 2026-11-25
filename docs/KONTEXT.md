# Kontext-Landkarte

Je Thema die Datei mit dem Ist-Zustand, der Code und die Tests. Wer an einem Thema arbeitet, liest nur diese Zeile und die genannten Dateien.

| Thema | Ist-Zustand | Code | Tests | Offene Fälle |
|---|---|---|---|---|
| Netzwerk, Lobby, Verbindung | [NETZWERK](themen/NETZWERK.md) | `src/Net`, `src/Mod/Lobby.cpp` | `tests/NetTests` | [lobby-rauswurf](faelle/lobby-rauswurf.md) |
| Mehrere Spieler (bis zu vier) | [MEHR_SPIELER](MEHR_SPIELER.md) | `src/Game/RemotePlayers.h`, `RemoteP2Policy.h`, `src/Sync` | `tests/GameTests/PatchTests.cpp`, `tests/NetTests/TransportTests.cpp` | – |
| Spieler, Bewegung, Kamera | [SPIELER_UND_KAMERA](themen/SPIELER_UND_KAMERA.md) | `src/Game/PlayerPatches.cpp`, `CameraPatches.cpp`, `MovementPatches.cpp` | `tests/GameTests/PatchTests.cpp` | [spawn-nach-kartenaufbau](faelle/spawn-nach-kartenaufbau.md) |
| Reise, Karte, Runde | [REISE_UND_RUNDE](themen/REISE_UND_RUNDE.md) | `src/Game/MapPatches.cpp`, `TravelPatches.cpp`, `src/Sync/World` | `tests/NetTests/MapTests.cpp`, `ProgressTests.cpp` | [wiedereintritt-karte](faelle/wiedereintritt-karte.md) |
| Inventar, Lager, Airdrop, Handel | [INVENTAR_UND_HANDEL](themen/INVENTAR_UND_HANDEL.md) | `src/Game/InventoryEdit.cpp`, `RewardPatches.cpp`, `src/Sync/Inventory` | `tests/GameTests/InventoryEditTests.cpp`, `tests/NetTests/RewardTests.cpp` | [airdrop-auswahl-absturz](faelle/airdrop-auswahl-absturz.md) |
| Gegner und Bosse | [GEGNER_UND_BOSSE](themen/GEGNER_UND_BOSSE.md) | `src/Game/EnemyTelegraphPatches.cpp`, `BossBarAdapter.cpp`, `src/Sync/Enemies` | `tests/NetTests/TelegraphTests.cpp`, `tests/GameTests/BossBarPolicyTests.cpp` | – |
| Darstellung beim Client | [DARSTELLUNG](themen/DARSTELLUNG.md) | `src/Game/*BarPatches.cpp`, `BloodPatches.cpp`, `WeaponSoundPatches.cpp` | `tests/GameTests` | – |
| Spiel, SDK, Sperren | [SPIEL_UND_SDK](themen/SPIEL_UND_SDK.md) | `src/Game/Offsets.h`, `src/Core` | `tests/CoreTests` | – |
| Kontrollprotokoll | [CONTROL_PROTOCOL](CONTROL_PROTOCOL.md) | `src/Net/ControlProtocol.cpp`, `src/Sync` | `tests/NetTests/ProtocolTests.cpp` | – |
| Offsets und RVAs | [OFFSETS](OFFSETS.md) | `src/Game/AddressMap.h`, `PatchSignatures.inc` | `tests/test_patch_registry.py` | – |
| Spieltests | [TESTING](TESTING.md) | `tools/spike/*.ps1` | – | – |
| Aufgaben und Abschluss | [PROZESS](PROZESS.md) | `tools/tasks.py`, `tools/task_status.py` | `tests/test_task_status.py` | – |
| Entscheidungen, Begriffe | [ENTSCHEIDUNGEN](ENTSCHEIDUNGEN.md), [GLOSSAR](GLOSSAR.md) | – | – | – |

## Leseregeln

- Archiv (`docs/archive/`), [ERLEDIGT](ERLEDIGT.md) und Dateien über 60 KB nie ganz lesen, nur per `grep -n` und Abschnitt.
- Das Archiv ist Geschichte. Eine Aussage von dort gilt nur, wenn eine Themen-Datei oder der Code sie bestätigt.
- Modulregeln stehen in der `CLAUDE.md` des jeweiligen Ordners unter `src/` und `tools/`.
