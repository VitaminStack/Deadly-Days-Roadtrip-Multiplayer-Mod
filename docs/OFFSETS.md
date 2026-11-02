# Offsets für Build 26187885

Alle Werte gelten nur für UE `5.8.1-56057345`, Steam-Build `26187885`, Exe `DDSurvivors-Win64-Shipping.exe`.
Nach einem Spiel-Update müssen sie neu bestimmt werden, siehe [Spiel-Updates](../CONTRIBUTING.md#spiel-updates).
Sie sind hier zum Nachschlagen gesammelt, damit niemand sie erneut suchen muss. Wer neue findet, trägt sie hier ein.

RVAs sind relativ zur Image-Base `0x140000000`. Die Patchstellen stehen als Konstanten am Anfang der jeweiligen
[Spike-Datei](../research/spikes/a-connect/README.md#dateien), ihre Bedeutung in der [Patch-Tabelle](../research/spikes/a-connect/README.md#patches).

## Globale Objekte

| Name | RVA | Hinweis |
|---|---|---|
| `GObjects` | `0x0AA61B00` | Eintrag `0x18` groß, Objekt bei +8, `0x10000` Einträge je Chunk |
| `GNames` | `0x0A993AC0` | Blöcke ab +0x10, Stride 2, Header `len >> 6`, Bit 0 = Wide |
| `GWorld` | `0x0AAE46F0` | |
| `GEngine` | `0x0AAE7408` | |
| `ProcessEvent` | `0x160C040` | Vtable-Index `0x4A` |

## Native Funktionen

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
| `UThreatLevel` | FreezeThreatLevelManager | `0x190` |

In einem TSet ist ein freier Platz daran zu erkennen, dass der Wert hohe Bits gesetzt hat.

`inventar`-Testgrenze: Reise auf Tag 3 führte zweimal beim Host in den Callback `0x57CC390` (Fehler bei `0x57CC395`/`0x57CC398`, indirekter Aufruf `+0x490`; nächster Stack-Eintrag `0x315A764`). Objektklasse unbekannt, **keine** neue Patchstelle. Ein nachfolgender Client-Absturz lag bei `0x57D080C`.

## Enums

`ERoundChangeReason`: `NoReason` 0, `NewDay` 1, `Win` 2, `Lose` 3.
