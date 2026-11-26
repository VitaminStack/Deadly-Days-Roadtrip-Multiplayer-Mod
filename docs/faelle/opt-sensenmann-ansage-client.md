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
- [Belegt, SDK und Spiel] Empfänger von `OnReaperSpawned`: `WBP_Stopwatch_C.OnReaperSpawned` (Stoppuhr: `ReaperAlert`, `ReaperColor`,
  `IsReaperActive`) und `UEnemyBiomeSpawnExpansionComponent::HandleReaperSpawned` (Spawn-Biome, Sache des Hosts). Gemessen sind beide
  auch beim Client gebunden, ein nativer Broadcast dort würde also die Biom-Ausweitung mit auslösen. `WBP_DayClock_C` bindet
  `OnReaperSpawned` nicht.
- [Belegt, Spiel] Die Sprechdefinition `Speech_ReaperSpawned` (Kennung `ReaperSpawned`) hat `PlayFrequency` Unlimited, `Delay` 4,5 s
  und `duration` 4 s: Die Blase erscheint 4,5 s nach dem Aufruf. Haben beide Spieler dieselbe Figur, steht sie an beiden Figuren, beim
  Host nativ genauso.
- [Belegt, Spiel] Ohne Ansage (zweite Sitzung, als der Adapter den Sensenmann wegen des Klassen-Caches unten nicht erkannte, also wie
  ohne Fix): Sensenmann beim Client 50 s lang mit Rolle 1, `WasReaperSpawned` des Client-Spawners 0, Stoppuhr `IsReaperActive` 0;
  beim Host beide 1. Ein Lauf mit dem Stand von `main` ist nicht gefahren.
- [Belegt, Spiel] Der Sensenmann des Hosts kommt beim Client ohne merkliche Verzögerung an: in `DDDynamic_Map` (Start über `-Map`) unter
  1 s, auf einer Karte über die Routenkarte (`MGP_City_LittleIndia`, Tag 1) ebenso unter 1 s nach seinem Erscheinen beim Host. Die rund 30 s
  aus `karte-nach-labyrinth-absturz` waren ab dem Befehl gemessen, ohne Zeitpunkt beim Host. Eine eigene Nachricht für die Ansage ist
  darum nicht nötig.

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

Der Adapter erkennt Spawner und Sensenmann mit `Objects::IsA`. Ein eigener Cache nur nach Klassenadresse verpasste im ersten Spieltest
den Sensenmann (Sensenmann beim Client 50 s da, keine Ansage): Seine Klasse lädt beim Client erst mit ihm. **[Vermutung]** Sie lag auf
der Adresse einer schon gemerkten, inzwischen freigegebenen Klasse; `Objects::IsA` prüft dafür den Namen mit. `EnemySoundPatches.cpp`
hat denselben Cache nach Adresse (`ActorKinds`), nicht geändert.

## Spieltest

26.11.2026, ein PC, zwei Instanzen, Build `26786272`, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map -Godmode`,
ohne Maus und Tastatur. `Patches: 62 gesetzt` bei beiden, kein `[ERROR]`, keine Absturzberichte.

| Schritt | Ergebnis |
|---|---|
| `!shop reaper 3` nur beim Client | „Spawn beim Client unterdrückt“, kein Sensenmann, keine Ansage, `WasReaperSpawned` bleibt 0 |
| `!shop reaper 3` beim Host | Host erzeugt den Sensenmann 3,0 bis 3,6 s nach dem Befehl; Client-Log 3,5 s nach dem Befehl „Ansage lokal: Empfänger [WBP_Stopwatch_C.OnReaperSpawned], ausgelassen [EnemyBiomeSpawnExpansionComponent.HandleReaperSpawned], Ton ja, Sprechblase ja“; `WasReaperSpawned` 1, Stoppuhr `IsReaperActive` 1 bei beiden |
| Reise des Hosts (`travel`) in eine neue Missionskarte | neuer Spawner mit `WasReaperSpawned` 0, kein Sensenmann, keine Ansage |
| `!shop reaper 3` beim Host auf der neuen Karte | Ansage beim Client 3,4 s nach dem Befehl, einmal; Bilder (PrintWindow) 5,5 s danach: Sprechblase „Was war das? Zeit zu verschwinden, aber sofort.“ und Stoppuhr mit Sensenmann bei Host und Client gleich |
| erste Sitzung, Sperre beim Client zweimal von Hand auf 0 | je genau eine weitere Ansage, Blase nach 4,5 s im Bild |
| dritte Sitzung, MetaBase → Routenknoten Tag 1 (`MGP_City_LittleIndia`), `!shop reaper 3` beim Host | Ansage beim Client 3,6 s nach dem Befehl, Sensenmann beim Client unter 1 s nach dem Host (Probe liest Host und Client nacheinander); `!fx dump`: `reaper_spawned` genau einmal beim Host und einmal beim Client |
| Wiedereintritt (`Test-Rejoin.ps1`) mit stehendem Sensenmann | Client verlässt die Mission und tritt der neuen Lobby des Hosts bei (das Skript meldete Zeitablauf, der Beitritt kam danach); als der Sensenmann bei ihm ankam, genau eine Ansage |

Der Takt hält den Frame einmal je Ansage rund 10 bis 15 ms auf (Suche der eigenen Sprechblasen-Komponente im Objekt-Array). Danach sucht
er in dieser Welt nicht mehr.

Nicht geprüft: Ton nach Gehör (Instanzen stumm; belegt über `!fx` und „Ton ja“), zwei PCs über das Internet, der Sensenmann nach den
natürlichen 12 Minuten, ein Sensenmann fern der Spieler, verschiedene Figuren bei Host und Client (die Blase des Clients nimmt den Sprecher der eigenen Figur), drei oder vier Spieler.
