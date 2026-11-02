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

### Technische Andockpunkte: neue statische Prüfung

Aufgabe `andockpunkte`, Build `26187885`, 02.11.2026:

- **[Belegt, statisch]** `VERSION.dll` ist regulärer Import mit
  `GetFileVersionInfoW`, `GetFileVersionInfoSizeW` und `VerQueryValueW`.
  `XINPUT1_4.dll` ist dagegen ein Delay-Import.
- **[Belegt, statisch]** Die drei Byte-Signaturen des
  [Audits](../research/spikes/andockpunkte/audit.py) treffen in ausführbaren
  Sektionen jeweils genau die bereits bekannten RVAs für `ProcessEvent`,
  `SetClientTravel` und `SeamlessTravelToLevel`. Keine neue Semantik/ABI hergeleitet.
- **[Hypothese]** `version.dll` eignet sich als Mod-Loader im Spiel. Ein isoliertes
  Windows-Testprogramm lädt den Proxy und erhält gültige Daten über alle drei
  Exporte; das ist kein Test mit der Spiel-Exe. Fehlversuch und Grenzen stehen in
  der [Versuchsbeschreibung](../research/spikes/andockpunkte/README.md).
- Kein neuer Spieltest und keine Änderung an der Spielinstallation. Die bereits
  vorhandene lokale `dwmapi.dll` bleibt unangetastet; Herkunft nicht untersucht.

### Vorhandene Netzwerkbefunde

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

### Ergaenzung: SDK-Schnittstellen fuer eigenen Sync

Aufgabe `sync-bedarf`, lokales Mission-SDK von Build `26187885`:

- **[Belegt, SDK]** `LevelSystemComponent` bietet XP-/Level-Delegates, Level-Getter
  und Prozentanzeige, aber keinen reflektierten rohen XP-Wert. Die Bedeutung des
  Delegate-XP-Werts wurde hier nicht dynamisch getestet.
- **[Belegt, SDK]** `AbstractCurrency` besitzt `GetAmount`, `AddAmount`,
  `OnCurrencyChanged`, SaveManager und SaveGame-Felder. Schrott und Reroll erben
  zusaetzlich Run-/Travel-Persistenz, Meta- und RareMeta-Wallet die Save-Basis.
- **[Belegt, SDK]** Reroll fuehrt Kontextzaehler. Der Coordinator hat Claim-,
  Transfer-, Reroll- und UI-Closed-Funktionen sowie einen strukturierten Reward-Zustand
  mit Auswahl, Quoten, Teilnehmern, Reroll-Lock und Queue-Zustand.
- **[Hypothese]** Die ueber GameState und PlayerState erreichbaren Run-Konten sind
  gemeinsam. Getter und Shared-Kontexthelfer reichen nicht als Identitaetsbeweis.
- Vorgeschlagene Sync-Wege und verbleibende Fragen stehen in
  [SYNC_REQUIREMENTS.md](SYNC_REQUIREMENTS.md). Kein neuer Laufzeit-Sync-Nachweis.

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

### Gegner-Spawning und Pooling

Stand: Disassembly und Laufzeit-Messung, Build `26187885`. Mission über das Spielmenü, Host und Client mit dem Spike, Gegner aller 15 bis 18 Sekunden aus dem Speicher beider Seiten gezählt. RVAs in [Offsets](OFFSETS.md#gegner).

**Zwei getrennte Mechanismen**

- **[Belegt]** Gegner laufen nicht über den `PoolManager`, sondern über den `AEnemySpawner` selbst.
  Die Spawn-Schleife `0x58CECE0` wählt per `FMath::RandRange` einen Spieler aus `SpawnedPlayers`. Dort liegt der bekannte `null`-Absturz bei `0x58CEDC7`.
  Danach räumt sie ungültige und gerade zerstörte Einträge aus `RecycleArray` (`+0x478`) und bestimmt je Gegner einen Spawnort.
  Ist `RecycleArray` nicht leer und der letzte Eintrag weiter vom Spieler entfernt als der neue Ort, nimmt sie diesen **lebenden** Gegner und versetzt ihn mit `PlaceCharacterOnGroundAtPosition` an den neuen Ort. Danach stößt sie seinen Controller neu an.
  Sonst spawnt sie mit `TrySpawnFreshEnemy` einen neuen Gegner.
- **[Belegt]** `RecycleArray` ist also kein Pool toter Gegner, sondern eine Liste zu weit entfernter Gegner, die nach vorn geholt werden. Wer Gegner einträgt, ist nicht gefunden: In `0x58A0000` bis `0x58D4000` schreibt nur die Initialisierung (`0x58A58E0`, VTable-Slot 213) in das Array, und die leert es.
- **[Belegt]** Getötete Gegner werden zerstört. In der Messung verschwanden beim Host 10 Gegner-Objekte und 10 neue entstanden, ohne Wiederverwendung. `RecycleArray` blieb in zwei Messungen leer, obwohl die Host-Figur umherlief. Zusammen waren das gut 5 Minuten, teils durch Tutorial-Hinweise pausiert, in einer Mission mit `MaxSpawnedEnemies=20`.
- **[Belegt]** Der `PoolManager` (World-Subsystem, auf beiden Seiten vorhanden) verwaltet Projektile, Schadenszahlen, Flächeneffekte, Drohnen und `EnemyChaosDestructionProxyActor`.
  Beim Host bleiben Projektile nach dem Einsatz als versteckte Objekte (`bHidden`) bestehen und werden wiederverwendet. Beim Client hat der Pool keine Einträge.

**Verhalten beim Client**

| Messung | Host | Client |
|---|---|---|
| Gegner vor dem Tod von 10 Gegnern | 20 | 20 |
| Rund 18 Sekunden danach | 20 (10 neu) | 30, davon 10 mit `bActorIsBeingDestroyed` |
| Rund 34 Sekunden danach | 20 | 20 |
| Projektile | 2, versteckt und wiederverwendet | 3 bis 4 Proxies, versteckt, 2 bleiben mit `bActorIsBeingDestroyed` über 9 Sekunden stehen |
| `EnemyChaosDestructionProxyActor`, `DamageNumberActor` | Vorhanden, gepoolt | Fehlen |

- **[Belegt]** Tote Gegner bleiben beim Client noch rund 15 Sekunden als Objekte stehen, markiert als „wird zerstört“. Das erklärt die abweichenden Gegner-Zähler (30 gegen 48) aus dem Spike.
- **[Belegt]** Gepoolte Projektile kommen beim Client als `SimulatedProxy` an, ihr verstecktes Zurücklegen in den Pool repliziert über `bHidden`. Ein Projektil des Hosts bleibt also ein langlebiger Actor-Kanal und wird nicht für jeden Schuss neu angelegt.
- **[Belegt]** Effekte beim Gegnertod und Schadenszahlen sind gepoolte, nicht replizierte Actors des Hosts. Darum sieht der Client sie nicht. Das ist die Ursache für „Blut- und Treffereffekte nur beim Host“.
- **[Hypothese]** Das Versetzen lebender Gegner aus `RecycleArray` ist für die Replikation unkritisch: Es ist ein normaler Positionssprung eines replizierten Pawns, der Client sieht einen Teleport.
  Kritisch wäre es nur, wenn das Spiel dabei Zustand zurücksetzt, der nicht repliziert. Das trat in der Messung nicht auf.
- **[Hypothese]** Mit zwei Spielern ist der Abstand zum Spieler beim Recycling nur gegen den zufällig gewählten Spieler geprüft. Gegner nahe dem anderen Spieler könnten also weggeholt werden. Das ist für die Spielbarkeit vermutlich nebensächlich.

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

## Run-Ablauf

Stand: Laufzeit-Trace und Disassembly, Build `26187885`. Der Ablauf gilt für zwei Instanzen mit dem [Spike](../research/spikes/a-connect/README.md). Ohne dessen Patches stürzt der Host schon beim Spawn ab.

### Wege zwischen den Karten

| Schritt | Auslöser | Weg im Spiel | Ergebnis mit Client |
|---|---|---|---|
| Menü nach `MetaBase` | Menüpunkt | Lokale Reise | Client folgt nur per `servertravel` |
| `MetaBase` in Mission | Figur läuft auf den Ring links neben dem Bus, Klick auf die Routenkarte | `FinalizeRunStart`, `LoadMapByInformation`, `ULoadingSubSystem::SeamlessTravelToLevel` | Beide in `DDDynamic_Map`, Gegner laufen von selbst |
| Missionsende | Bus oder `FinishMission` | `RoundEndPanel`, Lager, Routenkarte | Nur beim Host sichtbar, der Client spielt weiter |
| Mission in nächste Mission | Klick auf die Routenkarte | Wieder `SeamlessTravelToLevel` | Host erreicht Tag 2, Client stürzt beim Ankommen ab |
| Mission ins Hauptmenü | Pausemenü, `ReturnToMainMenu` | `SetClientTravel` | Host sauber im Hauptmenü, Client stürzt beim Trennen ab |

`SeamlessTravelToLevel` reist nur lokal. Der Spike leitet die Reise in `servertravel <Karte>?listen` um, damit der Client folgt. Damit entfällt der Ladeablauf des Spiels, die Mission lief trotzdem normal.
Die Umleitung gilt nicht für das Hauptmenü: Dort stürzte der Host mit Umleitung ab, im Original nicht. Eine Rückkehr in die `MetaBase` nach Run-Ende ist noch nicht getestet.

`TriggerEndRound` meldet das Rundenende nur an die Persistenz und löst keine Reise aus. `FinishMission` zeigt das normale Tagesende, auch wenn vorher `Lose` gesetzt wurde. Der echte Weg bei einer Niederlage ist also noch offen.

### Was beim Kartenwechsel passiert

- **Controller:** Die Engine ersetzt beide Controller. In `SpawnedPlayers` bleibt der alte Remote-Controller stehen, der neue kommt nie hinein. Nach der Garbage Collection steht dort `null`, und der Gegner-Spawner wählt daraus zufällig ein Ziel. Das erklärt einen nur manchmal auftretenden Host-Absturz.
- **HUD:** Ab dem zweiten Wechsel wird die HUD des Clients gebaut, bevor der neue `GameState` da ist. `WP_UI` wartet in `PreConstruct` in einer Blueprint-Schleife auf ihn. Diese Schleife läuft auf dem Game Thread, der den `GameState` erst empfangen müsste, der Client hängt also für immer. Eine höhere `NetPriority` des `GameState` half nicht.
- **Inventar:** Die Unterobjekte fehlen schon an Tag 1 dauerhaft, nicht erst kurz beim zweiten Wechsel. Ursache und Teilfix siehe unten. `OnRep_Slots` zurückzuhalten half nicht.

### Inventar-Unterobjekte: Untersuchung `inventar`

Build `26187885`, zwei Instanzen auf einem PC, Loopback, getrennte Test-Spielstände, Start ausschliesslich mit Crash-Schutz.

- **[Belegt] Kontrolllauf nach erster Spike-Änderung:** Menüweg aus `TESTING.md` erneut ausgeführt. Bei frischem Teststand erschien zuerst die Erklärung zum Roadtrip, danach die Routenkarte. Der Routenknoten startete beide Instanzen in der Mission und Gegner erschienen selbständig. Host: je Spieler acht Slots und ein Item; Client: dieselben Array-Längen, aber ausschliesslich Null-Verweise. Auch nach längerer Wartezeit entstanden beim Client keine entsprechenden Unterobjekte. Beim nächsten Tageswechsel wieder Client-Absturz `0x59661C3`.
- **[Belegt] Statischer Befund:** `UInventoryComponent::ReplicateSubobjects` existiert und sendet bereits `MyItems` und `Slots`. Die Komponente hat aber `bReplicateUsingRegisteredSubObjectList` gesetzt. Damit wird dieser klassische virtuelle Pfad ausgelassen. `UBackpackSlot` und `UInventoryItemInstance` verwenden ausserdem den UObject-Standard für `IsSupportedForNetworking`, statt dynamische Netzwerkobjekte ausdrücklich zu unterstützen.
- **[Belegt] Versuch 1:** Nur Netzwerk-Support für beide Unterobjektklassen freischalten: weiterhin acht Null-Slots und ein Null-Item beim Client.
- **[Belegt] Versuch 2:** Zusätzlich in den beiden Erzeugungsfunktionen auf den klassischen Pfad umschalten: An Tag 1 kommen sämtliche Objekte beim Client an. Tag 2 stürzt weiter bei `0x59661C3` ab. Neue Inventarkomponenten übernehmen dort die bestehenden Arrays, ohne die Erzeugungsfunktionen erneut aufzurufen. Die Umschaltung muss also für jede neue Komponente erfolgen, nicht nur beim Anlegen ihrer Inhalte.
- **[Belegt] Teilfix:** `ReadyForReplication` jeder `UInventoryComponent` schaltet vor dem ursprünglichen Aufruf ausschliesslich diese Komponente auf ihren vorhandenen klassischen Pfad. Zwei klassenspezifische virtuelle Overrides erlauben Netzwerk-Support der Slots und Items. Keine Änderung an Outer, Persistenz oder globalen Engine-Einstellungen. Die ursprünglichen drei Null-Schutzsprünge sind vollständig entfernt.
- **[Belegt] Zwei unabhängige Sitzungen mit Teilfix:** Menü → Tag 1 → `!finish` → Lager → Routenkarte → Tag 2. Host und Client bleiben verbunden; auf dem Client sind beide Inventare vollständig. An Tag 2 wurden Slot-Koordinaten, Belegung relativ zu `MyItems`, ItemData, Pivot, Turns und die replizierten Zustandsbits zwischen Host und Client verglichen: identisch. In der zweiten Sitzung zusätzlich derselbe Vergleich an Tag 1 und nach zehn weiteren Sekunden an Tag 2. Das Werkzeug dafür ist `tools/spike/inventory.py`.
- **[Belegt] Grenze:** Beide Versuche der anschliessenden Reise auf Tag 3 endeten mit einem Host-Absturz im indirekten Callback bei `0x57CC395` beziehungsweise `0x57CC398` (nächster Stack-Eintrag `0x315A764`). Der Callback liest ein Objekt und springt über dessen Vtable bei `+0x490`; die Objektklasse und der Zusammenhang mit dem Inventar sind ungeklärt. In einer Sitzung folgte ein Client-Absturz bei `0x57D080C`. Keine Aussage, dass ein kompletter Run funktioniert oder diese Folgefehler unabhängig vom Teilfix sind.
- **Nicht geprüft:** Inventarbedienung/RPCs des Clients, Belohnungen mit mehreren Items, Item-Effekte, Internet/Latenz/Paketverlust, vollständiger Run. `obj gc` wurde an beide Instanzen gesendet; dessen tatsächliche GC-Ausführung ist mit den vorhandenen Logs nicht belegt.

### Was beim Client laufen darf

| Bereich | Host | Client |
|---|---|---|
| Run-Zustand, Seed, Route, Persistenz | Führt | Nicht ausführen. Der Client hat diesen Zustand nicht und braucht einen eigenen Sync |
| Routenkarte, Rundenende, Lager | Führt | Nicht öffnen, Fortschritt nur anzeigen. Bisher sieht der Client davon nichts |
| Kartenwechsel | Startet ihn für beide | Folgt nur, startet keinen eigenen |
| Kartenerzeugung | Führt | Darf nicht frei laufen, siehe unten |
| Gegner, Spawner, Loot, Missionsende | Führt | Nur replizierte Ergebnisse anzeigen |
| Kamera | Führt | Läuft lokal auf den replizierten Spielern |
| Eigene Figur | Nimmt die Position vom Client an | Führt Bewegung und Eingabe |
| HUD | Normal | Erst bauen, wenn der `GameState` da ist |
| Inventar | Führt | Darf erst laufen, wenn die replizierten Einträge da sind. Ohne Lösung kein Run über mehrere Tage |

**Karte:** Im echten Ablauf erzeugt der Host 144 bis 167 `MapTile`, der Client bleibt bei 17 und zeigt eine andere Karte. Die Figur des Clients startet deshalb weit vom Bus entfernt, weil seine Position beim Host übernommen wird. Das ist der Einstieg in Thema 3.

## Systeme, die die Mod respektieren muss

- **[Belegt]** Bestenlisten: `PixelLeaderboards`, `DDLeaderboardSettings`
- **[Belegt]** Telemetrie und Berichte: `DDBalanceTelemetrySettings`, `DDCorruptedSaveAutoReportSettings`, `DDBugReportForm`, `PixelsplitFeedbackForm`
- **[Belegt]** Spielstände: `PixelsplitSaveSystem`, `LocalPlayerSaveGame`
- **[Belegt]** Zeitsteuerung: `PixelsplitTimescaleSystem`. Für Online-Spiel relevant, weil Pausen bei beiden gelten müssen.

## Allgemeines

- Entwickler laut SteamSpy: Pixelsplit. Version 1.0 mit fertigem Local-Coop erschien am 29.10.2026, also kurz vor Projektstart.
  Es ist mit weiteren Patches zu rechnen.
- Zu Mod-Support, Workshop oder offiziellem Online-Coop wurde nichts gefunden. Das heißt nicht, dass nichts geplant ist.
