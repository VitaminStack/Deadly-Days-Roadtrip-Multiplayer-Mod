# opt-ergebnis-werte: Rundenübersicht des Clients zeigt nur Nullen

## Reproduktion

Gemeldet am 10.11.2026 (`run-ende`, Build `26495302`): Am Run-Ende zeigt der Ergebnisbildschirm des Clients nur Nullen. Im Live-Test
vom 12.11.2026 zeigte die tägliche Rundenübersicht des Clients einmal nur Nullen und einmal rote Abzüge. Nachstellen auf einem PC mit
zwei Instanzen: Mission spielen, `!finish` beim Host, die Übersicht beider Seiten vergleichen (`tools/spike/round_stats.py`).

## Belege

- [Belegt, Disassembly 26786272] Das `UDDGameStatsSubsystem` führt die Werte gesamt in einer `TMultiMap<FMissionTimeIdentifier, FGameStat>`
  bei `+0x198` und je Spieler in einer `TMap<int32, gleicher Container>` bei `+0x1E8`. `GetGameStat` sucht dort. Layout in
  [OFFSETS.md](../OFFSETS.md#werte-der-rundenübersicht-opt-ergebnis-werte).
- [Vermutung] Die Übersicht (`URoundEndPanel.GenerateStats`) und die Wertung (Ratables wie Kills, Nachtminuten) lesen beim Öffnen
  diese Werte. Disassembliert sind nur `GetGameStat`, `IncrementGameStat` und ihre Helfer.
- [Belegt, Disassembly] Gebucht wird über `IncrementGameStat`: erst der Gesamtwert, dann eine Meldung (Vermutung:
  `ExecuteStatNotification`, daran hängen Erfolge, `DDStatBasedObjective` und `ItemEffectGameStats`), dann der Wert je Spieler.
  `MaxThreatLevel` und `DPS` nehmen das Maximum, die übrigen Typen die Summe.
- [Belegt, SDK] Das Subsystem ist ein `GameInstanceSubsystem` ohne Replikation. Jede Instanz zählt nur, was bei ihr selbst gebucht wird.
  [Vermutung] Treffer (`OnEnemyHit`) und Kills bucht nur der Host, dort laufen Schaden und Gegner.
- [Belegt, Speicher] Tag und Teilmission können -1 sein: Eine laufende Instanz meldete die Tageskennung `CurrentGameData.TimeIdentifier`
  als 1.-1. Die Regeln lassen darum -1 zu.
- Messung im Spiel: siehe unten.

## Fix (PR `task/opt-ergebnis-werte`)

- **Lokal bleibt** die ganze Übersicht beim Client: Öffnen über das native Rundenende, `GenerateStats`, Wertung, Animationen, Knöpfe.
- **Geteilt wird** nur, was die Übersicht liest: alle Werte des Subsystems des Hosts, gesamt und je Spieler, als Vollbild aller Tage.
  Neue Nachricht `0x402` vor dem `0x401` desselben Rundenendes ([Kontrollkanal](../CONTROL_PROTOCOL.md#rundenwerte-mod-kompatibilität-18)),
  `ModVersion` 18. Jeder Client (bis zu drei) bekommt dieselben Werte.
- **Es entscheidet** der Host: Er bucht, der Client übernimmt nur. Der Client setzt seine Werte direkt in die Container, über die nativen
  Helfer zum Suchen und Anlegen (`0x58EBFD0`, `0x58F45A0` mit Erhöhung 0, `0x58D56F0`), nicht über `IncrementGameStat`, damit keine
  Erfolge, Ziele oder Item-Effekte beim Client auslösen. Werte, die der Host nicht hat, werden 0.
- **Wiederverwendet** wird der Weg des Rundenendes (`MapSync.cpp`, `FollowRoundEnd` in `MapPatches.cpp`): Der Client übernimmt die
  Werte direkt vor `FollowHostRoundEnd`. Kommen sie nicht, öffnet er nach 2 s mit den eigenen und schreibt eine Warnung.
- **Regeln** in `src/Game/RoundStatsPolicy.h` (Schreibplan, Grenze von 3000 Werten mit den jüngsten Tagen, Warten), getestet in
  `tests/GameTests/RoundStatsTests.cpp`; Format in `tests/NetTests/MapTests.cpp`.
- Die nativen Stellen sind als Patch „Werte der Rundenübersicht vom Host“ der Liste Map geprüft (Prüfbytes, Sprungziele, Signaturen).
  Fehlt eine, bleibt nur dieser Teil aus, die Karte läuft weiter (`TickOptional`).

## Grenzen

- Nur die Werte des Subsystems. Bestes Item und Killer der Übersicht sowie die Wertung nach Bossen (`EnemyKillsPerType`) und Erfolgen
  kommen aus anderen Quellen und bleiben die des Clients, siehe Messung.
- Die Übersicht hat nur Felder für P1 und P2. Bei drei Clients zeigt sie die Werte der Spielerindizes 0 und 1 wie beim Host.
