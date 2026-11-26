# opt-sensenmann-ansage-client: Dem Client fehlt die Ansage des Sensenmanns

## Reproduktion

Ein PC, zwei Instanzen: `Start-Session.ps1 -Build -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map -Brief`, dann beim Host
`Send-SpikeCommand <host-pid> '!shop reaper 3'`. `py tools/spike/reaper_state.py <host-pid> <client-pid> --watch 90` zeigt je Seite
Spawner (`WasReaperSpawned`, Empfänger von `OnReaperSpawned`), den Sensenmann und `WBP_Stopwatch_C.IsReaperActive`.
Ohne Fix steht beim Client der Sensenmann, `WasReaperSpawned` bleibt 0, die Stoppuhr bleibt ohne Sensenmann-Anzeige, keine Sprechblase.

## Belege

- [Belegt, Disassembly] Die Spawn-Funktion des `AReaperSpawner` (`0x58C9E20`) tut nach dem Erzeugen des Sensenmanns (`0x58CA1EF`):
  `reaper+0x878 = spawner`, Broadcast von `OnReaperSpawned` (`+0x498`, `0x24E95F0`), Ton `ReaperSpawnedSoundEvent` (`+0x500`) über
  `UFMODBlueprintStatics::PlayEvent2D` (`0x47B9D20`, baut eine Einheits-Transformation und ruft `PlayEventAtLocation`), Sprechblase
  `ReaperSpawnedSpeech` (`+0x4F0`) über `UCharacterSpeechSubSystem::TriggerSpeechDefinition` (`0x5A53780`) und `WasReaperSpawned`
  (`+0x529`) = 1. Seit `karte-nach-labyrinth-absturz` kehrt der Hook beim Client vor all dem zurück.
- [Belegt, Disassembly und .rdata] Die Sprechblase des Hosts: `UseAbsolutePosition` 1, `ScreenLocation` (-1, -1), `ScreenPercentages`
  (0,5, 0,75), `TweenTime` 0,5, `TweenType` 8, kein Tween. Sprecher ist der `SpeakerDefinition` der Sprechblase
  (`USpeechBubbleWidgetComponent.SpeechBubbleWidget` → `UCharacterSpeechBubble.SpeakerDefinition`) der ersten Spielerfigur ohne ein
  bestimmtes Gameplay-Tag. **[Vermutung]** Das Tag ist der Tod der Figur.
- [Belegt, SDK] Empfänger von `OnReaperSpawned` laut SDK: `WBP_Stopwatch_C.OnReaperSpawned` (Stoppuhr: `ReaperAlert`, `ReaperColor`,
  `IsReaperActive`) und `UEnemyBiomeSpawnExpansionComponent::HandleReaperSpawned` (Spawn-Biome, Sache des Hosts).
  `WBP_DayClock_C` bindet `OnReaperSpawned` nicht.

## Prüfliste ([ARCHITECTURE.md](../ARCHITECTURE.md#verbindliches-architekturprinzip-lokal-zuerst-minimal-synchronisieren))

- **Was bleibt lokal?** Alles: Stoppuhr, Ton und Sprechblase führt der Client mit seinen eigenen Objekten aus, wie die Spawn-Funktion des Hosts.
- **Was wird geteilt?** Nichts Neues. Das Signal ist der Sensenmann des Hosts selbst, der wie jeder Gegner nativ repliziert.
  Keine Nachricht, keine `ModVersion`-Erhöhung.
- **Wer entscheidet?** Der Host, über Spawn, Bewegung und Schaden. Der Client erzeugt weiter keinen Sensenmann (`ReaperMaySpawn`) und
  führt die Ausweitung der Spawn-Biome nicht aus (`ReaperReceiverRunsOnClient`).
- **Welcher Weg wird wiederverwendet?** Die native Replikation des Gegners, der Broadcast des Spawners je Empfänger über `ProcessEvent`
  (wie bei den Lebensleisten), `FmodSound` und das Sprach-Subsystem des Spiels. Ersetzt wird nichts.
- **Wiederholung und Neuaufbau:** `WasReaperSpawned` des Spawners ist die Sperre gegen eine zweite Ansage; jede neue Karte bringt einen neuen
  Spawner. Tritt der Client einer Karte bei, in der der Sensenmann schon steht, hört er die Ansage, sobald dieser bei ihm ankommt.

## Fix

`src/Game/ReaperAnnouncementAdapter.cpp` (Takt je Sekunde, nur beim Client), Regeln in `src/Game/ReaperAnnouncementPolicy.h` mit Test
`tests/GameTests/ReaperAnnouncementPolicyTests.cpp`. Steht ein `BP_ReaperEnemy_C` in der Welt und hat der Spawner ihn noch nicht gemeldet,
setzt der Client `WasReaperSpawned`, ruft die Empfänger von `OnReaperSpawned` außer der Biom-Ausweitung, spielt den Ton 2D und startet die
Sprechblase mit dem Sprecher der eigenen Figur. Logzeile „Sensenmann: beim Client angekommen, Ansage lokal …“.

## Spieltest

Ausstehend.
