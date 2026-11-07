# Enemies

Gegner-Sync. Stand `gegnertyp` (06.11.2026): **kein eigener Code nötig**, der Ordner hält nur die Entscheidung fest.

| Frage | Antwort |
|---|---|
| Was bleibt lokal? | Darstellung, Animation und Bewegungsglättung der Gegner beim Client (`SimulatedProxy`) |
| Was wird geteilt und warum? | Nichts Eigenes. Spawn, Bewegung und Tod kommen über die native Actor-Replikation (Actor-Kanal öffnen und schließen) |
| Wer entscheidet? | Der Host: `AEnemySpawner`, Schaden und Tod laufen nur dort |
| Welcher Weg wird genutzt? | Native UE-Replikation aus Gate G1; geprüft wird über den Desync-Melder (Feld „Gegner“, seit `gegnertyp` im Abgleich) |
| Wie werden Ablehnung, Wiederholung, Neuaufbau getestet? | Join/Travel bauen die Gegner nativ neu auf. Abweichungen zeigt der Melder; `tools/spike/enemy_sync.py` misst Spawn und Tod je Gegner |

Nicht repliziert und darum hier nicht nachgebaut: Gegner-HP beim Client (dort 0/0) und die gepoolten Tod-Effekte
(`EnemyChaosDestructionProxyActor`, Schadenszahlen), siehe `opt-blut` und `vfx-sound`. Bosse und weitere Gegnertypen prüft
`gegner-alle`. [Befund und Messwerte](../../../docs/GAME_ANALYSIS.md#ein-gegnertyp-synchron-gegnertyp)

## Inventur Bosse (`gegner-alle`, 07.11.2026, nur statisch, noch nicht im Spiel belegt)

Quelle: SDK-Dump von Build `26187885`, Klassen `UBossComponent`, `UDDBossObjective`, `AEnemyBase`.

- **[Belegt, statisch]** `UBossComponent` und `UDDBossObjective` haben keine als `Net` markierte Eigenschaft. Phase (`OnNextPhase`), Intro-Cutscene
  (`OnTriggerBossFightCutscene`, `CutsceneCamera`, `PlayerCameraManager`) und Lebensleiste (`PhaseMarkers`, `MaxHealth`, `TrackedBosses`)
  sind lokale Abläufe, die aus Gegner-Zuständen abgeleitet werden.
- **[Vermutung]** Die Phase steigt über das GameplayEffect `IncreasePhaseGameplayEffect` und die Boss-HP über GAS-Attribute. Ob das beim
  Client ankommt (Boss-HP war bei Standardgegnern dort 0/0), ist offen und der erste Messpunkt.
- **[Offen]** Ob die `BossComponent` beim Client überhaupt `BeginPlay` und die Intro-Kamera ausführt, ob die Lebensleiste dort Werte bekommt,
  und ob `GruntEnemySpawner` (`UBossfightGruntSpawner`) nur beim Host läuft.
- Nächster Schritt: Boss im Spiel erreichen (`!cheats`, `DA_BossSpawnLookup`, Torboss nach Tag 4), dann `tools/spike/enemy_sync.py` für Boss-Felder erweitern.

**Versuch 1 (07.11.2026, Build 26495302, Spike, Godmode):** Beide Instanzen in `DDDynamic_Map` (`servertravel`, Profil `MGP_HQ8_Boss`).
Nach `!cheats`, `DD_SetTimeSinceMissionStart 400` und `!wave` gibt es beim Host keinen einzigen `EnemyBase` und keinen Boss
(`objects.py`: 4 `EnemySpawner`, 0 Gegner), also wieder wie im `boss-dump`. `Start-Session.ps1 -Map /Game/Maps/DDDynamic_Map` lief in
den Timeout (180 s), die Reise per `servertravel` danach ging. Es gibt keinen `DD_`-Befehl für Bosse (nur `DD_SpawnLootZombie`).
Zum Boss bleiben: Torboss-Route nach Tag 4 spielen, oder einen Spike-Befehl bauen, der `SpawnSpecificEnemy` mit dem `EnemyType`
aus `DA_BossSpawnLookup` aufruft.

**Versuch 2 (07.11.2026, Build 26495302): Spike-Befehl `!boss <Name>` (`Cheats.cpp`).** Er lädt den `EnemyType` über `DA_BossSpawnLookup`
und ruft `EnemySpawner::SpawnSpecificEnemy` beim Host, 600 Einheiten vor der ersten Figur. Namen: `SpikeBoss`, `FrankensteinBoss`,
`BlobBallBoss`, `BallerinaBoss`, `LazyBoss`, `HQBoss`. Der Typ liegt unter `/Game/DDSurvivors/Blueprints/Enemies/Definitions/<Name>`.

Messung mit `SpikeBoss` in `DDDynamic_Map`, eine Sekunde nach dem Spawn, Host gegen Client:

| Merkmal | Host | Client |
|---|---|---|
| `BP_SpikeBoss_C` (`EnemyBase`) | 1, Authority | 1, SimulatedProxy, Spawn gleichzeitig (`enemy_sync`: 17 von 17 Proben gleich) |
| Boss-HP (`CommonAttributeSet.Health/MaxHealth`) | 80000 / 80000 | 80000 / 80000, **kommt nativ an** (anders als bei `BP_StandardEnemy`) |
| Phase (`DDEnemyAttributeSet.Phase`) | 1 | 1 |
| `BossComponent` | 1, mit `BossAIController` | 1, **ohne** `BossAIController` (KI läuft nur beim Host) |
| `BossfightGruntSpawner` | 1 | 1, es entstehen beim Client keine eigenen Grunts (Gegnerzahl gleich) |

Noch offen, braucht einen Kampf: Phasenwechsel (HP-Schwellen), Boss-Tod, Intro-Kamera (`CutsceneCamera` ist auf beiden Seiten 0),
Angriffe der Fähigkeiten (`GA_*`) und die Lebensleiste `WP_Boss_HealthBar_PhaseBased` beim Client.

**Versuch 3 (07.11.2026, Build 26495302): echter Bosskampf per Skript.** `!boss` allein reicht nicht: Der Boss bleibt unverwundbar, weil Intro
und Kampfstart fehlen (`!bossstart`, ein direkter Aufruf von `StartBossBehaviour`, stürzt den Host ab: Schreibzugriff `0x3F8`, `0x58C4E0D`,
die Komponente ist ohne Intro nicht vorbereitet; der Befehl ist wieder entfernt). Der Weg des Spiels geht über einen `BossfightSpawnPoint`.
Neue Spike-Befehle in `Cheats.cpp`:

| Befehl | Wirkung |
|---|---|
| `!bossfight <Name> [Abstand]` | Setzt `BP_<Name>SpawnPoint` per `BeginDeferredActorSpawnFromClass` und `FinishSpawningActor` vor die Figur, ruft `SpawnBoss` und `TriggerBossfight(1)`. Intro, Objective, Boss-Leiste und Phasen laufen wie im Spiel |
| `!bossdmg <Schaden>` | `GameplayStatics::ApplyDamage` auf den Boss, wirkt erst nach `!bossfight` |
| `!boss <Name>` | Nur Spawn über `SpawnSpecificEnemy`, Boss bleibt unverwundbar (nur für Spawn-Tests) |

Messung (`py` über `ddmem`, Host gegen Client, ein Boss je Lauf, Godmode):

| Boss | HP | Ergebnis |
|---|---|---|
| `SpikeBoss` | 80000, Phasenwechsel bei 40000 (Phase 2) | HP und Phase stimmen auf beiden Seiten (Abweichung bis 25 HP durch Probenzeit), Tod: beim Host `bActorIsBeingDestroyed`, beim Client Actor weg, kein Absturz |
| `FrankensteinBoss` | 120000, Phase 2 unter etwa 80000 | HP gleich bis 0, Boss stirbt auf beiden Seiten, kein Absturz |
| `BlobBallBoss` | 250000, danach Phase 2 mit 175000 | beim Host lebt der Boss mit Phase 2, siehe Lücken |
| Ballerina, Lazy, `HQBoss` (Endboss) | nicht gemessen | |

**Gefundene Lücken beim Client (Screenshots beider Fenster, `BlobBallBoss`):**
- Die Boss-Leiste „Besiege den Boss!“ (`WP_Boss_HealthBar_PhaseBased`, `DDBossObjective`) fehlt beim Client ganz. Das Objective läuft nur beim Host. Kandidat für
  eine lokale Ableitung aus den replizierten Werten (Boss-HP, Phase), kein neuer Netzpfad.
- Die Angriffseffekte des Bosses (Partikel, Splitter) erscheinen nur beim Host.
- Die Phase ist beim Client teils nicht lesbar (`DDEnemyAttributeSet.Phase` 0 statt 1 bis 2 bei Frankenstein, Probe nicht belastbar, da alte tote Bosse im Speicher
  stehen). Prüfen, bevor sie als Lücke gilt.
- Beim `BlobBallBoss` stand ein Actor als zerstört beim Client, während der Host-Boss lebte. Ob der Boss beim Phasenwechsel dort neu entsteht oder verloren geht, ist offen.
