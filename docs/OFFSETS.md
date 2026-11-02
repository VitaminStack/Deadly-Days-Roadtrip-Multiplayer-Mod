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
| `UInventoryComponent` | Slots, MyItems | `0x100`, `0x120` |
| `UInventoryComponent` | Vtable-RVA | `0x94BA3F0` |
| `UActorComponent` | `bReplicateUsingRegisteredSubObjectList` | `0x98`, Bit 6 (`0x40`) |
| `UBackpackSlot` | Vtable-RVA; Coordinates, Occupier, ComponentOwnerPrivate | `0x939CE08`; `0x28`, `0x30`, `0x38` |
| `UInventoryItemInstance` | Vtable-RVA; ZeroPivotTile, Turns, ItemData | `0x94B46E8`; `0x28`, `0xD8`, `0xE0` |
| `UInventoryItemInstance` | Broken, IsInBackpack, IsReward, IsTrash | `0x110`, `0x180`, `0x182`, `0x184` |
| `APlayerState` | PlayerId (numerischer Replikationsschlüssel, keine Steam-ID) | `0x2AC` |
| `ULocalCoopSubsystem` | ActivePlayers (TMap, Element `0x14`), bCoopEnabled, MaxLocalPlayers | `0x100`, `0x180`, `0x184` |
| `ULocalCoopSubsystem` | Flag „P2 nach Kartenwechsel neu anlegen“ | `0x18A` |
| `UThreatLevel` | FreezeThreatLevelManager | `0x190` |

In einem TSet ist ein freier Platz daran zu erkennen, dass der Wert hohe Bits gesetzt hat.

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
