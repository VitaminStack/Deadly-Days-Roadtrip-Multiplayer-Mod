# projekt-aufraeumen-spike: Braucht die Mod den Spike noch?

Teil von `projekt-aufraeumen` (Aufgaben 16 und 17 des [Plans](../superpowers/plans/2026-11-24-projekt-aufraeumen.md)).
Ziel: Spike `research/spikes/a-connect` aus `main` entfernen, damit es nur noch eine Codebasis gibt.

## Reproduktion

Spieltest ohne Spike auf einem Windows-PC mit Spiel, zwei Instanzen:
`Start-Session.ps1 -Mod -Tunnel Lobby -NoSpike -Brief`, dann `Run-Route.ps1 -Start -NoSpike` über Tag 1 bis zum Rundenende mit Lager und Routenkarte beim Client.

## Belege

- [Belegt] Kopplung an den Spike (Stand dieses Branches, `git grep 'spikes/a-connect'`):
  - Nur Kommentare und Herkunftsangaben: `src/Game/ConsoleAdapter.cpp`, `src/Game/PatchPolicy.h`, `src/Game/RemoteP2Patches.cpp`, `src/Game/RemoteP2Policy.h`, `src/Net/SteamSockets.cpp`, `src/Core/README.md`, `src/Game/README.md`, `src/Net/README.md`, `tools/spike/Start-Session.ps1` (Zeile zu Builds).
  - Tests, die Spike-Header einbinden: `tests/attribute_readiness.cpp`, `tests/blockade_authority.cpp`, `tests/remote_p2.cpp`, `tests/round_phase.cpp`, `tests/round_phase_adapter.cpp`, `tests/round_phase_runtime.cpp`, `tests/spawnedplayers_set.cpp`, dazu `tests/Run-RoundPhaseRuntime.ps1` (liest `Mission.cpp`).
  - Build: `tools/spike/Build-Spike.ps1` baut `spike.dll`; `Start-Session.ps1` lädt sie ohne `-NoSpike`.
- [Belegt] Doppelt vorhanden ist nur `RemoteP2Policy.h`. Die Fassung in `src/Game/` (Namensraum `Game::Patches`) weicht von der Spike-Fassung (`Spike`) ab; die Prüfung der Hash-Kette ist in `src/` anders formuliert.
- [Belegt] Spike-Header ohne Gegenstück in `src/`: `AttributeReadiness.h`, `BlockadeAuthority.h`, `NetGuidExpiryGuard.h`, `NetGuidPolicy.h`, `RoundPhase*.h`, `SneakTargetReadiness.h`, `SpawnedPlayersSet.h`, `Spike.h`, `TestGodmodePolicy.h`.
  Gleichnamige Regeln stehen in `src/Game/PatchPolicy.h` (`AttributesComplete`, `BlockadeMayStart`, `RetainNetGuidPath`, `ReuseSneakTarget`, `ControllerSet`).
- [Belegt] Die Mod folgt dem Rundenende des Hosts selbst (`ClientMayFollowRoundEnd` in `src/Game/MapPatches.cpp`, Lager über `ClientReopensLager` in `src/Game/RewardPatches.cpp`).

## Hypothesen

- [Vermutung] Die Rundenphase des Spikes (`Mission.cpp`, `RoundPhase*.h`) wird nicht mehr gebraucht; `src/Game/README.md` (Abschnitt „Ohne Spike“, Stand 05.11.2026) nennt sie noch als Bedarf. Prüfidee: Spieltest oben.
- [Vermutung] Die Regeln in `PatchPolicy.h` decken die Spike-Header ab. Prüfidee: Tests unter `tests/*.cpp` gegen die `src/`-Funktionen umschreiben und vergleichen.

## Nächster Schritt

Spieltest ohne Spike auf einem Windows-PC (braucht den Menschen). Danach Aufgabe 17: Tag `spike-final`, Tests auf `src/` umziehen oder mit dem Spike löschen, `Build-Spike.ps1` und Spike-Schalter entfernen.

## Abnahme

`git grep 'spikes/a-connect'` trifft nur noch unter `docs/archive/`; CI baut und testet; ein Spieltest ohne Spike läuft über Tag 1 bis zum Rundenende ohne Absturz.
