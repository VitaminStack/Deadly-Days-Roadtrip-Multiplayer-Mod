# Spielanalyse

Stand: 02.11.2026. Neue Erkenntnisse bitte hier ergänzen, ausführliche Notizen gehören nach `research/`.

Vertrauensgrad:

- **[Belegt]** direkt an der aktuellen Installation geprüft
- **[Hypothese]** Schlussfolgerung, noch nicht getestet

## Unterstützte Spielversion

| | |
|---|---|
| Steam-AppID | `3026450` |
| Steam-Build-ID | `26187885` |
| Engine-Version | `5.8.1-56057345+++UE5+Release-5.8` |
| Spiel-Exe | `DDSurvivors/Binaries/Win64/DDSurvivors-Win64-Shipping.exe` (179 MB) |

Die Build-ID steht in `steamapps/appmanifest_3026450.acf`. Bei einem Spiel-Update siehe
[Update-Regel](../CONTRIBUTING.md#spiel-updates).

## SDK-Erzeugung unter UE 5.8

Dumper-7 (auch der aktuelle Main-Stand vom November 2026) stürzte beim Lesen der Property-Klassennamen ab. Mit dem Fix unten ist ein
vollständiges SDK für Build `26187885` erzeugt (02.11.2026), die Dateien `Basic`, `CoreUObject_functions`, `Engine_functions` und
`DDSurvivors_functions` kompilieren mit MSVC.

- **[Belegt] Ursache:** UE 5.8 hat vor dem Namen von `FFieldClass` einen zusätzlichen Zeiger. Dumper-7 las diesen als FName,
  daher die feste Absturzadresse `0xCDB0` (`0x66D8 * 2`).
- **[Belegt] Layout:** `Name` 0x08, `ClassFlags` 0x10, `Id` 0x18, `CastFlags` 0x20, `SuperClass` 0x28, Gesamtgröße 0x50.
- **Fix in Dumper-7:** In `OffsetFinder::FixupFieldClassOffsets()` wird das Layout daran erkannt, dass die gefundenen `CastFlags`
  nicht vor dem fest hinterlegten `SuperClass`-Offset liegen. Dann werden die vier Offsets oben gesetzt.
- **Nacharbeit nach jedem Dump:** In `Assertions.inl` sind die Prüfungen für `UEngineReplicationBridge`,
  `URepositionableTransformGizmo` und `UPolyEditPreviewMesh` abzuschalten. Sie schlagen sonst beim Kompilieren fehl.
  Die Ursache ist nicht untersucht. Bei Nutzung von `UEngineReplicationBridge` deren Layout zuerst prüfen.
- **Dump-Zeitpunkt:** Immer während einer laufenden Mission dumpen, denn Blueprint-Klassen werden nur erfasst, wenn sie geladen sind.
  Das Hauptmenü liefert 64.952 Objekte und 562 Blueprint-Klassen, eine Mission 110.688 Objekte und 925 Blueprint-Klassen plus 178 Widgets.
- **Grenze:** Auch der Mission-Dump enthält nur, was in dieser einen Mission geladen war. Bosse fehlen zum Beispiel weitgehend.

## Technische Basis

- **[Belegt]** Unreal Engine 5.8, Shipping-Build, monolithische Exe. Es gibt also keine Game-Module als einzelne DLLs.
- **[Belegt]** Keine PDB mitgeliefert. Sie steht zwar in `Manifest_DebugFiles_Win64.txt`, liegt aber nicht in der Installation.
- **[Belegt]** Paks sind **nicht verschlüsselt** (Pak-Footer und `.utoc` ohne Encryption-GUID, IoStore-Container komprimiert).
  Assets und Configs lassen sich also auslesen.
- **[Belegt]** In der Plugin-Liste steht kein Anti-Cheat.
- **[Belegt]** Plugins des Spiels: `OnlineSubsystemSteam`, `PixelSteam`, `PixelLeaderboards`, `PixelAchievements`,
  `PaperZD`, `FMODStudio`, `PoolManager`, `TwitchSDK`, `XInputDevice`, `Prismatiscape` und weitere.
- **[Belegt]** Steamworks-Laufzeit `Steamv164` (`steam_api64.dll`) ist vorhanden.
- **[Belegt]** Spielinhalte liegen unter anderem in `Content/DDSurvivors`, `Content/Map_Generation`, `Content/Characters`,
  `Content/Inventory`. Die Dateinamen enthalten keine Netzwerk-Assets.

## Netzwerk

- **[Belegt]** Die Exe enthält `IpNetDriver`, `IrisCore` (`net.Iris.*`) und `OnlineSubsystemSteam` mit Lobby-Code.
- **[Belegt]** Die Strings `SteamNetDriver` und `SteamSocketsNetDriver` kommen in der Exe nicht vor. Vorhanden sind nur
  `SteamSocketsIP` und `SteamSocketsP2P`. Für Verbindungen über Steam ist also vermutlich ein eigener Tunnel nötig.
- **[Belegt]** Die Exe enthält viele `OnRep_*`-Namen für Spielwerte: `OnRep_Health`, `OnRep_Armor`, `OnRep_Exp`,
  `OnRep_Level`, `OnRep_CurrentFuel`, `OnRep_DayNum`. Das passt zu Attributen im Gameplay Ability System.
- **[Belegt]** 44 native RPCs in Spielklassen (von 3914 Funktionen im aktuellen SDK): 40 Server, 2 Client, 2 Multicast.
  - `CharacterEffectManager` (15): `AddSpec`, `ApplyItemToBackpack`, `RemoveEffectsFromItem` und weitere
  - `InventoryComponent` (11): `AddToBackpack`, `RemoveFromBackpack`, `AddToInventory`, `RepairItem` und weitere
  - `InventoryItemInstance` (5): `UpdateIsInBackpack`, `UpdateIsReward` und weitere
  - `DDCharacter` (5): `StartFiring`, `FinishFiring`, `Server_SpawnBullet`, `ProxyRotateTowards`, `ProxySetAutoFire`
  - `DDPlayerController` (2), `Hands` (2), `LootBase`, `DDAbilitySystemComponent`, `DDCharacterBase` (`Multi_OnDie`), `Weapon`
- **[Belegt]** Gegenüber dem Juli-Dump (47 RPCs) kam kein RPC hinzu. Entfernt wurden `Crate.Multi_PlayOpenCrate`,
  `LevelSystemComponent.Server_CheckExpRecipient` und `LootAttractor.Client_SetExpEmissionHandlerRadius`.
- **[Belegt]** Keine der neun Coop-Klassen hat einen RPC. Das Coop-System wurde also ohne Netzwerk gebaut.
- **[Belegt]** Rund 79 replizierte Properties, vor allem in `UDDCharacterAttributeSet` (29), `UDDEnemyAttributeSet` (15) und
  `UCommonAttributeSet` (10), dazu `UInventoryItemInstance`, `UInventoryComponent`, `UFuelMeter`, `UCharacterEffectManager`
  und `ADDMissionGameState` (`SecondsSinceMissionStart`, `DayNum`).
- **[Belegt]** Die Blueprint-Ebene ist fast ohne Netzwerkcode: In 925 geladenen Blueprint-Klassen gibt es nur 2 RPCs
  (`BP_Airdrop_Lootable.StartFallDown`, `BP_PowerUpCrate.ShowEffect`) und 2 replizierte Properties (`BP_EnvironmentObjectLootspot`).
- **[Belegt]** Gegner, Projektile, Spawner und Item-Effekte liegen als Blueprints vor, zum Beispiel `BP_Enemy`, `BP_StandardEnemy`,
  `BP_EnemySpawner`, `BP_Projectile` und viele `BP_Effect_*`. Die meisten erben `bReplicates` von ihrer nativen Basisklasse, siehe unten.
- **[Hypothese]** Der Netzwerkcode ist Altlast aus einer früheren Entwicklungsphase: Er wurde nicht ausgebaut, aber auch nicht
  weiterentwickelt, wie die fehlenden RPCs bei neuen Systemen und die entfernten RPCs zeigen. Kampf, Attribute und Inventar sind
  damit mögliche Grundlagen. Ob dieser Code noch läuft, ist offen.

### Konfiguration

Aus `DefaultEngine.ini`, `DefaultGame.ini` und `BaseEngine.ini` im Pak (Spielversion `1.0.0-main-1b2201ce`). Die Dateien im Pak sind mit
Oodle komprimiert.

- **[Belegt]** `DefaultPlatformService=Steam`. Der `GameNetDriver` ist der `IpNetDriver` aus der Engine, das Spiel überschreibt ihn nicht.
- **[Belegt]** `DefaultEngine.ini` setzt `net.MaxRPCPerNetUpdate=10`. Die Einstellung ist die einzige `net.*`-Zeile des Spiels. Sie wäre ohne
  Netzwerkspiel wirkungslos, stammt also vermutlich aus einer Phase mit aktivem Netzwerk.
- **[Belegt]** Start über `GameDefaultMap=/Game/Maps/MainMenu`, `GM_DDMissionMode`, `GI_DDMissionGameInstance` und `CoopGameViewportClient`.
  `bUseSplitscreen=False`, `bEnableDevCoopButtons=false`.
- **[Belegt]** Engine-Standardwerte: `NetServerMaxTickRate=30`, `MaxClientRate=100000`, `ConnectionTimeout=60`.
- **[Belegt]** Iris ist für den `GameNetDriver` erlaubt (`bCanUseIris=true`), `net.Iris.UseIrisReplication` steht aber in keiner dieser Dateien.
  Im Test lief die klassische Replikation mit `ActorChannel`, nicht Iris.

### Verbindungstest

Details stehen in [Spike A und B](../research/spikes/a-connect/README.md).

- **[Belegt]** Native Verbindung funktioniert: Zwei Instanzen auf einem PC verbinden sich über den `IpNetDriver`, auch unter demselben Steam-Konto.
- **[Belegt]** `servertravel` nimmt den Client in eine andere Karte mit. Der normale Kartenwechsel über das Menü behält den Netzwerktreiber, lässt den Client aber zurück.
- **[Belegt]** Der Spawn eines Remote-Spielers stürzt beim Host ab, weil das Spiel einen `LocalPlayer` voraussetzt. Mit zwei kleinen Patches entstehen Controller, PlayerState und Character für beide Spieler.
- **[Belegt]** Beim Client fehlen Objekte, die das Spiel ungeprüft voraussetzt, etwa der `GameMode` in `AllPlayerFramingCamera`. Mit drei kleinen Patches läuft er stabil in `MetaBase` und `DDDynamic_Map`.
- **[Belegt]** Gegner replizieren in der Mission, beim Client als `SimulatedProxy`. Bewegung läuft in beide Richtungen.
- **[Belegt]** Bodenkacheln sind `Movable` und nicht repliziert. Auf ihnen sendet der Client Relativkoordinaten, die der Host nicht auflösen kann. Ohne Fix landet die Figur beim Host falsch.
- **[Belegt]** Selbst mit Weltkoordinaten bewegt der Host die Client-Figur kaum. Clientautoritative Position löst das, die Ursache ist offen.
- **[Hypothese]** Client-Schüsse treffen beim Host. Neben der Client-Figur sterben dort Gegner außerhalb der Reichweite der Host-Figur, ein direkter Messwert fehlt.
- **[Hypothese]** Die Replikation trägt, aber die Spiellogik rechnet weder beim Host noch beim Client mit Netzwerkspielern. Die Fehler sind bisher klein und lokal, ihre Gesamtzahl ist unbekannt.

### Replikation zur Laufzeit

Direkt aus dem Speicher einer laufenden Solo-Mission gelesen, an den Default-Objekten und den lebenden Instanzen. Die Mission lief mit nur
20 Gegnern, die Zahlen zeigen also keine Volllast.

- **[Belegt]** 274 von 843 Actor-Klassen replizieren, davon 218 von 482 Blueprint-Klassen. Von 892 lebenden Actors replizieren 476.
  Bei keiner lebenden Instanz weicht der Wert vom Default-Objekt ab.

| Repliziert | Repliziert nicht |
|---|---|
| `DDCharacter`, `EnemyBase`, `BP_StandardEnemy` (mit `bReplicateMovement`) | `EnemySpawner`, `SeekingAIController` |
| `Projectile` und alle Unterklassen | Die meisten Impact-Actors, `AreaEffectActor`, Gameplay-Cue-Actors |
| `Weapon`, `MeleeWeapon`, `Hands`, `DroneBase` | `OrbitingItemActor`, `VampireBatActor` |
| `LootBase`, `ItemLoot`, `FuelLoot`, `ScrapLoot`, `PowerUpLoot` | `Crate`, `Bus`, `RewardChest`, native `LootSpot` (aber 6 von 8 Blueprints) |
| `DDMissionGameState`, `DDPlayerState`, `DDPlayerController` | `DDMissionGameMode` (normal, läuft nur auf dem Server) |
| Alle 95 `ChaosDestructibleActor`-Blueprints (`DormantInitial`), 30 zerstörbare `StaticMeshActor` | `MapGenerator`, `MapTile`, `MapTileParcel` |

- **[Belegt]** Bewegung ist mit `NetUpdateFrequency=100` eingestellt. Bei voller Gegnerzahl ist das zu hoch und muss vermutlich gesenkt werden.
- **[Belegt]** Replizierende Komponenten: `DDAbilitySystemComponent`, `InventoryComponent`, `CharacterEffectManager`, `ThreatLevel`,
  `NightModeComponent`, `LootingTracker`. Bei `FuelMeter` sind die Objekte gemischt.
- **[Belegt]** Nicht replizierend: `LevelSystemComponent`, `ScrapWalletComponent`, `RerollCurrency`, `MetaCurrencyWallet`,
  `RareMetaCurrencyWallet`, `CoopInventoryCoordinatorComponent`. XP-Stand, Währungen und Reward-Ablauf kommen also nicht von selbst beim Client an.
- **[Hypothese]** Der Kern des Spiels ist für Replikation eingestellt: Spieler, Gegner, Projektile, Waffen und Loot. Das spricht deutlich stärker
  für Weg A als die RPC-Zahlen allein. Eigener Sync-Code wäre vor allem für die Komponenten oben und für Effekte auf Blueprint-Ebene nötig.
- **[Hypothese]** Karte und zerstörbare Objekte können sich beißen: Der `MapGenerator` repliziert nicht, die von ihm erzeugten Objekte teils schon.
  Läuft er auf beiden Seiten, könnten replizierte Objekte beim Client doppelt vorhanden sein.

### Karte und Seed

- **[Belegt]** Der Seed liegt in `AMapGenerator::MapSeed` und `UMapGenerationState::Seed` und `BaseSeed`. Jeder Sub-Generator hat einen eigenen
  `FRandomStream` (`RandomGeneratorLookup`).
- **[Belegt]** `UDDMissionGameInstance` bietet `DD_ForceStartRunSeed`, `DD_SetCurrentRunSeed` und `GetCurrentMissionSeed`.
- **[Belegt]** `GenerateOnStart` ist in der Mission aus. Die Generierung wird also von außen gestartet. Sie arbeitet mit Zeitbudget je Zyklus
  (`CycleTimeBudget`) und kann neu versuchen (`RetryAttemptsRemaining`).
- **[Hypothese]** Derselbe Seed ergibt dieselbe Karte. Zeitbudget und Wiederholungen könnten das stören, das ist in Spike C zu prüfen.

### Gegner und Pausen

- **[Belegt]** `AEnemySpawner` ist reine Host-Logik: Er repliziert nicht und nutzt Gegner über `RecycleArray` wieder. In der Testmission galt
  `EnemyCap=300`. `MaxSpawnedEnemies` lag beim aktiven Spawner bei 20.
- **[Belegt]** Pausen laufen über `UTimescaleManager` (ein Subsystem der GameInstance), `UPauseScreenManager` und
  `ADDMissionGameState::SetFreezeTime`. Coop-UI-Sitzungen tragen dazu `bBlocksGameplay`.
- **[Hypothese]** Der Timescale wirkt nur lokal. Pausen müssen also an den Client weitergegeben werden.

## Lokales Coop

- **[Belegt]** Zentrale Klassen laut Exe-Strings: `LocalCoopSubsystem`, `CoopInputRouterSubsystem`, `CoopInputPreProcessor`,
  `CoopGameViewportClient`, `CoopUISessionHost`, `CoopInventoryCoordinatorComponent`, `CoopPlayerPresentationComponent`,
  `CoopSpeakerAssignment`, `LocalCoopRunPersistData`.
- **[Belegt]** Die Coop-Logik ist fest auf zwei Spieler ausgelegt: `ECoopInventoryLocationType::P1Backpack` und `P2Backpack`,
  `CoopP1Field`, `CoopP2Field`.
- **[Belegt]** Inventar-Aktionen laufen als Anfragen über `CoopInventoryTransferRequest` und melden Fehler wie `NotAuthoritative`.
  Rewards haben eine Warteschlange mit `AwaitingClaims`.
- **[Belegt]** Es gibt Coop-Konsolenvariablen: `dd.Coop.UIAuthorityMode`, `dd.Coop.UIHandoffMode`, `dd.Coop.SplitBackpackView`.
- **[Belegt]** Auch in Exe-Strings: `CoopDiagnostics.json`, `bEnableDevCoopButtons`, `OnCoopReconnectRequiredChanged`,
  `bCoopBlocksGameplay`. Was sie genau tun, ist noch offen.
- **[Belegt]** „Authority“ meint die UI-Bedienung, nicht die Netzwerk-Autorität: `UCoopInputRouterSubsystem` hat
  `RequestUIAuthorityTransfer(PlayerIndex)` und `PushUISession(HostPanel, AuthorityPlayerIndex, bBlocksGameplay, ...)`.
- **[Belegt]** `ULocalCoopSubsystem` führt die Spieler in `ActivePlayers` als Zuordnung von Index zu `APlayerController`.
  Solo gilt `bCoopEnabled=0` und `MaxLocalPlayers=1`. Steuerbar über `SetCoopEnabled`, `SetMaxLocalPlayers` und `SpawnLocalPlayer`.
- **[Belegt]** `UCoopInventoryCoordinatorComponent` hängt am `ADDMissionGameState` und repliziert nicht.
- **[Belegt]** Die Coop-Aktionen sind schmale, aufrufbare Funktionen: `UCoopInventoryCoordinatorComponent::TransferItem(Request)`,
  `ClaimReward(PlayerIndex, Choice)`, `RerollRewardSession()`.
- **[Hypothese]** Diese Funktionen eignen sich gut als Andockpunkt: Der Client schickt eine Anfrage, die Mod ruft sie beim Host auf.
- **[Hypothese]** Weil `ActivePlayers` nur Controller speichert, könnte dort auch ein Remote-Controller stehen. Das ist in Phase 1 zu prüfen.

## Systeme, die die Mod respektieren muss

- **[Belegt]** Bestenlisten: `PixelLeaderboards`, `DDLeaderboardSettings`
- **[Belegt]** Telemetrie und Berichte: `DDBalanceTelemetrySettings`, `DDCorruptedSaveAutoReportSettings`, `DDBugReportForm`, `PixelsplitFeedbackForm`
- **[Belegt]** Spielstände: `PixelsplitSaveSystem`, `LocalPlayerSaveGame`
- **[Belegt]** Zeitsteuerung: `PixelsplitTimescaleSystem`. Für Online-Spiel relevant, weil Pausen bei beiden gelten müssen.

## Allgemeines

- Entwickler laut SteamSpy: Pixelsplit. Version 1.0 mit fertigem Local-Coop erschien am 29.10.2026, also kurz vor Projektstart.
  Es ist mit weiteren Patches zu rechnen.
- Zu Mod-Support, Workshop oder offiziellem Online-Coop wurde nichts gefunden. Das heißt nicht, dass nichts geplant ist.
