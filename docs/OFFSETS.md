# Offsets für Build 26187885

Alle Werte gelten nur für UE `5.8.1-56057345`, Steam-Build `26187885`, Exe `DDSurvivors-Win64-Shipping.exe`.
Nach einem Spiel-Update müssen sie neu bestimmt werden, siehe [Spiel-Updates](../CONTRIBUTING.md#spiel-updates).
Sie sind hier zum Nachschlagen gesammelt, damit niemand sie erneut suchen muss. Wer neue findet, trägt sie hier ein.

RVAs sind relativ zur Image-Base `0x140000000`. Die Patchstellen stehen als Konstanten am Anfang der jeweiligen
[Spike-Datei](../research/spikes/a-connect/README.md#dateien), ihre Bedeutung in der [Patch-Tabelle](../research/spikes/a-connect/README.md#patches).

## Fingerabdruck der Exe

Daran erkennt die Versionsprüfung in `src/Core/Version.cpp` den Build, Werte aus dem PE-Header.

| Feld | Wert |
|---|---|
| `TimeDateStamp` | `0x32C34474` |
| `SizeOfImage` | `0x0B742000` |

`TimeDateStamp` ist kein Datum, sondern ein Hash des deterministischen Builds.

## Globale Objekte

| Name | RVA | Hinweis |
|---|---|---|
| `GObjects` | `0x0AA61B00` | Eintrag `0x18` groß, Objekt bei +8, `0x10000` Einträge je Chunk |
| `GNames` | `0x0A993AC0` | Blöcke ab +0x10, Stride 2, Header `len >> 6`, Bit 0 = Wide |
| `GWorld` | `0x0AAE46F0` | |
| `GEngine` | `0x0AAE7408` | |
| `ProcessEvent` | `0x160C040` | Vtable-Index `0x4A` |

## Native Funktionen

Statische Gegenprüfung durch Aufgabe `andockpunkte` (02.11.2026): Die bekannten
RVAs für `ProcessEvent`, `SetClientTravel` und `SeamlessTravelToLevel` werden von
den drei Mustern in `research/spikes/andockpunkte/audit.py` jeweils eindeutig
gefunden. Geprüfte Exe, SHA-256:
`3afbf18e051fbceb1fec17a8da20165ba64e0f6c3c88cbe88cb046704a4eefb5`.
Keine neuen RVAs und keine Bestätigung eines nativen C++-Prototyps. Nur dieser
Build ist geprüft; vor einem Hook müssen auch die geladenen Originalbytes passen.

| Funktion | RVA |
|---|---|
| `SetClientTravel` | `0x3262830` |
| `SeamlessTravelToLevel` | `0x5976AD0` (Settings: Paket-`FName` bei +0x10) |
| `LoadLevelByObjectReference` | `0x596B9F0` |
| `LoadMapByInformation` | `0x5B11170` |
| Exec-Thunks | `0x57359D0`, `0x5733B60`, `0x5694050`, `0x5694260`, `0x5734200`, `0x56C30B0` |
| `UInventoryComponent::ReplicateSubobjects` | `0x5974C30`, Vtable `+0x2D0`; sendet Items und Slots über `UActorChannel` |
| `UInventoryComponent::ReadyForReplication` | `0x5969750`, Vtable `+0x3D8`; ursprünglicher Sprung nach `0x2819AA0` |
| UObject-Standard `IsSupportedForNetworking` | `0x1512250`, Vtable `+0x200`; delegiert an `IsFullNameStableForNetworking` |
| `CreateItemInstance`, `CreateSlotInstance` | `0x5959700`, `0x5959900`; nur im Diagnoseversuch gehookt, nicht im Teilfix |

### Kamera

Ablauf siehe [Kamera und Sichtbereich](GAME_ANALYSIS.md#kamera-und-sichtbereich).

| Funktion | RVA | Hinweis |
|---|---|---|
| `CalculateViewportEdgesInWorldSpace` | `0x5B02DD0` | Über alle Controller in `SpawnedPlayers`, Rückgabe 4 Bodenpunkte |
| Bildecken eines Controllers auf den Boden projizieren | `0x5B028D0` | `GetViewportSize` (`0x2E722B0`), `DeprojectScreenPositionToWorld` (`0x2A6CD50`) |
| Spawnort des Gegner-Spawners | `0x58AA0F0` | Prüft `UseViewportEdgesForSpawningPositions` |
| Spieler der Kamera aus `SpawnedPlayers` | `0x5A0DA90` | Spike-Patch beim Client |

### Gegner

Ablauf siehe [Gegner-Spawning und Pooling](GAME_ANALYSIS.md#gegner-spawning-und-pooling).

| Funktion | RVA | Hinweis |
|---|---|---|
| Spawn-Schleife des `AEnemySpawner` | `0x58CECE0` | Spielerwahl, `RecycleArray`, Spawn je Gegner |
| `AEnemySpawner::TrySpawnFreshEnemy` | `0x58CE600` | Exec-Thunk `0x56EE100` |
| `AEnemySpawner::SpawnSpecificEnemy` | `0x58C8A30` | Exec-Thunk `0x56ED7F0` |
| `AEnemySpawner::ForceSpawnWave` | `0x58B2780` | Exec-Thunk `0x56E4C90` |
| Initialisierung des Spawners, leert `RecycleArray` | `0x58A58E0` | VTable `0x94710D8`, Slot 213 |
| `PlaceCharacterOnGroundAtPosition` | `0x5B12DB0` | Versetzt recycelte Gegner |
| `PooledDestroy` (Blueprint-Helfer) | `0x5B12E10` | Exec-Thunk `0x5697410` |

### Karte

Ablauf siehe [Karte und Seed](GAME_ANALYSIS.md#karte-und-seed).

| Funktion | RVA | Hinweis |
|---|---|---|
| Aufbau: Profil, `MapSeed`, Start | `0x58F7710` | Aufgerufen von `0x58D9F40`, VTable `0x9522280` (`MapGenProfileInjectorAndBuildTrigger`) Slot 125 |
| `AMapGenerator::StartMapGeneration` | `0x59EF6D0` | Exec-Thunk `0x57560E0`, Zufalls-Seed bei `MapSeed == 0` (`0x59EF79A`) |
| `AMapGenerator::StartMapGenerationWithRetryAttempts` | `0x59EFE00` | Exec-Thunk `0x5756100` |
| `AMapGenerator::TryToRegenerate` | `0x59F4210` | Exec-Thunk `0x5756A60` |
| `UDDMissionGameInstance::GetCurrentMissionSeed` | `0x58EABF0` | Exec-Thunk `0x56BC5E0` |
| `UDDMissionGameInstance::DD_SetCurrentRunSeed` | `0x58E1B90` | Exec-Befehl, Exec-Thunk `0x56BAB80` |
| `UDDMissionGameInstance::DD_ForceStartRunSeed` | `0x58DF7A0` | Exec-Befehl, Exec-Thunk `0x56BA490` |

### Coop

Funktionen von `ULocalCoopSubsystem` und ihre Aufrufer, Ablauf siehe [Coop-Ablauf](GAME_ANALYSIS.md#coop-ablauf).

| Funktion | RVA | Hinweis |
|---|---|---|
| `SetCoopEnabled` | `0x59003E0` | Exec-Thunk `0x5735F10` |
| `SetMaxLocalPlayers` | `0x5900770` | Exec-Thunk `0x57363C0` |
| `SpawnLocalPlayer` | `0x5901E90` | Exec-Thunk `0x57374E0` |
| `GetPlayerController(Index)` | `0x58EE0D0` | |
| `GetPlayerIndex` | `0x58EE2A0` | Rückgriff ohne `LocalPlayer` |
| `IsSecondaryPlayer` | `0x58F47C0` | Rückgriff ohne `LocalPlayer` |
| `ApplyDeviceAssignmentDraftAndJoin` | `0x58D8AA0` | Ruft `SpawnLocalPlayer(1)` |
| Run-Zustand laden (`IRunPersistable`) | `0x58F5330` | VTable des Subsystems `0x94ED340`, Slot 91 |
| P2 nach Kartenwechsel neu anlegen | `0x58D9810` | VTable von `ADDMissionGameMode` `0x9430630`, Slot 111 |
| Figur spawnen | `0x5901C60` | Gleiche VTable, Slot 234, Spike-Patch bei `0x5901CE8` |
| Moduswechsel: Coop aus und Coop wieder an | `0x5914B30`, `0x593BCA0` | |
| `APlayerController::GetLocalPlayer` | `0x2E706D0` | |
| `UGameInstance::CreateLocalPlayer` | `0x2A6B5B0` | |
| `UGameplayStatics::GetPlayerController` | `0x2A76140` | |
| Tabelle `{Name, Exec-Thunk}` von `LocalCoopSubsystem` | um `0x94F6EE0` bis `0x94F7190` | Einträge `0x10` groß |

## Klassen

| Klasse | Feld | Offset |
|---|---|---|
| `UObject` | Flags, Index, Class, Name, Outer | `0x08`, `0x0C`, `0x10`, `0x18`, `0x20` |
| `UStruct` | Super, Script | `0x40`, `0x60` |
| `UFunction` | FunctionFlags (Native `0x400`), ExecFunction | `0xB0`, `0xD8` |
| `FFrame` | Node, Object, Code | `0x10`, `0x18`, `0x20` |
| `AActor` | Rep-Bits (Bit 3 `bReplicates`), RemoteRole | `0x5B`, `0x60` |
| `AActor` | Owner, Role, NetPriority, RootComponent | `0x158`, `0x168`, `0x190`, `0x1B8` |
| `USceneComponent` | RelativeLocation, RelativeRotation, Mobility | `0x148`, `0x160`, `0x1AB` |
| `UActorComponent` | bIsActive | `0x9A` |
| `APawn` | PlayerState, Controller | `0x2C8`, `0x2D8` |
| `AController` | Pawn | `0x2E8` |
| `APlayerController` | Player, AcknowledgedPawn, NetConnection | `0x348`, `0x350`, `0x520` |
| `APlayerState` | PawnPrivate | `0x320` |
| `ACharacter` | CharacterMovement, BasedMovement, ReplicatedMovement-Block | `0x330`, `0x340`, `0x398` |
| `ACharacter` (Spiel) | AttributeSet | `0x7D8` |
| `FBasedMovementInfo` | MovementBase, Location | `0x10`, `0x18` |
| `UCharacterMovementComponent` | Velocity, MovementMode, MaxWalkSpeed | `0xD8`, `0x241`, `0x288` |
| `UCharacterMovementComponent` | MaxAcceleration, Acceleration | `0x29C`, `0x338` |
| `UCharacterMovementComponent` | Flags: Bit 1 `bIgnoreClientMovementErrorChecksAndCorrection`, Bit 3 `bServerAcceptClientAuthoritativePosition` | `0x579` |
| `UWorld` | NetDriver, GameMode, GameState, GameInstance | `0x38`, `0x1A8`, `0x1B0`, `0x228` |
| `AGameStateBase` | PlayerArray | `0x2C0` |
| `UNetDriver` | ServerConnection, ClientConnections | `0x100`, `0x108` |
| `FWorldContext` | TravelURL | `0xA8` |
| `ADDMissionGameMode` | SpawnedPlayers (TSet) | `0x388` |
| TSet | Data, Num, FirstFree, NumFree, Element-Stride | `+0`, `+8`, `+0x30`, `+0x34`, `0x10` |
| GameInstance | PendingGameStateComponentTravelStates, LastRoundChangeReason | `0x248`, `0x299` |
| GameInstance | CurrentRunInfo (Seed +0, CurrentRoute +8) | `0x2A0` |
| GameInstance / `FRoute` | `CurrentRoute.MissionChain` (TArray), Num; CurrentMissionIndex | `0x2B0`, `0x2B8`; `0x2C0` |
| `ADDMissionGameState` | SecondsSinceMissionStart, DayNum | `0x38C`, `0x390` |
| `UAbstractPanel` | AnimationContainer | `0x330`; im `rundenende`-Test nicht als zuverlässiger Phasenindikator lesbar |
| `UPanelAnimationContainer` | IsOpen | `0x328`; nur lokaler Containerzustand, keine Netz-/Sichtbarkeitszusage |
| `UInventoryComponent` | Slots, MyItems | `0x100`, `0x120` |
| `UInventoryComponent` | Vtable-RVA | `0x94BA3F0` |
| `UActorComponent` | `bReplicateUsingRegisteredSubObjectList` | `0x98`, Bit 6 (`0x40`) |
| `UBackpackSlot` | Vtable-RVA; Coordinates, Occupier, ComponentOwnerPrivate | `0x939CE08`; `0x28`, `0x30`, `0x38` |
| `UInventoryItemInstance` | Vtable-RVA; ZeroPivotTile, Turns, ItemData | `0x94B46E8`; `0x28`, `0xD8`, `0xE0` |
| `UInventoryItemInstance` | Broken, IsInBackpack, IsReward, IsTrash | `0x110`, `0x180`, `0x182`, `0x184` |
| `APlayerState` | PlayerId (numerischer Replikationsschlüssel, keine Steam-ID) | `0x2AC` |
| `AAllPlayerFramingCamera` | Camera, MaxMovementDistance, UseZoomOutAtEdge, ZoomOutDistance, CameraSpringArm, GameMode | `0x2A8`, `0x2F0`, `0x2F4`, `0x2F8`, `0x310`, `0x318` |
| `UInViewportMovementRestrictor` | SlowdownZone, FullStopZone (je 4 Floats L, R, D, U) | `0xD8`, `0xE8` |
| `USpringArmComponent`, `UCameraComponent` | TargetArmLength, FieldOfView | `0x250`, `0x250` |
| `APlayerController` | PlayerCameraManager | `0x360` |
| `APlayerCameraManager` | ViewTarget.Target, ViewTarget.POV.Location | `0x340`, `0x350` |
| `AEnemySpawner` | AutoSpawn, UseViewportEdgesForSpawningPositions, AllowedViewportEdgesForSpawning | `0x348`, `0x34A`, `0x358` |
| `AActor` | Bits: `bHidden` (`0x58` Bit 7), `bActorEnableCollision` (`0x5D` Bit 0), `bActorIsBeingDestroyed` (`0x5D` Bit 1) | `0x58`, `0x5D` |
| `AEnemySpawner` | CurrentSpawnedEnemiesCount, MaxSpawnedEnemies, EnemyCap, GameMode, RecycleArray | `0x2F8`, `0x36C`, `0x370`, `0x3C8`, `0x478` |
| `UPoolManagerSubsystem` | PoolsInternal, AllFactoriesInternal | `0x38`, `0x48` |
| `AMapGenerator` | MapGenProfile, MapSeed, MaxAttempts, MaxTilePosition, CycleTimeBudget, GenerateOnStart | `0x2B8`, `0x2D8`, `0x2DC`, `0x2E8`, `0x2F8`, `0x2FD` |
| `AMapGenerator` | GenerationTags, CurrentGenData | `0x300`, `0x320` |
| `UMapGenerationState` | Seed, BaseSeed, SpawnedActors, HasFailedGeneration, RetryAttemptsRemaining, RandomGeneratorLookup | `0x50`, `0x54`, `0x88`, `0x111`, `0x114`, `0x258` |
| `ULocalCoopSubsystem` | ActivePlayers (TMap, Element `0x14`), bCoopEnabled, MaxLocalPlayers | `0x100`, `0x180`, `0x184` |
| `ULocalCoopSubsystem` | Flag „P2 nach Kartenwechsel neu anlegen“ | `0x18A` |
| `UThreatLevel` | FreezeThreatLevelManager | `0x190` |

In einem TSet ist ein freier Platz daran zu erkennen, dass der Wert hohe Bits gesetzt hat.

`CurrentRoute` ist eine **eingebettete Struktur**, kein UObject-Zeiger. Ihre ersten acht Bytes sind `RouteMissionStructureIdentifier`. Die Num bei `0x2B8` darf nicht aus einem vermeintlichen Zeiger bei `0x2A8` abgeleitet werden. Im `rundenende`-Test: Host Num 1 / Index 0, Client Num 0 / Index -1. Die reflektierten Funktionen `FinishMission`, `TriggerEndRound`, `SetFreezeTime` tragen kein `FUNC_Net` (`0x40`); ihre lokalen Aufrufe sind kein Client-Transport.

## Blueprint-Referenzinventar (`blueprint-logik`)

Ergaenzung aus Mission-SDK/lesender Pruefung von Build `26187885`:

| Klasse | Feld | Offset |
|---|---|---|
| `UClass` | ClassDefaultObject | `0x118` |
| `UStruct` | Script TArray: Data, Num | `0x60`, `0x68` |
| `BP_Effect_Add_ImpactEffect_Template_C` und Kinder | ImpactActor (Default-Objekt) | `0x40` |

Der Referenzscanner liest bekannte FunctionFlags (`UFunction+0xB0`) und das Script,
gibt aber keine Rohbytes/Adressen aus. Keine neuen nativen RVAs. `ShowEffect` der
PowerUpCrate: Flags `0x0C0040C0`; `StartFallDown` des Airdrops: `0x0C004040`.
Das Vorhandensein eines Multicast-Flags beweist keine Zustellung.

## Pausenfelder (Mission-SDK, Aufgabe `pausen`)

| Klasse | Feld | Offset |
|---|---|---|
| `UTimescaleManager` | ActiveRequester, TimeRequests | `0x40`, `0x48` |
| `FTimeRequest` | timescale, Priority, PauseRegardlessOfTimescale | `0x00`, `0x04`, `0x08` (Größe `0x0C`) |
| `UPauseScreenManager` | PauseScreen, TimeRequest, TimescaleManager | `0xD8`, `0xE4`, `0xF0` |
| `UPauseScreenManager` | PauseScreenBlockers, PauseScreenRequests | `0x100`, `0x110` |
| `UAbstractPanel` | bUseCoopUISession, bCoopBlocksGameplay, PauseGameWhileOpen | `0x348`, `0x349`, `0x34B` |
| `UCoopInputRouterSubsystem` | SessionStack | `0xA8` |
| `FCoopUISession` | SessionId, ParentSessionId, AuthorityPlayerIndex, bBlocksGameplay | `0x00`, `0x04`, `0x10`, `0x1A` (Größe `0x1C`) |
| `AWorldSettings` | TimeDilation, CinematicTimeDilation, PauserPlayerState | `0x3E0`, `0x3E4`, `0x4A8` |

Aus dem lokal erzeugten Mission-SDK von Build `26187885`; die genannten
Timescale-/Sitzungs-/Weltzustände wurden lesend verglichen. Nicht alle
Panel-Öffnungspfade und nicht jedes Feld wurden durch gezielte Änderung bestätigt.

## Felder fuer den eigenen Sync (SDK, `sync-bedarf`)

| Klasse | Feld | Offset |
|---|---|---|
| `ADDMissionGameState` | LevelSystem, ScrapWallet, RerollCurrency | `0x3B8`, `0x3C0`, `0x3C8` |
| `ADDMissionGameState` | MetaCurrencyWallet, RareMetaCurrencyWallet, CoopInventoryCoordinator | `0x3D0`, `0x3D8`, `0x3E0` |
| `UAbstractCurrency` | OnCurrencyChanged, PaidReceipts, CurrencyAmount | `0xC8`, `0xF0`, `0x100` |
| `URerollCurrency` | OnRerolled, RerollCountsPerContext | `0x148`, `0x158` |
| `UCoopInventoryCoordinatorComponent` | OnRewardSessionChanged, RewardSession, QueuedRewards | `0xC0`, `0x170`, `0x268` |

Nur SDK-Nachschlagewerte von Build `26187885`, hier nicht durch Mutation getestet.
Fuer rohe XP-/Level-Interna ist kein neuer Offset bestimmt worden. Keine
Speicherbereiche aus dem Padding als Protokollquelle behandeln.
`inventar`-Testgrenze: Reise auf Tag 3 führte zweimal beim Host in den Callback `0x57CC390` (Fehler bei `0x57CC395`/`0x57CC398`, indirekter Aufruf `+0x490`; nächster Stack-Eintrag `0x315A764`). Objektklasse unbekannt, **keine** neue Patchstelle. Ein nachfolgender Client-Absturz lag bei `0x57D080C`.

## Enums

`ERoundChangeReason`: `NoReason` 0, `NewDay` 1, `Win` 2, `Lose` 3.
