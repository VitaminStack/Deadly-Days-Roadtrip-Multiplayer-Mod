# Offsets für Build 26187885

Alle Werte gelten nur für UE `5.8.1-56057345`, Steam-Build `26187885`, Exe `DDSurvivors-Win64-Shipping.exe`.
Nach einem Spiel-Update müssen sie neu bestimmt werden, siehe [Spiel-Updates](../CONTRIBUTING.md#spiel-updates).
Sie sind hier zum Nachschlagen gesammelt, damit niemand sie erneut suchen muss. Wer neue findet, trägt sie hier ein.

RVAs sind relativ zur Image-Base `0x140000000`. Die Patchstellen stehen als Konstanten am Anfang der jeweiligen
[Spike-Datei](../research/spikes/a-connect/README.md#dateien), ihre Bedeutung in der [Patch-Tabelle](../research/spikes/a-connect/README.md#patches).
Die Spielfehler-Fixes nutzt auch die Mod, mit denselben Werten am Anfang von `src/Game/*Patches.cpp`, siehe [Patches der Mod](../src/Game/README.md#patches).

## Build 26495302 (`update-basis`)

Stand 07.11.2026. Alle Werte dieser Datei bleiben Referenz-RVAs von `26187885`. Für `26495302` rechnet `Game::AddressMap` sie über
`src/Game/RvaMap26495302.inc` um, erzeugt mit [`tools/rva_map.py`](../tools/README.md#rva_mappy). Exe: TimeDateStamp `0x65BFBBFB`, SizeOfImage `0x0B748000`.

| | 26187885 | 26495302 | Beleg |
|---|---|---|---|
| GObjects | `0x0AA61B00` | `0x0AA67B80` | rva_map (Code-Verweis) und Dumper-7-SDK |
| GNames | `0x0A993AC0` | `0x0A999B40` | rva_map und SDK |
| GWorld | `0x0AAE46F0` | `0x0AAEA770` | rva_map und SDK |
| ProcessEvent | `0x0160C040` | `0x0160C220` | rva_map (eindeutig) und SDK, Index `0x4A` unverändert |

- Feld-Offsets aus `src/Game/Offsets.h` sind unverändert: `SdkChecks.cpp` baut gegen das neue SDK ohne Fehler (dieselbe deaktivierte
  UE-5.8-Assert-Stelle `UEngineReplicationBridge` wie beim alten SDK).
- 97 Adressen aus `src/`: 81 eindeutig, 3 vtables, 3 Globale, 1 über den Funktionsanfang, 9 fehlen (Code geändert):
  `0x05666FA5` (Blockade, Einsprung), `OnRepHealth`, `OnRepMaxHealth`, `FinishMission`, `PanelConfirm`, `ClientRefreshFunction`,
  `RepresentationSize`, `SlotVtable`, `DetectorVtable`.
- Spieltest mit Mod auf `26495302` (ein PC, Lobby, `-NoSpike`): 33 von 43 Patch-Prüfungen bestehen, 10 nicht, darum setzt die Mod keine
  Patches. Handshake mit Build-Prüfung klappt. Der Host stürzt beim Beitritt des Clients ab (`0x5904DE8`), wie ohne den Patch
  „SpawnedPlayers bei der Registrierung“ zu erwarten. Auf `26187885` setzt dieselbe Mod weiterhin alle 43 Patches.
- Mit Teilinstallation (`-PartialPatches`): 8 von 13 Listen gesetzt, ausgelassen Player, Client, Inventory, HealthBar, Reward. Absturz beim
  Beitritt unverändert an `0x5904DE8`, bis `update-client` die Player-Liste portiert.

### Client- und Player-Patches (`update-client`)

Stand 07.11.2026, Dateien `ClientPatches.cpp`, `PlayerPatches.cpp`, `RemoteP2Patches.cpp`, `LocalDevicePatches.cpp`.

- Von Hand nachgetragen (`// von Hand` in der Tabelle):

  | Referenz | 26495302 | Beleg |
  |---|---|---|
  | `0x05666FA5` Blockade-Thunk | `0x056676F5` | einziger Thunk mit `jmp` auf das umgerechnete `BlockadeLoadingFinished`, gleiche Befehle davor |
  | `0x058E84C0` `FinishMission` | `0x058EA1A0` | Ziel beider umgerechneter Aufrufer, gleicher Prolog, Signatur `(this, bool IsLoading)` unverändert |
  | `0x05A8BAB0` `ClientRefreshFunction` | `0x05A90940` | Ziel des `call` an `ClientRefreshCallSite` |

- Geänderter Spielcode: `FinishMission` prüft zusätzlich zwei nicht reflektierte Flags im `Pad_410` von `ADDMissionGameState`. Der Hook
  sperrt weiter nur das lokale Rundenende beim Client und ist davon nicht betroffen. Die Client-Refresh-Funktion liest `this+370h` weiter
  ohne Nullprüfung, der Patch bleibt nötig. Ihre übrigen Felder haben sich verschoben (`0x900` → `0x8F8`), die nutzt die Mod nicht.
- Vier Prüfungen hatten feste `call rel32`-Bytes (Exp-Aggregation zweimal, TSet-Add und Broadcast in der Registrierung). Alle zeigen auf
  die umgerechneten Ziele. Sie prüfen jetzt über `ExpectBranch` das Sprungziel statt der Bytes, ebenso der Blockade-Thunk, Client-Refresh
  und Attribut-Tick.
- Sprungziele der Code-Höhlen (`…Exit`, `…SkipTarget`, `RunEndContinue`, `Sneak…`) haben denselben Abstand zur Patch-Stelle wie im Referenz-Build.
  Die Feld-Offsets der vier Dateien sind laut SDK-Vergleich unverändert. Geändert haben sich unter anderem `UInventoryPanel`, `UToolTip`,
  `URoundEndPanel` und `URarity` (Thema von `update-inventar`).
- `tools/expect_check.py`: auf beiden Builds 42 von 42 prüfbaren Prüfungen der vier Dateien `ok`, 2 nur im Spiel prüfbar (lokale `Site`).
- Spieltest ein PC, Lobby, `-NoSpike`: `26187885` weiter 43 Patches. `26495302` mit `-PartialPatches`: 10 von 13 Listen, ausgelassen nur
  Inventory, HealthBar und Reward. Der Client tritt ohne Host-Absturz bei, `SpawnedPlayers` meldet `Spieler=2 gültig=true`.
  `Run-Route.ps1 -Days 1 -StayLast`: MetaBase und Tag 1 bei beiden `sauber` (2/2 Spieler, 102/103 Kacheln). Abweichend ist nur der
  Inventarvergleich, weil die Inventory-Liste noch fehlt. Seit Missionsbeginn keine neuen Fehler im Log.

## Diagnoseadapter (`desync-melder`)

Quelle: Felddeklarationen des lokalen Dumper-7-SDK für denselben Build. Optionale
`DDCOOP_WITH_SDK`-Assertions in `DesyncAdapter.cpp` prüfen die Offsets beim Bauen.
Neue Messung noch nicht im Spiel abgenommen, siehe [Testgrenzen](DESYNC.md).

| Klasse/Feld | Offset | Verwendung |
|---|---|---|
| DDPlayerState.HealthAttributeSet | `0x380` | aktuelle/maximale HP |
| CommonAttributeSet.Health / MaxHealth | `0xA0` / `0x30` | FGameplayAttributeData.CurrentValue jeweils `+0xC` |
| InventoryComponent.Slots / MyItems | `0x100` / `0x120` | replizierte Arrays |
| BackpackSlot.Coordinates / Occupier | `0x28` / `0x30` | Inhalt und Position |
| InventoryItemInstance.ZeroPivotTile / Turns / ItemData | `0x28` / `0xD8` / `0xE0` | kanonischer Iteminhalt |
| InventoryItemInstance.Broken / IsInBackpack / IsReward / IsTrash | `0x110` / `0x180` / `0x182` / `0x184` | replizierte bool-Felder |
| Level.Actors / WorldSettings | `0xA0` / `0x2B8` | Gegnerzählung und Pause |
| WorldSettings.TimeDilation / PauserPlayerState | `0x3E0` / `0x4A8` | Diagnosephase 0/1 |
| Actor-Flagbyte, Hidden-Maske | `0x58`, `0x80` | versteckte Pool-Actors ausnehmen, wie AirdropAdapter |

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
| `GObjects` | `0x0AA61B00` | Eintrag `0x18` groß, Objekt bei +8, Seriennummer für schwache Verweise bei +0x10, `0x10000` Einträge je Chunk |
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
Keine neuen RVAs und keine Bestätigung eines direkten nativen C++-Prototyps. Nur
dieser Build ist geprüft. Nachtrag: alle drei Muster auch in den geladenen
ausführbaren Sektionen auf Host und Client je eindeutig gefunden; der kleine
ProcessEvent-VTable-Test mit Rückgabeprüfung und Slot-Wiederherstellung bestand.
Fremde Änderungen an den Einsprungbytes müssen weiterhin zur Ablehnung führen.

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
| `AItemLoot::Loot` (nativer Interface-Pfad) | `0x59AB040`; `this` ist `Actor+0x2A8`, ItemData bei `Actor+0x2E0`, Role bei `Actor+0x168`. Exec-Thunk `0x57145E0`, Item-Erzeugung bei `0x59AB0AE`. Bytegeprüfter Hook in `InventoryPatches.cpp`, [Befund](../research/spikes/itemsack-absturz/README.md) |

### Trennen

Ablauf siehe [Trennen ins Hauptmenü](GAME_ANALYSIS.md#trennen-ins-hauptmenü-trennen).

| Funktion | RVA | Hinweis |
|---|---|---|
| `AActor::EndPlay` | `0x25E3350` | Ruft `EndPlay` der gestarteten Komponenten (Vtable `+0x3F0`) |
| `UInventoryComponent::EndPlay` | `0x595BBD0` | Vtable `0x94BA3F0` `+0x3F0`, Schleife über `MyItems` ab `0x595C01A` |
| `UInventoryComponent::RemoveAllItemEffects` | `0x5971BB0` | Exec-Thunk `0x5717A90`, aufgerufen von `LoadMapByInformation` |
| Zustandsobjekte eines Items durchgehen | `0x5975A40` | Ohne Null-Prüfung, Absturzstelle `0x5975A6A` liest `Item+0x340` |

### Kamera

Ablauf siehe [Kamera und Sichtbereich](GAME_ANALYSIS.md#kamera-und-sichtbereich).

| Funktion | RVA | Hinweis |
|---|---|---|
| `CalculateViewportEdgesInWorldSpace` | `0x5B02DD0` | Über alle Controller in `SpawnedPlayers`, Rückgabe 4 Bodenpunkte |
| Bildecken eines Controllers auf den Boden projizieren | `0x5B028D0` | `GetViewportSize` (`0x2E722B0`), `DeprojectScreenPositionToWorld` (`0x2A6CD50`) |
| Spawnort des Gegner-Spawners | `0x58AA0F0` | Prüft `UseViewportEdgesForSpawningPositions` |
| Spieler der Kamera aus `SpawnedPlayers` | `0x5A0DA90` | Spike-Patch beim Client |
| `UInViewportMovementRestrictor::PreProcessMovementInput` | `0x5A17AB0` | Interface-Vtable `0x94C8678` Slot 2, Absturzstelle `0x5A17ACC` (`cmp [rax+2C8h],1; jle 0x5A17B88`, weiter bei `0x5A17AD9`), Mod-Wächter dort; Exec-Thunk `0x57525C0` |
| `UInViewportMovementRestrictor::BeginPlay` | `0x5A03C20` | Vtable `0x94C8138` Slot 129, setzt `MissionGameState` aus `World->GameState` |

### Gegner

Ablauf siehe [Gegner-Spawning und Pooling](GAME_ANALYSIS.md#gegner-spawning-und-pooling).

| Funktion | RVA | Hinweis |
|---|---|---|
| Spawn-Schleife des `AEnemySpawner` | `0x58CECE0` | Spielerwahl, `RecycleArray`, Spawn je Gegner |
| Blockaden-Laden abgeschlossen | `0x58BB360` | Gemeinsames `UBlockadeEventComponent::OnLoadingFinished`, Exec-Thunk `0x5666F90`; legt Timer an `+0x118` an. Client darf hier keinen Host-Ablauf starten (`guidcache`) |
| Verzögerter Blockadenstart | `0x58AA5F0` | Timeradapter ruft VTable-Byteoffset `0x4E8`; Highway-Implementierung `0x58B4E40`, spezifischer Spawn `0x58C9180` |
| GameMode-Mengen-Kopie im Spawner | `0x58CED42`, `0x58CED70`, `0x58CED7B` | Lädt `EnemySpawner+0x3C8`, addiert `SpawnedPlayers+0x388`, ruft Kopierer `0x56B16C0`. Null-GameMode führt bei `0x56B16D9` zu Lesen `0x3D0` |
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
| `UGameplayStatics::BeginDeferredActorSpawnFromClass` | `0x2A678F0` | Exec-Thunk `0x2A95230`; Spike-Patch beim Client gegen doppelte PowerUp-Objekte |
| `UChildActorComponent::CreateChildActor` | `0x288B510` | Vtable `0x87DAC90` Slot 202, Spike-Patch beim Client |
| Child-Klasse repliziert (Prüfung in `CreateChildActor`) | `0x289DB60` | Wahr, wenn das Default-Objekt von `ChildActorClass` (`+0x250`) oder die Vorlage `ChildActorTemplate` (`+0x260`, gleiche Klasse) `bReplicates` hat |
| `UChildActorComponent::SetChildActorClass` | `0x28A86C0` | Benennt die Vorlage `%s_%s_CAT` |
| `FSoftObjectPtr::LoadSynchronous` | `0x15E9550` | Lädt im Starter das Profil |
| IAT der Exe: WS2_32 `sendto` (Ordinal 20), `recvfrom` (Ordinal 17) | `0x82786F8`, `0x8278710` | Spike-Patch für die Lastmessung |
| `UPCGVolumeGenerator`, Slot 85: PCG-Volume spawnen, Graph wählen | `0x59EE6A0` | Vtable `0x95626D8`, Spike-Patch |
| `UPCGVolumeGenerator`, Slot 86: Graph laden, PCG starten | `0x59D95E0` | Asynchron mit Callback |
| Kartengenerator: Erzeugung fortsetzen, aus einem Timer | `0x59F0C30` | Ruft `StartMapGeneration` und `0x59D9090` |
| Kartengenerator: Schritt nach `StartMapGeneration` | `0x59D9090` | Absturzort nach der Reise auf Tag 3 |

### Airdrop-Auswahl

Aufgabe `airdrop-auswahl`, Ablauf in [Airdrop-Auswahl](GAME_ANALYSIS.md#airdrop-auswahl-für-beide-spieler-airdrop-auswahl).

| Funktion oder Feld | RVA oder Offset | Hinweis |
|---|---|---|
| `RewardChest.OpenAirdropChoices` | `0x5A42F50` | Hook in `AirdropPatches.cpp` |
| `InventoryComponent::SetRewardItems` | `0x5979B30` | `(this, TArray*, Level)`, öffnet das Panel |
| `InventoryPanel.HandleConfirmButton` | `0x5A6CB20` | liest `+0x7A0`, dann `+0x3E0`; bricht bei `IsDragging` (`+0x790`) ab; ruft `OpenNextReward` der Queue `+0x6B8` |
| `InventoryPanel.SetupInventory` | `0x5A8BAB0` | liest `InventoryComponent` bei `+0x370`; positioniert aus `InventoryItemInstance.CachedBackpackLocalPosition` (`+0x150`, FVector2D, nicht repliziert) |
| `DefaultFocusableButton.ParentPanel` | `+0x3A8` | Panel des Knopfs (UUserWidget); der Bestätigen-Knopf des Inventar-Panels heißt `MainButton`, gefärbt über `UserWidget.SetColorAndOpacity` (`bestaetigen-alle`) |
| `InventoryPanel.SnapRepresentationToTile` | `0x5A92030` | richtet UI und Pixelpositions-Cache aus einem Pivot-Feld aus; reflektierte Parameter: ItemRepresentation `+0`, FIntVector2 `+8`, CanvasPanel `+0x10`, Größe `0x18` |
| `UItemRepresentation`, Größe aus der Itemform | `0x5AB5120` | `void(this)`; liest `ItemInstance` (`+0xA28`), davon `ItemData` (`+0xE0`) und Form (`+0xE8`) ohne Prüfung, setzt die Größe des Canvas-Slots (`+0x30`) auf je 66 px minus 3; neun direkte Aufrufer. Hook in `InventoryPatches.cpp` (`lager-absturz-client`) |
| `InventoryItemInstance.SetZeroPivotTile` | `0x5979ED0` | schreibt ausschließlich `+0x28`; berechnet den Pixelpositions-Cache nicht neu |
| `InventoryComponent::CreateItemInstance` | `0x5959700` | `(this, ItemData, FlagAsNew, Rarity)`, nativ; UFunction `BlueprintAuthorityOnly` |
| `URewardQueueComponent::OpenNextReward` | `0x59ADAC0` | Exec-Thunk `0x57975F0` |
| `URewardQueueComponent::RegisterReward` | `0x59B07B0` | Exec-Thunk `0x5797F10` |
| `BP_DD_Character_C.RewardQueue` | `+0x980` | |
| `URewardQueueComponent` Kiste offen, aktuelle Kiste | `+0x108`, `+0x10C` | Byte; schwacher Zeiger |
| `ARewardChest.CanBeOpened` | `+0x2E1` | |
| `UInventoryItemInstance` `ZeroPivotTile`, `Turns`, `IsInBackpack`, `IsReward` | `+0x28`, `+0xD8`, `+0x180`, `+0x182` | |
| `UInventoryItemInstance` `Broken`, `Rarity`, `ItemInstanceId` | `+0x110`, `+0x118`, `+0x120` | `ItemInstanceId` ist eine FGuid, nicht repliziert |
| `UInventoryItemInstance` `IsInStash`, `IsTrash` | `+0x181`, `+0x184` | `IsInStash` nicht repliziert |
| `UBackpackSlot` `Coordinates`, `Occupier` | `+0x28`, `+0x30` | |
| `UInventoryComponent.PlacedBagWrappers` | `+0x150` | nicht repliziert |
| `UInventoryComponent` Feld-Delegate | `+0x110` | nativer Multicast-Delegate ohne SDK-Namen, an ihm hängen die Feld-Widgets des Panels |
| `InventoryComponent.OnRep_Slots` | `0x596D970`, Exec-Thunk `0x5716370` | ruft nur `Broadcast` des Delegates `+0x110`; Parameter TArray OldValue, Größe `0x10` (`ausruestung-felder`) |
| `InventoryComponent.Server_CreateSlotsFromBagItem` | Exec-Thunk `0x57192F0` | Implementierung über die Vtable `+0x510`; löst `OnRep_Slots` nicht aus |
| `InventoryComponent.RemoveFromInventory` Parameter | Item `+0`, Context `+8`, Destroy `+9`, Größe `0x10` | `inventar-lokal` |
| `InventoryComponent.AddToInventory` Parameter | Item `+0`, SystemAdd `+8`, Position `+0xC`, AllowP2Reserve `+0x10`, Größe `0x18` | |
| `InventoryComponent.OccupyBackpackSlots` Parameter | Item `+0`, TArray Slots `+8`, Größe `0x18` | |
| `InventoryComponent.TryGetSlotWithCoordinates` Parameter | X `+0`, Y `+4`, Slot `+8`, Found `+0x10`, Größe `0x18` | |
| `UFunction` `FunctionFlags`, `ParmsSize` | `+0xB0`, `+0xB6` | `FUNC_Net` = `0x40`, `FUNC_NetServer` = `0x200000` |
| `AController` `PlayerState`, `Pawn` | `+0x2B0`, `+0x2E8` | |
| `InventoryComponent::UpgradeItems` | `0x5980AE0`, Exec-Thunk `0x571DA70` | `(this, TArray* Inputs, Data, Rarity, FlagAsNew, Position)`, nativ; ruft `CreateItemInstance` und `AddToInventory` (`werkbank-client`) |
| Aufrufer von `UpgradeItems` | `0x5A98843` in `InventoryPanel.UpgradeDroppedItem` (`0x5A98490`), `0x5A5A7CE` Werkbank, `0x5980A0B` in `UpgradeItemToNextLevelAndReEquip` (`0x5980900`) | Rücksprungadressen |
| `URarity.NextUpgradableRarity` | `+0x58` | Ziel beim Ablegen auf ein gleiches Item |
| `UInventoryItemInstance.ItemRarity` | `+0x118` | **nicht repliziert**, beim Client null für replizierte Items |
| `UDataTable` `RowStruct`, `RowMap` | `+0x28`, `+0x30` | `RowMap` als TSet: Elemente `0x18` Byte (Wert bei `+8`), Anzahl `+0x38`, Belegungsbits ab `+0x40` oder Zeiger `+0x50`, freie Plätze `+0x64` |
| `FRecipeTableRow` `OutputItem`, `OutputRarityOverride`, `RequiredItems` | `+0x18`, `+0x20`, `+0x28` | `RequiredItems` je `0x10`: Item, RequiredRarityOverride; Tabelle `DT_RecipeTable` |
| `InventoryPanel.WorkbenchUnlock` | `+0x4D0` | `OnetimeUnlock_Workbench` |

### Coop

Funktionen von `ULocalCoopSubsystem` und ihre Aufrufer, Ablauf siehe [Coop-Ablauf](GAME_ANALYSIS.md#coop-ablauf).

| Funktion | RVA | Hinweis |
|---|---|---|
| `SetCoopEnabled` | `0x59003E0` | Exec-Thunk `0x5735F10` |
| `SetMaxLocalPlayers` | `0x5900770` | Exec-Thunk `0x57363C0` |
| `SpawnLocalPlayer` | `0x5901E90` | Exec-Thunk `0x57374E0`; in der Mod `LocalDevicePatches.cpp` |
| `GetPlayerController(Index)` | `0x58EE0D0` | |
| `GetActivePlayerCount` | `0x58E8890` | Zählt die nichtleeren Ergebnisse von `GetPlayerController(0/1)`, nicht die Map-Slots |
| `ActivePlayers`: natives Map-Add | `0x58D1940` | `Map, int32*-Schlüssel, WeakController*-Wert`; benutzt in `SpawnLocalPlayer` bei `0x590214E` |
| `ActivePlayers`: nativer SparseArray-Entferner | `0x1344470` | Elemente `0x14` Byte, `Map, Index, Count`; Hash-Link vorher aushängen, nicht der `SpawnedPlayers`-Entferner für `0x10`-Byte-Elemente |
| Lokale P2-Gerätepflicht im Reconnect-Update | `0x59072AA` | In der Mod `RemoteP2Patches.cpp`, früher Spike-Probe. Ersetzt nur fehlende P2-Bindung bei echtem aktuellem Remote-P2; P1-Test davor, Solo-Verlusttest ab `0x59072AF`, Reconnect-Pfad ab `0x59072B4` |
| Script-Multicast-Broadcast | `0x24E9410` | `OnPlayerJoined`: Delegate an `LocalCoopSubsystem+0x38`, Parameter Index bei `+0`, Controller bei `+8`, Größe `0x10`; [Probe](../research/spikes/a-connect/RemoteP2.md) |
| `GetPlayerIndex` | `0x58EE2A0` | Rückgriff ohne `LocalPlayer` |
| `IsSecondaryPlayer` | `0x58F47C0` | Rückgriff ohne `LocalPlayer` |
| `ApplyDeviceAssignmentDraftAndJoin` | `0x58D8AA0` | Ruft `SpawnLocalPlayer(1)` |
| `ApplyDeviceAssignmentDraft` | `0x58D8720` | Exec-Thunk `0x5730E90` |
| `AssignPhysicalDevice(DeviceId, PlayerIndex)` | `0x58D93B0` | Exec-Thunk `0x5731400`; Index 0 bis 1. In der Mod `LocalDevicePatches.cpp` |
| `DraftAssignPhysicalDevice(DeviceId, PlayerIndex)` | `0x58E5690` | Exec-Thunk `0x5731AD0`; Index -1 bis 1, nur bei offenem Entwurf (`+0x188`). In der Mod `LocalDevicePatches.cpp` |
| Run-Zustand laden (`IRunPersistable`) | `0x58F5330` | VTable des Subsystems `0x94ED340`, Slot 91 |
| P2 nach Kartenwechsel neu anlegen | `0x58D9810` | VTable von `ADDMissionGameMode` `0x9430630`, Slot 111 |
| Figur spawnen | `0x5901C60` | Gleiche VTable, Slot 234, Spike-Patch bei `0x5901CE8` |
| Moduswechsel: Coop aus und Coop wieder an | `0x5914B30`, `0x593BCA0` | |
| `APlayerController::GetLocalPlayer` | `0x2E706D0` | |
| Handler je Controller beim Run-Ende | `0x584CD20` | `GetLocalPlayer()` und `+0x330` ohne Nullprüfung bei `0x584CD82`, weiter bei `0x584CD90`. Spike-Patch `RunEnd.cpp` |
| `UGameInstance::CreateLocalPlayer` | `0x2A6B5B0` | |
| `UGameplayStatics::GetPlayerController` | `0x2A76140` | |
| Tabelle `{Name, Exec-Thunk}` von `LocalCoopSubsystem` | um `0x94F6EE0` bis `0x94F7190` | Einträge `0x10` groß |

### Absturzberichte

Ablauf siehe [Crash-Reporter](../research/spikes/a-connect/README.md#sperre-in-mod-und-spike-crash-reporter).

| Funktion | RVA | Hinweis |
|---|---|---|
| Start des Crash-Reporters nach einem Absturz | `0x145DD6E` | Baut die Kommandozeile, ` -Unattended` bei unattended oder `bImplicitSend` |
| `FPlatformProcess::CreateProc` | `0x144D740` | Ruft `CreateProcessW` über den Import |
| Import `KERNEL32!CreateProcessW` | `0x82774D8` | Eintrag in der Importtabelle, Spike-Patch |

### Laden, Zeitanfragen und Busplatz (`testumgebung`)

Ablauf siehe [Pausierte Welt und Busplatz](GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung).

| Funktion | RVA | Hinweis |
|---|---|---|
| Ladestart von `ULoadingSubSystem` | `0x595C910` | Fordert eigene Pause an (Priorität 1000) und ruft `ClearAllRequests(Exclude=[self])`. Aufrufer `0x596BB14`, `0x596BB2E`, `0x5976BF4`, `0x5976C0E` |
| `UTimescaleManager::ClearAllRequests(TArray<UObject*>)` | `0x46A5320` | Exec-Thunk `0x46A4830` |
| `UTimescaleManager::RequestTimescale` | `0x46A5980` | |
| `UCommonAttributeSet::OnRep_Health`, `OnRep_MaxHealth` | `0x57DC840`, `0x57DCEA0` | Implementierungen hinter den exec-Thunks `0x5696760`, `0x56968C0`; Spike-Hook `HealthBars.cpp` |
| `BusSpawnPositionGenerator`, Bus und Figuren setzen | `0x59A6CE0` | VTable-Slot 86, indirekt aus dem Timer `0x59D9090` |
| Spawnplatz freiräumen | `0x5991B70` | `(const TArray<AActor*>* Keep, ctx)`, schont Keep und deren Besitzer-Kette über `AActor+0x158`. Spike-Hook, Einsprung `48 89 5C 24 18` |

| Klasse | Feld | Offset |
|---|---|---|
| `ULoadingSubSystem` | TimescaleManager | `0xE0` |
| `UTimescaleManager` | TimeRequests: Num, freie Plätze | `0x50`, `0x7C`; ein Mapelement ist `0x1C` groß: Weak-Zeiger auf den Anfragenden, dann `FTimeRequest` |
| `UTutorialStateSubsystem` | SkipTutorialFlag | `0xB8`, `int32`, nicht im Spielstand, nach jedem Start aus. Zur Laufzeit über `Game::Reflection` |
| `UDDBalanceTelemetrySettings` | bEnabled, ServerBaseURL (`FString`), bAttachSaveFile | `0x38`, `0x40`, `0x60`, am Klassen-Defaultobjekt, ab Werk an. Zur Laufzeit über `Game::Reflection` |
| `UDDCorruptedSaveAutoReportSettings` | bEnableAutomaticCorruptedSaveReports | `0x38`, am Klassen-Defaultobjekt, ab Werk an. Zur Laufzeit über `Game::Reflection` |
| `USceneComponent` | Weltposition (Translation von ComponentToWorld) | `0x210`; `RelativeLocation` `0x148` ist bei angehängten Actors relativ |
| `APlayerState` | PawnPrivate | `0x320` |
| `APawn` | Controller | `0x2D8` |
| `APlayerController` | NetConnection | `0x520` |
| `UObject` | Flags (Klassen-Defaultobjekt `0x10`); `UStruct::SuperStruct` | `0x08`; `0x40`. In `src/Game/Offsets.h` |
| `FUObjectItem` | Flags, interne Flags, Objekt, Serial | `+0`, `+4` (Garbage `0x00200000`, Unreachable `0x10000000`), `+8`, `+0x10` |

## Klassen

Was die Mod nutzt, steht zusätzlich in `src/Game/Offsets.h` und wird dort beim Bauen gegen das SDK geprüft, siehe [Game](../src/Game/README.md#sdk).

| Klasse | Feld | Offset |
|---|---|---|
| `UObject` | Flags, Index, Class, Name, Outer | `0x08`, `0x0C`, `0x10`, `0x18`, `0x20` |
| `UStruct` | Super, Children (`UField*`), ChildProperties (`FField*`), PropertiesSize, Script | `0x40`, `0x48`, `0x50`, `0x58`, `0x60` |
| `UField` | Next | `0x28` |
| `FField` | ClassPrivate (`FFieldClass*`), Next, Name (`FName`) | `0x08`, `0x18`, `0x20` |
| `FFieldClass` | Name (`FName`, etwa `IntProperty`) | `0x08` |
| `FProperty` | ArrayDim, ElementSize, Offset_Internal | `0x30`, `0x34`, `0x44` |
| `FBoolProperty` | FieldMask (`0xFF` = natives `bool`) | `0x73` |
| `UFunction` | FunctionFlags (Native `0x400`), ExecFunction | `0xB0`, `0xD8` |
| `FFrame` | Node, Object, Code | `0x10`, `0x18`, `0x20` |
| `AActor` | Rep-Bits (Bit 3 `bReplicates`), RemoteRole | `0x5B`, `0x60` |
| `AActor` | Owner, Role, NetPriority, RootComponent | `0x158`, `0x168`, `0x190`, `0x1B8` |
| `USceneComponent` | RelativeLocation, RelativeRotation, Mobility | `0x148`, `0x160`, `0x1AB` |
| `UActorComponent` | bIsActive | `0x9A` |
| `APawn` | PlayerState, Controller | `0x2C8`, `0x2D8` |
| `AController` | Pawn | `0x2E8` |
| `UGameInstance` | LocalPlayers (`TArray`) | `0x38` |
| `UPlayer` | PlayerController | `0x30` |
| `UAbstractPanel` | Parameter von `OnOpenStateChanged`: `bOpen` | `0x00` |
| `APlayerController` | Player, AcknowledgedPawn, NetConnection | `0x348`, `0x350`, `0x520` |
| `APlayerState` | PawnPrivate | `0x320` |
| `ADDPlayerController` | LastMovementInput (`FVector2D`, Doubles, gelesen von `GetLastNormalizedMovementInput`; beim Host für Client-Controller jeden Frame (0, 0), `ausweichen-client`) | `0x910` |
| `GA_Dash_C` | CachedCharacter, laufende `ApplyRootMotionConstantForce`-Aufgabe (erst nach dem Start gesetzt) | `0x3B0`, `0x400` |
| `UAbilityTask_ApplyRootMotionConstantForce` | FinishVelocityMode, WorldDirection, Strength, Duration | `0x88`, `0xD0`, `0xE8`, `0xEC` |
| `BP_DD_Character_C` | Attributset mit Ausdauer; darin MaxStamina, Stamina (`FGameplayAttributeData`, CurrentValue je `+0x0C`) | `0x7D8`; `0x30`, `0x50` |
| `APlayerController` | CheatManager, CheatClass | `0x410`, `0x418` |
| `UBossSpawnLookup` | BossSpawnInfos (je `0x68`: BossID `0x00`, IsEndBoss `0x08`, BossEnemyType `0x10`, SpawnPointClass `0x38`) | `0x28` |
| `UEnemyType` | EnemyBlueprint (`TSoftClassPtr`) | `0x128` |
| `ACharacter` | CharacterMovement, BasedMovement, ReplicatedMovement-Block | `0x330`, `0x340`, `0x398` |
| `ACharacter` (Spiel) | AttributeSet | `0x7D8` |
| `ADDCharacterBase` | CommonAttributeSet | `0x700` |
| `UCommonAttributeSet` | Health (`FGameplayAttributeData`, CurrentValue bei `+0xC`) | `0xA0` |
| `UCommonAttributeSet` | MaxHealth, OnMaxHealthChanged, OnHealthChanged (Multicast, Einträge je `0x10`: schwacher Zeiger, `FName`) | `0x30`, `0x40`, `0xC0` |
| `UDDCharacterAttributeSet` (Outer ist der PlayerState) | ExtraHands, OccupiedHands (`FGameplayAttributeData`, CurrentValue bei `+0xC`) | `0x100`, `0x110` |
| `UHandBarDisplay` | PlayerIndex, CachedCharacter | `0x328`, `0x348` |
| `FDamageInformation` | Damage, DamagedActor, InstigatedBy, DamageCauser (je `TWeakObjectPtr`) | `0x00`, `0x04`, `0x14`, `0x1C` |
| `FBasedMovementInfo` | MovementBase, Location | `0x10`, `0x18` |
| `UCharacterMovementComponent` | Velocity, MovementMode, MaxWalkSpeed | `0xD8`, `0x241`, `0x288` |
| `UCharacterMovementComponent` | MaxAcceleration, Acceleration | `0x29C`, `0x338` |
| `UCharacterMovementComponent` | Flags: Bit 1 `bIgnoreClientMovementErrorChecksAndCorrection`, Bit 3 `bServerAcceptClientAuthoritativePosition` | `0x579` |
| `UWorld` | PersistentLevel, NetDriver, GameMode, GameState, GameInstance | `0x30`, `0x38`, `0x1A8`, `0x1B0`, `0x228` |
| `AGameStateBase` | PlayerArray | `0x2C0` |
| `UNetDriver` | ServerConnection, ClientConnections | `0x100`, `0x108` |
| `UNetDriver` | bClampListenServerTickRate (Bit 0), NetServerMaxTickRate, MaxNetTickRate, MaxInternetClientRate, MaxClientRate | `0xAC`, `0xB0`, `0xD0`, `0xD4`, `0xD8` |
| `UPlayer` (auch `UNetConnection`) | CurrentNetSpeed, ConfiguredInternetSpeed, ConfiguredLanSpeed | `0x38`, `0x3C`, `0x40` |
| `UTutorialStateSubsystem` | SkipTutorialFlag | `0xB8` |
| `UDDBalanceTelemetrySettings` | bEnabled, ServerBaseURL, bAttachSaveFile | `0x38`, `0x40`, `0x60` |
| `UDDCorruptedSaveAutoReportSettings` | bEnableAutomaticCorruptedSaveReports | `0x38` |
| `FWorldContext` | TravelURL | `0xA8` |
| `ADDMissionGameMode` | SpawnedPlayers (TSet) | `0x388` |
| Controller-TSet | Eintrag / HashNext / HashIndex | Stride `0x10`, Felder `+0`, `+8`, `+0xC` |
| Controller-TSet | Allokationsbits / sekundäre Bits / Bitanzahl | Inline `+0x10`, Zeiger `+0x20`, Anzahl `+0x28` |
| Controller-TSet | FirstFree / NumFree / Hash | `+0x30` / `+0x34`; Inline-Hash `+0x38`, sekundär `+0x40`, Größe `+0x48` |
| Controller-TSet | Nativer Add-Helfer (RVA) | `0x57BD5E0`, `(Set*, int32* ResultIndex, Controller* const*, bool* AlreadyPresent)` |
| `ADDMissionGameMode` | Nativer Controller-Registrierungsweg (RVA) | `0x58FB020`, `(GameMode*, Controller*)`; TSet-Add bei `0x58FB11D`, danach Broadcast bei `0x58FB133`. Spike hängt **nach** der Originalfunktion an; kein erneuter Registrierungsaufruf. Statisch belegt, Travel-Abdeckung des Hooks noch zu messen. |
| Controller-TSet | Nativer SparseArray-RemoveAt-Helfer (RVA) | `0x12D6BA0`, `(Set*, int32 Index, int32 Count)`; Hash vorher aushängen |
| TSet | Data, Num, FirstFree, NumFree, Element-Stride | `+0`, `+8`, `+0x30`, `+0x34`, `0x10` |
| GameInstance | PendingGameStateComponentTravelStates, LastRoundChangeReason | `0x248`, `0x299` |
| GameInstance | CurrentRunInfo (Seed +0, CurrentRoute +8) | `0x2A0` |
| GameInstance / `FRoute` | `CurrentRoute.MissionChain` (TArray), Num; CurrentMissionIndex | `0x2B0`, `0x2B8`; `0x2C0` |
| GameInstance / `FCurrentRunInfo` | IsEndOfMissionChain, ChosenMissionIndexes (TArray) | `0x2C8`, `0x328` |
| `ADDMissionGameState` | SecondsSinceMissionStart, DayNum | `0x38C`, `0x390` |
| `UAbstractPanel` | AnimationContainer | `0x330`; im `rundenende`-Test nicht als zuverlässiger Phasenindikator lesbar |
| `UPanelAnimationContainer` | IsOpen | `0x328`; nur lokaler Containerzustand, keine Netz-/Sichtbarkeitszusage |
| `UInventoryComponent` | Slots, MyItems | `0x100`, `0x120` |
| `UInventoryComponent` | Vtable-RVA | `0x94BA3F0` |
| `UActorComponent` | `bReplicateUsingRegisteredSubObjectList` | `0x98`, Bit 6 (`0x40`) |
| `UBackpackSlot` | Vtable-RVA; Coordinates, Occupier, ComponentOwnerPrivate | `0x939CE08`; `0x28`, `0x30`, `0x38` |
| `UInventoryItemInstance` | Vtable-RVA; ZeroPivotTile, Turns, ItemData | `0x94B46E8`; `0x28`, `0xD8`, `0xE0` |
| `UInventoryItemInstance` | StateObjectLookup (`TMap`, Belegungsbits-Anzahl bei `+0x340`) | `0x318` |
| `UInventoryItemInstance` | Broken, IsInBackpack, IsReward, IsTrash | `0x110`, `0x180`, `0x182`, `0x184` |
| `APlayerState` | PlayerId (numerischer Replikationsschlüssel, keine Steam-ID) | `0x2AC` |
| `AAllPlayerFramingCamera` | Camera, MaxMovementDistance, UseZoomOutAtEdge, ZoomOutDistance, CameraSpringArm, GameMode | `0x2A8`, `0x2F0`, `0x2F4`, `0x2F8`, `0x310`, `0x318` |
| `UInViewportMovementRestrictor` | SlowdownZone, FullStopZone (je 4 Floats L, R, D, U) | `0xD8`, `0xE8` |
| `UCharacterMovementComponent` | Velocity, GravityScale, MovementMode, GroundFriction (`bewegung`) | `0xD8`, `0x1A8`, `0x241`, `0x244` |
| `ACharacter` | ReplicatedBasedMovement (`FBasedMovementInfo`: Flags `+0x02`, MovementBase `+0x10`), ReplicatedMovementMode (gepackt) | `0x398`, `0x472` |
| `AActor` | ReplicatedMovement (`FRepMovement`: LinearVelocity `+0x00`, Location `+0x30`), RootComponent, Role | `0xD0`, `0x1B8`, `0x168` |
| `USceneComponent` | RelativeLocation | `0x148` |
| `UClass` | ClassDefaultObject | `0x118` |
| `UInViewportMovementRestrictor` | Interface `IMovementPreProcessorInterface`, MovementInputPreprocessorSubsystem, MissionGameState | `0xC0`, `0x100`, `0x110` |
| `USpringArmComponent`, `UCameraComponent` | TargetArmLength, FieldOfView | `0x250`, `0x250` |
| `APlayerController` | PlayerCameraManager | `0x360` |
| `APlayerCameraManager` | ViewTarget.Target, ViewTarget.POV.Location | `0x340`, `0x350` |
| `AEnemySpawner` | AutoSpawn, UseViewportEdgesForSpawningPositions, AllowedViewportEdgesForSpawning | `0x348`, `0x34A`, `0x358` |
| `BP_PowerUpCrate_C` | DestructablePowerUp | `0x390` |
| `AActor` | Bits: `bHidden` (`0x58` Bit 7), `bActorEnableCollision` (`0x5D` Bit 0), `bActorIsBeingDestroyed` (`0x5D` Bit 1) | `0x58`, `0x5D` |
| `AEnemySpawner` | CurrentSpawnedEnemiesCount, MaxSpawnedEnemies, EnemyCap, GameMode, RecycleArray | `0x2F8`, `0x36C`, `0x370`, `0x3C8`, `0x478` |
| `UPoolManagerSubsystem` | PoolsInternal, AllFactoriesInternal | `0x38`, `0x48` |
| `AMapGenerator` | MapGenProfile, MapSeed, MaxAttempts, MaxTilePosition, CycleTimeBudget, GenerateOnStart | `0x2B8`, `0x2D8`, `0x2DC`, `0x2E8`, `0x2F8`, `0x2FD` |
| `AMapGenerator` | GenerationTags, CurrentGenData | `0x300`, `0x320` |
| `UMapGenerationState` | Seed, BaseSeed, SpawnedActors, HasFailedGeneration, RetryAttemptsRemaining, RandomGeneratorLookup | `0x50`, `0x54`, `0x88`, `0x111`, `0x114`, `0x258` |
| `UMapGenerationState` | QueuedSubGenerators, QueuedPostGenerators, CurrentSubGenerator | `0x60`, `0x70`, `0x80` |
| `UChildActorComponent` | ChildActorClass, ChildActorTemplate | `0x250`, `0x260` |
| `UActorComponent` | OwnerPrivate | `0xA8` |
| `UPCGVolumeGenerator` | bUseGraphFromBiome, GraphToGenerate, PCGActor, PCGComponent, BiomeGraphToGenerate | `0x28`, `0x30`, `0x40`, `0x48`, `0x50` |
| `UDDMissionGameInstance` | Route: Einträge (je `0x290`), Anzahl, Index; Biom im Eintrag | `0x2B0`, `0x2B8`, `0x2C0`; `0x1A8` |
| `UBiomeDefinition` | GraphToGenerate | `0x118` |
| `ABP_PCGVolume_C` | PCG_Component | `0x2B0` |
| `UPCGComponent` | Seed, bGenerated | `0xC8`, `0x6F8` |
| `UStaticMeshComponent` | StaticMesh | `0x5E8` |
| `UInstancedStaticMeshComponent` | PerInstanceSMData (je `0x80`), InstancingRandomSeed | `0x6A8`, `0x734` |
| `ULocalCoopSubsystem` | ActivePlayers (TMap, Element `0x14`), bCoopEnabled, MaxLocalPlayers | `0x100`, `0x180`, `0x184` |
| `ULocalCoopSubsystem` | Flag „P2 nach Kartenwechsel neu anlegen“ | `0x18A` |
| `ULocalCoopSubsystem` | OnPlayerJoined (Script-Delegate), Gerätebindungen (TArray), ReconnectRequired | `0x38`, `0x150` (Element `0x38`), `0x189`; reine lokale Zustände, kein Netzwerk-Sync |
| `UThreatLevel` | FreezeThreatLevelManager | `0x190` |

In einem TSet ist ein freier Platz daran zu erkennen, dass der Wert hohe Bits gesetzt hat.

`CurrentRoute` ist eine **eingebettete Struktur**, kein UObject-Zeiger. Ihre ersten acht Bytes sind `RouteMissionStructureIdentifier`. Die Num bei `0x2B8` darf nicht aus einem vermeintlichen Zeiger bei `0x2A8` abgeleitet werden. Im `rundenende`-Test: Host Num 1 / Index 0, Client Num 0 / Index -1. Die reflektierten Funktionen `FinishMission`, `TriggerEndRound`, `SetFreezeTime` tragen kein `FUNC_Net` (`0x40`); ihre lokalen Aufrufe sind kein Client-Transport.

## Blueprint-Referenzinventar (`blueprint-logik`)

### Darstellungsdiagnose (`client-visuell`)

Ergänzung vom 04.11.2026 für Build `26187885`, aus dem lokalen SDK und begrenzten
lesenden Laufzeitvergleichen. Keine zusätzlichen Produktionspatches.

| Klasse | Feld | Offset |
|---|---|---|
| `AActor` | bReplicateMovement, bHidden | `0x58`, Masken `0x10`, `0x80` |
| `AActor` | bActorIsBeingDestroyed | `0x5D`, Maske `0x02` |
| `AActor` | RootComponent | `0x1B8` |
| `UActorComponent` | bReplicates | `0x98`, Maske `0x80` (`kampf`) |
| `USceneComponent` | RelativeScale3D (Net, RepNotify) | `0x178` |
| `ULevel` | Actors (TArray) | `0xA0` |
| `AProjectile` | ProjectileVisualComponent | `0x400` |
| `AExpEmissionHandler` | ExpEmitter | `0x2F8` |
| `AExpEmissionHandler` | ExpBoundsBuffer / ExpBoundsName | `0x2D0` / `0x2E0` |
| `AExpEmissionHandler` | berechnete Sicht-Halbausdehnung / Spielerzentrum (FVector) | `0x320` / `0x338` |
| `AExpEmissionHandler` | PlayerCameraWorldSubsystem | `0x3E8` |
| `UPlayerCameraWorldSubsystem` / `ABasePlayerCameraActor` | CurrentPlayerCamera / Camera | `0x38` / `0x2A8` |
| `AExpEmissionHandler` | ExpData, PendingExpData (TArray) | `0x378`, `0x388` |
| `AExpEmissionHandler` | SpawnLocationsInNiagara, EndLocationsInNiagara (TArray) | `0x398`, `0x3A8` |
| `AExpEmissionHandler` | OrbValuesInNiagara, OrbColorsInNiagara (TArray) | `0x3B8`, `0x3C8` |
| `ADDMissionGameState` | DayNum, LevelSystem | `0x390`, `0x3B8` |
| `UDDWidgetController` | PlayerState | `0x30` |
| `WP_PlayerOverlay_C` | ExperienceProgressBar, ExperienceProgress, TargetExpProgress | `0x3D0`, `0x3E8`, `0x3F0` |
| `WP_PlayerOverlay_C` | OverlayWidgetController | `0x418` |

Ergänzung 05.11.2026: XP-Anzeige liest native Levelsystem-Getter und ändert nur
HUD-Zustand. Gegen stale Widgets werden PlayerState-Zugehörigkeit zur aktuellen
GameState-PlayerArray sowie lokale Objektindex-/Serial-Identität geprüft. Letztere
bleibt lokal. Die Weltzuordnung im Snapshot verwendet die originale GameState-
NetGUID aus dem unten beschriebenen Cache; vor dessen Verfügbarkeit kein Apply.
Die neuen SDK-Felder haben optionale `static_assert`s in `ProgressAdapter.cpp`.

Airdrop-Anbindung, Build `26187885`, 05.11.2026: lokale SDK-Deklarationen und
begrenzte Laufzeitprüfung der nativen Einstiegspunkte; keine generierten Körper
übernommen. `AirdropAdapter.cpp` prüft die SDK-Felder optional beim Bauen.

| Objekt | Feld / Funktion | Offset / RVA |
|---|---|---|
| `ACrate` | bIsOpened, bMapLootDistributionReady | `0x2A8`, `0x2A9` |
| `ARewardChest` | CanBeOpened | `0x2E1` |
| `BP_Airdrop_Lootable_C` | SK_AirDropAnimMerged, LootableReady | `0x358`, `0x360` |
| `ARewardChest` | OpenCrate, nativer virtueller Einstieg | `0x5A43400` |
| `ARewardChest` | OpenAirdropChoices, nativer Einstieg | `0x5A42F50` |

Hook-Präfixe: OpenCrate `48 89 5C 24 08 48 89 6C 24 10`, OpenChoices
`48 89 5C 24 08 57 48 83 EC 20`. Beide werden vor der Installation geprüft.
Hooks unterdrücken nur Airdrop-Gameplay auf Netzwerkclients bzw. Nichtautorität;
andere Kistentypen bleiben unverändert. Anzeige verwendet `Crate.SetIsOpened`,
nicht `OpenCrate`/`OpenAirdropChoices`. Fehlende Host-Actors werden nur verborgen,
nicht zur Erzeugung von Rewards oder künstlichen Öffnungsereignissen benutzt.

XP-Bälle, Build `26187885`, 05.11.2026, aus Aufrufzielen und Laufzeitprüfung:

| Objekt | Funktion | RVA |
|---|---|---|
| `UExpSubsystem` | Spawn-Anfrage, gemeinsamer Kern aller Ausstöße (rcx Subsystem, rdx Anfrage) | `0x58C1030` |
| `UExpSubsystem` | RequestExpSpawning, nativer Einstieg (ruft `0x58C1030`) | `0x58C1260` |
| `UExpSubsystem` | ReceiveExp, nativer Einstieg | `0x58BE8B0` |
| `AExpEmissionHandler` | Sichtbereich aktualisieren (CPU-Grenze und Niagara-Variable) | `0x58BE7C0` |
| `DDBlueprintHelperFunctions` | native vier Viewport-Bodenecken; rcx Ausgabe (4 FVector), rdx Controller, r8 CameraComponent, r9 Ausgabe Spielerposition | `0x5B028D0` |
| `DDBlueprintHelperFunctions` | Mehrspieler-Viewport-Ecken über GameMode.SpawnedPlayers / BoundsAroundPlayers | `0x5B02DD0` / `0x5B0C210` |
| `UExpSubsystem` | exec-Thunks RequestExpSpawning / ReceiveExp | `0x56EAD70` / `0x56EA930` |

Spawn-Anfrage, 0x58 Byte: Menge `int32` @0x00, Start `3×double` @0x08, Ziel `3×double` @0x20,
Farbe `4×float` @0x38, Zusatz `double` @0x48, Zusatz `float` @0x50. Die beiden Zusatzwerte sind
in ihrer Bedeutung offen und werden bitgenau übertragen. Hook-Präfixe: Spawn-Anfrage
`48 89 5C 24 08 48 89 6C 24 10`, ReceiveExp `48 89 5C 24 18 55 56 57 48 83 EC 30`.

Projektile: `AProjectile.OnTakeFromPool_BP` läuft bei jeder Pool-Entnahme über ProcessEvent,
auch ohne Blueprint-Implementierung. Daran schaltet der Host `Actor.SetReplicateMovement(true)`
einmal je Actor ein. Keine neue RVA. **Korrektur 05.11.2026 (`kampf`):** Beim Host wurde der Aufruf über
ProcessEvent nie gesehen. Der Host erfasst neue Projektile stattdessen am Ende von `Level->Actors`, siehe
[Kampf beim Client](GAME_ANALYSIS.md#kampf-beim-client-kampf).

Arraygrößen sind gespeicherte CPU-Daten, keine lebenden GPU-Partikelzahlen.
[Beobachtungen und verworfener Schalterversuch](../research/spikes/client-visuell/README.md).

Nachtrag 03.11.2026: Grenzpruefung fuer 42 Funktionen und 25 Eventgraph-Einspruenge
sowie gezielte Runtime-Traces bestanden; keine neuen nativen RVAs. Zusaetzlich
im Mission-SDK nachgeschlagen: `Projectile.BulletSize` bei `0x2FC` (nicht Net),
`BP_Effect_Add_ImpactEffect_Template.ApplyToSelf` bei `0x48`. Der Effekt-Guard wurde
am frisch erzeugten Testobjekt mit beiden Werten ausgefuehrt. Keine SDK-Dateien
oder Script-Rohbytes gespeichert; [Testgrenzen](../research/spikes/blueprint-logik/README.md#nachtrag-03112026-grenzpruefung-und-ereignistests).

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
`inventar`-Testgrenze: Reise auf Tag 3 führte zweimal beim Host in den Callback `0x57CC390` (Fehler bei `0x57CC395`/`0x57CC398`, indirekter Aufruf `+0x490`; nächster Stack-Eintrag `0x315A764`). `tag3` ordnet ihn nun `HitByEnemyTypeAchievementDetectorComponent` zu. Ein nachfolgender Client-Absturz lag bei `0x57D080C`; im neuen direkten Traveltest bei `0x178BA90` (Lesen `0x8C`).

Timer-Lebensdauerfix `tag3`, nur Build `26187885`:

| Stelle | RVA / Layout | Befund |
|---|---|---|
| Detector-Vtable | `0x94C1758`, BeginPlay `+0x3E8`, EndPlay `+0x3F0` | BeginPlay `0x57CB220`, EndPlay `0x57D0A40` |
| Next-Tick-Timer registrieren | CALL `0x57CB3C5` → `0x314D4D0` | `(TimerManager*, uint64* ResultHandle, TFunction*)`, roher Capture bei `TFunction+0x18` |
| FindTimer | `0x31471C0` | `(TimerManager*, const uint64* Handle)`; vollständigen Handle vor Clear prüfen |
| Timer intern löschen | `0x314D0B0` | `(TimerManager*, uint64 Handle)`; nativer K2_ClearTimerHandle-Pfad |

Neuer Client-Folgefehler: Spawner-Initialisierung `0x58A58E0`, Aufruf `0x58A6060` → Aggregationsschleife `0x58B4160`, dort Aufruf `0x58B41A6` → Attribut-Getter `0x178BA90`. `Character+0x7D8` wird ohne Prüfung genutzt, Attribut `+0x80`, Getter-Lesefeld `+0xC`; fehlendes AttributeSet ergibt `0x8C`. Inzwischen behoben über den Einstieg `0x58B4160`, siehe unten.

Route bis Tag 4, `tag3`, nur Build `26187885`:

| Stelle | RVA / Layout | Befund |
|---|---|---|
| MapGen-Lambda | Vtable `0x9613280`, Execute `+0x0` = `0x59CAE70`, ExecuteIfSafe `+0x8` = `0x59CB020` | Capture: Generator `+0x18`, `&CurrentGenData` `+0x20`; Schritt `0x59D9090` |
| Exp-Aggregation | `0x58B4160`, Figuren-Snapshot `0x5B07680`, Array freigeben `0x12F8400` | Ergebnis bei Handler `+0x314`; Aufrufer `0x58A6060` (BeginPlay), `0x58B0F6E` (Update) |
| AbilitySystem-Suche | `0x4B68180`, Lesen `[rcx+0x1080]` bei `+0x15` | ohne Prüfung; `tageswechsel` prüft `rcx` am Einstieg |
| Suche im Attribut-Tick | Call `0x59B7E17`, Ausgang `0x59B858A` | Client ohne PlayerState |
| Suche nach `OnRep_DayNum` | Call `0x5847AE6`, Ausgang `0x5847AF2` | Tag 4, Client; Patch durch den Einstieg ersetzt |
| Kartengenerator-Schritt | Funktion `0x59DFC70`, Lesen `+0xC8` bei `0x59DFD5A`, Ausgang `0x59DFE07` | `+0xC8` nur beim Host |
| Dritter Aufrufer | Rücksprung `0x59A4B90` | Aufrufer von `0x4B68180` mit leerem Besitzer, Tag 4; durch den Einstieg gedeckt |

NetGUID-Cache und neue Actors, `tageswechsel`, nur Build `26187885`:

| Stelle | RVA / Layout | Befund |
|---|---|---|
| `SerializeNewActor` | `0x2D09B90` (PackageMap-Vtable `+0x2C0`), Archetyp-Prüfung `0x2D0A426` (`cmp qword [rsp+38h],0`) | `Archive+0x28 & 1` = Laden; Spawn-Aufruf `0x2BFB580` |
| `UActorChannel::ProcessBunch` | IsValid-Prüfung `0x2995F58`, Broken `0x2995F7B`, `NMT_ActorChannelFailure` `0x29A2310` | Host ignoriert die Meldung |
| UChannel | Connection `+0x28`, Flags `+0x30` (OpenAcked `0x1`, Broken `0x20`, OpenedLocally `0x1000`), Index `+0x34` | UActorChannel: Actor `+0x68`, ActorNetGUID `+0x70` |
| `FNetGUIDCache` | `NetDriver+0x1C8` = `PackageMapClient+0x310` (Connection `+0x68`) | ObjectLookup: Daten `+0x8`, Num `+0x10`, Max `+0x14`, Bitarray `+0x18` inline oder Zeiger `+0x28`; NetGUIDLookup `+0x58` |
| ObjectLookup-Element | `0x40` Bytes | Schlüssel `+0x0` (Maske `0x3FFFFFFFFFFFFF`, statisch = ungerade), Weak-Pointer `+0x8` (Index, Serial), OuterGUID `+0x10`, Pfad-FName `+0x18`, Prüfsumme `+0x20`, Stempel `+0x28` (double), Flags `+0x30` (1 NoLoad, 2 IgnoreWhenMissing, 4 Pending, 8 Broken), HashNext `+0x38` |
| `CleanReferences` | `0x2CF34B0`, Stempel lesen `0x2CF3622`, Ablauf `0x2CF362E`, Löschen `0x2CF363F`, Schleife weiter `0x2CF3C38`, Stempel setzen `0x2CF3C33` | Ablauf 90 s, Weak-Pointer gültig? `0x1643520` |
| Test-Invincibility | `DDCharacter.DD_ToggleInvincible` Exec `0x56BB350`, Implementierung `0x5809390`; Tag `0xAC61FE8` | Laufzeitdekodierung ergibt `State.Invincible`; Character-ASC `+0x6E0`, Tag-Interface `ASC+0x148`, Query-VTable `+0x18`; Loose-Tag-Delta `0x4B7D7D0(ASC, Tag*, Delta, 0)`. Nur Spike-Testhilfe, nicht allgemeiner Damage-Fix |
| `FNetGUIDCache` eigener NetDriver | `+0x150` | Laufzeitbeobachtung mit identischem Driver wie `World+0x38`; Rückverweis über `Driver+0x1C8` prüfen. `GWorld` kann während Travel-GC null sein und eignet sich dann nicht zur Cache-Rollenprüfung |
| `SneakApproachComponent` Tick / Zustandsabfrage | `0x58CBBB0`, `0x58C5D40` | Tick-VTable `0x95D1428`, Slot 128; Zustandsabfrage dereferenziert PlayerCharacterMesh bei `0x58C5D8B` (`+0xA8`) |
| Sneak-Zielinitialisierung / Wiederverwendung | `0x58A51F0`, Same-Target-Zweig `0x58A5266` | CachedPlayer `+0xE0`, Mesh `+0xE8`, PlayerState `+0xD8`; normale Initialisierung `0x58A526C`, vollständiger Erfolg `0x58A530D`. Wiederverwendung muss die Abhängigkeiten erneut prüfen |
| `ObjectLookup.Remove` | `0x2D05470`, Aufrufer `0x2D04C2D`, `0x2D04CB9` in `0x2D04BE0` | SparseArray `RemoveAt` `0x15B44B0` |
| Einfügen mit Ersetzen | `0x2CE9BA0`, Ersetzen `0x2CE9D42`, Aufruf `0x2D04B94` | noch nicht untersucht |
| GUObjectArray-Element | `0x18` Bytes | Flags `+0x0`, interne Flags `+0x4` (Garbage `0x00200000`, Unreachable `0x10000000`, RootSet `0x40000000`), Objekt `+0x8`, Serial `+0x10` |

## Enums

`ERoundChangeReason`: `NoReason` 0, `NewDay` 1, `Win` 2, `Lose` 3.

## Tank (`rundenende-absturz`)

| Stelle | Wert |
|---|---|
| `UFuelMeter` | `MaxRequiredFuel` `0xF4`, `CurrentFuel` `0xF8` (Net) |
