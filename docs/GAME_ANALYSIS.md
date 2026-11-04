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
| Manifest-IDs | Depot `3026451`: `1832737755528485607`, Depot `3026452`: `7724756528743620812` |
| Engine-Version | `5.8.1-56057345+++UE5+Release-5.8` |
| Spiel-Exe | `DDSurvivors/Binaries/Win64/DDSurvivors-Win64-Shipping.exe` (179 MB) |

Build-ID und Manifest-IDs stehen in `steamapps/appmanifest_3026450.acf`. Die Manifest-IDs benennen die gewünschten Depotstände;
ein erneuter Download über die Steam-Konsole ist noch nicht getestet. Bei einem Spiel-Update gilt die
[Update-Regel](../CONTRIBUTING.md#spiel-updates): einfrieren und nur bei Bedarf nachziehen.

### Gemeinsame Build-ID (`build-id`)

Stand: 03.11.2026, ein Windows-PC. Erst lesend geprüft, danach eine getrennte Entwicklungskopie erstellt und im Spiel getestet.
Keine Steam-Bedienung oder Downloads, keine Änderung an Steam-Einstellungen, installierten Spieldateien oder echten Spielständen.

- **[Belegt, lokale Dateien]** Das installierte Steam-Manifest nennt `26187885` und die beiden oben stehenden Depot-Manifeste.
  `AutoUpdateBehavior` steht auf `0`; die Einstellung wurde nicht umgestellt und ist für die neue, getrennte Kopie nicht maßgeblich.
- **[Belegt, öffentliche Metadaten]** [api.steamcmd.net](https://api.steamcmd.net/v1/info/3026450) listet weiterhin nur den öffentlichen Zweig
  `public` mit Build `26187885`. `privatebranches=1` ist gesetzt: Private Zweige können existieren, ihre Zugänglichkeit für das Team ist unbekannt.
  Ein allgemein zugänglicher Altversions-Zweig ist in dieser Antwort nicht ausgewiesen. Die API ist ein Drittanbieter, kein Nachweis der Kontoberechtigung.
- **[Belegt, öffentliche Metadaten]** Depot `3026451` ist leer; Depot `3026452` enthält die Windows-Dateien, Größe `1886116738` Byte,
  Downloadgröße `1605871856` Byte. Die Manifest-IDs stimmen mit der lokalen Installation überein. Ein aufgelistetes Manifest ist noch kein erfolgreicher Download.
- **[Belegt, Quellcode und Exe-Header]** Der bisherige Steam-Startweg in `Assert-SupportedBuild` liest nur `buildid` aus dem Steam-Manifest.
  Die Core-Prüfung nutzt dagegen PE-`TimeDateStamp` und `SizeOfImage`; lokal passen `0x32C34474` und `0x0B742000` zu `SupportedBuilds`.
  Diese beiden Prüfungen vergleichen keine Inhaltsdateien. Der neue `PinnedGameDir`-Startweg verwendet deshalb stattdessen den vollständigen
  SHA-256-Beleg der Entwicklungskopie; die Core-Prüfung bleibt zusätzlich aktiv. Kein gefälschtes Steam-Manifest nötig.

**Dateireferenz:** SHA-256 der lokalen Exe und der fünf Container unter `DDSurvivors/Content/Paks`, rein lesend ermittelt.
Das sind Vergleichswerte der vorhandenen Testinstallation, keine unabhängig durch einen frischen Depot-Download bestätigte Steam-Referenz.
Sie belegen bei Übereinstimmung nur diese sechs Dateien, nicht alle DLLs, zusätzliche Mods oder die Startfähigkeit.

| Datei | SHA-256 |
|---|---|
| `DDSurvivors/Binaries/Win64/DDSurvivors-Win64-Shipping.exe` | `3afbf18e051fbceb1fec17a8da20165ba64e0f6c3c88cbe88cb046704a4eefb5` |
| `DDSurvivors/Content/Paks/DDSurvivors-Windows.pak` | `68e6272f63219e928667c85938663e1044c3e7d5ffa1dc279ecc3e640001e3fa` |
| `DDSurvivors/Content/Paks/DDSurvivors-Windows.ucas` | `e9d5d3d978db4e6beb34078dfded4b602059345b1084dd783a4faae8d4653ab7` |
| `DDSurvivors/Content/Paks/DDSurvivors-Windows.utoc` | `e9dfef912c120fec38c66ed2f13ed5e7e95b3342fbe0d455aab036bb5a6ec2e9` |
| `DDSurvivors/Content/Paks/global.ucas` | `22eb9df449492c0cacf5cb5e1f8584bf23ee5e655d96f01fb8b5159c45440d4e` |
| `DDSurvivors/Content/Paks/global.utoc` | `ece14ac99e2787b2cbea14444c285ae1c403750b1386bdc892fa341d3ec99e9a` |

**Lösung und Laufzeitnachweis:** `Freeze-GameBuild.ps1 -Activate` erstellt echte Dateikopien außerhalb der Steam-Bibliothek und setzt
`PinnedGameDir` in der lokalen Konfiguration. Es wurden 54 unveränderliche Dateien kopiert und vollständig gehasht; der Beleg enthält nur
relative Dateinamen, Größen und Hashes. Ein bestehendes Ziel wird nie ersetzt. Der getrennte Ordner hat keine Hardlinks, Junctions oder Symlinks
zur Steam-Installation. Steam aktualisiert weiterhin seine eigene Installation, nicht diese Entwicklungskopie.

- `Start-Session.ps1 -Build -Mod`: Host und Client starten aus der Kopie, in der `MetaBase` verbunden, Host `clients=1`, auf beiden Seiten `players=2`.
  Beide tatsächlichen Prozesspfade wurden gegen die eingefrorene Exe geprüft. Beide Mod-Logs bestätigen Build `26187885` und gesperrten Crash-Reporter.
- `Stop-Session.ps1` beendet die eigene Sitzung und entfernt die Mod aus der Kopie. `Test-Setup.ps1` prüft danach wieder alle 54 Dateien erfolgreich.
- Mit dem endgültigen Startweg ein zweites Mal verbunden; anschließend alle 54 Originaldateien der Steam-Quelle mit dem Ausgangsbeleg
  verglichen: unverändert. Release-Build, 18 C++-Tests mit 77 Assertions und nach dem Review 58 Python-Tests erfolgreich.
- Automatische Tests simulieren Änderungen an der Steam-Quelle: Die Kopie bleibt bytegleich und startseitig prüfbar. Veränderte Exe/DLL,
  fehlende Datei, zusätzliches Paket, ungültiger Beleg, falscher Build, Steam-Zielordner, Hardlinks und Änderungen während der Kopie werden abgewiesen.
  Das ist kein absichtlich ausgelöstes echtes Steam-Update; die installierten Spieldateien wurden nicht verändert.

**Grenzen:** Das Team kann nach der [Valve-Dokumentation zu Branches](https://partner.steamgames.com/doc/store/application/branches) keinen
eigenen Steam-Altversions-Zweig bereitstellen. Deshalb wird ein solcher Zweig nicht vorausgesetzt. Die technische Build-Trennung ist belegt;
jedes Teammitglied richtet sie aus seiner eigenen lizenzierten Kopie ein, siehe [Einrichtung](SETUP.md#gleiche-build-id-im-team).
Nicht getestet: zwei PCs/Konten, Depot-Redownload oder Start nach einem künftigen Steam-/Backendwechsel. Keine DRM-Umgehung und keine Garantie
gegen Änderungen am Steam-Dienst; diese können einen eigenen Anpassungsfall ergeben, aktualisieren aber nicht automatisch die eingefrorenen Dateien.

## SDK-Erzeugung unter UE 5.8

Dumper-7 (auch der aktuelle Main-Stand vom November 2026) stürzte beim Lesen der Property-Klassennamen ab. Mit dem Fix unten ist ein
vollständiges SDK für Build `26187885` erzeugt (02.11.2026), die Dateien `Basic`, `CoreUObject_functions`, `Engine_functions` und
`DDSurvivors_functions` kompilieren mit MSVC.

- **[Belegt] Ursache:** UE 5.8 hat vor dem Namen von `FFieldClass` einen zusätzlichen Zeiger. Dumper-7 las diesen als FName,
  daher die feste Absturzadresse `0xCDB0` (`0x66D8 * 2`).
- **[Belegt] Layout:** `Name` 0x08, `ClassFlags` 0x10, `Id` 0x18, `CastFlags` 0x20, `SuperClass` 0x28, Gesamtgröße 0x50.
- **Fix in Dumper-7:** In `OffsetFinder::FixupFieldClassOffsets()` wird das Layout daran erkannt, dass die gefundenen `CastFlags`
  nicht vor dem fest hinterlegten `SuperClass`-Offset liegen. Dann werden die Offsets oben gesetzt.
- **[Belegt] Gemeinsamer Stand (03.11.2026):** Upstream-`main` `2c1eae3` stürzt weiter ab. Getestet ist eine Variante des Fixes, die die
  Offsets relativ zu `Id` und `CastFlags` berechnet und den UE-5.7-Pfad unverändert lässt. Mit ihr lief ein Dump im Host einer Mission
  (über das Menü gestartet) vollständig durch: 2.636 SDK-Dateien, `FFieldClass` wie oben, `UInViewportMovementRestrictor`,
  `UInventoryItemInstance` und `UInventoryComponent` gleich zum SDK vom 02.11. Der Host lief weiter. Diese Variante ist Commit
  `1aae006`. Sie liegt auf `main` des eigenen Forks [`VitaminStack/Dumper-7`](https://github.com/VitaminStack/Dumper-7) (Merge `2570b5b`,
  Dateistand gleich `1aae006`), eingebunden als Submodul `third_party/Dumper-7`, gebaut wird sie mit [`Build-Dumper7.ps1`](../tools/README.md#dumper7).
  Nicht geprüft: Kompilieren dieses SDK, andere Engine-Versionen.
- **Upstream:** Der Fehler ist bei Dumper-7 als Issue #544 bekannt, offen ist dort PR #576 (nicht von uns getestet). Entschieden am
  03.11.2026: Wir reichen nichts upstream ein, der Fork bleibt dauerhaft die Quelle für die Mod. Neue Upstream-Stände werden nur bei
  Bedarf in den Fork übernommen und vor dem Umstellen des Submoduls mit einem Dump geprüft.
- **Nacharbeit nach jedem Dump:** In `Assertions.inl` sind die Prüfungen für `UEngineReplicationBridge`,
  `URepositionableTransformGizmo` und `UPolyEditPreviewMesh` abzuschalten. Sie schlagen sonst beim Kompilieren fehl.
  Die Ursache ist nicht untersucht. Bei Nutzung von `UEngineReplicationBridge` deren Layout zuerst prüfen.
- **Dump-Zeitpunkt:** Immer während einer laufenden Mission dumpen, denn Blueprint-Klassen werden nur erfasst, wenn sie geladen sind.
  Das Hauptmenü liefert 64.952 Objekte und 562 Blueprint-Klassen, eine Mission 110.688 Objekte und 925 Blueprint-Klassen plus 178 Widgets.
- **Grenze:** Auch der Mission-Dump enthält nur, was in dieser einen Mission geladen war. Für Bosse gibt es deshalb den [Boss-Dump](#boss-dump-boss-dump).
- **[Belegt] Dump in einer Testsitzung:** Dumper-7 lässt sich mit `inject.py` in den Host einer laufenden Sitzung laden, auch neben dem Spike. Die Instanz läuft danach weiter.
  Der DLL-Pfad muss absolut sein, `LoadLibraryW` sucht einen relativen Pfad im Arbeitsordner des Spiels und scheitert still. Aus Git Bash also `cygpath -wa`, nicht `cygpath -w`.
  Die Ausgabe `LoadLibrary low32 0x0` ist kein sicheres Zeichen für einen Fehler, eine DLL kann auf einer Basis mit unteren 32 Bit 0 landen.
  Mit `-Map /Game/Maps/DDDynamic_Map` kamen 77.801 Objekte zusammen, weniger als über das Spielmenü.
- **[Belegt] Funktionsadressen:** Die Adressen in `Dumpspace/FunctionsInfo.json` sind unter UE 5.8 unbrauchbar. Zur Laufzeit zeigt `UFunction::ExecFunction` (`+0xD8`) nach `!flow` auf die Stubs des Spikes.
  Verlässlich sind die statischen Tabellen `{const char* Name, FNativeFuncPtr}` in `.rdata`. Je Klasse gibt es zwei: eine mit den `Z_Construct_UFunction_*`-Funktionen, eine mit den Exec-Thunks. Der Thunk ruft am Ende die eigentliche Funktion auf.

### Boss-Dump (`boss-dump`)

Stand: 03.11.2026, Build `26187885`, Dumper-7 aus [`Build-Dumper7.ps1`](../tools/README.md#dumper7) (Submodul `1aae006`), Testsitzung mit Spike.

**Ergebnis:** Ein SDK mit allen sechs Bossen: 2.571 Dateien, 90.209 Objekte, 973 Blueprint-Klassen. Der Dump vom 02.11. aus der direkt geladenen Mission hatte 2.353 Dateien, 77.805 Objekte und 868 Blueprint-Klassen. Neu sind 112 Pakete, darunter:

| Bereich | Neue Pakete, Auswahl |
|---|---|
| Bosse | `BP_SpikeBoss`, `BP_FrankensteinBoss`, `BP_BlobBallBoss`, `BP_BallerinaBoss`, `BP_LazyBoss`, `BP_HQ_Endboss`, Basis `BP_BossEnemy`, `BP_BossProxy`, `BP_BossAIController` |
| Spawnpunkte | `BP_<Boss>SpawnPoint` für alle sechs, Basis `BP_BossBaseSpawnPoint` |
| Fähigkeiten und Effekte | `GA_SpikesRingAttack`, `GA_GroundPoundAttack`, `GA_BlobBallJumpAttack`, `GA_DropBigRobots`, `GE_GrantBossTag`, `GE_OverrideHealth` |
| Endboss | `BP_Endboss_Drone`, `BP_Endboss_Shield`, `GA_Endboss_ShapeLaser_HalfCircle`, Sequenzen `LS_Endboss_*` |
| KI | `BTD_*`, `BTT_*`, `EQS_*`, `NavArea_BossArea` |
| UI | `WP_BaseBoss_HealthBar`, `WP_Boss_HealthBar_PhaseBased`, `WBP_BossIntroOverlay` |

Nicht geprüft: Kompilieren dieses SDK.

**Weg dorthin:** Ein echter Bosskampf ließ sich nicht erreichen, die Klassen sind deshalb gezielt geladen.

- **[Belegt]** Ein Run hat 8 Tage („Tag 1/8“). Die Routenkarte zeigt rot gestreifte Knoten am Übergang zwischen zwei Gebieten nach Tag 4 und einen Endboss-Knoten am Ende. **[Vermutung]** Die gestreiften Knoten sind Torbosse.
- **[Belegt]** Die Reise auf Tag 3 lässt den Host abstürzen, auch allein nach `disconnect` des Clients und auch mit abgeschaltetem `MapGen.cpp` und `Pcg.cpp`. Zweimal, auf zwei verschiedenen Routenkarten, je beim Laden von Tag 3:
  Ein Timer (`0x315A7D6`) ruft im Kartengenerator `0x59F0C30` und von dort `0x59D9090` auf. Diese Funktion liest beim ersten `GetWorld` (`0x25E9870`) freigegebenen Speicher, Absturz bei `0x25E9889` beziehungsweise `0x59D9139`.
  **[Vermutung]** Ein Timer des Kartengenerators überlebt den Kartenwechsel. Das ist eine andere Stelle als der Callback `0x57CC395` aus `tag3`.
- **[Belegt]** Wird `DDDynamic_Map` direkt geladen (`servertravel`), nutzt der Generator sein Standardprofil `MGP_HQ8_Boss`: das Dach des HQ mit Arena und Bühnen des Endbosses (`BP_ManagerBoss_Stage*`, `DD_HQBoss_SurviveWaves`). Der Endboss selbst erschien auch nach `!wave` und vorgestellter Missionszeit nicht.
- **[Belegt]** Die `DD_`-Befehle liegen fast alle in `UDDCheatManager`. Im Shipping-Build ist `APlayerController::CheatManager` leer, die Konsole verwirft sie still, etwa `DD_QuickFinishMission`. Der neue Spike-Befehl `!cheats` legt den Manager per `GameplayStatics.SpawnObject` an, danach wirkt etwa `DD_SetTimeSinceMissionStart`.
- **[Belegt]** `DA_BossSpawnLookup` nennt alle sechs Bosse mit `EnemyType` und Spawnpunkt. Der `EnemyType` verweist in `EnemyBlueprint` (`+0x128`) auf die Gegnerklasse. `!load <Pfad>` lädt jedes davon.
- **[Belegt]** Geladene, aber unbenutzte Klassen räumt die Garbage Collection nach etwa einer Minute wieder ab. Ein erster Dump acht Minuten nach dem Laden enthielt keine Boss-Klasse. Mit `gc.TimeBetweenPurgingPendingKillObjects 36000` vor dem Laden blieben sie erhalten.

**Ablauf zum Wiederholen:**

1. `Start-Session.ps1`, dann `servertravel /Game/Maps/DDDynamic_Map?listen` an den Host.
2. An den Host: `gc.TimeBetweenPurgingPendingKillObjects 36000`.
3. Für jeden Boss `!load` mit dem Pfad aus `DA_BossSpawnLookup`, etwa `/Game/DDSurvivors/Blueprints/Enemies/Bosses/BP_SpikeBoss.BP_SpikeBoss_C` und `.../BP_SpikeBossSpawnPoint.BP_SpikeBossSpawnPoint_C`. Der Endboss heißt `BP_HQ_Endboss`, sein Spawnpunkt `BP_HQBossSpawnPoint`.
4. Dumper-7 mit absolutem Pfad per `inject.py` in den Host laden. Der Dump dauerte rund drei Minuten, der Host lief danach weiter.

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
- **[Belegt, nachgeholter Spieltest]** Vollständiger VERSION-Proxy mit 17 Exporten
  lädt in beiden Spielinstanzen; beide erreichen `DDDynamic_Map`. Die drei
  Signaturen treffen auch im geladenen Code eindeutig. Der temporäre
  Character-ProcessEvent-Hook leitet `K2_GetActorLocation` genau einmal weiter,
  erhält den Rückgabewert und wird danach zurückgebaut.
- **[Belegt, Testaufbau]** Die vorgefundene Installation lädt zusätzlich UE4SS und
  eine vorhandene Mod. Diese Dateien wurden nicht verändert. Ein genehmigter
  eigener VERSION-Hardlink wurde nur während des Tests benutzt und danach
  entfernt. Der Drei-Export-Proxy bestand nur isoliert, im Spiel nicht; siehe
  [End-to-End-Nachtrag](../research/spikes/andockpunkte/README.md#nachtrag-vollstaendiger-exportproxy-und-end-to-end-test).

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
- **[Belegt]** Client-Schüsse treffen beim Host, verursachen Schaden und töten Gegner. Gemessen über die Treffermeldung des Spiels, siehe [Treffer des Clients](#treffer-des-clients-treffer).
- **[Hypothese]** Die Replikation trägt, aber die Spiellogik rechnet weder beim Host noch beim Client mit Netzwerkspielern. Die Fehler sind bisher klein und lokal, ihre Gesamtzahl ist unbekannt.

#### Treffer des Clients (`treffer`)

Stand: Spike `Hits.cpp`, Läufe über das Spielmenü am 03.11.2026, Build `26187885`, `RunSeed=20795`, Tag 1 oben. Ohne `MapGen.cpp`, die Karte des Clients war also eine andere.

**Messweg:** Beim Host ruft das Spiel für jeden Treffer auf einen Gegner `UDDGameStatsSubsystem::OnEnemyHit(FDamageInformation)` auf, und zwar über `ProcessEvent`. Der Spike tauscht `ProcessEvent` in der VTable dieser Klasse und liest je Treffer:

- `Damage`, das getroffene Ziel (`DamagedActor`), den Auslöser (`InstigatedBy`) und den Verursacher (`DamageCauser`) aus `FDamageInformation`
- die Lebenspunkte des Ziels aus `CommonAttributeSet` (`ADDCharacterBase+0x700`, `Health` bei `+0xA0`). Die Meldung kommt nach dem Abzug, ein Wert von 0 ist also ein Kill.

- **[Belegt]** Auslöser ist der `PlayerState` des Schützen, Verursacher das Projektil (`BP_Projectile_C`). Der Spike ordnet den Treffer über die Figur des PlayerStates zu: Beim Host ist die Figur des Clients `AutonomousProxy`, die eigene nicht. Ein erster Versuch über die Besitzerkette des Auslösers fand keine Figur und zählte alles als „Sonstige“.
- **[Belegt]** Treffer des Clients ziehen beim Host Leben ab und töten. Ein `BP_StandardEnemy_C` hat 50 Leben, jeder Treffer nimmt 25, nach dem zweiten steht er bei 0.
- **[Belegt]** Die Zuordnung trennt beide Spieler. In Lauf 3 meldete sie einen Treffer des Hosts.
- **[Belegt]** In der Mission halten Tutorial-Hinweise das Spiel an, die Uhr bleibt etwa bei 00:08 stehen. Auf einen Hinweis folgt der nächste, das Sprinten löst einen weiteren aus. Der Spike-Befehl `!notutorial` an beide Instanzen schaltet sie ab (`UTutorialStateSubsystem::SetForceSkipTutorials`).

| Lauf | Tutorial | Treffer Client | Treffer Host | Kills des Clients |
|---|---|---|---|---|
| 1 | hielt das Spiel an | mindestens 5, noch als „Sonstige“ gezählt | – | – |
| 2 | hielt das Spiel an | 1 | 0 | 0 |
| 3 | hielt das Spiel an | 0 | 1 | – |
| 4 | erster Hinweis weg, der zweite hielt an | 4 | 0 | 2 |
| 5 | alle Hinweise weg, rund 2 Minuten | 50 (1.250 Schaden) | 0 | 25 |
| 6 | wie 5, danach Eingabe beim Host, Ausdauer-Hinweis hielt an | 30 (750 Schaden) | 0 | 15 |

Offen:

- Warum die Host-Figur in den Läufen 5 und 6 nicht traf, ist nicht untersucht. **[Vermutung]** Ohne Eingaben in ihrem Fenster schießt sie nicht, oder es waren keine Gegner in ihrer Reichweite.
- Nicht geprüft: XP und Loot für Kills des Clients, andere Waffen und Gegnertypen, Schadenszahlen beim Client (stehen unter den optionalen Optimierungen).

#### Last (Spike D, `last`)

Stand: Spike `Load.cpp`, 03.11.2026, Build `26187885`, Host und Client auf einem PC über Loopback, Mission über das Spielmenü, `RunSeed=20795`, Tag 1 oben, mit `MapGen.cpp` und `Pcg.cpp`.

**Messweg:** `Load.cpp` tauscht die IAT-Einträge der Exe für `sendto` und `recvfrom` (WS2_32, Ordinal 20 und 17) und zählt die UDP-Nutzdaten und Pakete. Die Framerate kommt aus `KismetSystemLibrary.GetFrameCount`. Jede Sekunde schreibt der Spike eine Zeile `Last:` ins Log. Gegner sind alle `EnemyBase`-Objekte laut `objects.py`, also auch gepoolte.

| Phase | Gegner Host / Client | Host sendet | Host empfängt | Pakete je Richtung | Framerate Host / Client |
|---|---|---|---|---|---|
| A: `MetaBase`, 30 s | – | 3,0 KB/s | 5,2 KB/s | 58–59/s | 60 / 60 |
| B: Mission, 40 s | 20 / 19 | 7,2 KB/s (max 9,4) | 1,1 KB/s | 59/s | 60 / 60 |
| C: Nacht, Bedrohung 10, 60 s | 82–89 / 81–87 | **46,5 KB/s** (max 47,3) | 1,1 KB/s | 59/s | 60 / 60 (min 60) |

Phase C entstand mit `!cheats`, dann `DD_ToggleInvincible`, `DD_ToggleFreezeTime 1`, `DD_SetThreatLevel 10` und `DD_SetTimeSinceMissionStart 300`. Im Bild drängt sich eine Horde um den Bus.

- **[Belegt]** Mit rund 85 Gegnern sendet der Host knapp 47 KB/s, also etwa 370 kbit/s, an einen Client. Der Client sendet in der Mission konstant 1,1 KB/s.
- **[Belegt]** Die ausgehandelte Rate liegt auf beiden Seiten bei 100.000 B/s (`CurrentNetSpeed`, `MaxClientRate`, `MaxInternetClientRate`). Die Last nutzt davon knapp die Hälfte. Der Listen-Server sendet einmal je Frame, `bClampListenServerTickRate` ist aus, daher rund 59 Pakete je Sekunde bei 60 fps.
- **[Belegt]** Die Framerate blieb in allen Phasen bei 60 auf beiden Seiten. **[Vermutung]** Das ist die Grenze der Bildrate, die Messung sagt also nur, dass beide Instanzen zusammen auf diesem PC nicht unter 60 fallen.
- **[Vermutung]** Je zusätzlichem Gegner kommen rund 0,6 KB/s hinzu ((46,5 − 7,2) KB/s für etwa 65 Gegner mehr). Dann wäre die Grenze von 100.000 B/s bei etwa 150 Gegnern erreicht. Ob es im späten Spiel so viele gleichzeitig gibt, ist nicht gemessen.
- **Folgerung für Gate G1:** Für einen Client ist die Last tragbar, auch mit gewöhnlichem Upload. Sie spricht nicht gegen Weg A.

Fehlversuche:

- Die ersten Läufe hielten Tutorial-Hinweise an, Phase C maß dann einen stehenden Zustand. Danach `!notutorial`.
- `DD_SetTimeSinceMissionStart 480` beendete den Tag. Weitergeklickte Hinweise führten ins Lager, gemessen wurde dort mit 0,7 KB/s.
- Ein Client stürzte beim Laden der Mission in `TickPcg` ab (`spike.dll`+`0x18868`, `TickPcg`+`0xF8`). Die Schleife las den Besitzer `+0xA8` vor der Klassenprüfung an jedem Objekt. Behoben in `Pcg.cpp`, danach zwei Läufe ohne Absturz.

Offen:

- Nur ein PC über Loopback: keine Latenz, kein Paketverlust, die CPU teilen sich beide Instanzen. Eine Messung über das Internet fehlt.
- Ohne Bildratengrenze und mit einem langsameren PC ist die Framerate nicht gemessen. Spätere Tage und Bosse sind nicht gemessen.

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
- ~~**[Hypothese]** Derselbe Seed ergibt dieselbe Karte.~~ Belegt, siehe unten.

**Wer die Erzeugung startet** (Disassembly und Laufzeit, Build `26187885`, RVAs in [Offsets](OFFSETS.md#karte))

- **[Belegt]** Der Starter ist die Komponente `MapGenProfileInjectorAndBuildTrigger` am `BP_MapGenerator_C` der Karte. Ihre virtuelle Funktion in VTable-Slot 125, vermutlich `BeginPlay`, ruft den Aufbau `0x58F7710` auf.
  Weder Komponente noch Generator replizieren. Sie laufen also auf jeder Seite für sich, auch beim Client.
- **[Belegt]** Der Aufbau liest die aktuelle Mission aus der GameInstance, lädt das passende `MapGenerationProfile` und setzt es in `MapGenProfile`. Danach setzt er
  `MapSeed = GetCurrentMissionSeed() * 0x0BB38435 + 0x3619636B` (32 Bit), außerdem `MaxTilePosition` und `GenerationTags`.
  Dann meldet er Seed und Profil an den Bug-Report (`SetDDMissionSeed`, `SetDDMapProfileName`) und ruft `StartMapGeneration` auf.
  Lässt sich das Profil nicht laden, überspringt er Seed und Profil und startet den Generator mit dessen Standardwerten.
- **[Belegt]** `GetCurrentMissionSeed` ist eine reine Hash-Funktion (`HashCombine`) aus dem Run-Seed (`GameInstance+0x2A0`) und einem 64-Bit-Wert der aktuellen Mission. Es gibt keinen Zufall.
- **[Belegt]** `StartMapGeneration` nimmt `MapSeed`. Nur wenn dieser 0 ist, würfelt es mit `FMath::RandRange(0, 1000000)` einen Seed, also nicht reproduzierbar. Danach ruft es `StartMapGenerationWithRetryAttempts(Seed, MaxAttempts)` auf.
  `TryToRegenerate` startet ebenfalls diesen Weg. Jeder Sub-Generator zieht aus seinem eigenen `FRandomStream` in `RandomGeneratorLookup`.
- **[Belegt]** `DD_SetCurrentRunSeed <n>` (Exec-Befehl der GameInstance) setzt den Run-Seed und verwirft die gecachte Missionsstruktur. Die Routenkarte entsteht danach neu aus diesem Seed.
  `DD_ForceStartRunSeed 4242` vor Run-Start ergab dagegen zweimal verschiedene Routen. Was der Befehl tut, ist nicht geklärt.

**Derselbe Seed ergibt dieselbe Karte**

Test über das Spielmenü mit dem Spike, Mission „Tag 1, oberer Knoten“, zwei getrennte Sitzungen, beide Male mit `RunSeed=20795`. In Lauf 2 wurde er über `DD_SetCurrentRunSeed` gesetzt:

| | Lauf 1 Host | Lauf 2 Host | Client (beide Läufe) |
|---|---|---|---|
| Profil | `MGP_City_Chinatown` | `MGP_City_Chinatown` | `MGP_HQ8_Boss` (Standard) |
| `MapSeed` | `-1868504954` | `-1868504954` | `1` |
| `MapGenerationState` | Seed `MapSeed+25`, 102 Actors, nicht gescheitert | gleich | Seed 1, 19 Actors, gescheitert |
| `MapTile` | 101 | 101, gleiche Klassen, Positionen und Drehungen | 17 unsichtbare Kacheln |

- **[Belegt]** Gleicher Run-Seed und gleiche Mission ergeben über Prozessgrenzen hinweg dieselbe Routenkarte und dieselbe Karte. Zeitbudget (`CycleTimeBudget=0.006`) und Wiederholungen haben das Ergebnis nicht verändert.
  Verglichen wurden die Klassen, Positionen und Drehungen aller `MapTile` sowie die Zahl der erzeugten Actors, nicht jedes einzelne Objekt auf der Karte.
- **[Belegt]** Beim Client läuft die Erzeugung ebenfalls, aber ohne Missionsdaten: Er hat einen eigenen, zufälligen Run-Seed, kein Profil und `MapSeed=1`.
  Die Erzeugung scheitert, übrig bleiben 17 Kacheln. Bei einer direkt geladenen `DDDynamic_Map` (`-Map`) gilt das auf beiden Seiten, daher die gleichen 17 Kacheln aus Spike C.
- **[Hypothese]** Für dieselbe Karte beim Client reicht es, ihm vor dem Kartenwechsel Run-Seed und aktuelle Mission zu geben, oder direkt `MapSeed` und `MapGenProfile` vor `StartMapGeneration` zu setzen.
  Wie sich das mit den replizierten Kartenobjekten verträgt, steht im nächsten Abschnitt.

### Karte und replizierte Kartenobjekte

Bestandsaufnahme aller Actors im `PersistentLevel`, Mission über das Spielmenü (`RunSeed=20795`, Tag 1 oben, `MGP_City_Chinatown`), Host und Client mit dem Spike, rund 25 Sekunden nach dem Start.
Beim Client zählt `Role=Authority` als lokal erzeugt, `SimulatedProxy` und `AutonomousProxy` als übers Netz gekommen.

| Gruppe | Host | Client lokal | Client übers Netz |
|---|---|---|---|
| Kacheln, Parzellen, Straßen, Nebel (`MapGenerationState.SpawnedActors`) | 102, nicht repliziert | 19 (eigene, gescheiterte Karte) | 0 |
| `BP_EnvironmentObjectLootspot_C` | 36 | 0 | 25 |
| Zerstörbare Objekte (`BPC_*Destructable*`, `BP_DEST_*`, `BPC_Explosive_*` und andere) | rund 190 | 0 | rund 120 |
| `BP_PowerUpCrate_C` | 14 | 0 | 7 |
| `BP_ChaosDestructable_PowerUp_C` | 14 | **7** | 7 |
| Gegner, Figuren, Loot, `GameState` | wie gewohnt | 0 | relevante Teilmenge |

- **[Belegt]** Der Generator erzeugt selbst nur nicht replizierte Actors: Kacheln, Parzellen, Straßen, Nebel und ein `BP_PCGVolume_C`.
  Die replizierten Kartenobjekte stehen nicht in `SpawnedActors`. Sie heißen `…_GEN_VARIABLE_…_CAT`, sind also Child Actors von `ChildActorComponent`s an den erzeugten Parzellen und Fahrzeugen, etwa `CarLootspot`.
  Alle diese Klassen replizieren schon im Default-Objekt (`bReplicates=1`).
- **[Belegt]** Beim Client kommen diese Objekte heute nur übers Netz an, als eigenständige Actors ohne Eltern-Parzelle. Die Parzelle repliziert nicht und fehlt beim Client.
  Weniger als beim Host kommen an, weil nur relevante Objekte innerhalb der Netz-Reichweite übertragen werden. Doppelte Objekte gibt es dabei nicht, weil die Karte des Clients scheitert und keine Child Actors erzeugt.
- **[Belegt]** Es gibt schon heute Duplikate: Jede `BP_PowerUpCrate_C` erzeugt ihre `BP_ChaosDestructable_PowerUp_C` selbst (normaler Spawn, kein Child Actor). Beim Client tut das jede angekommene Kiste lokal, dazu kommt die replizierte vom Host.
  Das ergibt 7 lokale plus 7 replizierte Exemplare. Ursache und Spike-Patch stehen unter [Doppelte PowerUp-Objekte](#doppelte-powerup-objekte-powerup-duplikat).
  Möglicherweise hängt auch die Abweichung der Gegner-Zähler an solchen Spawns.
- **[Belegt]** Erzeugt der Client dieselbe Karte selbst, entstehen die replizierten Child Actors doppelt, siehe [Client erzeugt die Host-Karte](#client-erzeugt-die-host-karte-karte-client).
  Die Parzellen sind beim Client lokal erzeugt und haben dort `Role=Authority`. Die Sperre der Engine für replizierte Child Actors auf Clients greift nur, wenn der Besitzer keine Autorität hat.

**Optionen** (Entscheidung offen)

| Weg | Vorteile | Nachteile |
|---|---|---|
| A: Client erzeugt dieselbe Karte, lokale Child Actors und Spawns replizierter Klassen werden beim Client unterdrückt | Boden, Kollision und Navigation lokal und deckungsgleich, wenig Netzlast. Die Bodenkacheln passen zur Host-Position | Braucht Seed und Profil beim Client sowie einen Hook auf das Erzeugen von Child Actors und Spawns. Server-Kopien hängen nicht an einer Parzelle |
| B: Host repliziert auch Kacheln und Parzellen | Kein Generator beim Client | Viele große Actors ohne stabile Namen. Die Kacheln sind `Movable`, das bricht heute schon die Bewegung. Hohe Last |
| C: Wie heute, Client ohne Karte | Kein Aufwand | Falscher Boden, keine Kollision, Figur beim Client weit weg vom Bus |

Empfehlung: Weg A. Nächster Schritt ist ein Spike, der beim Client vor `StartMapGeneration` `MapSeed` und `MapGenProfile` des Hosts setzt.
Er zählt danach die Duplikate wie oben und unterdrückt im Erfolgsfall replizierte Klassen beim lokalen Erzeugen. Für `BP_PowerUpCrate_C` gilt dieselbe Sperre.

#### Client erzeugt die Host-Karte (`karte-client`)

Stand: Spike `MapGen.cpp`, mehrere Läufe über das Spielmenü am 03.11.2026, Build `26187885`, `RunSeed=20795`, Tag 1 oben.

**Was der Client vom Host braucht** (alles, was der Starter `0x58F7710` beim Host setzt):

| Wert | Feld | Beim Client ohne Patch |
|---|---|---|
| Seed | `MapSeed` (`+0x2D8`) | `1` |
| Profil | `MapGenProfile` (`+0x2B8`) | `MGP_HQ8_Boss`, oft ist das Profil der Mission gar nicht geladen |
| Kartengröße | `MaxTilePosition` (`+0x2E8`) | `(3, 3)` statt zum Beispiel `(5, 5)` |
| Tags | `GenerationTags` (`+0x300`) | leer, der Host hat etwa `Rain` und erzeugt damit `BP_Rain_C` |

- **[Belegt]** Das Profil einer Mission folgt nicht allein aus dem Seed: Derselbe `MapSeed` ergab je nach Lauf `MGP_City_LittleIndia`, `MGP_GolfCourse` oder `MGP_Forest`. Es muss also mit übertragen werden.
- **[Belegt]** Ist das Profil beim Client nicht geladen, lädt der Spike es wie der Starter des Spiels über einen Soft-Verweis: `MakeSoftObjectPath`, `Conv_SoftObjPathToSoftObjRef` und `LoadAsset_Blocking` der `KismetSystemLibrary`.
- **[Belegt]** Tags gehen als Namen über, der FName-Index gilt nur je Prozess. Beim Client entsteht der Tag über `Conv_StringToName` und `BlueprintGameplayTagLibrary.AddGameplayTag`, so legt das Spiel den Speicher selbst an.
- **Spike-Ersatz für den Kontrollkanal:** Der Host schreibt die Werte in `mapgen.txt` im Befehlsordner des Spikes, der Client auf demselben PC liest sie. Kommt der Client vorher an, wartet seine Erzeugung und startet aus dem Sekunden-Tick. Über das Netz geht das noch nicht.

**Duplikate:** `UChildActorComponent::CreateChildActor` (`0x288B510`) lässt replizierte Child Actors nur aus, wenn der Besitzer keine Autorität hat (Prüfung mit `0x289DB60`). Beim Client sind die erzeugten Parzellen lokal und `Authority`.
Der Spike setzt deshalb für genau diesen Aufruf die Rolle des Besitzers auf `SimulatedProxy`, wenn alles zutrifft: Client, die eigene Erzeugung läuft noch (`MapGenerationState` hat einen aktiven oder wartenden Sub-Generator), Besitzer nicht repliziert, Child-Klasse repliziert. Die Engine nimmt dann ihren eigenen Ausstieg.
Ein erster Versuch ohne die Bedingung „Erzeugung läuft“ unterdrückte auch rund 300 Child Actors der fest gebauten `MetaBase`. Für die ist die lokale Erzeugung richtig, und danach kam der Host über das Menü nicht mehr in die Mission.

| Lauf | Profil | Kacheln Host / Client | Erzeugte Actors Host / Client | Lokale Kopien replizierter Kartenobjekte beim Client |
|---|---|---|---|---|
| Ohne Patch | `MGP_Forest` (Host) | 46 / 17 | 47 / 19, Client gescheitert | – |
| Seed und Profil, Profil beim Client nicht geladen | `MGP_City_LittleIndia` | 150 / 0, Client wartete | – | – |
| Seed, Profil (nachgeladen), Größe | `MGP_GolfCourse` | 88 / 88, gleicher Fingerabdruck | 90 / 89, nur `BP_Rain_C` fehlt | – |
| Dazu Tags | `MGP_GolfCourse` | 100 / 100, gleicher Fingerabdruck | 102 / 102 | Loot-Spots 51 lokal plus 31 übers Netz, Zerstörbares ebenso doppelt |
| Dazu Sperre für Child Actors | `MGP_Forest` | 121 / 121, gleicher Fingerabdruck | 123 / 123 | **0**, außer 6 `BP_ChaosDestructable_PowerUp_C` aus den Kisten (Patch in `MapObjects.cpp`, PR #33) |

Verglichen wurden Klasse, Position und Drehung aller `MapTile` und die Klassen aller vom Generator erzeugten Actors. Beim Client stehen Straße, Gehwege, Laternen und Fahrzeuge an denselben Stellen wie beim Host. Die Figur des Clients läuft darauf, der Host sieht dieselbe Position.

Offen:

- Bäume und Pflanzen aus PCG fehlten beim Client, gelöst in [PCG-Vegetation beim Client](#pcg-vegetation-beim-client-pcg).
- Die Werte müssen über einen echten Kanal zum Client, im Spike geht es über eine Datei auf demselben PC.
- Die Sperre zählte rund 1.400 unterdrückte Child Actors, der Host repliziert weniger Objekte. **[Vermutung]** Ein Teil der Komponenten wird mehrfach erzeugt oder per Zufall ersetzt (`RandomChildActorSwapWrapper`). Fehlende Objekte waren im Bestand nicht zu sehen. Was außer Reichweite liegt, kommt erst bei Annäherung übers Netz.
- Nicht getestet: Tag 2 und spätere Karten, Bosskarten, ein Kartenwechsel per `servertravel` mit neuer Karte, Verbindungen über das Internet.

#### Doppelte PowerUp-Objekte (`powerup-duplikat`)

Stand: Blueprint-Referenzen, Disassembly und drei Spieltests am 03.11.2026, Build `26187885`.

- **[Belegt]** `ExecuteUbergraph_BP_PowerUpCrate` ruft `GameplayStatics.BeginDeferredActorSpawnFromClass`, `FinishSpawningActor` und `Actor.K2_AttachToActor` auf und legt das Ergebnis in `DestructablePowerUp` (`+0x390`) ab. Das ist der Spawn aus `ReceiveBeginPlay`.
- **[Belegt]** Die Kiste wird auf beiden Seiten gespawnt und hängt die neue `BP_ChaosDestructable_PowerUp_C` an sich. Beim Host repliziert das neue Objekt (`bReplicates=1`) zum Client. Beim Client spawnt die angekommene Kiste ein zweites, lokales Exemplar mit `Role=Authority`, das an keine Netzverbindung gebunden ist.
- **[Belegt]** Ohne Patch lag beim Client ein lokales PowerUp ohne zugehörige Kiste in der Welt. **[Vermutung]** Seine Kiste hatte die Netz-Reichweite verlassen und wurde beim Client gelöscht, das lokale PowerUp blieb übrig.
- **[Belegt]** Der Spike-Patch in `MapObjects.cpp` hängt sich in `UGameplayStatics::BeginDeferredActorSpawnFromClass` (`0x2A678F0`) ein. Beim Client gibt er für `BP_ChaosDestructable_PowerUp_C` aus einer `BP_PowerUpCrate_C` `null` zurück. `FinishSpawningActor` und `K2_AttachToActor` laufen dann ins Leere, `DestructablePowerUp` der Client-Kiste bleibt leer. Beim Host greift der Patch nie.

| Lauf | Host | Client |
|---|---|---|
| Ohne Patch, Mission über das Menü | 11 Kisten, 11 PowerUps | 3 Kisten, 4 lokale plus 4 replizierte PowerUps, eins davon ohne Kiste |
| Mit Patch, Mission über das Menü, danach Bewegung | 14 Kisten, 14 PowerUps, nichts unterdrückt | 1 Kiste, 1 repliziertes PowerUp, 1 lokale Kopie unterdrückt |
| Mit Patch, zweiter Lauf mit Bewegung und `servertravel` | 4 Kisten, 4 PowerUps, nichts unterdrückt | 3 Kisten, 4 replizierte PowerUps, 7 lokale Kopien unterdrückt, kein lokales Exemplar |

Im ersten Lauf mit Patch stürzte beim `servertravel` erst der Host bei `0x57CC398` und dann der Client bei `0x57D080C` ab. Das sind die bekannten Abstürze der Weiterreise, siehe [Inventar](../research/spikes/a-connect/README.md#inventar-ursache-teilfix-und-verbleibende-grenze). Im zweiten Lauf mit gleichem Ablauf trat kein Absturz auf. Ein Zusammenhang mit dem Patch ist deshalb nicht erkennbar, aber auch nicht ausgeschlossen.

Nicht getestet: Zerbrechen einer Kiste und was der Client dabei sieht, das Einsammeln des PowerUps durch den Client, Kisten aus anderen Quellen (`BP_PowerUpCrateSpawner_C`, Airdrops). Ob die Client-Kiste ohne `DestructablePowerUp` etwas anders darstellt, ist offen (**[Vermutung]**: nein, Schaden und Zerbrechen laufen nur beim Host).

#### PCG-Vegetation beim Client (`pcg`)

Stand: Spike `Pcg.cpp` zusammen mit `MapGen.cpp`, Läufe über das Spielmenü am 03.11.2026, Build `26187885`, `RunSeed=20795`, Tag 1 oben.

**Ablauf beim Host:** Bäume, Büsche und Pflanzen einer Waldkarte kommen aus PCG. Der Sub-Generator `BP_Foliage_PCGVolumeSubGenerator_C` (native Klasse `UPCGVolumeGenerator`, Vtable `0x95626D8`) arbeitet in zwei Schritten:

1. Slot 85 (`0x59EE6A0`) spawnt `BP_PCGVolume_C` und wählt den Graphen. Er legt ihn als Soft-Verweis in `BiomeGraphToGenerate` (`+0x50`) ab.
2. Slot 86 (`0x59D95E0`) lädt den Graphen asynchron, startet PCG und meldet sich danach per Callback beim Kartengenerator.

- **[Belegt]** Mit `bUseGraphFromBiome` (`+0x28`) kommt der Graph aus dem Biom der aktuellen Route in der GameInstance. Die Route liegt ab `+0x2B0` (Einträge zu je `0x290` Bytes, Anzahl `+0x2B8`, Index `+0x2C0`), das Biom steht im Eintrag bei `+0x1A8`. Von dort nimmt der Generator `UBiomeDefinition::GraphToGenerate` (`+0x118`), Überschreibungen aus dem Generierungszustand und aus `BiomeMapGenerationOverrideProvider` haben Vorrang.
  Beim Host war das `Biome_Woods` mit `PCG_ForestSpawner_v2`.
- **[Belegt]** Beim Client ist die Route der GameInstance leer (Anzahl 0, Index −1). Slot 85 findet kein Biom, der Verweis bleibt leer, Slot 86 lädt nichts und PCG läuft nicht. Das Volume steht, aber ohne Instanzen.
- **[Belegt]** Mit übertragenem Graphen stimmten Anzahl, Mesh und Position aller Instanzen überein, die Baumart an derselben Stelle aber nicht immer. Jede ISM-Komponente würfelt `InstancingRandomSeed` (`+0x734`) beim Erzeugen, auch zwei Läufe des Hosts unterscheiden sich darin. Das Material wählt damit die Art je Instanz.

**Spike-Ersatz:**

- **Graph:** Der Host schreibt nach Slot 85 Kartenseed und Graph in `pcg.txt` im Befehlsordner des Spikes. Der Client setzt nach Slot 85 denselben Verweis, wenn seiner leer ist und der Seed passt. In zwei Läufen kam der Client 390 und 516 ms vor dem Host an diese Stelle. Er wartet dann im Spielthread, höchstens 15 s.
- **Seeds:** Nach der PCG-Erzeugung setzen beide Seiten den Seed jeder ISM-Komponente aus Mesh-Name, Instanzzahl und Summe der Positionen, unabhängig von der Reihenfolge der Instanzen. Danach `SetStaticMesh(nullptr)` und wieder `SetStaticMesh(Mesh)`, damit der Render-Zustand mit dem neuen Seed entsteht. Der Host sieht dadurch eine andere zufällige Verteilung der Arten als ohne Mod.

| Lauf | Profil | PCG-Instanzen Host / Client | Baumarten an derselben Stelle |
|---|---|---|---|
| Ohne Patch | `MGP_Forest_Roadstops` | 50.701 / 0 | – |
| Ohne Patch | `MGP_Forest_Campgrounds` | 90.414 / 0 | – |
| Graph übertragen | `MGP_Forest_Roadstops` | 80.399 / 80.399, 7 Komponenten mit gleichem Mesh und gleichen Positionen | teils verschieden, Seeds verschieden |
| Dazu Seeds | `MGP_Forest_Roadstops` | 77.590 / 77.590, gleiche Positionen | gleich, Seeds aller 7 Komponenten gleich |

Die Instanzzahl schwankt zwischen den Läufen, weil der Host je Lauf eine andere Karte erzeugte. Die feste Vegetation der `MetaBase` (`PCGVolume`) war schon ohne Patch auf beiden Seiten gleich, je 39.502 Instanzen.

Offen:

- **[Vermutung]** Die leere Route betrifft beim Client auch andere Werte aus dem Biom, etwa den Texturtausch (`BiomeTextureSwappingGenerator`), Gegnermaterialien und Wetter-Tags. Geprüft ist nur PCG. Die Route selbst auf den Client zu bringen gehört zu `rundenende`.
- Die Seeds wirken erst mit dem nächsten Sekunden-Tick, bis dahin können die Arten kurz abweichen. Die Kosten des neuen Render-Zustands sind nicht gemessen.
- **[Belegt]** Die erste Fassung von `TickPcg` las den Besitzer jedes Objekts vor der Klassenprüfung und ließ einmal einen Client abstürzen. Behoben in `last`, siehe [Last](#last-spike-d-last).
- Nicht getestet: andere Biome, Tag 2 und später, Bosskarten, Internet.

### Gegner und Pausen

- **[Belegt]** `AEnemySpawner` ist reine Host-Logik: Er repliziert nicht und nutzt Gegner über `RecycleArray` wieder. In der Testmission galt
  `EnemyCap=300`. `MaxSpawnedEnemies` lag beim aktiven Spawner bei 20.
- **[Belegt]** Pausen laufen über `UTimescaleManager` (ein Subsystem der GameInstance), `UPauseScreenManager` und
  `ADDMissionGameState::SetFreezeTime`. Coop-UI-Sitzungen tragen dazu `bBlocksGameplay`.
- **[Hypothese, durch neue Messung eingeschränkt]** Die Requestverwaltung des TimescaleManagers ist lokal; daraus folgt nicht, dass ihre Weltwirkung lokal bleibt. `TimeDilation`, `CinematicTimeDilation` und `PauserPlayerState` sind im Engine-SDK als Netzwerk-Properties markiert.
- **[Belegt, SDK und Laufzeit]** Aufgabe `pausen`: In einer direkt geladenen Mission mit aktivem Tutorialdialog hatte nur der Host einen `DialogComponent`-Zeitrequest und eine blockierende Coop-UI-Sitzung. Auf beiden Seiten waren aber `TimeDilation` etwa `0.0001` und ein `PauserPlayerState` vorhanden. Client-UI-Sitzung und Weltpause sind daher getrennt zu betrachten; die genaue Aufrufkette wurde nicht nachgewiesen.
- **[Belegt, Default-Objekte]** Das native und Blueprint-Inventarpanel haben `bCoopBlocksGameplay=true`, aber `PauseGameWhileOpen=false`. Der eine Schalter ersetzt nicht den anderen. Der Inventar-Methodentest fand keinen Animationscontainer; `RequestLevelUp` kehrte ohne neue nachweisbare Sitzung zurück. Normaler Level-up und Inventarablauf bleiben offen, siehe [Versuche](../research/spikes/pausen/README.md).

**Nachtrag `pausen`, 03.11.2026 (Build `26187885`):**

- **[Belegt, isolierter Eingabeablauf]** Pausemenue → Inventar ansehen → Zurueck → Fortsetzen: Sitzungszahl auf der oeffnenden Seite `0 → 1 → 2 → 1 → 0`, Inventar ist Kind der Pausemenue-Sitzung. Auf Host und Client getrennt gemessen. Aktiver Zeitrequest bleibt beim `PauseScreenManager`; kein zweiter Inventar-Zeitrequest behauptet.
- **[Belegt]** Host-Pause: Faktor etwa `0.0001` und Pauser auf beiden Seiten. Client-Pause: nur Client-Faktor etwa `0.0001`, kein Pauser, Host bleibt bei Faktor 1. UI-Sitzungen werden in keinem der beiden Wege auf die Gegenseite uebertragen.
- **[Belegt]** Bei gleichzeitig offenen Pausemenues stellt Host-Fortsetzen den Client-Weltfaktor auf 1, obwohl dessen lokale blockierende Sitzung noch offen ist. Replizierte Weltpause ersetzt daher keine abgestimmte Verwaltung von Pausengruenden.
- **[Belegt, kontrollierter Aufnahmeweg]** 200 testweise am Host-Pawn erzeugte XP werden aufgenommen: Host Level 1 / 0.10 → Level 2 / 0.547264; Client bleibt Level 1 / 0. Vorher/nachher kein Request, keine UI-Sitzung, Faktor 1 und kein Pauser auf beiden Seiten. Keine anhaltende Level-up-Pause oder Upgradeauswahl in diesem Ablauf; kurze Effekte zwischen Snapshots nicht ausgeschlossen. Kein Nachweis eines bestimmten Gegnertods.
- **[Folgerung fuer die Umsetzung]** Client meldet Pausen-/UI-Wuensche an den Host; Host bestaetigt Gruende und gemeinsamen Weltzustand. Session-/Parent-IDs und blockierende Gruende getrennt von Weltzeit und XP/Level uebertragen. Disconnect-/Travel-Bereinigung bleibt Implementierungsarbeit. Details und abgegrenzte Fehlversuche im [Nachtrag](../research/spikes/pausen/README.md#nachtrag-03112026-isolierte-ablaeufe).

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

## Kamera und Sichtbereich

Stand: Disassembly und Laufzeit, Build `26187885`. Mission über das Spielmenü mit dem Spike, Werte aus dem Speicher beider Instanzen. RVAs in [Offsets](OFFSETS.md#kamera).

**Was `AllPlayerFramingCamera` macht**

- **[Belegt]** `BP_PlayerCameraActor_C` (Basis `AAllPlayerFramingCamera`) ist eine Kamera für alle Spieler. Sie holt die Figuren aus `GameMode->SpawnedPlayers` und rahmt sie gemeinsam ein.
  Werte der Mission: `MaxMovementDistance=250`, `UseZoomOutAtEdge=1`, `ZoomOutDistance=750`, Spring-Arm 8750, FOV 40, geglättete Position.
- **[Belegt]** Am Kamera-Actor hängt `UInViewportMovementRestrictor`. Er bremst Spieler ab 60 % des Weges vom Bildmittelpunkt zum Rand und stoppt sie bei 90 %, in alle vier Richtungen. Die Spieler können sich also nicht weiter trennen, als ein Bild zeigt. Das ist der Kern des lokalen Coop.
- **[Belegt]** Beim Host ist die gemeinsame Kamera das ViewTarget beider Controller. Der `PlayerCameraManager` des Remote-Controllers wird beim Host aber nicht berechnet (`POV` bleibt `(0, 0, 0)`).
- **[Belegt]** Beim Client läuft eine eigene Instanz derselben Kamera auf den replizierten Figuren, mit dem Spike-Patch für den fehlenden `GameMode`. Sie stand im Test rund 490 Einheiten neben der des Hosts. Bei etwa 6000 Einheiten Bildhöhe sind das unter 10 % des Bildes, durch Glättung und Verzögerung der replizierten Positionen.

**Spawnen an den Viewport-Rändern**

- **[Belegt]** Beide `AEnemySpawner` der Mission haben `UseViewportEdgesForSpawningPositions=1` und keine Einschränkung der Ränder. Die Ränder kommen aus `CalculateViewportEdgesInWorldSpace` (`0x5B02DD0`).
- **[Belegt]** Diese Funktion läuft über alle Controller in `SpawnedPlayers`, nimmt jeweils deren Kamera-Komponente und projiziert die vier Bildecken auf den Boden (`0x5B028D0`: `GetViewportSize` und viermal `DeprojectScreenPositionToWorld`).
  Je Ecke behält sie die am weitesten außen liegende. Das Ergebnis ist also ungefähr die Vereinigung aller Spieler-Ansichten.
- **[Belegt]** Die Projektion prüft nicht, ob es einen Viewport gibt. Für einen Remote-Controller ohne `LocalPlayer` liefert `GetViewportSize` nichts, und die Projektion scheitert.
- **[Hypothese]** Für einen Remote-Controller entsteht deshalb kein sinnvoller Beitrag, und es zählt nur die Ansicht des Hosts. Bei gemeinsamer Kamera ist das egal, weil beide dasselbe Bild haben.
  Bei getrennten Kameras spawnen Gegner nur an den Rändern des Host-Bildes. Beim Client können sie dann mitten im Bild erscheinen, oder er bekommt in seiner Gegend gar keine.
- **[Belegt]** Die Spawn-Schleife wählt den Zielspieler zufällig aus `SpawnedPlayers` (siehe [Gegner-Spawning](#gegner-spawning-und-pooling)). Die Gegnerzahl (`MaxSpawnedEnemies`) gilt für beide Spieler zusammen.

**Entscheidung vom 03.11.2026:** Auf ausdrückliche Vorgabe des Projektverantwortlichen gilt vorerst **Option A: gemeinsame Kamera** für den ersten spielbaren Stand. Ursprünglicher Zoom und Bewegungsbegrenzung bleiben erhalten. Das ist eine Designentscheidung, keine abgeschlossene Umsetzung in der Mod; diese bleibt die Aufgabe `kamera-code`. Getrennte Kameras (Option B) sind als optionales Zukunfts-TODO erfasst und keine Voraussetzung für diesen Stand.

**Bewertete Optionen**

| Option | Vorteile | Nachteile |
|---|---|---|
| A: Gemeinsame Kamera wie im lokalen Coop | Entspricht dem Spieldesign: Zoom, Bewegungsgrenze und Spawnen an den Rändern passen ohne Änderung. Läuft heute schon mit dem Spike-Patch. Kein zusätzlicher Netzwerkverkehr | Beide Spieler sind aneinander gebunden wie am selben Bildschirm. Die Client-Kamera ist eine eigene Berechnung und kann leicht abweichen. Wegen der Latenz sieht der Client den Rand anders als der Host |
| B: Getrennte Kameras je Spieler | Volle Freiheit, jeder sieht seine Umgebung, mehr Platz auf der Karte | Bewegungsgrenze abschalten, Kamera beim Client und beim Host je Spieler neu bauen. Spawnen an den Rändern kennt nur den Host-Viewport, der Client müsste seine Ansicht melden (eigener Sync), oder das Spawnen bekommt einen eigenen Ersatz. Gegnerzahl und Schwierigkeit sind für einen gemeinsamen Bereich ausgelegt |
| C: Gemeinsame Kamera mit gelockerter Grenze | Wenig Aufwand: Restrictor-Zonen und `ZoomOutDistance` anpassen, mehr Abstand möglich | Bleibt ein gemeinsames Bild. Stärkeres Herauszoomen macht alles kleiner, Spawnen und Sichtbarkeit bleiben an das gemeinsame Bild gebunden |

**Empfehlung:** Option A für den ersten spielbaren Stand. Sie ist schon lauffähig, braucht keine neuen Systeme und passt zu Spawnen, Zoom und Bewegungsgrenze des Spiels.
Den Abgleich der Client-Kamera mit der des Hosts nur angehen, falls die Abweichung im Spiel stört. Option C kann danach als Feinschliff kommen. Option B erst, wenn das Team ausdrücklich freie Bewegung will, denn sie zieht einen eigenen Sync der Viewports nach sich.

### ViewTarget beim Client (`kamera-viewtarget`)

Stand: Spieltest am 04.11.2026, Build `26187885`, ein PC über Loopback, Route über das Spielmenü mit Seed `20795`.

- **[Belegt]** Vorher schaute der Client in der Mission nicht durch die gemeinsame Kamera, sondern aus der eigenen Figur: ViewTarget des lokalen Controllers war `BP_DD_Character_C`, die Blickposition lag 64 Einheiten über der Figur. Im Bild sah man nur Boden aus der Nähe. Der Host schaute die ganze Zeit korrekt durch `BP_PlayerCameraActor_C`.
- **[Belegt]** Ursache im Spike: `ViewThroughSharedCamera` nahm den ersten `BP_DDPlayerController_C` im Objekt-Array. Kurz nach einem Kartenwechsel lebt der alte Controller ohne `LocalPlayer` noch und steht davor. Die 15 Wiederholungen landeten an ihm, das Log meldete trotzdem „Kamera: gesetzt“. Danach griff auch `!cam` nicht mehr, weil nur eine neue Kamera die Wiederholungen neu startete.
- **Fix in `Camera.cpp`:** nur Controller mit `LocalPlayer` (`+0x348`) und die Kamera aus `PersistentLevel` der aktuellen Welt. Jede Sekunde prüft der Spike das ViewTarget (`PlayerCameraManager+0x340`) und setzt es nur, wenn es nicht die Kamera ist. Das Log nennt das vorherige Ziel.
- **[Belegt]** Mit dem Fix: Je Weltwechsel genau ein „Kamera: gesetzt, vorher BP_DD_Character_C“, also beim Betreten von MetaBase, Tag 1 und Tag 2. Danach blieb das ViewTarget die Kamera. Die Blickposition des Clients war gleich der des Hosts, die Bilder fast gleich. Lief die Client-Figur 2,5 s, folgte die Client-Kamera ruhig und lag höchstens rund 10 Einheiten neben der des Hosts.
- Nicht geprüft: weitere Tage, Internet, Mission → MetaBase.

### Client-Absturz im Restrictor (`tick-absturz`)

Stand: Disassembly und Spieltest am 02.11.2026, Build `26187885`.

- **[Belegt]** `0x5A17ACC` liegt in `UInViewportMovementRestrictor::PreProcessMovementInput` (`0x5A17AB0`), der Implementierung von `IMovementPreProcessorInterface` (Interface bei `+0xC0`). Die Funktion liest `this+0x50` über die Interface-Basis, das ist `MissionGameState` (`+0x110`). Danach vergleicht sie `PlayerArray.Num` (`GameState+0x2C8`) mit 1 und bremst nur bei mehr als einem Spieler. Eine Null-Prüfung fehlt. Ist `MissionGameState` null, gibt das genau den gemeldeten Lesefehler an `0x2C8`.
- **[Belegt]** Der Restrictor setzt `MissionGameState` nur einmal, in `BeginPlay` (`0x5A03C20`, Vtable-Slot 129): Er castet `World->GameState` auf `ADDMissionGameState`, und ist der GameState leer oder hat eine andere Klasse, bleibt das Feld null. Vorher hat er sich schon beim `MovementInputPreprocessorSubsystem` angemeldet. Danach wird das Feld nie wieder aktualisiert.
- **[Belegt]** Die Funktion läuft nur bei einer Bewegungseingabe des lokalen Spielers, nicht im Tick. Aufrufkette laut Absturzbericht: Eingabe der Figur (`0x5A0B530`) → Subsystem (`0x5A178C4`) → `ProcessEvent` → `execPreProcessMovementInput` (`0x57525C0`) → `0x5A17AB0`.
- **[Belegt]** Die Kamera mit dem Restrictor (`BP_PlayerCameraActor_C`) ist auf beiden Seiten ein lokales Actor-Objekt in `PersistentLevel`, nicht repliziert (`Role` `Authority`). Der Client baut seinen Restrictor also selbst und braucht dafür seinen eigenen, replizierten GameState.
- **[Vermutung]** Ursache des einmaligen Absturzes: Auf dem Client lief `BeginPlay` des Restrictors einmal, bevor der GameState repliziert war. Dass Spiellogik beim Client vor dem GameState loslaufen kann, ist schon belegt: Beim zweiten Kartenwechsel entsteht die HUD vor ihm, deshalb hält der Spike sie zurück (`Widgets.cpp`). Der Absturz kommt dann erst mit der ersten Bewegungseingabe des Clients. In den Tests bekommt der Client selten Eingaben, das passt zum seltenen Auftreten.
- **[Vermutung]** Ein zweiter Weg zu null: Zerstört der Client seinen GameState, während die Mission weiterläuft, setzt die Garbage Collection den `TObjectPtr` auf null. Beobachtet ist das nicht.

| Test | Ergebnis |
|---|---|
| `MissionGameState` beim Client nach dem Start der `MetaBase`, nach der Reise über das Menü und nach vier weiteren `servertravel` in die Mission | Fünfmal gleich dem aktuellen GameState, kein Absturz. Ein später GameState trat dabei nicht auf (keine zurückgehaltene HUD im Log) |
| Feld beim Client testweise auf null gesetzt, 15 s ohne Eingabe | Kein Absturz |
| Danach eine Bewegungseingabe (`D`) im Client-Fenster | Absturz `EXCEPTION_ACCESS_VIOLATION reading address 0x2C8` bei `0x5A17ACC`, genau wie gemeldet |

Folgerung für die Mod: Den Absturz verhindert, wer beim Client `MissionGameState` des Restrictors nachträgt, sobald der GameState ankommt, ähnlich wie der Spike die HUD zurückhält. Ein Null-Schutz direkt in `0x5A17AB0` ginge auch, dann bremst der Restrictor aber bis zum Nachtragen nicht. Den späten GameState gezielt zu erzeugen, ist nicht gelungen; Netzwerk-Emulation ist im Shipping-Build nicht vorhanden (Vermutung, nicht geprüft).

## Lokales Coop

### Blueprint-Stichprobe (`blueprint-logik`)

**Nachtrag `blueprint-logik`, 03.11.2026:**

- **[Belegt, begrenzter Bytecode-Audit]** Alle 42 ausgewaehlten Funktionen auf Host und Client vollstaendig als Ausdruecke gelesen; absolute statische Sprungziele und 25 konstante Ubergraph-Einspruenge gueltig, identische Ergebnisse. Kein exportierter Funktionskoerper, keine vollstaendige Semantik aller Zweige.
- **[Belegt, kontrollierter Ereignistest]** Host-Kistenschaden erreicht Authority=true, `SpawnLoot` und FMOD-Aufruf. Client empfaengt `ShowEffect`, erzeugt im Test kein Loot. Client-ApplyDamage liefert 0 vor dem Schadensevent; kein erfundenes gemessenes Authority=false.
- **[Belegt]** Host-Airdrop zur Laufzeit nicht repliziert, kein zugehoeriger Client-Actor. Fall-Callback erreicht latente Fortsetzung/Sound; Oeffnungs-Callback erreicht Authority=true und einmal `OpenAirdropChoices`. Kein erfolgreicher Reward-Claim damit bewiesen.
- **[Belegt]** Frischer Explosions-Testeffekt: ApplyToSelf=false ohne native Eigenaenderung; true erreicht Add-/RemoveImpactActor. Fremditempfad erreicht dieselben Itemaenderungen. Beim Host und bei explizitem lokalem Client-Aufruf belegt, also keine reine kosmetische Wirkung.
- **[Belegt, kontrollierter Projektiltest]** Lokales SetBulletSize(2) aendert Skala 1/1/1 zu 1.5/1.5/1. Nur Host SetBulletSize(3): Host 2.25/2.25/1, Client weiterhin 1/1/1 ohne entsprechenden Callback. Nicht-Net-BulletSize und Actor-Replikation ersetzen keinen geprueften Groessensync.
- **[Belegt, lokale Ereignisse]** HUD-OnSecondChanged tritt auf beiden Seiten auf. Das Feld intseconds blieb bei explizitem Callback unveraendert; kein nachgewiesener visueller Setter. Alle Beobachtungen und Grenzen im [Ereignis-Nachtrag](../research/spikes/blueprint-logik/README.md#nachtrag-03112026-grenzpruefung-und-ereignistests). Die folgende Tabelle beschreibt den frueheren Referenzscan.

Mission-SDK und lesende Pruefung geladener Klassen/Scriptreferenzen, Build `26187885`:

- **[Belegt, SDK/Default]** `BP_EnemySpawner_C` hat keine eigenen Blueprint-
  Funktionen, erbt native Spawner-Logik und repliziert im Default nicht.
  `BP_Enemy_C` und `BP_Projectile_C` replizieren im Default.
- **[Belegt, Default/Flags]** `BP_PowerUpCrate_C` repliziert und hat einen
  verlaesslichen Multicast `ShowEffect`. `BP_Airdrop_Lootable_C` repliziert im
  Default nicht, obwohl `StartFallDown` als nicht verlaesslicher Multicast
  deklariert ist. Deklaration ist kein Zustellungsnachweis.
- **[Belegt, SDK/Default]** Explosivschaden- und Giftwolken-Itemeffekte teilen den
  Blueprint-Parent `BP_Effect_Add_ImpactEffect_Template_C`; die Default-Impact-
  Klassen unterscheiden sich. Explosion: `RadialDamageImpactActor`, nicht
  replizierend. Giftwolke: `AreaGameplayEffectImpactActor`, replizierend.
- **[Belegt, begrenzter Referenzscan]** Der gemeinsame Itemeffekt-Graph referenziert
  Add/RemoveImpactActor. Kisten-/Airdrop-Graphen referenzieren sowohl Spielwirkungen
  als auch Sound/Animation/Effekte. Das ist noch kein Kontrollflussbeweis.
- Eigene Verhaltensbeschreibungen, Sync-Folgerungen, Fehlansaetze und Grenzen:
  [Blueprint-Untersuchung](../research/spikes/blueprint-logik/README.md).
  Kein vollstaendiger Audit, keine neuen Treffer-/Event-Laufzeittests.

### Lokale Coop-Schnittstellen

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

### Coop-Ablauf

Stand: Disassembly und Laufzeit-Speicher, Build `26187885`, SDK aus einer direkt geladenen Mission (02.11.2026). Die RVAs stehen in [Offsets](OFFSETS.md#coop).

**Wie P2 entsteht**

- **[Belegt]** `SetCoopEnabled(b)` legt keinen Spieler an. Es setzt nur `bCoopEnabled` (`+0x180`), feuert `OnCoopStateChanged` und meldet den Zustand weiter.
  Einschalten geht nur, wenn eine Einstellung des Spiels Coop erlaubt. Ausschalten geht nur, wenn ein vorhandener P2 vorher entfernt werden kann.
- **[Belegt]** P2 entsteht nur über `SpawnLocalPlayer(1)`. Die Funktion prüft `0 <= Index < MaxLocalPlayers` und, für Index 1, `IsPlayerExpected`.
  Liefert `GetPlayerController(Index)` schon einen Controller, gibt sie diesen zurück und legt nichts an.
  Sonst ruft sie `UGameInstance::CreateLocalPlayer(Index, Error, bSpawnPlayerController=true)` auf. P2 ist also ein echter zweiter `ULocalPlayer`, die Engine spawnt seinen Controller über den normalen Login-Weg des `GameMode`.
  Danach trägt die Funktion ihn in `ActivePlayers` ein, setzt das Merkflag `+0x18A` („P2 nach Kartenwechsel neu anlegen“), feuert `OnPlayerJoined` und aktualisiert die Geräte.
  Für Index 1 übernimmt sie vorher die Vorschau-Figur als gewählte Figur von P2, falls noch keine gewählt ist.
- **[Belegt]** `SpawnLocalPlayer` hat drei Aufrufer: den Exec-Thunk (Blueprint), `ApplyDeviceAssignmentDraftAndJoin` und eine virtuelle Funktion von `ADDMissionGameMode` (VTable-Slot 111).
- **[Belegt]** `ApplyDeviceAssignmentDraftAndJoin` ist der Beitritt über die Geräte-Zuordnung: Ein Draft für P2 muss eine Eingabe haben. Dann setzt die Funktion `MaxLocalPlayers=2` und ruft `SpawnLocalPlayer(1)` auf.
  Scheitert das, stellt sie den alten Stand wieder her.
- **[Belegt]** Der `GameMode`-Slot 111 legt P2 nach jedem Kartenwechsel neu an, wenn `bCoopEnabled` und `+0x18A` gesetzt sind und `GetPlayerController(1)` nichts liefert.
  Wann der Slot läuft, ist nicht geprüft. Weil er den `GameState` liest, ist es vermutlich ein Start-Ereignis der Karte (**[Hypothese]**).
- **[Belegt]** `SetCoopEnabled` hat drei native Aufrufer:
  - Das Laden des Run-Zustands (`IRunPersistable` am Subsystem, VTable-Slot 91). Es setzt `MaxLocalPlayers=2`, stellt die Geräte-Zuordnung wieder her und übernimmt `bCoopEnabled` aus dem gespeicherten Run.
  - Ein Moduswechsel, der sich `bCoopEnabled` merkt, P2 entfernt und `SetCoopEnabled(false)` aufruft.
  - Sein Gegenstück, das `RestoreNormalSaveSlot` aufruft und Coop danach wieder einschaltet.

  Vermutlich ist das der Wechsel in einen Modus mit eigenem Spielstand-Slot, etwa die wöchentliche Herausforderung (**[Hypothese]**).
- **[Belegt]** In den Blueprint-Klassen dieses Mission-Dumps ruft keine `SetCoopEnabled`, `SpawnLocalPlayer` oder `SetMaxLocalPlayers` auf, nur Abfragen wie `IsCoopEnabled` und `GetPlayerController`.
  Der Beitritt läuft also in Widgets des Menüs oder der `MetaBase`, die in diesem Dump nicht geladen sind.

**Controller ohne lokalen Spieler**

- **[Belegt]** Beim Host steht im Feld `Player` (`+0x348`) des Remote-Controllers die `IpConnection`. `GetLocalPlayer()` liefert also `null`.
  Im laufenden Spike gilt auf beiden Seiten `bCoopEnabled=0`, `MaxLocalPlayers=2`, `+0x18A=0`.
  In `ActivePlayers` des Hosts steht nur ein veralteter schwacher Verweis aus der vorigen Karte. Das Subsystem lebt in der GameInstance und überdauert den Kartenwechsel.
- **[Belegt]** Diese Funktionen kommen ohne lokalen Spieler zurecht:
  - `GetPlayerIndex` und `IsSecondaryPlayer` fallen auf `UGameplayStatics::GetPlayerController(World, 0..1)` zurück. Beim Host liefert das für den Remote-Controller Index 1.
  - `GetPlayerController(Index)` sucht zuerst in `ActivePlayers`. Danach liefert es Index 0 immer über `GetPlayerController(World, 0)`, Index 1 nur bei `bCoopEnabled` oder gesetztem `+0x18A`.
- **[Belegt]** Im Spielcode gibt es 25 Aufrufe von `GetLocalPlayer()`. Fünf davon nutzen das Ergebnis ohne Prüfung:
  - `GetLocalPlayerIndex` (`0x5B0A2E0`) und das Spawnen der Figur im `GameMode` (`0x5901CE3`, Slot 234) sind die beiden bekannten Host-Abstürze, der Spike patcht sie.
  - Die übrigen drei liegen in der Figuren- und Haustier-Auswahl: `PetSelectionChanged` (`0x584C3B1`) und zwei Helfer (`0x583F364`, `0x584CD7D`).
    Dort wird der Index über `GetLocalPlayer()->GetControllerId()` bestimmt, ein Remote-Controller ließe das Spiel abstürzen (**[Hypothese]**, nicht ausgelöst).
- **[Belegt]** Die Figurenwahl hängt am Index des lokalen Spielers: Der `GameMode` holt die Figur in Slot 234 über `GetLocalPlayer()` und dessen Controller-ID. Fehlt er, nimmt der Spike-Patch den Standard-Pfad mit der Standardfigur.

**Folgerungen** (**[Hypothese]**, nicht getestet)

- `SpawnLocalPlayer(1)` darf beim Host nicht laufen, solange P2 ein Netzwerkspieler ist. Sonst entsteht ein zweiter lokaler Spieler neben dem Remote-Controller.
  Der Weg über Geräte-Zuordnung und `GameMode`-Slot 111 bleibt aus, solange `+0x18A` nicht gesetzt ist.
- Mit `SetCoopEnabled(true)` beim Host liefert `GetPlayerController(1)` den Remote-Controller, weil der Rückgriff auf `GetPlayerController(World, 1)` greift. Das ist der wahrscheinlich kleinste Weg, ihn als P2 in die Coop-Logik zu bringen.
  Zusätzlich müsste die Mod ihn in `ActivePlayers` eintragen, ohne `CreateLocalPlayer` aufzurufen. Zu prüfen ist, was `bCoopEnabled` sonst noch einschaltet, etwa Coop-UI und Eingabe-Router.
- Beim Client ist die eigene Figur lokaler Spieler 0, er hält sich also für P1. Die Felder für P1 und P2 (`CoopP1Field`, `P2Backpack`) müssen beim Client daher umgedreht oder von der Mod gesetzt werden.
- Die Figuren- und Haustierwahl des Clients braucht einen eigenen Sync. Ohne lokalen Spieler beim Host bekommt P2 nur die Standardfigur.

## Run-Ablauf

Stand: Laufzeit-Trace und Disassembly, Build `26187885`. Der Ablauf gilt für zwei Instanzen mit dem [Spike](../research/spikes/a-connect/README.md). Ohne dessen Patches stürzt der Host schon beim Spawn ab.

### Wege zwischen den Karten

| Schritt | Auslöser | Weg im Spiel | Ergebnis mit Client |
|---|---|---|---|
| Menü nach `MetaBase` | Menüpunkt | Lokale Reise | Client folgt nur per `servertravel` |
| `MetaBase` in Mission | Figur läuft auf den Ring links neben dem Bus, Klick auf die Routenkarte | `FinalizeRunStart`, `LoadMapByInformation`, `ULoadingSubSystem::SeamlessTravelToLevel` | Beide in `DDDynamic_Map`, Gegner laufen von selbst |
| Missionsende | Bus oder `FinishMission` | `RoundEndPanel`, Lager, Routenkarte | Nur beim Host sichtbar, der Client spielt weiter |
| Mission in nächste Mission | Klick auf die Routenkarte | Wieder `SeamlessTravelToLevel` | Mit Inventar-Teilfix erreichen beide Tag 2, die Reise auf Tag 3 endet mit einem Host-Absturz |
| Mission ins Hauptmenü | Pausemenü, `ReturnToMainMenu` | `SetClientTravel` | Mit Inventar-Teilfix beide sauber im Hauptmenü. Ohne ihn stürzt der Client beim Trennen ab, siehe [Trennen](#trennen-ins-hauptmenü-trennen) |

`SeamlessTravelToLevel` reist nur lokal. Der Spike leitet die Reise in `servertravel <Karte>?listen` um, damit der Client folgt. Damit entfällt der Ladeablauf des Spiels, die Mission lief trotzdem normal.
Die Umleitung gilt nicht für das Hauptmenü: Dort stürzte der Host mit Umleitung ab, im Original nicht. Eine Rückkehr in die `MetaBase` nach Run-Ende ist noch nicht getestet.

`TriggerEndRound` meldet das Rundenende nur an die Persistenz und löst keine Reise aus. `FinishMission` zeigt das normale Tagesende, auch wenn vorher `Lose` gesetzt wurde. Der echte Weg bei einer Niederlage ist also noch offen.

### Trennen ins Hauptmenü (`trennen`)

Stand: Disassembly und zwei Spieltests am 02.11.2026, Build `26187885`.

- **[Belegt]** Der Client-Absturz bei `0x5975A6A` ist eine Folge der fehlenden Inventar-Items, keine eigene Ursache beim Trennen. Er trat nur in Builds ohne den [Inventar-Teilfix](../research/spikes/a-connect/README.md#inventar-ursache-teilfix-und-verbleibende-grenze) auf.
- **[Belegt]** Aufrufkette laut Absturzbericht: Der Client bekommt die Trennung und baut seine Welt ab. Dabei ruft `AActor::EndPlay` (`0x25E3350`) für jede gestartete Komponente `EndPlay` auf (Vtable `+0x3F0`). `UInventoryComponent::EndPlay` (`0x595BBD0`) läuft über `MyItems` (`+0x120`) und ruft für jedes Item `0x5975A40` auf.
- **[Belegt]** `0x5975A40` geht durch `UInventoryItemInstance::StateObjectLookup` (`TMap` bei `+0x318`) und ruft für jedes Zustandsobjekt die virtuelle Funktion `+0x2A0` und `0x572FBE0` auf. Eine Null-Prüfung des Items fehlt. Die erste Anweisung liest `Item+0x340`, die Bitanzahl der Belegungsbits der Map. Bei einem Null-Item ergibt das genau den gemeldeten Lesefehler an Adresse `0x340`.
- **[Belegt]** Dieselbe Schleife steckt in `RemoveAllItemEffects` (`0x5971BB0`). Das ruft unter anderem `LoadMapByInformation` (`0x5B11170`) für jeden Spieler auf. Fehlende Items können also auch dort abstürzen.

| Test | Client-Inventar in der Mission | Host über Pausemenü ins Hauptmenü |
|---|---|---|
| Spike mit Inventar-Teilfix | `complete` und `equal` wahr | Beide Instanzen landen sauber in `MainMenu`, kein Absturzbericht |
| Gegenprobe ohne Teilfix, lokal gebaut | `complete` und `equal` falsch | Client stürzt ab: `EXCEPTION_ACCESS_VIOLATION reading address 0x340` bei `0x5975A6A`, Rücksprung `0x595C039` in `UInventoryComponent::EndPlay` |

Folgerung: Kein eigener Null-Schutz nötig. Der Absturz verschwindet mit dem Teilfix, weil der Client dann echte Items hat. Fehlt ein Item trotzdem einmal, etwa auf Tag 3 oder bevor die Replikation fertig ist, stürzt der Client an dieser Stelle wieder ab.

Nicht getestet: Trennen an Tag 2 oder später, Trennen aus der `MetaBase`, Abbruch der Verbindung ohne Menü (Host beendet oder Netzwerkfehler). Mit Teilfix gab es nur einen Lauf.

### Was beim Kartenwechsel passiert

- **Controller:** Die Engine ersetzt beide Controller. In `SpawnedPlayers` bleibt der alte Remote-Controller stehen, der neue kommt nie hinein. Nach der Garbage Collection steht dort `null`, und der Gegner-Spawner wählt daraus zufällig ein Ziel. Das erklärt einen nur manchmal auftretenden Host-Absturz.
  `spawnedplayers`, 03.11.2026: Der Spike entfernt jetzt alte/null/doppelte belegte Einträge unter Erhalt von Hash und SparseArray-Buchhaltung und fügt aktuelle Controller über den nativen TSet-Helfer ein. Allokationsbits statt Zeiger-Sentinel; Prüfung im Game-Thread-Sekundentick. Das lesende `players.py` prüft Belegung, Pointer-Hash, Hash-Ketten und exakte PlayerArray-Übereinstimmung. Zwei direkte Missionstravels mit dem isolierten Fix waren konsistent; der bekannte Host-Callback `0x57CC398` trat beim folgenden Wechsel weiterhin auf. Einzelheiten und Testgrenzen im [Spike](../research/spikes/a-connect/README.md#spawnedplayers-echte-mengeneinträge-spawnedplayers).
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

#### Tag-3-Callback: Lebensdauer des Achievement-Detektors (`tag3`)

Stand 03.11.2026, Build `26187885`. Kein weiterer Null-Schutz in Inventar-Schleifen.

- **[Belegt, Disassembly und Laufzeit-Vtable]** Der einzige RIP-relative Verweis auf `0x57CC390` im untersuchten Codebereich kommt aus `0x57CB220`. Diese Funktion steht bei `HitByEnemyTypeAchievementDetectorComponent` im BeginPlay-Slot `+0x3E8`. Sie erzeugt einen Next-Tick-Timer mit einem **rohen Component-Zeiger** als TFunction-Capture. Der Callback liest diesen Zeiger und ruft virtuell `+0x490` auf.
- **[Belegt, Disassembly]** Der dazugehörige EndPlay-Override `0x57D0A40` ruft die Basisklasse und räumt eine Delegate-Bindung auf. Den Next-Tick-Timer räumt er nicht auf. Ein UObject-gebundenes Delegate, dessen Lebensdauer die Engine prüfen könnte, wird für diesen Callback nicht benutzt.
- **[Belegt, Gegenprobe]** Mit korrigierter `SpawnedPlayers`-Menge, aber unverändertem Timerpfad, reproduzierte ein direkter Wechsel nach zwei Missionstravels den Host-Absturz `0x57CC398`, Stack danach `0x315A764`. Eine intakte Controller-Menge alleine reicht also nicht.
- **[Umgesetzt]** `Inventory.cpp` bewahrt nur für diesen Callsite den echten zurückgegebenen TimerHandle und TimerManager auf. Beim EndPlay dieser Component wird der Handle über den nativen FindTimer-Pfad geprüft und ein noch lebender Timer vor dem ursprünglichen EndPlay gelöscht. Die ursprünglichen BeginPlay-/EndPlay- und Callback-Inhalte bleiben erhalten; keine Objekt-Nullprüfung im abstürzenden Callback, kein Abschalten des Achievement-Systems, keine neue Inventar-/Outer-Manipulation.
- **[Belegt, begrenzter Laufzeittest]** Byte-/Vtable-Prüfungen bestanden, Timer registriert und bei EndPlay wiederholt tatsächlich abgeräumt. MetaBase → Mission → Mission → MetaBase blieb verbunden; die Inventare waren nach dem zweiten Missionstravel vollständig und identisch. Beim folgenden Wechsel in die Mission blieb der Host am Leben, aber der Client stürzte neu bei `0x178BA90` ab (Lesen von `0x8C`, danach `0x58B41AB`, `0x58A6065`). Im lokalen Integrationstest mit `spawnedplayers` erneut reproduziert.
- **[Statisch eingegrenzt, nicht behoben]** Der neue Client-Stack führt über die Spawner-Initialisierung `0x58A58E0` nach `0x58B4160`. Dessen Schleife prüft die Figur, aber nicht deren `AttributeSet` bei `Character+0x7D8`; sie übergibt `AttributeSet+0x80` an den Getter `0x178BA90`, der nochmals `+0xC` liest. Ein fehlendes AttributeSet erklärt exakt `0x8C`. Ob Replikationsreihenfolge oder ein falscher lokaler Initialisierungspfad zugrunde liegt, ist offen. Kein weiterer Null-Schutz eingefügt.

**Einordnung:** Die Lebensdauerhypothese ist durch Capture, fehlenden Cleanup und den begrenzten Vergleich stark gestützt. Kein echter Menü-/Tagesablauf bis Tag 3 oder vollständiger Run mit dem neuen Fix belegt; die Inventar-Aufgabe bleibt offen. Direkte `servertravel`-Stresstests sind keine Tageswechsel über die Host-Route. Folgeabstürze und mögliche weitere unsichere Timer bleiben offen.

#### Echte Route bis Tag 4 (`tag3`, Übergabestand)

Stand 03.11.2026, Build `26187885`. Getestet nur über `Start-Session.ps1`: Menü → MetaBase → Run → Routenkarte, je Tag Knoten wählen, `!finish`, Lager bestätigen. Zwei Instanzen, Loopback.

- **[Belegt] Host-Absturz bei der Reise auf Tag 3 war nicht der Timer.** Mit Timer-Fix stürzte der Host auf der echten Route weiterhin ab. Ursache ist ein Lambda-Delegate des `AMapGenerator` (Vtable `0x9613280`, Execute `0x59CAE70`, ExecuteIfSafe `0x59CB020`). Es speichert einen rohen Generator-Zeiger bei `+0x18` und `&CurrentGenData` bei `+0x20` und ruft den Generierungsschritt `0x59D9090` ungeprüft auf. Nach der Reise feuert es für den Generator der alten Karte.
- **[Umgesetzt] `Inventory.cpp`:** Beide Vtable-Einträge prüfen, ob der Generator noch in `GUObjectArray` steht und zur aktuellen Welt gehört (Outer = Level, dessen Outer = `GWorld`). Sonst wird nur dieser Schritt ausgelassen und geloggt (`MapGen step: generator of an old world`).
- **[Gescheitert, korrigiert]** Die erste Fassung der Prüfung las den alten Generator ohne Schutz. Auf dem Weg zu Tag 4 war dessen Speicher schon freigegeben, und die Prüfung selbst stürzte beim Host ab. Jetzt fängt SEH eine Zugriffsverletzung ab und wertet sie als „alt“.
- **[Umgesetzt] `Client.cpp`, Absturz `0x178BA90`:** `ReadyExpAggregate` ersetzt den Einstieg `0x58B4160`. Fehlt bei einer Figur noch das AttributeSet (`Character+0x7D8`), gibt sie den bisherigen Wert bei `+0x314` zurück; der nächste Aufruf versucht es erneut. Keine Figuren übersprungen, keine Attribute erfunden. Die Entscheidung ist ohne Spiel getestet (`tests/Run-AttributeReadiness.ps1`). Auf der Route trat `0x178BA90` nicht mehr auf, im Log steht nur einige Male „awaiting character AttributeSet“.
- **[Belegt, zweimal] Tag 3 erreicht.** Beide Läufe kamen über die echte Route auf Tag 3, mit je genau einem ausgelassenen Generator-Schritt. `inventory.py` ergab an Tag 1, 2 und 3 `complete: true` und `equal: true`. Beim Host gab es danach keinen Absturz mehr.
- **[Belegt] Client-Abstürze an Tag 4**, jeweils direkt nach `OnRep_DayNum`. Der Client kennt dort Objekte nicht, die nur der Host hat:
  1. `0x4B68195`, Lesen `0x1080`, über `0x5847AEB`. Die AbilitySystem-Suche `0x4B68180` bekommt einen leeren Besitzer. Behoben mit `PatchDayAttributeLookup`: bei leerem `rcx` den Rest bis `0x5847AF2` auslassen. Die Funktion hat `+0xC8` zuvor schon geleert, der nächste Aufruf versucht es also erneut.
  2. `0x59DFD61` über `0x59D8194` (ProcessEvent). Ein Kartengenerator-Schritt `0x59DFC70` nutzt `+0xC8`, das es nur beim Host gibt. Behoben mit `PatchMapGenHostObject`: bei null zum vorhandenen Ausgang `0x59DFE07`.
  3. **Offen:** wieder `0x4B68195`, Lesen `0x1080`, diesmal über die Rücksprungadresse `0x59A4B90`. Der Aufruf liegt vermutlich bei `0x59A4B8B`. Stack: `0x59A4B90`, `0x25DDABB`, `0x2D8CBEC`, `0x5801443`, `0x25E19AD`, `0x323576A`, `0x2A842DC`. Kurz davor im Client-Log: „Exp aggregate: awaiting character AttributeSet“ und die Übernahme von `MGP_Forest_Roadstops` durch den Kartengenerator. Die Patches 1 und 2 waren aktiv und meldeten keine Byte-Abweichung, ihre Abstürze traten in diesem Lauf nicht mehr auf. Belegt ist das nur mit einem Lauf.

**Weiter in [Tageswechsel](#tageswechsel-client-verliert-die-playerstates-tageswechsel-übergabestand).** Die drei Abstürze waren Folgen davon, dass dem Client ab Tag 4 die PlayerStates fehlen.

**Abnahme:** Ein Run über die echte Route durch alle 8 Tage, mit dem Gebietswechsel nach Tag 4 (gestreifte Knoten, vermutlich Torboss) und dem Endboss am Ende. Tag 5 liegt auf der Routenkarte bei x≈400. Danach den Endlosmodus starten und über mindestens zwei Gebietswechsel weiterfahren. An jedem Tag `inventory.py` mit `complete` und `equal`, kein Absturz auf beiden Seiten.

**[Belegt, SDK] Endlosmodus**, nur statisch aus dem Dump, im Spiel nicht geprüft:

- `UEndlessmodeSubsystem` ist ein GameInstance-Subsystem mit `StartEndlessMode`, `ResetEndlessMode`, `IsEndlessModeActive` und `IsEndlessModeUnlocked`. Die Freischaltung hängt an einem Content-Locker, der Zustand an der Run-Persistenz.
- Tagesinhalte werden über den Tag innerhalb der Karte und `bIsEndlessMode` bestimmt, etwa Spezialruns und `RequiresGatewayBossForDay`. `GetNumDaysPerMissionMap` gibt die Tage je Karte vor.
- Gegnerwerte skalieren mit dem absoluten Tag (`CalculateScaledValueForAbsoluteDay`). Ein Fix darf deshalb weder eine Obergrenze von 8 Tagen noch feste Tageswerte annehmen.
- Offen: ob die Test-Spielstände den Endlosmodus freigeschaltet haben und wie der Wechsel vom Endboss in den Endlosmodus beim Client ankommt.

**Nicht geprüft:** Tag 5 und später, Endlosmodus, Client-Inventarbedienung, Belohnungen mit mehreren Items, Internet. Ob Patch 1 und 2 nur Symptome abfangen oder der Client dort grundsätzlich einen Host-Pfad ausführt, ist offen.

#### Tageswechsel: Client verliert die PlayerStates (`tageswechsel`, Übergabestand)

Stand 03.11.2026, Build `26187885`. Drei Läufe über die echte Route mit Seed `20795` und den Knoten (62, 213), (160, 213), (255, 213), (352, 112), (448, 230), Ablauf wie `Travel-Day.ps1`. Gestartet nur über `Start-Session.ps1`, zwei Instanzen, Loopback.

- **[Umgesetzt] Absturz 3 behoben, Patch 1 ersetzt.** `PatchAttributeLookupEntry` prüft am Anfang der AttributeSet-Suche `0x4B68180` selbst: Ist `rcx` null, gibt sie null zurück, wie für ein fehlendes Set. Das prüfen die Aufrufer schon. Damit sind alle Aufrufer gedeckt, auch `0x59A4B90`, und der tagesgebundene `PatchDayAttributeLookup` (Call `0x5847AE6`) entfällt. In allen drei Läufen gab es an Tag 4 keinen Absturz mehr bei `0x4B68195`.
- **[Belegt, drei Läufe] An Tag 4 hat der Client 0 PlayerStates**, der Host 2. An Tag 1 bis 3 sind beide Seiten vollständig, mit `complete` und `equal`. Das Client-Log zeigt dabei `NewActor failed: ... archetype=null` für die PlayerState-Kanäle. An Tag 5 stürzt der Client ab (`0x56B16D9`, Lesen `0x3D0`, über `0x58CED80`, `0x58C9311`, `0x58B4F08`), vermutlich als Folge davon. Die Abstürze 1 bis 3 aus `tag3` waren ebenfalls Folgen: Code, der beim Client einen PlayerState erwartet.
- **[Belegt] Ablauf beim Client.** `SerializeNewActor` (`0x2D09B90`) löst den Archetyp über eine NetGUID auf. Bei `0x2D0A426` ist er null, der Actor wird nicht angelegt. `UActorChannel::ProcessBunch` setzt den Kanal dann bei `0x2995F7B` auf Broken und schickt `NMT_ActorChannelFailure` (`0x29A2310`). Der Host ignoriert das, der Actor fehlt für die ganze Karte.
- **[Belegt] Im NetGUID-Cache des Clients fehlen an Tag 4 genau zwei Einträge:** `0x125` (`Default__BP_DDPlayerState_C`, der Archetyp) und `0x127` (Paket `/Game/DDSurvivors/Blueprints/Player/BP_DDPlayerState`). An Tag 1 bis 3 sind beide da, ihre Nachbarn `0x123` und `0x129` bleiben auch an Tag 4. Der Host hat beide Einträge weiter. Der Host schickt für bekannte GUIDs nur die Nummer, also kann der Client den Archetyp nicht mehr über den Pfad finden. Zwischen Tag 3 und 4 sank die Zahl der Einträge beim Client von 7257 auf 7055. Werkzeug: `tools/spike/guidcache.py <pid> DDPlayerState`.
- **[Belegt] Der Client lädt die PlayerState-Klasse beim Wechsel von Tag 1 auf 2 neu** (neuer Objektindex), der Host behält sie. Danach blieb sie in einem Lauf bis Tag 4 dieselbe, die Einträge fehlten trotzdem. Das Entladen der Klasse allein erklärt den Verlust also nicht.
- **[Gescheitert] Klassen im RootSet halten.** Jede Klasse, die der Client über das Netz anlegt, im RootSet halten, änderte nichts: Die Einträge fehlten weiter.
- **[Gescheitert] Ablauf in `CleanReferences` abschalten.** `FNetGUIDCache::CleanReferences` (`0x2CF34B0`) stempelt Einträge mit ungültigem Objekt und löscht sie 90 s später bei `0x2CF363F`, ohne erneut zu prüfen. Ein Patch bei `0x2CF362E`, der beim Client statische GUIDs behielt und nur den Stempel löschte, war aktiv und änderte nichts. Er ist wieder ausgebaut. Die Einträge gehen also auf einem anderen Weg verloren.
- **[Gescheitert] Diagnose an `ObjectLookup.Remove`.** `0x2D05470` hat nur zwei Aufrufer, `0x2D04C2D` und `0x2D04CB9`, beide in `0x2D04BE0` (Registrierung einer GUID für ein Objekt, entfernt vorher alte Einträge zur GUID und zum Objekt). Ein Log-Hook für statische GUIDs meldete in einem ganzen Lauf bis Tag 4 nichts.
- **[Gescheitert] Schnelltest mit `servertravel`.** Eine Schleife aus `servertravel /Game/Maps/DDDynamic_Map?listen` mit `gc.CollectGarbageEveryFrame 1` beim Client zeigte den Fehler auch ohne jeden Fix nicht. Sie bildet den Tageswechsel also nicht nach und taugt nicht als Testumgebung.

**Offene Spuren:** Weitere Wege, auf denen Einträge verschwinden oder unauffindbar werden: das Einfügen mit Ersetzen bei `0x2CE9D42` (in `0x2CE9BA0`, aufgerufen bei `0x2D04B94`), eingebettete Löschungen außerhalb von `0x2D05470`, ein Neuaufbau des ganzen Cache oder ein geänderter Schlüssel. Der Schlüssel wird mit `0x3FFFFFFFFFFFFF` maskiert, die Rohwerte der Slots um `0x125` sind noch nicht angesehen. Offen ist auch, ob Tag 4 am Tag selbst oder an der Laufzeit seit dem ersten Wechsel hängt.

**Nachtrag `testumgebung` (04.11.2026):** Die Läufe oben hatten einen kaputten Ladeablauf. Ab Tag 2 blieb die Welt pausiert, und die Figur des Clients wurde am Busplatz zerstört, siehe [Pausierte Welt und Busplatz](#pausierte-welt-und-busplatz-testumgebung). Mit beiden Korrekturen fehlten die PlayerStates an Tag 4 in keinem Lauf mehr, auch `0x125` und `0x127` blieben im Cache. Der Verlust hing also sehr wahrscheinlich am kaputten Laden. Offen bleibt der Client-Absturz an Tag 5 bei `0x56B16D9`, er kommt auch mit sauberem Laden. Das übernimmt `guidcache`.

#### Pausierte Welt und Busplatz (`testumgebung`)

Stand 04.11.2026, Build `26187885`, Route und Seed wie oben, Läufe mit `tools/spike/Run-Route.ps1`. Läufe liegen lokal unter `research/local/session/runs/`.

- **[Belegt] Ab Tag 2 war die Welt pausiert.** `AWorldSettings.PauserPlayerState` (`+0x4A8`) war gesetzt und wird zum Client repliziert. Die Mission startete nie, beide Figuren standen am statischen `PlayerStart` (3467, -399, 952, auf einem Dach), die Spielzeit blieb bei 00:00. Das war das „seltsame Laden“: Die Figuren erschienen irgendwo, bevor die Zone fertig aussah.
- **[Belegt] Ursache: liegengebliebene Zeitanfragen.** `FinishMission` legt im `UTimescaleManager` eine Anfrage des GameState an (Priorität 20, Pause). Normal räumt der Ladestart `0x595C910` aus `SeamlessTravelToLevel` (`0x5976AD0`) mit `ClearAllRequests` auf. Die Spike-Umleitung ersetzt diese Reise durch `servertravel` und übersprang damit das Aufräumen.
- **[Umgesetzt] Spike räumt vor der Reise auf.** `Travel.cpp` ruft vor dem `servertravel` `ClearAllRequests` mit leerer Ausnahmeliste auf, das Log meldet „Reise: Zeitanfragen abgeraeumt 2 -> 0“. Danach lief an jedem Tag die Spielzeit, und keine Seite war pausiert.
- **[Belegt] Die Figur des Clients wurde am Busplatz zerstört.** Beim Rundenstart setzt `BusSpawnPositionGenerator` (`0x59A6CE0`) Bus und Figuren an den Spawnplatz. Danach räumt `0x5991B70` alles weg, was die gesetzten Actors überlappt. Die Figur des Remote-Spielers stand auf derselben Stelle wie die Host-Figur, war aber nicht in der Schonliste. Sie bekam `EndPlay` mit Grund 0 (Destroyed), Stapel `0x5992238`, `0x59A7DDE`, `0x59D9223`. Der Client stand danach ohne Figur da.
- **[Umgesetzt] Busplatz schont alle Spielerfiguren.** `TestEnv.cpp` hängt sich an `0x5991B70` und ergänzt die Schonliste um die Figuren aller PlayerStates (`PawnPrivate`). Log: „Busplatz: 1 geschuetzt, 1 Spielerfiguren ergaenzt“. Für die Mod gehört das in den Sitzungsteil beim Host.
- **[Belegt, zwei Läufe] Ergebnis.** Mit Tutorial-Sperre, Aufräumen und Busplatz-Hook waren Tag 1 bis 4 in beiden Läufen sauber: beide Seiten 2 PlayerStates, 2 Figuren am Bus, gleiche Zahl an `MapTile`s, keine Pause, NetGUID-Cache wachsend (2248, 3339, 5435, 5620 und 2260, 3374, 5489, 5572), kein `NewActor failed`. An Tag 5 stürzt der Client in beiden Läufen bei `0x56B16D9` ab (Lesen `0x3D0`, Stapel `0x58CED80`, `0x58C9311`, `0x58B4F08`).
- **[Hinweis] Ring in der `MetaBase`.** Der Startring löst nur bei einer durchgehenden Bewegung hinein aus, nicht bei mehreren kurzen Tastendrücken. Steht der Host schon im Ring, muss er erst hinaus. `Start-Run.ps1` liest dafür die Position und versucht es bis zu dreimal.

#### Tag-5-Absturz: lokaler Blockaden-Ablauf auf dem Client (`guidcache`)

Untersuchung am 04.11.2026, Build `26187885`, eingefrorene Entwicklungskopie. Eigene Sitzung mit `Start-Session.ps1 -Build`/CrashGuard und getrennten Test-Spielständen, Seed `20795`, echte Route. Tag 1 bis 4 erneut sauber geprüft; beim nächsten Knoten (448, 230) derselbe Client-Absturz: `0x56B16D9`, Lesen `0x3D0`. Keine fehlenden PlayerStates bis Tag 4. Der automatische erste Lauf erreichte wegen einer zu kurzen äußeren Laufzeitgrenze nur die Rundenübersicht an Tag 4; Fortsetzen/Lager und die Tag-5-Auswahl wurden danach in derselben Sitzung manuell über die Testskripte bedient.

- **[Belegt, dynamisch mit REA/Frida]** Auf dem Client startet ein `HighwayBlockageEventComponent` den Ablauf `0x58B4E40` → `0x58C9180` → `AEnemySpawner`-Spawn-Schleife `0x58CECE0`. Welt und NetDriver sind vorhanden, `ServerConnection` ist gesetzt, der World-GameMode und `EnemySpawner.GameMode` (`+0x3C8`) sind null. Der Host führt Spawns mit gültigem GameMode aus. Client-Beobachtung: Evidence `ev_86713c41335be51388d87208e5e66f66aedd61b01bc2ddf8e6c327db63930209`; Host-Gegenbeobachtung `ev_ccc1b3ccf1407d9ec7b406d4ec90dbd7fca53016733c8761ce43d0f10bfd984c` (begrenzte Aufzeichnungsmenge). Rohdaten bleiben privat.
- **[Belegt, Disassembly]** Bei `0x58CED42` wird der GameMode des Spawners geladen. Ohne Nullprüfung addiert `0x58CED70` dessen `SpawnedPlayers`-Offset `0x388` und ruft die Mengen-Kopie `0x56B16C0`. Deren Zugriff `Quelle+0x48` bei `0x56B16D9` ergibt bei null genau `0x388+0x48=0x3D0`. Der ältere Nullschutz bei `0x58CEDC7` liegt erst dahinter und schützt nur einen ausgewählten Controller, nicht den GameMode.
- **[Belegt, Reflection und Disassembly]** `UBlockadeEventComponent::OnLoadingFinished` führt über den Exec-Thunk `0x5666F90` zum gemeinsamen Einstieg `0x58BB360`. Dort wird ein verzögerter Timer eingerichtet (`DelayedLoadHandle`, `+0x118`); dessen Adapter `0x58AA5F0` ruft den virtuellen Blockadenstart bei VTable-Byteoffset `0x4E8` auf. Es handelt sich nicht um `UActorComponent::BeginPlay`. Der Highway-Ablauf erzeugt Gegner und Missionsziele und ist damit ein Host-/Autoritätsablauf, kein bloßes Client-Rendering.
- **[Fix in Prüfung]** `Client.cpp` prüft am gemeinsamen `OnLoadingFinished`-Einstieg die aktuelle Netzrolle über World/NetDriver/ServerConnection, bevor der Timer angelegt wird. Netzwerk-Clients führen diesen lokalen Blockadenablauf nicht aus; Host und Standalone behalten das Original. Kein erfundener Client-GameMode, kein Tag-/Biomvergleich, keine NetGUID-Sonderliste und kein globaler Schutz im Mengen-Kopieroperator. Der Fix soll somit auch andere Blockaden-Unterklassen und spätere Gebietswechsel abdecken; diese Reichweite ist noch im Spiel zu prüfen.

**Offene Abnahme:** Korrigierten Lauf wiederholen, Tag 5 und spätere Tage prüfen, anschließend Endlosmodus mit mindestens zwei Gebietswechseln. Ein behobener Tag-5-Einstieg ist noch kein Beleg für einen dauerhaft stabilen Run oder vollständig replizierte Blockaden-/Missionszielanzeigen.

**Folgebefund (04.11.2026):** Drei Läufe mit Blockaden-Schutz blieben am Leben, verloren an Tag 5 aber die beiden PlayerStates. Der gezielte Frida-Trace belegt das Entfernen von PlayerState-Paket und Archetyp (GUIDs `0x127`/`0x125`, Flags `0x10`) durch den Ablaufzweig von `CleanReferences`, Aufrufer `0x2CF369A` (Evidence `ev_917dca7c04a1e6bfd24ce90991f884ed8a3f6e7c38caca213a297c24f5c56bb6`). Ein erster allgemeiner Trace war auf 256 Meldungen begrenzt und enthielt diese späteren Löschungen nicht.

**Nicht erfolgreich:** Der anschließende Lauf mit zusätzlichem Ablauf-Schutz für nachladbare statische Pfade verlor an Tag 5 wieder die PlayerStates und meldete zwei neue `NewActor failed`. Das Client-Log bestätigt die Installation des Patches; synthetische Tests allein sind daher kein Wirksamkeitsnachweis. Der Schutz bleibt ein zu untersuchender Kandidat, keine abgeschlossene Lösung. Ein weiterer instrumentierter Lauf mit explizitem Test-Godmode untersucht die Laufzeitbedingungen des Schutzes. Acht Tage und Endloswechsel sind weiterhin nicht abgenommen.

**Ursache des fehlenden Schutzes belegt:** Im instrumentierten Folgelauf ist `GWorld` beim Ablauf und Entfernen beider GUIDs null, obwohl der iterierte Cache weiterlebt (Evidence `ev_04e83adc6c227abbe9d8058d2f17d8ffcb1760aa43c08b5f695b8cd4e4da5a52`). Damit ist die bisherige World-basierte Cache-Prüfung gerade während Travel-GC unwirksam. Der nächste Kandidat liest den eigenen Driver aus `FNetGUIDCache+0x150` (Laufzeitbeobachtung `ev_8437130ab5cb5a1cc53d9282cc333308e3113994df1954d2ba29a1bd88ae27c8`), prüft dessen ServerConnection und den Rückverweis `Driver+0x1C8` auf genau den iterierten Cache. Kein gecachter alter World-Zeiger und weiterhin kein starker UObject-Verweis.

**Test-Godmode, erste Schadens-Gegenprobe:** Auf Tag 5 blieben beide autoritativen Figuren beim Host mit `State.Invincible` nach je 5 angefordertem Schaden bei 100 HP. Nach `!god off` senkten dieselben Befehle beide auf 95 HP; anschließend wieder eingeschaltet. Das belegt den Schadenstest für Host- und Client-Spieler beim Host, noch nicht die vollständige Client-Replikation (in diesem Lauf fehlen dort bereits die PlayerStates). Wiederholbare Vollprobe: `tools/spike/Test-Godmode.ps1`, nur ohne gleichzeitige Routenautomatisierung; setzt beide Instanzen abschließend auf Godmode EIN.

**Erster erfolgreicher Acht-Tage-Übergangslauf:** Mit Cache-eigenem Driver statt `GWorld` wurden am 04.11.2026 Tag 1–8 einschließlich der zweiten Tag-5-Mission erreicht. Sämtliche protokollierten Einstiegskontrollen waren sauber: beidseits zwei PlayerStates/Spielerfiguren, passende Kartenobjekte/Positionen, erhaltene Spieler-GUIDs und null neue `NewActor failed`. Die Inventarvergleiche an Tag 5, der Folgemission sowie Tag 6–8 waren vollständig und gleich. Die sinkende Zahl lebender Cache-Einträge ist kein Fehler für sich; `check_round.py` trennt jetzt SparseArray-Slots von tatsächlich gelesenen Einträgen und prüft weiterhin die benötigten Identitäten.

**Godmode-Vollprobe:** EIN/AUS/EIN mit Schaden an beiden autoritativen Spielern und Prüfung ihrer Client-Replikate bestand an Tag 5 und erneut nach Reise an Tag 6. Mit Schutz bleiben die HP konstant, ohne Schutz sinken sie und der niedrigere Wert repliziert; erneutes Einschalten heilt nicht und verhindert weiteren Abzug. An Tag 6 lief während der ausgeschalteten Gegenprobe zusätzlich Kampf (ein Spieler sank auf 25 statt nur 95 HP); damit ist dieser Lauf kein isolierter numerischer 5-Schaden-Nachweis, wohl aber eine positive/negative Schutz- und Replikationsprobe. Die Testhilfe sollte nur in einer ruhigen Situation geprüft werden.

**Grenze:** Missionen wurden mit `!finish` abgekürzt, an Tag 8 erschien der Sieg-Bildschirm. Das belegt Reisen/Rundenübergänge, keinen natürlich ausgespielten Bosskampf. Der sichtbare Endlos-Einstieg wurde bestätigt, anschließend scheiterte aber bereits das Laden von Tag 9 (Siedlungen) an einem neuen Client-Absturz `0x58C5D8B`, Lesen `0xA8`, Aufrufer `0x58CBBCE`. Die Endlos-Abnahme mit zwei weiteren Gebietswechseln bleibt ausdrücklich offen.

#### Endlos-Tag 9: SneakApproach verwendet unvollständiges Ziel wieder (`guidcache`)

- **[Belegt, Crash/Disassembly]** `0x58C5D40` ruft die Zielinitialisierung `0x58A51F0` auf, liest anschließend `Component+0xE8` und daraus ungeschützt `+0xA8`; ein nuller PlayerCharacterMesh erklärt genau den Zugriff `0xA8` bei `0x58C5D8B`. Der Tick `0x58CBBB0` ruft die Funktion auf.
- **[Belegt, Runtime-VTable]** Der Tick liegt im Slot 128 von `SneakApproachComponent` (VTable `0x95D1428`), identifiziert am Defaultobjekt des Hosts mit REA/Frida: Evidence `ev_f9bdb05be2f72c1203183f37744f4c6b7c4eb79b7f2346750e20d80fbf40987d`.
- **[Belegt, Disassembly]** Die Initialisierung schreibt zunächst das ausgewählte Ziel an `+0xE0`, danach dessen Mesh und PlayerState an `+0xE8`/`+0xD8`. Am Ende prüft sie beide Abhängigkeiten. Beim folgenden Aufruf überspringt sie aber alle Prüfungen und liefert direkt Erfolg, sobald der Zielzeiger unverändert ist (`0x58A5266` → `0x58A530D`). Ein noch nicht vollständig repliziertes Ziel kann damit im nächsten Tick als vollständig gelten.
- **[Kandidat, noch im Lauf zu prüfen]** Der Wiederverwendungszweig prüft nun ebenfalls Mesh und PlayerState; fehlen sie, wird die originale Zielinitialisierung erneut ausgeführt. Keine erfundenen Meshes, kein pauschales Abschalten von Client-Gegnern und kein Überspringen des letzten Speicherzugriffs. Die vollständige Entscheidungsmatrix und der tatsächlich erzeugte Stub sind an synthetischem Speicher geprüft. Ein frischer Wiederholungslauf mit allen drei Korrekturen läuft; erst dessen Ergebnisse können Laufzeitwirksamkeit belegen.

**Wiederholungsergebnis:** Mit allen drei Korrekturen Tag 1–8 erneut geladen und den Endlosmodus bis Tag 13 gefahren. Tag 9–11 und Tag 13 hatten saubere Einstiegskontrollen und vollständige, gleiche Inventare. Der zweite Endlos-Biomwechsel ist damit im begrenzten Übergangstest erreicht: Siedlungen (9/10) → Felder (11/12) → Wälder (13). Tag 13 zeigt auf beiden Instanzen dieselbe Highway-Szene und Spieler am Bus; das Missionsziel-Banner ist nur beim Host sichtbar und nicht als replizierte UI abgenommen.

Der Tag-9-Client-Trace beobachtete genau den neuen Wiederholungszweig: gleiches Ziel, PlayerState vorhanden, Mesh noch null (Evidence `ev_1dafead1d3b2e7e0783b95f9eee9fccd35dc273bb3c5f3f5a6194b8e20585f94`, auf zwölf Meldungen begrenzt). Die ursprüngliche Absturzsituation ist also im korrigierten Lauf tatsächlich wieder aufgetreten, nicht nur zufällig ausgeblieben.

**Nicht als sauber umetikettieren:** Im Wiederholungslauf meldete die spätere Tag-8-Kontrolle nur den Abstand beider Figuren vom Bus als Abweichung; Positionen waren zwischen Host/Client gleich, keine neuen Actor-Fehler und Inventare vollständig/gleich. Diese spätere Kampfaufnahme ersetzt keine Spawn-Messung direkt nach Laden. An Tag 12 meldete die Prüfung eine pausierte Welt; ein sichtbares Belohnungsfenster beim Host erforderte das Ablegen eines Gegenstands ins Inventar und Bestätigen. Danach war die Pause auf beiden Seiten aufgehoben, die erneute Prüfung sauber und auch der neue Revolver vollständig/gleich repliziert. Kein Entfernen von PauserPlayerState per Speicherpatch.

**Testgrenzen vor der Abschlussentscheidung:** Der Lauf verwendet Godmode und abgekürzte Missionen (`!finish`), also keinen natürlich ausgespielten Endboss. Die automatische Route wurde erst im Verlauf um gespeicherte Inventarvergleiche für jeden einzelnen Tag ergänzt; der neue automatische Einzelnachweis für Tag 1–4 wurde in diesen Läufen nicht nachgeholt. Ein begrenzter Lauf bis Tag 13 beweist keine unbegrenzte Stabilität. Die eigene Sitzung wurde nach Sicherung von Logs, Gesundheits-/Inventarzustand und Frida-Evidence beendet; anschließend waren alle 54 eingefrorenen Originaldateien unverändert.

#### Abschluss und akzeptierter Testumfang (`guidcache`)

**Nutzerentscheidung vom 04.11.2026:** Der erreichte Run-/Endlos-Stand gilt für diese Aufgabe als ausreichend geklärt und abgesichert; `guidcache` ist abgeschlossen. Weitere genauere Endlos-Tests, ein natürlich ausgespielter Bosskampf oder das Nachholen der automatischen Einzelprotokolle sind vorerst keine Abschlussvoraussetzung. Die oben genannten Testgrenzen bleiben als Tatsachen erhalten, nicht als offene Restaufgaben. Erst wenn im weiteren Testing ein konkretes neues Problem auftritt, wird dafür ein neues, abgegrenztes TODO mit Reproduktion und Nachweisen angelegt. Keine vorsorgliche Dauer-Testaufgabe und keine Behauptung unbegrenzter Stabilität.

Wichtiger Übergabestand für Build `26187885`:

- **Blockaden:** Auf dem Client keine lokalen Host-Ziel-/Spawn-Timer anlegen; Host und Standalone behalten ihren Originalablauf.
- **NetGUIDs:** Nachladbare statische Pfadidentitäten im Verbindungscache erhalten, nicht UObjects pinnen. Dynamische, NoLoad- und Broken-Einträge bleiben bereinigbar; Read-only-Zeitstempel unverändert lassen. Während Travel-GC ist `GWorld` null: Cache-eigenen Driver und dessen Rückverweis prüfen.
- **SneakApproach:** Dasselbe Ziel ist erst mit Mesh und PlayerState wiederverwendbar. Bei fehlenden Abhängigkeiten die Originalinitialisierung erneut versuchen; keinen pauschalen Client-KI-Stopp einbauen.
- **Testhilfe:** `Start-Session.ps1 -Build -Godmode`, je Instanz `!god on`/`!god off`; nur Spike, nicht Spielstand oder Produktionsmod. EIN/AUS/EIN-Schaden und Client-HP-Replikation geprüft; keine Heilung. Gegenprobe nur in ruhiger Situation ohne Kampf/Reise.
- **Abnahme:** Bis Tag 13, Endlos-Wechsel Siedlungen → Felder → Wälder; PlayerStates, Spieler-GUIDs, Positionen und Inventare geprüft. Tag-8-Abstandsabweichung und normale Tag-12-Belohnungspause bleiben im Verlauf dokumentiert. UI-/Internet-Sync und Portierung nach `src/Game` bleiben bei ihren bestehenden Aufgaben.
- **Prüfungen/Datenschutz:** 84 Python-Tests vor der späteren TODO-Struktur-Integration, native Stub-/Policy-Tests und Spike-Build bestanden. Rohlogs, Screenshots, SDK und dekompiliertes Material bleiben privat. Den aktuellen Nach-Integrations-Teststand nennt der PR.

#### Tutorial-Pause hält den Run an (`testumgebung`)

Ein offener Tutorial-Hinweis pausiert die Welt für beide Seiten über eine Zeitanfrage seiner DialogComponent. In der Testumgebung verfälscht das Ladezeiten und Positionen, im Coop hält es den Partner an. `UTutorialStateSubsystem::SetForceSkipTutorials(true)` schaltet alle Hinweise ab. Das Flag liegt bei `+0xB8`, steht nicht im Spielstand und ist nach jedem Start wieder aus. Der Spike setzt es deshalb auf beiden Seiten, sobald das Subsystem da ist, und prüft es jede Sekunde (`Cheats.cpp`, `SkipTutorials`). `!notutorial` meldet den Stand.

Die Mod macht dasselbe in `src/Game/TutorialAdapter.cpp` (`tutorial-aus`): Als Host oder Client ruft sie einmal je Sekunde `SetForceSkipTutorials(true)`, solange der Schalter nicht steht. Allein, also ohne NetDriver, bleibt das Tutorial an. Geprüft am 04.11.2026 mit einem Spike ohne eigenes Abschalten: Der Host schaltet im Hauptmenü ab, der Client direkt nach dem Verbinden. `Start-Run.ps1` kam ohne Dialoge bis zur Routenkarte, und in der Mission an Tag 1 lief die Uhr ohne offenen Hinweis. Der Schalter blieb über den Kartenwechsel stehen, es gab keinen zweiten Aufruf.

### Was beim Client laufen darf

| Bereich | Host | Client |
|---|---|---|
| Run-Zustand, Seed, Route, Persistenz | Führt | Nicht ausführen. Der Client hat diesen Zustand nicht und braucht einen eigenen Sync |
| Routenkarte, Rundenende, Lager | Führt | Nicht öffnen, Phase nur anzeigen und Eingabe sperren, im Spike umgesetzt (`phasenadapter`) |
| Kartenwechsel | Startet ihn für beide | Folgt nur, startet keinen eigenen |
| Kartenerzeugung | Führt | Darf nicht frei laufen, siehe unten |
| Gegner, Spawner, Loot, Missionsende | Führt | Nur replizierte Ergebnisse anzeigen |
| Kamera | Führt | Läuft lokal auf den replizierten Spielern |
| Eigene Figur | Nimmt die Position vom Client an | Führt Bewegung und Eingabe |
| HUD | Normal | Erst bauen, wenn der `GameState` da ist |
| Inventar | Führt | Darf erst laufen, wenn die replizierten Einträge da sind. Ohne Lösung kein Run über mehrere Tage |

### Rundenende: Befund und offene Phasenübertragung (`rundenende`)

Build `26187885`, ein PC, zwei Loopback-Instanzen, getrennte Test-Spielstände und CrashGuard. Untersuchung auf eigenem Branch von `origin/main`, ohne den noch nicht gemergten Inventar-Teilfix.

- **[Belegt] Menü-/Host-Ablauf erneut beobachtet:** Nach `!finish` Rundenübersicht, nach Fortsetzen Lager, nach Bestätigen Routenkarte. Der Client zeigt bei allen drei Schritten weiter die Mission, nicht diese Ansichten. Die Folge wurde bis zur Routenkarte geprüft, keine Weiterreise mit dem bekannten Inventarfehler ausgelöst.
- **[Belegt] Zustandsgrenze:** Vor Missionsende Host `MissionChain.Num=1`, `CurrentMissionIndex=0`, Client `0` bzw. `-1`. Nach Missionsende Host `LastRoundChangeReason=1`, Client weiterhin `0`. `DayNum` und `SecondsSinceMissionStart` sind auf beiden Seiten vorhanden; nach Host-Ende lag der replizierte Sekundenwert bei 53 auf beiden Seiten. Gleiche Sekunden sind deshalb **kein** Beleg, dass der Client seine lokale Endphase verarbeitet.
- **[Belegt] Ansatz Ereignis weiterreichen:** Die reflektierten Funktionen `FinishMission`, `TriggerEndRound` und `SetFreezeTime` sind keine Netzwerk-RPCs. `OnMissionFinishing`, `OnMissionFinished`, `OnBeforeMissionFinished` und die Freeze-Delegates sind lokale Delegates. Der vorhandene Aufruf allein transportiert die UI-Phase nicht.
- **[Verworfener Ansatz] Host-Ablauf lokal wiederholen:** `TriggerEndRound` läuft über die GameInstance/Persistenz, und dem Client fehlt die Route. Entsprechend der bestehenden Trennung zwischen Host-Spiellogik und Client-Anzeige wurden diese Pfade nicht als Client-Sync ausgeführt. `Mission.cpp` weist nun die beiden Diagnosebefehle `!finish` und `!round` auf einem verbundenen Client ausdrücklich zurück. Negativtest in einer neuen geschützten Sitzung: beide Befehle abgelehnt, beide Prozesse weiter in MetaBase, Client-Grund unverändert `NoReason`.
- **[Belegt] Ansatz UI-Phase aus Panels erkennen:** `UAbstractPanel::AnimationContainer` und `UPanelAnimationContainer::IsOpen` sind mögliche Andockpunkte, lieferten im Test aber bei den beobachteten Panels keinen lesbaren Containerzustand. Widgets können ausserdem an der GameInstance hängen und Karten überleben. Objektpräsenz oder Class-Namen allein sind deshalb kein zuverlässiger Phasenwechsel. Das lesende Werkzeug `tools/spike/roundstate.py` weist unbekannte Container und World-/GameInstance-/ungeklärte Outer-Zuordnung explizit aus; es behauptet keine sichtbare Phase.
- **Teilumsetzung 03.11.2026, ohne Laufzeitanschluss:** `research/spikes/a-connect/RoundPhase.h` implementiert einen UE-unabhängigen Codec und Empfänger für `Playing/RoundEnd/Stash/Route/Travel`. Feste 48-Byte-Little-Endian-Snapshots enthalten Session, Run, Weltgeneration, Sequenz, Tag, Grund und Host-Fortschritt. Keine Objektzeiger, Befehle oder Run-Persistenz. Eine getrennt verifizierte Bindung muss vor Empfang Session/Run/Welt festlegen; eine Nachricht darf diese Bindung nicht selbst wechseln.
- **[Belegt, synthetische Tests]** `tests/Run-RoundPhase.ps1` prüft den Ablauf, initiale Route, Win/Lose, vollständigen Spätbeitritt-Snapshot, Duplikate, widersprüchliche Wiederholungen, alte/fremde Session-/Run-/Welt-Nachrichten, rückläufige Phasen, Sequenzüberlauf, ungültige Felder und abgeschnittene Nachrichten. Der Empfänger übernimmt Anzeigedaten und `InputBlocked`-**Modell** gemeinsam; bei neuer verifizierter Welt wartet er gesperrt auf den ersten passenden Snapshot. Disconnect entfernt nur dieses Modell. Tests mit MSVC C++20 `/W4 /WX` bestanden; kein SDK/Spiel nötig.
- **Weiter offen:** Autoritative Ereignisquelle am Host, tatsächlicher Transport/Handshake und Weltbindungsnachweis, sichtbare Client-Anzeige und Anwendung/Rücknahme der Sperre für Tastatur, Maus, Gamepad und bereits gehaltene Eingaben. Der Modellwert `InputBlocked` ist **keine im Spiel wirksame Eingabesperre**. Dafür sind geeignete Engine-/UI-Anschlüsse separat nachzuweisen. Im Spike ist der Header absichtlich noch nicht aktiviert; er darf nicht als funktionierender UI-Sync ausgegeben werden.

Die Aufgabe bleibt ungelöst. Getestet sind Beobachtung, Befehlsgrenzen sowie Codec-/Empfängerregeln an künstlichen Nachrichten, **nicht** eine synchronisierte Endphase, wirksame Client-Eingabesperre, echter Spätbeitritt/Netzwerktransport, Internet oder vollständiger Run. Keine erneute lokale Ausführung der Host-Persistenz und keine Panelpräsenz als Phasensignal. Initialisierungs-/Tick-Anschluss in `Mission.cpp` und gemeinsame Schnittstellen bleiben ein eigener Integrationsschritt.

**Nachtrag 03.11.2026, ohne Spielbetrieb:** Ein lokaler Adapter in `Mission.cpp` ergänzt den Modellkern um einen monotonen Host-Publisher, atomaren 48-Byte-Dateitransport, Heartbeat/Timeout und Client-Textausgabe/Win32-Nachrichtenfilter. Die tatsächliche Adapterimplementierung wurde gegen eigene Speicherpuffer geprüft, der Dateitransport zwischen zwei eigenen fensterlosen Testprozessen; C++20 `/W4 /WX` und vollständiger Spike-DLL-Build bestanden. `Main.cpp` verbindet Befehl/Tick/Filter, Host-`!finish`/`!round` liefern ihre Aufrufe; bis zur expliziten Diagnosebindung bleibt der Adapter inaktiv. Der globale Beobachter für normale Menü-/Busereignisse ist noch nicht eingehängt, Host-Rundenendeaufrufe sind noch nicht im Spiel beobachtet; Lager/Route werden ausschließlich manuell diagnostisch markiert. Keine automatische Panelerkennung behauptet, kein vollständiger UE-/Gamepad-Inputschutz, kein Internetkanal und kein P0-Abschluss. Siehe [konkrete Integration, Tests und verbleibende Nachweise](../research/spikes/a-connect/RoundPhaseRuntime.md).

**Abschluss `phasenadapter`, 04.11.2026:** Die Phase läuft jetzt automatisch vom Host zum Client. Damit ist die Aufgabe für einen PC gelöst.

- **[Belegt] Hostquellen:** Jedes Panel meldet sich über `AbstractPanel.OnOpenStateChanged(bOpen)`, das durch `ProcessEvent` läuft. Rundenübersicht ist `WP_Round_Stats_C`, Lager `WP_Inventory_Panel_C`, Routenkarte `WI_MissionChoicePanel_C`, auch die erste Route in der MetaBase. Dazu kommen `FinishMission` und `TriggerEndRound`, beide ebenfalls über `ProcessEvent`. Das Inventar-Panel öffnet sich auch bei der Belohnungsauswahl mitten in der Mission und gilt deshalb nur nach einem Rundenende als Lager.
- **[Belegt] Eingabesperre:** `AActor::DisableInput(PC)` auf Controller und Figur des Clients nimmt beide aus dem Input-Stack. Nur damit bewegte sich die Client-Figur bei gedrückter Taste nicht, Esc öffnete kein Pausenmenü. Ohne Sperre ging beides. Der Fensterfilter kommt dazu, lässt aber Loslassen-Nachrichten durch.
- **[Belegt] Lauf:** Erste Route und Rundenende an Tag 1, 2 und 3, beim Client jeweils dieselbe Folge wie beim Host, danach in der neuen Welt wieder frei. Fällt der Host weg, gibt der Client nach 5 s frei.
- **Offen, mit eigener Aufgabe:** Transport über das Netz (`transport`, `protokoll`). Natürliches Tagesende und Gamepad sind nicht im Spiel geprüft, siehe [Grenzen](../research/spikes/a-connect/RoundPhaseRuntime.md#grenzen).

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
