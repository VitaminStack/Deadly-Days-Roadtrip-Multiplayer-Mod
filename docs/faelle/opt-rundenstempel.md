# opt-rundenstempel: Rundenübersicht des Clients stempelt den Tag nicht ab

## Reproduktion

Gemeldet im Live-Test vom 10.11.2026 (zwei PCs, Build `26495302`): Der Client stempelt das geschaffte Level nicht ab. Am 13.11.2026
abends: Der Stempel erscheint falsch oder ohne Animation. Nachstellen auf einem PC: Sitzung mit frischem Client-Prozess, Tag 1 mit
`!finish` beim Host beenden, Stempel beider Seiten mit `tools/spike/round_stamps.py <host-pid> <client-pid> ...` lesen.

## Belege

- [Belegt, Disassembly 26786272] `URoundEndPanel::GenerateStats` setzt die Stempel selbst. Die Schleife läuft über die Tage der Seite
  bis zur Tageskennung des `UDDGameStatsSubsystem`. Frühere Tage bekommen `SetupDayStamp(Tag, Sieg, sofort)`. Der aktuelle Tag hängt
  an zwei lokalen Merkern:
  - Sind `CurrentMissionIndex` und `CachedCurrentMissionIndex` der Route in der GameInstance verschieden und nicht alle Figuren tot,
    nimmt er den Weg für einen Event-Stempel (`0x5AD6CD0`) statt `SetupDayStamp`.
  - Sonst `SetupDayStamp(Tag, nicht alle tot, animiert)`: Siegstempel mit `StickerPositive` oder `SetFailedSticker`.
  - Das Bild kommt aus `CharacterVictoryStickerAnimation` der Figur mit Index 0 im `CharacterSelectionManager`.
- [Belegt, Disassembly] `TriggerEndRound` (`0x5909FE0`) setzt am Ende `Cached = Current`, `Current = clamp(Current+1, 0, Kette-1)`.
  Der Host bekommt je Tag eine neue Route mit einer Mission, in der Mission `0/-1`, nach dem Rundenende `0/0`.
- [Belegt, Spiel 26.11.2026, ein PC, main `18fb62c`] Der Client hat keine eigene Route (Kette leer, `Current` am Anfang -1).
  - Nach seinem ersten Rundenende steht `Current 0`, `Cached -1`. Am Tagesende 1 war der Stempel beim Host sichtbar (`survivor`),
    beim Client versteckt, Event-Bilder leer. PrintWindow bestätigt das: Host mit Stempel, Client mit leerem Feld.
  - Ab Tag 2 steht `0/0`: Tag 1 und 2 waren bei beiden gleich. Das passt zum Befund aus
    [opt-ergebnis-werte](opt-ergebnis-werte.md#grenzen) („an Tag 2 standen beide“).
- [Belegt, Spiel] Niederlage an Tag 3 (`DD_DamagePlayerAtIndex` beim Host): Der Host stempelt Tag 3 mit `died`, der Client mit dem
  Siegstempel `survivor`. Beim Client gelten die Figuren für `GenerateStats` nicht als tot (wie in opt-ergebnis-werte). Die
  Übersicht blättert bei beiden sofort auf die Wertung, der Unterschied liegt im Stempel und im Ton.
- [Belegt, Disassembly] `0x5AFE9F0` (Killer-Feld) läuft nur, wenn alle tot sind. Ohne Einträge in seiner Liste springt es ans
  Ende, ein ungültiger Zeiger führt auf denselben Ausgang. Damit ist es beim Client mit dessen eigenen Daten sicher aufrufbar.
- Vor `opt-ergebnis-werte` stand die Tageskennung des Clients auf 0. Dann läuft die Schleife gar nicht, es gibt keinen Stempel. Das
  war der Stand im Live-Test vom 10.11.

## Fix (PR `task/opt-rundenstempel`)

- **Lokal bleibt** alles: Übersicht, `GenerateStats`, Stempel, Animation, Ton. Keine neue Nachricht, kein neues Format, `ModVersion`
  bleibt 19. Die Run-Info des Clients (`CurrentRunInfo`) schreibt die Mod nicht.
- **Es entscheidet** der Host: Rundenende und Niederlage kommen schon mit `0x401`, der Tag mit `0x402`.
- **Wiederverwendet** wird `FollowRoundEnd` (`MapPatches.cpp`): Folgt der Client einem Rundenende des Hosts, merkt sich die Mod die
  Welt und ob es eine Niederlage war (`RoundStats::NoteRoundEndFollowed`).
- Zwei Code-Höhlen in `GenerateStats`, nur Patch-Eintrag „Tagesstempel der Rundenübersicht beim Client“ der Liste Map. Beide rufen
  die Regel mit dem Wert des Spiels und schreiben das Ergebnis an seine Stelle:
  - am Merker „alle tot“ (`0x5AE3676`): beim Client tot genau bei Niederlage des Hosts,
  - am Routenvergleich (`0x5AE3D84`): beim Client kein Event-Stempel.
- Host und Solo behalten den Wert des Spiels. Fehlt eine Signatur, fällt nur dieser Eintrag aus, die Werte aus `opt-ergebnis-werte`
  bleiben.
- **Regeln** `PanelEventStamp`, `PanelAllDead` in `src/Game/RoundStatsPolicy.h`, getestet in `tests/GameTests/RoundStatsTests.cpp`.
- Prüfliste ARCHITECTURE: lokal bleibt die ganze Darstellung; geteilt wird nichts Neues; der Host entscheidet; wiederverwendet werden
  Rundenende, Niederlage und Tag. Ablehnung und Wiederholung gibt es nicht. Ein Weltwechsel hebt den Merker auf (Objektverweis mit
  Seriennummer), ein neues Rundenende setzt ihn neu.

## Abnahme (26.11.2026, ein PC, Host und zwei Clients, Mod über die Lobby, nur Spike-Befehle)

- `Signaturen: 134 Adressen geprüft`, `Patches: 63 gesetzt` bei allen drei, kein Absturz.
- Tagesende 1 mit frischen Client-Prozessen: Beide Clients stempeln Tag 1 wie der Host (`survivor`, sichtbar, Animation
  gelaufen), ihre Route steht unverändert auf `-1/0`. Je Instanz genau ein `meta_stickerGood` (`!fx`). Die Bildserie zeigt den
  Stempel beim Client wie beim Host.
- Tagesende 2: Tag 1 und 2 gestempelt bei allen drei, je ein `meta_stickerGood`, `round_stats.py` „gleich“ für beide Clients.
- Niederlage an Tag 3 (Tod aller drei über `DD_DamagePlayerAtIndex`): Alle drei stempeln Tag 3 mit `died`. Wertung (2), Werte und
  „Du bist gestorben“ wie beim Host, die Knöpfe des Clients bleiben wie in opt-ergebnis-werte verborgen.
- Niederlage an Tag 1 mit frischem Client-Prozess (Host und ein Client, Route des Clients `-1/0`): Client stempelt `died` wie der
  Host, `round_stats.py` „gleich“.
- `Management_SaveSlot_0.sav` der Clients blieb über Tag 2 bis zur Niederlage und über die Niederlage an Tag 1 Byte für Byte gleich.

## Grenzen

- **Zweiter Stempelton bei der Niederlage:** Die Clients spielen `meta_stickerBad` zweimal, der Host einmal. Das Umschalten auf den
  Ergebnisbildschirm (`ShowRunEndPanel`, run-ende) ruft `RefreshAsGameOver` (`0x5AF28A0`), und das ruft `GenerateStats` noch einmal
  ([Belegt, Disassembly]). Vor diesem Fix waren es zwei Siegtöne (aus dem Ablauf abgeleitet, nicht gemessen). Gehört zum Umschalten,
  nicht zum Stempel.
- **Event-Stempel:** Der Client stempelt immer den Tag. Zeigt der Host einen Event-Stempel (Route mit mehr als einer Mission), sieht
  der Client den Tagesstempel. Nicht gemessen: Wann der Host so eine Route hat, ist offen. Die Bilder dafür kommen aus einem
  Subsystem (`0x582F670`, `0x58EFD10`), das der Client nicht gefüllt hat (Vermutung). Gehört zur Run-Info (`opt-routenkarte`).
- **Bild des Stempels:** Beide Seiten nehmen die Figur mit Index 0 ihres eigenen `CharacterSelectionManager`. Im Test hatten Host und
  Client dieselbe Standardfigur. Wählt der Client eine andere Figur, sieht er vermutlich deren Stempel. Nicht gemessen.
