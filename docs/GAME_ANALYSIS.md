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
  `BP_EnemySpawner`, `BP_Projectile` und viele `BP_Effect_*`. Ob sie `bReplicates` setzen, zeigt das SDK nicht und ist noch offen.
- **[Hypothese]** Native Replikation deckt Attribute und einen Teil von Kampf und Inventar ab, aber nicht die Blueprint-Logik. Was dort
  entsteht (Effekte, Events, Spawns), bräuchte beim Client eigenen Sync-Code. Das spricht eher für den Hybrid aus Weg A und B.
- **[Hypothese]** Der Netzwerkcode ist Altlast aus einer früheren Entwicklungsphase: Er wurde nicht ausgebaut, aber auch nicht
  weiterentwickelt, wie die fehlenden RPCs bei neuen Systemen und die entfernten RPCs zeigen. Kampf, Attribute und Inventar sind
  damit mögliche Grundlagen. Ob dieser Code noch läuft, ist offen.

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
