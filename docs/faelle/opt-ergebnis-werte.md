# opt-ergebnis-werte: Rundenübersicht des Clients zeigt nur Nullen

## Reproduktion

Gemeldet am 10.11.2026 (`run-ende`, Build `26495302`): Am Run-Ende zeigt der Ergebnisbildschirm des Clients nur Nullen. Im Live-Test
vom 12.11.2026 zeigte die tägliche Rundenübersicht des Clients einmal nur Nullen und einmal rote Abzüge. Nachstellen auf einem PC:
Mission spielen, `!finish` beim Host, Werte beider Seiten mit `tools/spike/round_stats.py <host-pid> <client-pid> ...` vergleichen.

## Belege

- [Belegt, Disassembly 26786272] Das `UDDGameStatsSubsystem` führt die Werte gesamt in einer `TMultiMap<FMissionTimeIdentifier, FGameStat>`
  bei `+0x198` und je Spieler in einer `TMap<int32, gleicher Container>` bei `+0x1E8`. `GetGameStat` sucht dort. Layout in
  [OFFSETS.md](../OFFSETS.md#werte-der-rundenübersicht-opt-ergebnis-werte).
- [Belegt, Disassembly] Gebucht wird über `IncrementGameStat`: erst der Gesamtwert, dann eine Meldung (Vermutung:
  `ExecuteStatNotification`, daran hängen Erfolge, `DDStatBasedObjective` und `ItemEffectGameStats`), dann der Wert je Spieler.
  `MaxThreatLevel` und `DPS` nehmen das Maximum, die übrigen Typen die Summe.
- [Belegt, Disassembly] `URoundEndPanel::GenerateStats` zeigt den Tag aus `CurrentGameData.TimeIdentifier` und vergleicht mit dem Vortag.
  Die Summen aller Tage zeigt es nur, wenn `WasFinalMission` wahr ist oder alle Figuren tot sind. `OnRoundChanged` des Subsystems ruft
  `StopTracking`, das am Tagesende Dauer, DPS, Leben und Gefahr unter der aktuellen Tageskennung bucht. `StartNewDay` zählt die
  Tageskennung hoch, `OnRunStarted` leert alles.
- [Belegt, Spiel 26.11.2026, ein PC, Host und zwei Clients] In der Mission hat nur der Host Werte (Tag 1: 18 Kills, 900 Schaden,
  36 Treffer), beide Clients keine, ihre Tageskennung steht auf 0.0. Der Client startet nie selbst einen Run oder Tag. Am Rundenende
  bucht er nur sein eigenes Tagesende unter 0.0 und zeigt „TAG 0/8“ mit Nullen. Mit den Werten des Hosts unter Tag 1 kamen genau
  die roten Abzüge aus dem Live-Test (−24 Kills, −1.200 Schaden).
- [Belegt, Spiel] Bei der Niederlage zeigt der Host die Summen (alle Figuren tot), der Client nur den letzten Tag. Beim Client sind
  die Figuren für `GenerateStats` nicht tot.
- [Belegt, Speicher] Tag und Teilmission können -1 sein (Tageskennung 1.-1 einer laufenden Instanz). Die Regeln lassen -1 zu.

## Fix (PR `task/opt-ergebnis-werte`)

- **Lokal bleibt** die ganze Übersicht beim Client: Öffnen über das native Rundenende, `GenerateStats`, Wertung, Animationen, Knöpfe.
- **Geteilt wird** nur, was die Übersicht liest: Tageskennung und alle Werte des Hosts, gesamt und je Spieler, als Vollbild aller
  Tage. Neue Nachricht `0x402` vor dem `0x401` desselben Rundenendes ([Kontrollkanal](../CONTROL_PROTOCOL.md#rundenwerte-mod-kompatibilität-18)),
  `ModVersion` 18. Jeder Client (bis zu drei) bekommt dasselbe Bild.
- **Es entscheidet** der Host: Er bucht, der Client übernimmt nur, direkt vor `FollowHostRoundEnd`. Er schreibt über die nativen
  Helfer zum Suchen und Anlegen, nicht über `IncrementGameStat`, damit beim Client keine Erfolge, Ziele oder Item-Effekte auslösen.
  Werte, die der Host nicht hat, werden 0. Ein Hook auf `OnRoundChanged` setzt den Stand nach der eigenen Tagesend-Buchung des
  Clients noch einmal. Enthält der Stand des Clients Tage nach der Tageskennung des Hosts (früherer Run), ruft er zuerst `OnRunStarted`.
- **Run-Ende:** Nur der Aufruf von `WasFinalMission` in `GenerateStats` ist umgeleitet. Hat der Host den Run in dieser Welt beendet,
  liefert er beim Client wahr, die Übersicht zeigt dann wie beim Host die Summen. Ratables und Blueprints rufen `WasFinalMission` unverändert.
- **Sieg:** Mit der Tageskennung des Hosts hält der Blueprint auch den Client für das Ende der Route. Dann zeigt er die Wertung wie
  beim Host, aber auch „Zurück zur Basis“ und „Endlosen Modus starten“. Gemessen ließ „Zurück zur Basis“ den Client allein in seine
  MetaBase reisen und die Sitzung verlassen. Der Endlosmodus öffnete seinen Bestätigungsdialog. Ohne die Tageskennung fehlte dagegen
  die Wertung. Darum setzt die Mod im Sekundentakt `SetContinueButton` und blendet `SizeBox-Endlessmode` aus, solange einer der
  beiden sichtbar ist (Feldnamen mit Bindestrich, das SDK schreibt Unterstriche). „Fortsetzen“ führt wie vor dem Fix ins Lager.
- **Wiederverwendet** wird der Weg des Rundenendes (`MapSync.cpp`, `FollowRoundEnd` in `MapPatches.cpp`). Kommen keine Werte, öffnet
  der Client nach 2 s mit den eigenen und schreibt eine Warnung.
- **Regeln** in `src/Game/RoundStatsPolicy.h` (Schreibplan, früherer Run, Summen am Run-Ende, Grenze von 3000 Werten, Warten),
  getestet in `tests/GameTests/RoundStatsTests.cpp`; Format in `tests/NetTests/MapTests.cpp`.
- Die nativen Stellen sind der Patch „Werte der Rundenübersicht vom Host“ der Liste Map (Prüfbytes, Sprungziele, Signaturen). Fehlt
  eine, bleibt nur dieser Teil aus, die Karte läuft weiter (`TickOptional`).

## Abnahme (26.11.2026, ein PC, Host und zwei Clients, Mod über die Lobby, nur Spike-Befehle)

- Tag 1, Tag 2, Niederlage an Tag 1 (Tod aller drei über `DD_DamagePlayerAtIndex`), Sieg über `DD_SetCurrentDay 8`: `round_stats.py` meldet bei allen drei dieselbe
  Tageskennung und dieselben Werte („gleich“). Die Bilder beider Clients zeigen dieselbe Übersicht wie der Host: Überschrift, Werte,
  Vortagsdifferenzen, an Tag 2 die Tagesstempel, am Run-Ende Summen mit „Gesamt/Max/Durchschnitt“ und dieselbe Wertung (Niederlage
  1 und 2, Sieg 42 mit „Runde gewonnen“). Am Sieg hat der Client nur „Fortsetzen“. Es führte ihn ins Lager, die Rückkehr des
  Hosts nahm alle drei in die MetaBase mit.
- `Management_SaveSlot_0.sav` beider Clients blieb über Niederlage, Sieg und Rückkehr zur Basis Byte für Byte gleich, beim Host
  änderte er sich. Der Run-Slot der Clients blieb leer (`run_save.py`).
- Früherer Run: Beim Client die Tageskennung in der Mission auf 8.0 gesetzt (Test, Speicher der eigenen Instanz). Am Rundenende
  „stammt aus einem früheren Run, Subsystem zurückgesetzt“, danach gleiche Werte. Ein echter zweiter Run im selben Prozess lief nicht:
  Mit dem Fortschritt nach dem Sieg fanden die Spike-Befehle die Routenknoten der MetaBase nicht mehr.
- `Signaturen: 131 Adressen geprüft`, `Patches: 61 gesetzt` bei allen drei, kein Absturz durch den Fix. Tagesende und Niederlage
  liefen zuletzt auf dem Stand vor der Knopfregel am Sieg (betrifft nur den Sieg), der Sieg auf dem Endstand.
- **Testhinweis:** `!shop call WP_Round_Stats_C …BackToBaseButton…` stürzt den Host ab (Lesen `0x110`): Der Spike ruft `ReturnFromRun`
  auch an der Vorlage ohne Besitzer. `!shop callat <n>` mit der Instanz, deren Outer-Kette an der GameInstance hängt, geht.

## Grenzen

- Nur die Werte des Subsystems. „Stärkste Items“ (Kills je Item) fehlt am Run-Ende beim Client. Killer, Wertung nach Bossen
  (`EnemyKillsPerType`) und Erfolge kommen aus anderen Quellen, nicht geprüft.
- Der Tagesstempel des ersten Tags fehlte beim Client am Tagesende 1, an Tag 2 standen beide. Nicht weiter untersucht.
- Werte je Spieler: In allen Läufen führte auch der Host keine (nur Gesamtwerte). Der Weg ist da, aber im Spiel nicht geprüft. Die
  Übersicht hat nur Felder für P1 und P2.
- Endlosmodus nach einem Sieg: Der Client folgt nur über „Fortsetzen“ und die Reise des Hosts, nicht geprüft.
- Nicht geprüft: zwei PCs, Bosskampf, Wiedereintritt mitten in der Runde.
