# Spielanalyse

Stand: 02.11.2026. Neue Erkenntnisse bitte hier ergänzen, ausführliche Notizen gehören nach `research/`.

Vertrauensgrad:

- **[Belegt]** direkt an der aktuellen Installation geprüft
- **[Hypothese]** Schlussfolgerung, noch nicht getestet

## Unterstützte Spielversion

| | |
|---|---|
| Steam-AppID | `3026450` |
| Steam-Build-ID | `26495302` (seit `spiel-update`, 09.11.2026) |
| Manifest-IDs | Depot `3026451`: `1832737755528485607`, Depot `3026452`: `2302428105354984436` |
| Exe-Kennung | TimeDateStamp `0x65BFBBFB`, SizeOfImage `0x0B748000` |
| Engine-Version | `5.8.1-56057345+++UE5+Release-5.8` |
| Spiel-Exe | `DDSurvivors/Binaries/Win64/DDSurvivors-Win64-Shipping.exe` (179 MB) |

Build-ID und Manifest-IDs stehen in `steamapps/appmanifest_3026450.acf`. Die Manifest-IDs benennen die gewünschten Depotstände;
ein erneuter Download über die Steam-Konsole ist für den Vorgänger getestet (07.11.2026): `download_depot 3026450 3026452 7724756528743620812` lieferte
alle 52 Dateien von `26187885`, die Prüfsummen passten; Depot `3026451` hatte beim Update auf `26495302` denselben Stand. `26187885` war bis
`spiel-update` (09.11.2026) unterstützt, seitdem verlangen Mod, Skripte und Installer ihn nicht mehr. Bei einem Spiel-Update gilt die
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

#### Kampf beim Client (`kampf`)

Stand 05.11.2026, Build `26187885`, ein PC, `Run-Route.ps1 -Start -NetProfile Internet -Days 1 -StayLast`
(gemessen 59 ms je Richtung, 2,0 bis 2,5 % Verlust), Mod mit Spike, Seed `20795`, Tag 1.

Lokal zuerst: Der Client simuliert keine Projektile und keinen Schaden. Der Host schaltet nur zwei native
Replikationen ein, siehe `ProjectilePatches.cpp`:

- **Größe:** `SetBulletSize` skaliert beim Host die Root-Komponente, `BulletSize` selbst ist kein Net-Feld.
  `RelativeScale3D` ist eins. Der Host setzt deshalb `bReplicates` der Root-Komponente, auf den Klassen-Defaultobjekten
  und als Rückfall an lebenden Actors. Ort und Drehung der Root lässt die Engine bei `bReplicateMovement` weg.
- **[Belegt]** Die Root-Komponente neuer Actors erbt `bReplicates` von der Vorlage im Defaultobjekt. `bReplicateMovement`
  am Actor erbt sie **nicht**: Alle 28 Projektil-Defaultobjekte hatten das Bit, die ersten zwei Projektile der Mission
  trotzdem nicht. Darum prüft der Host je Tick die neu ans `Level->Actors`-Array (`0xA0`) angehängten Actors.
  Danach meldete das Log nur noch „neu sofort“, der Sekunden-Abgleich fand nichts mehr.
- **[Belegt] Größe beim Client:** 40 s Abtastung je rund 30 ms. Fliegende Projektile hatten beim Host und beim Client
  dieselbe Skalierung `(0.75, 0.75, 1)`, versteckte im Pool beide `(1, 1, 1)`. Der Client ruft `SetBulletSize` nicht auf
  ([Blueprint-Probe](../research/spikes/blueprint-logik/README.md#konkrete-befunde)), der Wechsel kommt also aus der
  Replikation. Vorher blieb der Client bei `(1, 1, 1)`.
- **[Belegt] Treffer:** Der Treffer-Zähler des Spikes (`OnEnemyHit` beim Host) zählte nach rund 4 Minuten 90 Treffer, Client 43
  (32 Kills), Host 47 (12 Kills), je 25 Schaden.
- **[Belegt] Schaden an Spielern:** `DD_DamagePlayerAtIndex 20 0` und `35 1` beim Host. Beide Seiten zeigen danach HP 80 und 65,
  in anderer Reihenfolge der `PlayerArray`. Der Desync-Melder meldete kurz „offen HP“ und nach rund 5 s wieder „OK“.
- **[Belegt] Gegnerangriffe, Mensch am Spiel:** Per Skript liefen die Figuren nicht nah genug heran, die Waffen töteten
  auch nach `!wave` alle Gegner vorher. Der Nutzer lief mit der Client-Figur in eine Gruppe: HP 65 → 5 → 0, danach mit der
  Host-Figur: 80 → 60. Host und Client zeigten jeweils dieselben Werte (Abtastung je 0,5 s), der Desync-Melder war nach
  3 bis 4 s wieder „OK“. Der Tod der Client-Figur bei lebender Host-Figur lief ohne Absturz, die Runde ging weiter.
- **Nicht geprüft:** Upgrades mit anderer Größe, andere Waffen als `BP_Projectile_C`, Treffer- und Todeseffekte
  (`opt-blut`, `vfx-sound`), echtes Internet zwischen zwei PCs (Gate G2).

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

### Lebensleisten beim Client (`hp-leisten`)

Stand: Spieltest am 04.11.2026, Build `26187885`, ein PC über Loopback, Route über das Spielmenü mit Seed `20795`. Schaden über `!cheats` und `DD_DamagePlayerAtIndex` beim Host.

- **[Belegt]** Die HP selbst replizieren richtig, beide Seiten hatten gleiche Werte. Die große HUD-Leiste unten zeigte beim Client seinen eigenen Wert richtig.
- **[Belegt]** Die kleinen Leisten über den Figuren (`WP_World_Player_HealthBar_C`) blieben beim Client auf ihrem Startwert stehen: die eigene auf 0 % (sie entstand vor der Replikation), die des Host-Spielers auf 100 %. Bei 70 und dann 50 HP zeigte der Host 70 % und 50 %, der Client weiter 100 %.
- **[Belegt]** Die Leisten melden sich über `ListenToHealthChange` an `UCommonAttributeSet::OnHealthChanged` (`+0xC0`) an, beim Client genauso wie beim Host (je ein Eintrag `WP_HealthBar_AutoGenFunc`). Das Spiel löst dieses Ereignis aber nur beim Host aus. Beim Client läuft für einen neuen Wert nur `OnRep_Health` (`0x57DC840`), das ist das Standard-Makro von GAS: `GetOwningAbilitySystemComponent`, dann `SetBaseAttributeValueFromReplication` an `ASC+0x8D8`, kein eigener Broadcast. Gleich gebaut ist `OnRep_MaxHealth` (`0x57DCEA0`) für `OnMaxHealthChanged` (`+0x40`).
- **Fix im Spike (`HealthBars.cpp`):** Beide OnRep-Funktionen sind umschlossen. Nach dem Original ruft der Spike jeden Empfänger des passenden Delegates mit dem neuen `CurrentValue` über `ProcessEvent` auf: schwacher Zeiger über das Objekt-Array mit Prüfung der Seriennummer, Funktion über ihren `FName` in der Klassenkette. Beim Host laufen die OnRep-Funktionen nicht, der Patch wirkt also nur beim Client.
- **[Belegt]** Mit dem Fix starteten beide Leisten beim Client bei 100 %. Nach Schaden zeigten Host und Client dieselben Werte: 40 % und 70 %, dann 50 %, nach dem Wechsel auf Tag 2 75 %. Kein Absturz.
- Nicht geprüft: Rüstung (`OnArmorChanged`, `OnMaxArmorChanged` liegen im abgeleiteten Charakter-Set) und die Überheilung. Ob weitere Empfänger dieser Delegates beim Client jetzt zusätzlich laufen sollten, ist nicht geprüft, gefunden war nur die Leiste. Internet nicht geprüft.

### Ausdauerbalken beim Client (`opt-ausdauerbalken`)

Stand 10.11.2026, Build `26495302`, zwei Instanzen auf einem PC, Mod mit Spike.

- **[Belegt, Disassembly]** Gleiche Ursache wie bei den [Lebensleisten](#lebensleisten-beim-client-hp-leisten): `UDDCharacterAttributeSet` (`Stamina` `+0x50`, `OnStaminaChanged` `+0x60`, `MaxStamina` `+0x30`, `OnMaxStaminaChanged` `+0x40`) repliziert die Werte, das Spiel löst die Delegates aber nur beim Host aus. `OnRep_Stamina` (`0x57DDE40`) und `OnRep_MaxStamina` (`0x57DD080`) fanden sich über die Exec-Thunks der `FNameNativePtrPair`-Tabelle; die Kontrolle mit `OnRep_Health` ergab die bekannte `0x57DC810`. `OnRep_Stamina` liest im Rumpf `+0x50`, `OnRep_MaxStamina` `+0x30`.
- **Umsetzung:** `HealthBarPatches.cpp` umschließt beide OnRep-Funktionen wie die der Lebensleisten und ruft danach die angemeldeten Empfänger auf. Keine neue Nachricht, keine zweite Replikation.
- **[Belegt, Live-Test]** Der Host weicht aus, der Client sieht seinen Ausdauerbalken wie der Host selbst (vom Nutzer im Bild bestätigt).

### Busbalken beim Client (`opt-busbalken`)

Stand 10.11.2026, Build `26495302`, zwei Instanzen auf einem PC, Mod mit Spike, Tank beim Host per Speicherschreiben auf 120 von 120, Busflucht vom Nutzer.

- **[Belegt, SDK]** Der Balken ist `WP_Bus_Countdown_Display_C` (`BusProgressBar`, `Progress` `+0x348`, `UpdateBusProgress(double)`) im `CountDownWidget` (`+0x3B8`) des Kindaktors `BP_SpawnAndLeaveBus_C`. Die Abfahrt treibt das Blueprint über `OnStartBusTimerTick` und `UpdateBusProgressDisplay` (`ElapsedTime` `+0x428`, `RequiredTime` `+0x420`, `UpdateInterval` `+0x490`). Die Aktor-Felder sind nicht repliziert.
- **[Belegt, Live-Messung]** Beim Host stieg der Fortschritt auf 1,0 (`ElapsedTime` 3,05 s von `RequiredTime` 3,0 s, `tankFull` 1). Beim Client blieben Fortschritt, `ElapsedTime` und `tankFull` die ganze Abfahrt über auf 0, obwohl `PlayersInBusRange` (`+0x2D8`) auf beiden Seiten 2 zeigte und der Tank repliziert war.
- **[Belegt, früherer Befund]** Beim Client liefert `CanBusTakeOff` immer nein (`CanBusTakeOffHook`, [Busabfahrt nur beim Host](#busabfahrt-nur-beim-host-bus-ende)), weil ihm der GameMode fehlt. Ein lokaler Start über das native Blueprint ist darum kein Weg.
- **[Belegt, Live-Messung mit 20 Hz]** Beim Host schaltet das Blueprint die Sichtbarkeit des `CountDownWidget` (Komponentenflag `0x43` auf `0x63`) mit dem Start der Abfahrt ein und lässt `ElapsedTime` in 0,05-s-Schritten bis 3,05 s laufen; der Fortschritt des Widgets folgt geglättet, in rund 2,6 s von 0 auf 0,96.
- **Umsetzung** (`BusBarPatches.cpp`, `TickBusBar`, Regeln `BusBarActive` und `BusBarElapsed` in `PatchPolicy.h`, mit Tests): Beim Client gilt die Abfahrt als gestartet, wenn der Tank des Missions-GameStates voll ist (`FuelMeter`, `CurrentFuel` ≥ `MaxRequiredFuel`, repliziert) und `PlayersInBusRange` alle Figuren der `PlayerArray` enthält. Dann macht die Mod das `CountDownWidget` mit der nativen `SceneComponent.SetVisibility` sichtbar und setzt den Fortschritt mit `WP_Bus_Countdown_Display_C.UpdateBusProgress` aus der lokal gezählten Zeit (`RequiredTime` `+0x420`). Entfällt eine Bedingung vor dem Ende, blendet sie den Balken aus und setzt ihn zurück. Abfahrt und Rundenende entscheidet weiter der Host. Keine neue Nachricht, keine Änderung an `CanBusTakeOffHook`.
- **[Belegt, Live-Test 10.11.2026]** Tank beim Host auf 120 von 120 gesetzt, beide Figuren am Bus: Der Client-Balken lief im Gleichschritt mit dem des Host (0 bis 0,96 in rund 2,6 s), die Komponente wurde sichtbar, Mod-Log „Busbalken: Abfahrt beim Client abgeleitet (Tank voll, 2 von 2 Figuren im Bereich)“. Der Nutzer sah den Balken im Client-Fenster laufen.
- **Nicht geprüft:** Verlassen des Busbereichs mitten in der Abfahrt (ob der Host den Balken ebenfalls zurücksetzt), zwei PCs. Die Suche nach dem Bus-Objekt hielt den Frame zweimal 13 bis 14 ms auf (`Takt: Busbalken …`), sie läuft höchstens einmal je Sekunde, bis der Bus gefunden ist.
- **Hinweis zum Test:** Der Client stürzte in einem früheren Lauf nach rund 12 Minuten am bekannten Sensenmann-Fehler (`0x15C0407`, siehe [Wiederbelebung](#tod-des-clients-tod-client)); Läufe kurz halten.

### Lootbalken beim Client (`opt-lootbalken`)

Stand 11.11.2026, Build `26495302`, zwei Instanzen auf einem PC über die Lobby, Mod mit Spike, `-Godmode`, Client von Skript gesteuert, Speicher beider Seiten nur gelesen.

- **[Belegt, SDK]** `BP_EnvironmentObjectLootspot_C` repliziert `Progress` (`+0x3C0`, double) und `IsLooting` (`+0x3C8`, RepNotify). Der Balken ist `Widget` (`+0x3A8`, `WBP_LootWidget_C`), dessen `UAbstractProgressWidget.Progress` (`+0x328`) die Blueprint-Funktion `Update Progress Indicator(double)` über `SetProgress` setzt. Der Name enthält Leerzeichen.
- **[Belegt, Bytecode]** `OnRep_IsLooting` startet und stoppt nur den Sound (`FMODAudioComponent.Play`, `Stop`), für `Progress` gibt es keinen RepNotify.
- **[Belegt, Live-Messung vor der Änderung]** Client und Host zeigen `Progress` im Gleichschritt (0 bis 0,99 in rund 1 s, Abstand etwa 30 ms). Beim Host folgt das Widget, beim Client blieb es auf 0. Die frühere Vermutung, dem Client fehle die Lootdauer, trifft nicht zu: Es fehlt nur der Aufruf ins Widget.
- **Vermutung:** Dass nur ein Autoritätspfad das Widget treibt, ist nicht einzeln belegt, das Ubergraph enthält `HasAuthority`, die Sprünge wurden nicht nachvollzogen.
- **[Belegt, Live-Messung]** Ende beim Host: `IsLooting` 0, `Progress` 1,0. Beim Client bleibt der letzte Stand stehen (`IsLooting` 1, `Progress` 0,96 bis 0,99), die letzte Änderung kommt nicht mehr an. Die Stelle bleibt bis zu rund 20 s bestehen, dann wird das Objekt freigegeben und der Speicher neu benutzt. Ein Versuch, nach Stillstand kurz vor dem Ende 1,0 zu setzen, änderte das Widget nicht und wurde wieder entfernt; 0,99 und 1,0 sind im Bild nicht zu unterscheiden.
- **[Belegt, Live-Messung]** Abbruch durch Verlassen des Bereichs: `IsLooting` 0, `Progress` bleibt auf beiden Seiten stehen (gemessen 0,45, 0,75, 0,73, 0,84), kein Zurücksetzen. Beim Wiederkommen läuft der Balken ab diesem Wert weiter (0,73 bis 1,0 gemessen). Der Client zeigt dasselbe.
- **Umsetzung** (`LootBarPatches.cpp`, `TickLootBar`, Regeln `LootBarValidProgress`, `LootBarNeedsUpdate`, `LootBarDone` in `PatchPolicy.h`, mit Tests): Ein Beobachter am vorhandenen ProcessEvent-Hook merkt sich beim Client jede Stelle, deren `OnRep_IsLooting` läuft. Je Takt gibt der Client den replizierten `Progress` an das native `Update Progress Indicator`, nur bei einer Änderung über 0,001. Kein Timer, keine Nachricht, kein neuer Hook, also keine `ModVersion`-Erhöhung.
- **Antworten zum neuen Pfad:** Lokal bleibt die ganze Anzeige. Geteilt wird nichts Neues, `Progress` und `IsLooting` replizieren schon nativ. Es entscheidet weiter der Host, der Client ruft nur die Anzeige auf, die keine Spielwerte schreibt. Wiederverwendet wird der bestehende Beobachter-Pfad, ersetzt wird nichts. Ablehnung gibt es nicht; bei Verlust ist der nächste replizierte Wert maßgeblich, nach Join oder Reconnect meldet die Engine das abweichende `IsLooting` erneut (Vermutung, nicht getestet).
- **Stabilität:** Jede gemerkte Stelle wird vor jedem Zugriff über ihren Platz im Objekt-Array geprüft (`Objects::At(Index) == Objekt`, dazu Klasse und Flags), freigegebener Speicher wird also nie als Lootstelle benutzt. Beim Weltwechsel vergisst die Mod Stellen und UFunctions. Die Liste ist auf 16 Einträge begrenzt, bei Überlauf weicht die am längsten unveränderte Stelle, denn fertige Stellen melden beim Client kein Ende.
- **[Belegt, Live-Test 11.11.2026]** Vollständig durchsucht auf dem endgültigen Stand: eine Tonne (Stelle 80 Einheiten neben `BP_Lootable_TrashcanBig01_C`) am ersten Tag, Fahrzeug-Lootstellen an beiden Tagen. Das Client-Widget folgte dem Host mit etwa 20 ms Abstand (0,91 bei Host 0,96). Kartenwechsel durch das Tagesende und erneutes Durchsuchen am nächsten Tag bestanden. Keine Absturzberichte, Inventare „vollständig, gleich“.
- **[Belegt, Bild]** Der sichtbare Balken ist die Füllung des Sack-Symbols über der Stelle. Während des Durchsuchens zeigen Client und Host dieselbe Teilfüllung, nach dem Ende ist das Symbol auf beiden Seiten weg. `UProgressBar.Percent` ist kein Maß dafür, er stand auf beiden Seiten durchgehend auf 0,5.
- **[Belegt, Live-Messung] Abbruch:** Ein gezielter Skript-Abbruch gelang auf dem endgültigen Stand nicht, weil das Durchsuchen nur rund 1 s dauert und die Tasteneingabe zu spät kommt. Belegt sind Abbrüche durch Weiterlaufen: Stellen blieben bei 0,73 und 0,84 stehen, Client-Widget gleich Host-Widget, eine lief von 0,73 bis 1,0 weiter.
- **[Belegt] Keine lokale Vergabe:** `Update Progress Indicator` ruft nur `GetUserWidgetObject` und `SetProgress` auf (Bytecode), die Inventare beider Seiten blieben gleich. Das Loot erzeugt weiter nur der Host.
- **Takt:** Die Suche der beiden UFunctions hielt den Frame je Mission einmal etwa 10 ms auf (`Takt: Lootbalken …`).
- **Nicht geprüft:** zwei PCs, Reconnect mitten im Durchsuchen. Eine Stelle mit eigener Kistenoptik ließ sich in den Testkarten nicht zuordnen. `BP_ChestLootSpot_C` (`Progress` nicht repliziert, kein Widget) und `BP_PowerUpCrate_C` rufen weder Widget noch Fortschrittsfunktionen auf, dort gibt es also keinen solchen Balken. Säcke wurden nicht einzeln gemessen. **Vermutung:** Sie laufen über dieselbe Klasse wie die getesteten Stellen.

### Warnanzeigen der Gegner beim Client (`opt-gegner-indikatoren`)

Stand 11.11.2026, Build `26495302`. Gemeldet im [Live-Test vom 10.11.](#live-test-auf-zwei-pcs-am-10112026-livetest-1011): Beim Client
fehlen die Warnanzeigen sprintender und spuckender Zombies.

- **[Belegt, SDK und Speicher]** Wer sprintet und spuckt: `BigEnemy` und `FirefighterEnemy` haben `GA_TackleAttack_C`, `BloaterEnemy`
  und `IceBloaterEnemy` `GA_BlobThrower_C` / `GA_BlobThrower_Ice_C` (`UEnemyType.AttackAbilitiesWithoutWeights` `+0x1D8`). **Vermutung:** Weitere
  Kreise zeigen Sprungangriffe (`GA_SmallJumpAttack_C` bei `StandardEnemy`, `NurseEnemy`) und Bosse, im Test kam keiner davon vor.
- **[Belegt, SDK]** Beide Anzeigen sind lokale Niagara-Komponenten der Angriffsfähigkeit, ohne Net-Feld und ohne RPC:
  - **Kreis:** `UEnemyAttackAbility::SpawnCircleTelegraph(System, Ort, Delay, Lifetime, Size, AutoDestroy, Pool, Attached)` (`Final, Native`),
    beim Projektil gemerkt in `AProjectile::OptionalProjectileTelegraph` (`+0x378`, kein Net-Feld). Auch das Projektil trägt kein
    repliziertes Ziel, der Client kann den Landepunkt also nicht ableiten.
  - **Strich:** `UTackleAbilityExecution` am Gegner (`LineTelegraph` `+0x238`), `ActivateLineTelegraph()` und
    `CalculateLineTelegraphEndPoint(Ziel, Anteil)` (`Final, Native, Private`). Delegates wie `OnTackleMovementStarted` sind lokal.
- **[Belegt, Disassembly]** `SpawnCircleTelegraph` (`0x58908C0`) spawnt das System über die Niagara-Bibliothek am Ort (oder angehängt)
  und setzt danach drei Float-Parameter, deren Namen in der Fähigkeit stehen (`SizeVariableName` `+0x440`, `LifetimeVariableName`
  `+0x448`, `DelayVariableName` `+0x450`). Direkt aufgerufen von drei nativen Stellen und über den Thunk aus Blueprints.
- **[Belegt, Disassembly]** `ActivateLineTelegraph` (`0x58A3DB0`) aktiviert nur die vorhandene `LineTelegraph`-Komponente und setzt Breite
  (aus den Grenzen des Besitzers, `OwnerPrivate` `+0xA8`), Startfarbe und Startpunkt als Niagara-Parameter. `CalculateLineTelegraphEndPoint`
  (`0x58A9D30`) setzt den Endpunkt aus Ziel und Anteil und wechselt ab `SwitchTelegraphColorPercentage` einmal auf `LockTelegraphColor`.
  Beide schreiben nur in die Komponente selbst, keine Spielwerte. `ExecuteTackle` (`0x58B1F40`) blendet den Strich zuerst aus.
  `ActivateLineTelegraph` hat keine direkten Aufrufer, es läuft nur aus dem Blueprint der Fähigkeit.
- **Folgerung:** Die Fähigkeiten laufen nur beim Host (KI nur dort, siehe [Bosse](../src/Sync/Enemies/README.md)), also entstehen die
  Anzeigen nur dort. Eine native Replikation des Auslösers gibt es nicht, darum eine kleine Nachricht (nachgewiesene Lücke).
- **Umsetzung** (`EnemyTelegraphPatches.cpp`, Format `Sync/Enemies/TelegraphState.h`, Nachricht `0x900`, Mod-Version 11, siehe
  [Kontrollkanal](CONTROL_PROTOCOL.md#warnanzeigen-der-gegner-mod-kompatibilität-11)): Hooks auf die fünf nativen Funktionen melden nur
  beim Host. Kreis: Systempfad, Ort, Zeiten, Parameternamen; der Client spawnt dasselbe System über `SpawnSystemAtLocation` und setzt die
  Parameter mit `SetVariableFloat`. Strich: NetGUID des Gegners, an, Stand (höchstens alle 50 ms) und aus; der Client ruft an seiner
  eigenen `UTackleAbilityExecution` des Gegners dieselben nativen Funktionen und blendet wie `ExecuteTackle` über die
  virtuelle Funktion `+0xAD0` der Niagara-Komponente aus.
- **Antworten zum neuen Pfad:** Lokal bleibt die ganze Darstellung (Niagara-System, Länge, Farbwechsel, Ende). Geteilt wird nur der
  Auslöser je Anzeige, weil die Fähigkeit nur beim Host läuft. Es entscheidet der Host, beim Client entsteht weder Schaden noch Sprint.
  Wiederverwendet werden der Kontrollkanal und die nativen Anzeigefunktionen, ersetzt wird nichts. Ablehnung gibt es nicht; Verlust kostet
  eine Anzeige, ein Strich ohne Nachricht geht nach 3 s aus, ein Kreis nach Delay + Lifetime + 1 s. Join, Travel und Reconnect holen
  nichts nach, der Weltwechsel vergisst alle Einträge.
- **Stabilität:** Gemerkte Systeme, Komponenten und `UTackleAbilityExecution` werden vor jedem Zugriff über ihren Platz im Objekt-Array,
  Klasse und Flags geprüft. Gegner kommen nur über ihre NetGUID, nie über Zeiger des Hosts. Der Client ruft die nativen Strich-Funktionen
  nur bei gesetzter `LineTelegraph` und gesetztem Besitzer. Beim Host und allein zeigt nur das Spiel selbst an, eine doppelte Anzeige
  gibt es also nicht: Die Hooks melden beim Client nichts, auch nicht für die eigenen Aufrufe der Mod.
- **[Belegt, Spieltest 11.11.2026]** Ein PC, zwei Instanzen über die Lobby, Mod mit Spike, `-Godmode`, `DDDynamic_Map`. Gegner gezielt mit
  `!load …/Definitions/<Typ>.<Typ>` und `!boss BigEnemy 1500` bzw. `!boss BloaterEnemy 1500` (der Befehl findet seitdem auch normale
  Gegnertypen). Gelesen mit `tools/spike/enemy_telegraph.py` (Speicher beider Seiten) und den Zählern im Mod-Log.
  - **Vor der Änderung bestätigt:** Die Fähigkeiten `GA_TackleAttack_C` und `GA_BlobThrower_C` gibt es nur beim Host (31 bzw. 18
    Instanzen, Client 0), `OptionalProjectileTelegraph` der Spuck-Projektile wurde beim Host 10-mal gesetzt, beim Client nie.
  - **Strich beim Client vorbereitet:** `LineTelegraph` war an allen drei `UTackleAbilityExecution` des Clients gesetzt (wie beim Host),
    die nativen Funktionen laufen dort also auf einer fertigen Komponente.
  - **Zähler:** Host meldete 27 Kreise und 7 Striche, der Client zeigte 27 und 7. Nach einem `servertravel` mitten in Angriffen und
    neuen Gegnern: Host 63 / 17, Client 63 / 17, keine Absturzberichte. Ein erster Strich-Start fand die Komponente noch nicht
    („übersprungen“ 1), der nächste Strich-Stand 120 ms später zeigte ihn.
  - **Zwei weitere Läufe mit dem endgültigen Stand** (Suche gecacht, Zeiten beim Host auf 0 bis 600 s geklemmt, verworfene Nachrichten
    im Log): Host 52 Kreise / 6 Striche, Client 51 / 6; danach mit Kartenwechsel Host 39 / 4, Client 38 / 4, keine Absturzberichte.
    In beiden Läufen fehlte genau ein Kreis, ohne „verworfen“ beim Senden oder Empfangen, der Transport meldete 0 verworfene Pakete.
    **[Belegt, Code]** Ursache: `Sync::Start` holt `0x900` jeden Takt gezielt ab, alle 250 ms nimmt der gebündelte Abruf aber alle übrigen
    Nachrichten, und keine Familie kannte `0x900`. Kam eine Anzeige zwischen beiden Abrufen an, war sie weg. Seitdem gibt der Client auch
    `0x900` aus dem gebündelten Abruf weiter. Dieselbe Lücke betrifft vermutlich `0x500` (Dodge-Richtung) beim Host.
  - **Letzter Lauf mit dem endgültigen Stand** (Abrufkorrektur, Ausblenden über `+0xAD0`): Host 49 Kreise / 6 Striche, Client 49 / 6,
    keine Absturzberichte. Ein erster Strich-Start fand die Komponente noch nicht und wurde mit dem nächsten Stand gezeigt.
  - **[Belegt, Bild]** Gleichzeitige Bildpaare (beide Fenster im Abstand weniger Millisekunden): Host und Client zeigen denselben weißen
    Sprint-Strich vom `BigEnemy` zur Figur; nach dem Sprint ist er auf beiden Seiten weg.
  - **[Belegt, Bild]** Host und Client zeigen im selben Moment den roten Warnkreis unter der getroffenen Figur, ein Bild vorher beide keinen.
  - **Takt:** Das erste Auflösen des Systempfads je Welt hielt den Frame einmal 14 bis 25 ms auf, eine Suche der Strich-Komponente über das
    Objekt-Array etwa 14 ms. Die Suche ist seitdem gecacht und auf vier je Sekunde begrenzt.
- **Nebenbefund, nicht Teil dieser Aufgabe:** Der Aufprall der Spucke (`NS_BlobExplosion`) erschien beim Host 13-mal, beim Client nie,
  ebenso Blut und Fleisch (`NS_ZombieBlood`, `NS_Zombieflesh`). Das gehört zu `opt-blut` und `vfx-sound`.
- **Nicht geprüft:** zwei PCs mit echter Latenz, Bosse (`SpikeBoss`, `FrankensteinBoss` nutzen dieselben Funktionen), angehängte Kreise
  (`Attached`, der Client zeigt sie am Weltort), Reconnect mitten im Angriff.

### XP-Bälle beim Client (`xp-baelle`)

Stand 04.11.2026, Build `26187885`. Beim Spieltest fiel auf: Der Client sieht die XP-Bälle nicht. Ursache bisher nur aus dem lokalen SDK abgeleitet, im Spiel nicht gemessen.

- **[Belegt, SDK]** Die XP-Bälle sind keine replizierten Actors.
  - Angezeigt werden sie von `AExpEmissionHandler`, einem Niagara-Partikelsystem.
  - Position, Ziel, Wert und Farbe jedes Balls stehen in Listen des Handlers (`ExpData`, `SpawnLocationsInNiagara`, `OrbValuesInNiagara`, `OrbColorsInNiagara`).
  - Die Klasse `BP_EXPOrb_C` (`AOrb`, Feld `OrbExpValue`) gibt es zusätzlich.
- **[Belegt, SDK]** Bälle entstehen über `UExpSubsystem::RequestExpSpawning(ExpAmount, Location)`, das Aufsammeln läuft über `ReceiveExp(ExpAmount, Recipient)`. Beides ist kein RPC.
- **Vermutung:**
  - Gegner sterben beim Host, also ruft nur er `RequestExpSpawning` auf. Der Handler des Clients bekommt nie Daten.
  - Die XP selbst zählen trotzdem: Der Host sammelt auch mit der Figur des Clients ein, und die XP sind gemeinsam. Der Client sieht nur nicht, wo die Bälle liegen.
- **Möglicher Weg:**
  - Der Host meldet jeden Aufruf von `RequestExpSpawning` (Menge, Ort) über den Kontrollkanal.
  - Der Client erzeugt damit nur die Anzeige.
  - Beim Client wird `ReceiveExp` gesperrt, damit nichts doppelt zählt.
- **Offen:** Das alles im Spiel belegen. Dazu prüfen, ob die Partikel des Clients beim Aufsammeln mit denen des Hosts gleich verschwinden.

**Nachprüfung des Entfernungsfehlers (05.11.2026, Build `26187885`).** Die Spawn-Übertragung aus
`client-visuell` enthält keinen Radiusfilter. In einer geschützten Lobby-/Tag-1-Sitzung waren die
nativen Sicht-Halbausdehnungen des Handlers beim Host `(4370.42, 4851.49)`, beim Client dagegen
`(1050, 5750)`, trotz nahezu gleichem Spielerzentrum. Das begrenzt insbesondere die Breite der Anzeige.
SDK und gezielte Disassembly mit dem vorhandenen MSVC-Dumpbin zeigen: Die Bounds-Berechnung geht
über `GameMode.SpawnedPlayers`; der Netzwerkclient hat keinen GameMode und fällt auf Ersatzecken
zurück. Ghidra über REA war wegen eines nicht mehr vorhandenen konfigurierten JDK-Pfads nicht verfügbar;
die Serverkonfiguration wurde nicht geändert.

`ExpPatches.cpp` korrigiert jetzt ausschließlich beim Client den nativen Bounds-Update:
Der vorhandene Einzelcontroller-Helper bekommt den lokalen PlayerController und die aktuelle
CameraComponent aus dem PlayerCameraWorldSubsystem. Aus den vier Bodenecken und dem originalen
`ExpBoundsBuffer` entstehen CPU-Grenze und dieselbe Niagara-Variable. Host, Solo, Sammelradius und
XP-Vergabe bleiben unverändert. Bei neuen Spawn-Nachrichten wird eine zu früh fehlende Kamera
erneut geprüft. Ungültige/degenerierte Projektionen werden nicht übernommen.

**Testgrenze:** Geometrie-Regressionstests und Build prüfen die Umsetzung; ein sichtbarer
Host-/Client-Vergleich mit diesem neuen Hook sowie die vollständige Einsammel-Abnahme stehen noch aus.
Die laufende Sitzung des Nutzers wurde dafür nicht neu gestartet. `xp-baelle` bleibt offen.

**Gegenprobe im Spiel (`xp-baelle-live`, 06.11.2026, Build `26187885`).** `Start-Session.ps1 -Restart -Mod -Tunnel Lobby
-Godmode -Brief`, `Run-Route.ps1 -Days 1 -StayLast`, Messung mit einer lesenden Probe (Scratchpad, nicht im Repo) und Bildern
beider Fenster. Code unverändert, keine Korrektur nötig, `ModVersion` bleibt 6.

- **[Belegt]** Die Sicht-Halbausdehnung des Handlers (`+0x320`) ist beim Client jetzt gleich der des Hosts: `(4370.4, 4851.5)`,
  vorher `(1050, 5750)`. Das Spielerzentrum (`+0x338`) ist auf beiden Seiten der Mittelpunkt beider Figuren, auch bei
  rund 3200 Einheiten Abstand zwischen Host- und Client-Figur.
- **[Belegt]** Bälle am Bildschirmrand: Bei auseinanderstehenden Figuren lagen die gelben XP-Bälle am linken und am rechten
  Rand (x ≈ 65 bzw. 700–770 von 800) im Client-Bild an denselben Stellen wie beim Host. Client-Log: „XP-Baelle: 1 Host-Ausstoss(e) angezeigt“.
- **[Belegt]** Aufsammeln: Die Client-Figur lief durch eine Gruppe Bälle. Danach fehlten sie auf beiden Bildern, die übrigen
  Bälle lagen weiter an denselben Stellen.
- **[Belegt]** Keine doppelte XP: Level und XP-Wert im `LevelSystemComponent` (`+0xF0` Level, `+0xF8` XP, Bedeutung aus dem
  Verlauf abgeleitet) waren auf beiden Seiten gleich, etwa Level 3/119, 4/166, 5/340. Die kleinen Abweichungen von 10 bis 30 XP
  passen dazu, dass die Probe die beiden Prozesse nacheinander liest. Die HUD-Leiste zeigte auf beiden Seiten denselben Anteil.
  Die Log-Zeile „Aufnahme beim Client nur angezeigt“ kam nicht vor. Der Client hat `ReceiveExp` in diesem Test also nicht aufgerufen.
- Gegenproben: Bewegung und Dodge (Richtungstaste mit Leertaste) wirkten je Seite nur auf die eigene Figur. Zwei Level-up-Airdrops
  hatten auf beiden Seiten dieselbe Auswahl, es wurde zuerst beim Client und dann beim Host gewählt, danach lag derselbe Gegenstand
  in beiden Rucksäcken. Lager am Ende von Tag 1 über `Complete-RoundEnd` bei beiden bestätigt, beide auf der Routenkarte. Kein Absturz.
- Nebenbefunde, nicht behoben: Kurze `Desync`-Warnungen (Gegner 43/42, Inventar während der Airdrop-Wahl), die vor dem Tagesende
  aufhörten. Den weißen Gegnerschutt (Knochen, Blüten) zeigt nur der Host. Das Client-HUD zeigt oben eine Schrottanzeige (etwa
  „9.339“), der Host an derselben Stelle „9“. Beides gehört nicht zu den XP-Bällen.
- Nicht geprüft: Latenzprofil `-NetProfile Internet`, ein Wiederbeitritt mitten am Tag und zwei PCs.

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
- **Spike-Ersatz für den Kontrollkanal:** Der Host schreibt die Werte in `mapgen.txt` im Befehlsordner des Spikes, der Client auf demselben PC liest sie. Kommt der Client vorher an, wartet seine Erzeugung und startet aus dem Sekunden-Tick.
- **Über das Netz (`karte-tunnel`):** Mit dem Steam-Tunnel schickt der Host-Tunnel jede neue Fassung von `mapgen.txt` zuverlässig an den Client-Tunnel, der sie bei sich ablegt, siehe [Steam-Tunnel](../research/spikes/a-connect/README.md#spike-e-steam-tunnel-tunnel).
  - **[Belegt]** Ohne das blieb der Client beim Test auf zwei PCs (04.11.2026) bei `MapGen: Client wartet auf die Host-Karte` stehen. Er erzeugte keine Karte, und die Figuren fielen in der Mission durch den Boden. Der Tunnel-Bericht war trotzdem grün, weil er nur Verbindung, Ping und Absturzfreiheit prüft.
  - **[Belegt]** Auf einem PC mit `-Tunnel Ip` kam die Karte über den Tunnel an (`Tunnel: Host-Karte empfangen`), der Client übernahm Seed und Profil, Inventare an Tag 1 gleich, 0 verworfene Pakete.
  - Ohne Tunnel, also bei direkter Verbindung über `-Peer <Adresse>:7777`, geht die Karte weiterhin nur auf einem PC. Das löst der Kontrollkanal (`protokoll`).

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
- ~~Die Werte müssen über einen echten Kanal zum Client.~~ Erledigt, siehe [Gleiche Karte in der Mod](#gleiche-karte-in-der-mod-missionsstart).
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

#### Gleiche Karte in der Mod (`missionsstart`)

Stand 05.11.2026, Build `26187885`, ein PC, zwei Instanzen, Lobby und Tunnel der Mod (`Run-Route.ps1 -Start`). Die Eingriffe aus
`MapGen.cpp` und `Pcg.cpp` stehen jetzt in `src/Game/MapPatches.cpp`. Die Werte gehen über den Kontrollkanal statt über Dateien
(`src/Sync/World/MapSync.cpp`, Typ `0x400`).

- **Host:** Nach `StartMapGeneration` liest er Seed, Profil (Paket und Name), Größe und Tags und vergibt eine zufällige Kennung. Nach
  Slot 85 des PCG-Generators kommt der Graph dazu oder „kein Graph“. Der Kontrollkanal schickt den Stand bei jeder Änderung und jede
  Sekunde; nach einem Kartenwechsel ohne neue Erzeugung schickt er „keine Karte“.
- **Client:** `StartMapGeneration` wartet, bis Werte mit einer noch nicht übernommenen Kennung da sind, und höchstens 5 s länger auf den
  Graphen. Jede Kennung gilt nur einmal, alte Werte passen so nie zur nächsten Karte. Ausnahme: Versucht das Spiel die Erzeugung in derselben Welt mit demselben Generator neu (`StartMapGenerationWithRetryAttempts`, `TryToRegenerate`), nimmt der Client dieselben Werte noch einmal. Kommen 60 s lang keine Werte, erzeugt der Client
  eine eigene Karte (Log `Karte: nach 60 s keine Werte vom Host`), statt ewig zu laden.
- **Child Actors und PCG-Seeds** wie im Spike. Die Seeds setzt die Mod nur zu zweit, allein bleibt die Vegetation wie ohne Mod.

| Lauf | Profil | Warten beim Client | Kacheln Host / Client | Ergebnis |
|---|---|---|---|---|
| Netzprofil Internet (120 ms ±30, 2 % Verlust), Tag 1 | `MGP_City_LittleItaly`, Tag `Rain` | 1.610 ms, davon die meiste Zeit auf den Graphen | 159 / 159 | `check_round` sauber, Inventare gleich, PCG-Graph und 7 Seeds übernommen |
| Ohne Netzprofil, Tag 1 | dasselbe | 2.015 ms | 159 / 159 | sauber |
| Netzprofil Internet, Tag 1 und 2 | Tag 2 `MGP_City` | 1.485 und 438 ms | 159 / 159, 63 / 63 | sauber, die zweite Karte mit neuer Kennung |
| Nur Spike zum Vergleich | `MGP_City_LittleItaly` | – | 159 / 159 | sauber |
| Nur Mod, ohne `spike.dll` (`-NoSpike`), Zufalls-Seed | `MGP_GolfCourse`, Profil beim Client nachgeladen | 1.109 ms | 96 / 96 | sauber, Wald und Baumarten im Spielbild gleich |

- **[Belegt]** Nicht replizierte Actors gleich bis auf Host-Logik (`BP_ChestLootSpot_C`, Loot-Kisten, `SeekingAIController`) und
  Laternen. Die Laternen fehlten im Abgleich teils beim Client, eine Minute später gab es auf keiner Seite mehr Laternen-Instanzen. Der
  Abgleich ist also ein Schnappschuss laufender Objekte, kein Unterschied der Erzeugung. Replizierte Objekte unterscheiden sich nur
  nach Netz-Reichweite.
- **[Belegt]** Im Spielbild stimmen Straßen, Gebäude, Fahrzeuge und Bus überein. Kleine Abweichungen blieben: ein Baum in anderer Art,
  ein anderes Dach, ein Pickup nur beim Client. **[Vermutung]** Zufällige Varianten einzelner Child Actors (`RandomChildActorSwapWrapper`)
  oder ein Objekt, das beim Host schon zerstört war. Kleine Fehler, siehe `opt-karte-details`.
- **Nicht geprüft:** zwei PCs, Bosskarten, Biome außer Stadt und Wald.
- Mit `-Mod` und dem Tunnel des Spikes (`-Tunnel Ip`, `P2p`, `None`) fehlt der Kontrollkanal, der Client erzeugt dann nach 60 s eine
  eigene Karte. `Start-Session.ps1` warnt davor, `Test-TwoPc.ps1 -Mod` bricht ab.

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

### Ein Gegnertyp synchron (`gegnertyp`)

Stand 06.11.2026, Build `26187885`, eine Sitzung `Start-Session.ps1 -Mod -Tunnel Lobby -Godmode` und `Run-Route.ps1 -Days 1 -StayLast`,
Tag 1 mit `BP_StandardEnemy_C` als einzigem Gegnertyp. Gemessen lesend mit `tools/spike/enemy_sync.py` (je Viertelsekunde,
Objektliste je Sekunde neu) und dem Desync-Melder.

- **[Belegt]** Spawn und Tod laufen über die native Actor-Replikation, ohne eigenen Code. Der Host entscheidet allein
  (`AEnemySpawner`, Schaden, Tod), der Client sieht die Gegner als `SimulatedProxy`. Bildpaar Host/Client: dieselben Gegner an
  denselben Orten; beim Client fehlen nur die gepoolten Tod-Effekte (`opt-blut`, `vfx-sound`).
- **[Belegt]** Tod: In fünf Messungen von 90 bis 360 s starben beim Host 48, 155, 111, 139 und 186 Gegner, beim Client 48, 150,
  111, 139 und 178. Zugeordnete Tode kommen beim Client im Median ohne messbare Verzögerung an (unter 0,25 s), höchstens 2,9 s später.
- **[Belegt]** Spawn: Ohne Welle kommen neue Gegner im selben Messschritt an (70 von 70 zugeordnet, keiner über 2 s). Unter
  `!wave`-Last erscheinen einzelne neue Gegner beim Client bis rund 8 s später, sie spawnen am Rand des Bildes. Das steht als
  `opt-gegner-spawnverzug`. Fehlende Zuordnungen und Tode in der Zählung des Werkzeugs sind vor allem Gegner, die zwischen zwei
  Neuaufnahmen der Objektliste entstehen und sterben; die Proben beider Seiten liegen nicht im selben Frame.
- **[Belegt]** Gegner-HP repliziert beim Client nicht: `CommonAttributeSet` (`Actor+0x700`) steht dort auf 0/0, beim Host auf 50/50.
  `OnRep_Health` läuft für Gegner also nie, ein lokaler Auslöser „HP 0“ wäre falsch. Lebensleisten, Treffer und Tod brauchen den
  Wert beim Client bisher nicht.
- **[Belegt] Ursache von `opt-gegnerzahl`:** Getötete Gegner tragen auf **beiden** Seiten bis zur Garbage Collection noch
  `bActorIsBeingDestroyed` (Host zum Beispiel 11, Client 12 gleichzeitig). Sie sind schon aus `Level->Actors` entfernt und
  unsichtbar. Zählungen über das Objekt-Array (`objects.py`) schließen sie ein und schwanken mit dem GC-Zeitpunkt. Die Zählung
  des Desync-Melders über `Level->Actors` war in 24 von 40 Halbsekunden-Proben gleich, in ruhigen Phasen durchgehend (25/25).
  Ausblenden toter Gegner ist darum nicht nötig.
- **[Belegt] Desync-Melder mit Gegnerzahl:** Im Lauf mit `desync_ignore_enemies=false` (rund 6 min, eine Welle) kein Alarm,
  „offen Gegner“ jeweils wenige Sekunden. Ein zweiter Lauf (rund 6 min, eine Welle) bestätigte einmal 31 zu 30 Gegner für etwa
  2 s, dann wieder OK: ein spät replizierter Spawn. Seitdem ist die Gegnerzahl standardmäßig im Abgleich und braucht 8 statt
  3 Bestätigungen (rund 16 s). Abnahmelauf damit: rund 7 min, zwei Wellen, 221 Spawns und 186 Tode beim Host, kein Alarm, kein
  Absturz, auf beiden Seiten 25-mal „OK … Gegner“ im Log.
- Nicht geprüft: andere Gegnertypen, Bosse und Boss-Phasen (`gegner-alle`), Latenz über das Internet, Reconnect. Eigene
  Nachrichten oder Dateien in `src/Sync/Enemies` gibt es nicht, die Entscheidung steht in dessen README.

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

**Entscheidung vom 03.11.2026:** Auf ausdrückliche Vorgabe des Projektverantwortlichen gilt vorerst **Option A: gemeinsame Kamera** für den ersten spielbaren Stand. Ursprünglicher Zoom und Bewegungsbegrenzung bleiben erhalten. Umgesetzt und geprüft mit `kamera-code`, siehe [Gemeinsame Kamera in der Mod](#gemeinsame-kamera-in-der-mod-kamera-code). Getrennte Kameras (Option B) sind als optionales Zukunfts-TODO erfasst und keine Voraussetzung für diesen Stand.

**Bewertete Optionen**

| Option | Vorteile | Nachteile |
|---|---|---|
| A: Gemeinsame Kamera wie im lokalen Coop | Entspricht dem Spieldesign: Zoom, Bewegungsgrenze und Spawnen an den Rändern passen ohne Änderung. Läuft heute schon mit dem Spike-Patch. Kein zusätzlicher Netzwerkverkehr | Beide Spieler sind aneinander gebunden wie am selben Bildschirm. Die Client-Kamera ist eine eigene Berechnung und kann leicht abweichen. Wegen der Latenz sieht der Client den Rand anders als der Host |
| B: Getrennte Kameras je Spieler | Volle Freiheit, jeder sieht seine Umgebung, mehr Platz auf der Karte | Bewegungsgrenze abschalten, Kamera beim Client und beim Host je Spieler neu bauen. Spawnen an den Rändern kennt nur den Host-Viewport, der Client müsste seine Ansicht melden (eigener Sync), oder das Spawnen bekommt einen eigenen Ersatz. Gegnerzahl und Schwierigkeit sind für einen gemeinsamen Bereich ausgelegt |
| C: Gemeinsame Kamera mit gelockerter Grenze | Wenig Aufwand: Restrictor-Zonen und `ZoomOutDistance` anpassen, mehr Abstand möglich | Bleibt ein gemeinsames Bild. Stärkeres Herauszoomen macht alles kleiner, Spawnen und Sichtbarkeit bleiben an das gemeinsame Bild gebunden |

**Empfehlung:** Option A für den ersten spielbaren Stand. Sie ist schon lauffähig, braucht keine neuen Systeme und passt zu Spawnen, Zoom und Bewegungsgrenze des Spiels.
Den Abgleich der Client-Kamera mit der des Hosts nur angehen, falls die Abweichung im Spiel stört. Option C kann danach als Feinschliff kommen. Option B erst, wenn das Team ausdrücklich freie Bewegung will, denn sie zieht einen eigenen Sync der Viewports nach sich.

### Gemeinsame Kamera in der Mod (`kamera-code`)

Stand 05.11.2026, Build `26187885`, ein PC, Lobby und Tunnel der Mod, Tag 1, nur Mod-Patches (`CameraPatches.cpp`: Kamera-Spieler aus der
`PlayerArray`, gemeinsame Kamera als ViewTarget, Restrictor mit nachgetragenem GameState). Gemessen alle 0,25 s auf beiden Seiten:
Figuren, `BP_PlayerCameraActor_C`, Spring-Arm-Länge und Sichtfeld. Die Figuren liefen per Tastendruck auseinander, jeweils eine Seite
nach der anderen.

| Lauf | Kamera Host / Client | Bewegungsgrenze | Zoom |
|---|---|---|---|
| Ohne Netzprofil | höchstens 21 Einheiten auseinander | Client-Figur hielt 1.620 Einheiten neben der Kameramitte, Host-Figur 1.621 in der Gegenrichtung | Arm 8000, Sichtfeld 40 auf beiden Seiten |
| Netzprofil Internet (120 ms ±30, 2 % Verlust) | höchstens 28 Einheiten auseinander | quer wieder ±1.620, hoch und runter beide bei ±850; Abstand der Figuren auf beiden Seiten bis auf 21 Einheiten gleich | unverändert, auf beiden Seiten gleich |

- **[Belegt]** Der Restrictor stoppt die Figur des Clients beim Client und die des Hosts beim Host, symmetrisch zur gemeinsamen
  Kameramitte. Keiner kann aus dem gemeinsamen Bild laufen. Im Spielbild zeigen beide Seiten denselben Ausschnitt.
- **[Belegt]** Ein eigener Abgleich der Client-Kamera mit dem Host ist nicht nötig: Die Abweichung bleibt auch mit Latenz unter 30 Einheiten
  bei rund 3.300 Einheiten Abstand der Figuren.
- Nicht gesondert beobachtet: das Herauszoomen am Rand (`UseZoomOutAtEdge`), der Arm blieb in beiden Läufen bei 8000. Nicht geprüft: zwei
  PCs, spätere Tage. Kleine Bildunterschiede sind Kartendetails (`opt-karte-details`) und Host-Effekte (`opt-blut`).

### ViewTarget beim Client (`kamera-viewtarget`)

Stand: Spieltest am 04.11.2026, Build `26187885`, ein PC über Loopback, Route über das Spielmenü mit Seed `20795`.

- **[Belegt]** Vorher schaute der Client in der Mission nicht durch die gemeinsame Kamera, sondern aus der eigenen Figur: ViewTarget des lokalen Controllers war `BP_DD_Character_C`, die Blickposition lag 64 Einheiten über der Figur. Im Bild sah man nur Boden aus der Nähe. Der Host schaute die ganze Zeit korrekt durch `BP_PlayerCameraActor_C`.
- **[Belegt]** Ursache im Spike: `ViewThroughSharedCamera` nahm den ersten `BP_DDPlayerController_C` im Objekt-Array. Kurz nach einem Kartenwechsel lebt der alte Controller ohne `LocalPlayer` noch und steht davor. Die 15 Wiederholungen landeten an ihm, das Log meldete trotzdem „Kamera: gesetzt“. Danach griff auch `!cam` nicht mehr, weil nur eine neue Kamera die Wiederholungen neu startete.
- **Fix in `Camera.cpp`:** nur Controller mit `LocalPlayer` (`+0x348`) und die Kamera aus `PersistentLevel` der aktuellen Welt. Jede Sekunde prüft der Spike das ViewTarget (`PlayerCameraManager+0x340`) und setzt es nur, wenn es nicht die Kamera ist. Das Log nennt das vorherige Ziel.
- **[Belegt]** Mit dem Fix: Je Weltwechsel genau ein „Kamera: gesetzt, vorher BP_DD_Character_C“, also beim Betreten von MetaBase, Tag 1 und Tag 2. Danach blieb das ViewTarget die Kamera. Die Blickposition des Clients war gleich der des Hosts, die Bilder fast gleich. Lief die Client-Figur 2,5 s, folgte die Client-Kamera ruhig und lag höchstens rund 10 Einheiten neben der des Hosts.
- Nicht geprüft: weitere Tage, Internet, Mission → MetaBase.

### Dodge beim Client (`bewegung`)

Stand 05.11.2026, Build `26187885`, ein PC, Mod über die Lobby, Netzprofil Internet (120 ms, 2 % Verlust), Tag 1. Gemessen mit
lokalen, nur lesenden Testskripten alle 20 ms, die Dodges hat ein Mensch in beiden Fenstern ausgelöst.

- **[Belegt]** Der Dodge ist `GA_Dash_C` (`LocalPredicted`, eine Instanz je Ausführung). Für etwa 0,12 bis 0,16 s setzt er an der
  Bewegungskomponente `GroundFriction` 0 und `GravityScale` 20, schiebt die Figur mit Tempo 6500 an
  (`UAbilityTask_ApplyRootMotionConstantForce`) und stellt beides danach zurück (`RestoreBeforeDashSettings`).
- **[Belegt] Client ohne Trägheit:** Beim Host kamen die Werte nach jedem Dodge zurück. Beim Client nach drei Dodges nicht mehr: Reibung 0
  und Schwerkraft 20 blieben stehen, jeder weitere Dodge rutschte bis zu 2,3 s mit sinkendem Tempo weiter. In einer zweiten Messung trat
  das nicht auf. Beim Host lief der Dodge des Clients jeweils mit (Spec-ActiveCount 0/1), seine Kopie der Client-Figur änderte die
  Werte aber nie. **Vermutung:** Endet die vorhergesagte Fähigkeit beim Client über eine Bestätigung oder Ablehnung des Hosts statt über
  ihre Aufgabe, fehlt das Zurückstellen; mit Ping häufiger.
- **[Belegt] Host-Figur friert beim Client ein:** Nach einem Dodge des Hosts stand seine Kopie beim Client in `MOVE_None` oder
  `MOVE_Falling` mit Tempo 6500 und bewegte sich nicht mehr, obwohl `ReplicatedMovement.Location` den richtigen Host-Ort trug. Ursache: Der
  Host stand danach auf `BP_Fog_Edge_BASE_C`, einer beweglichen, nicht replizierten Unterlage. Die Engine repliziert den Ort dann relativ
  zur Unterlage (`ReplicatedBasedMovement`, `bServerHasBaseComponent`=1); beim Client ist `MovementBase` leer, und die Kopie wartet ohne
  Ende darauf. Zeitlich fiel das zweimal mit einer Weltpause durch ein Item-Panel zusammen; **Vermutung:** Der Dodge schob den Host an
  den Rand, die Pause selbst ist nicht die Ursache.
- **[Belegt] Nebenbei:** Beim Client stehen `MaxWalkSpeed` und `MaxAcceleration` beider Figuren auf 600 und 2048, beim Host auf 630 und
  2205 (`opt-tempo-upgrades`). Beim Host hatte die Kopie der Client-Figur zeitweise nur ein Zehntel davon.

**In der Mod** (`src/Game/MovementPatches.cpp`, nur Client, jeder Frame, Figurenliste je Sekunde):

| Fall | Reparatur |
|---|---|
| Eigene Figur länger als 0,75 s auf Reibung 0 und Schwerkraft 20 | Werte aus dem Klassen-Defaultobjekt der Bewegungskomponente zurück |
| Fremde Figur steht auf einer Unterlage, die der Client nicht kennt | Jeden Frame weich zum replizierten Ort, mit dem replizierten Tempo höchstens 150 ms vorausgerechnet; Tempo und Modus übernommen, ab 6 m Abstand Sprung |
| Fremde Figur 0,5 s in `MOVE_None` oder mehr als 3 m neben dem replizierten Ort | Ort, Tempo und Modus aus der Replikation |

Im Spiel danach: Der Host dodgte an den Rand und in Items, seine Figur folgte beim Client (Log „folge dem Host-Ort“, „Engine übernimmt“,
wenn die Unterlage wieder bekannt ist). Mit Vorausberechnung bestätigt der Mensch „passt“, vorher „etwas laggy“. Kein Absturz, Inventare
gleich. **Nicht geprüft:** zwei PCs, Blickrichtung gesondert (nur im Spielbild mitgesehen). Sauberer wäre, wenn der Host bei einer nicht
replizierten Unterlage gleich absolut repliziert; dann übernähme die Glättung der Engine. Das braucht einen Eingriff in die Replikation
des Characters und ist nicht umgesetzt.

### Dodge des Clients beim Host (`ausweichen-client`)

Stand 06.11.2026, Build `26187885`, ein PC, Mod über die Lobby, Netzprofil Internet (120 ms, 2 % Verlust), Tag 1. Gemessen mit
lokalen, nur lesenden Testskripten und einem vorübergehenden Protokoll in der Mod, die Dodges hat ein Mensch ausgelöst.

- **[Belegt] Symptom:** Ein kurzer Druck auf die Leertaste lief beim Client wie beim Host. Bei gehaltener Leertaste rutschte die Figur
  weit wie auf Eis, und der Client konnte ohne Grenze ausweichen, obwohl jeder Spieler nur zwei Ladungen Ausdauer hat, die sich langsam
  wieder füllen. Die Anzeige sprang kurz nach unten und sofort wieder voll.
- **[Belegt] Ursache:** `GA_Dash_C` liest die Laufrichtung über `DDPlayerController.GetLastNormalizedMovementInput`
  (`+0x910`). Die setzt nur die lokale Eingabe. Beim Host steht sie für den Controller des Clients immer auf (0, 0), sein eigener
  Controller-Code überschreibt sie jeden Frame. Der Dodge bricht beim Host darum vor Kosten (`GE_DashCost`) und Schub ab. Beim Client
  rollt die Vorhersage die Kosten zurück: unbegrenzte Ausdauer. Gehaltene Leertaste kettet vorhergesagte Dodges, deren
  „Vorher“-Werte schon die Dodge-Werte (Reibung 0, Schwerkraft 20) sind, daher das Rutschen.
- **[Belegt]** Beim Host sind Tempo und Beschleunigung der Client-Figur fast immer 0. Verlässlich ist nur ihr replizierter Ort.
- **[Belegt]** Ein Dodge aus dem Stand bricht auch beim Host ab (Eingabe 0). Das ist das normale Verhalten und bleibt so.
- **Gescheitert:** Die Laufrichtung jeden Frame in `+0x910` schreiben. Der Host überschrieb sie im selben Frame wieder mit (0, 0), der
  Dodge des Clients brach weiter ab (Protokoll „activate … task=false input=(0.00,0.00)“).

**In der Mod** (`src/Game/MovementPatches.cpp`, nur Host): Je Tick misst die Mod alle 50 ms die Ortsänderung jeder Figur, die ein
Client steuert. Startet `GA_Dash_C.K2_ActivateAbility` für diese Figur, trägt der ProcessEvent-Hook direkt vorher die daraus
geschätzte, normierte Laufrichtung in `+0x910` ein (Eingabe (X, Y) läuft in Weltrichtung (Y, X)). Das Tempo muss über 50 Einheiten
je Sekunde liegen und gilt noch 200 ms, weil der Ort in Schüben ankommt. Danach führt der Host den Dodge nativ aus: Kosten, Schub, Ende.
Die Ausdauer des Clients repliziert wie gewohnt, eine eigene Nachricht gibt es nicht. Die Figur der Fähigkeit erkennt die Mod am
Outer der Instanz (Figur oder ihr PlayerState).

Im Spiel danach: Protokoll beim Host 5 von 5 Client-Dodges mit Aufgabe und Richtung. Der Mensch bestätigt: Weite und Abbremsen wie
beim Host, Ausdauer wird verbraucht und lädt nach, gehaltene Leertaste rutscht nicht mehr. **Nicht geprüft:** zwei PCs, mehr als
ein Client.

- **[Beobachtet] Nebenbei:** Der Tod einzelner Spieler funktioniert, die Runde endet erst, wenn alle Spieler tot sind.
- **[Beobachtet] Nebenbei:** Dodgt der Client in einen Airdrop, stürzt er manchmal in `InventoryPanel.SetupInventory` ab
  (`InventoryComponent` an `+0x370` leer), nicht reproduzierbar. Steht als `airdrop-dodge-absturz`.
  **[Belegt, Disassembly]** `SetupInventory` (`0x5A8BAB0`) reicht `+0x370` ohne Prüfung an `0x59635B0` weiter. Keiner der acht
  direkten Aufrufer prüft es vorher, mehrere sind Ereignis-Handler des Panels. **[Vermutung]** Ein solches Ereignis feuert beim Client,
  bevor das Panel sein Inventar hat. Seit `airdrop-dodge-absturz` lässt die Mod `SetupInventory` ohne Inventar aus und schreibt
  „SetupInventory ohne Inventar ausgelassen“ mit der RVA des Aufrufers ins Log, damit der Weg beim nächsten Auftreten belegt ist.

### Dodge im Zwei-PC-Test (`dodge-client-live`)

Stand 06.11.2026, Auswertung der archivierten Mod-Logs aus den beiden oben
beschriebenen Live-Sitzungen, ohne neuen Spielstart:

- **[Belegt, Client, zweite Sitzung]** 29 Meldungen ueber zurueckgesetzte
  Dodge-Werte zwischen 18:58:34 und 19:13:58 UTC. Im Kampf ab 19:10 treten sie
  wiederholt auf, einmal schon nach 801 ms erneut. Der lokale Rueckfall-Fix
  fehlt also nicht; wiederholtes Ruecksetzen ersetzt aber kein korrektes
  natives Ende und beweist keinen Ausdauerverbrauch.
- **[Belegt, Code]** Die bisherige Host-Meldung erscheint nach erfolgreichem
  Schreiben der Richtung nur einmal je Figur. Auch der Nullvektor wird
  erfolgreich geschrieben: Fehlt eine gemessene Bewegung oder ist sie aelter
  als 200 ms, bleibt die Richtung (0, 0). Diese Meldung belegt weder eine
  gueltige Richtung fuer jeden Dodge noch Ausdauerkosten oder Ability-Abschluss.
- **[Grenze]** Host-Log der ersten und Client-Log der zweiten Sitzung sind
  kein zeitgleiches Paar. Aus ihnen laesst sich nicht entscheiden, ob der
  jeweilige Host eine veraltete Richtung liefert, der native Dodge anders
  abbricht oder das Wiederherstellen beim Client fehlschlaegt. Eine Aenderung
  von Richtungshaltezeit, Ausdauer oder Dodge-Ende waere noch unbegruendet.

**Diagnose im Arbeitsstand:** `MovementPatches.cpp` meldet jetzt jede erkannte
Client-Aktivierung beim Host mit laufender Nummer, tatsaechlich geschriebener
Richtung, Bewegungs-/Probenalter, Grund fuer eine Nullrichtung und
Schreibergebnis. Eine nicht zugeordnete Figur wird separat gemeldet, die
gefundene `GA_Dash`-UFunction einmal je Welt. Der Client zaehlt erfolgreiche
Ruecksetzungen mit Dauer und den alten Reibungs-/Schwerkraftwerten. Keine neue
Nachricht, kein neuer Hook, keine neuen Offsets; native Vorhersage, Kosten und
die bisherige Richtungsregel bleiben unveraendert. Die Meldung
`Client-Dodge beim Host vorbereitet` bedeutet ausdruecklich **nicht erfolgreich
ausgefuehrt**.

**Ohne Spiel geprueft:** Release-Build mit `Build-Mod.ps1 -Test`: 192 C++-Tests
(davon 59 Game-Tests) und 140 Python-Tests bestanden. Die neue reine Diagnose-
Regel prueft insbesondere gueltige Richtung bei 200 ms, veraltete Richtung ab
201 ms, fehlende Bewegung und Uebereinstimmung mit dem geschriebenen Vektor.
`task_status.py check` und `git diff --check` bestehen. Der erste Gesamtaufruf
wurde nach erfolgreichem Build am 120-s-Werkzeuglimit abgebrochen; der erneute
vollstaendige Lauf mit groesserem Zeitbudget bestand. Keine neue Spielabnahme.

**Naechste Abnahme auf zwei PCs:** identischen Arbeitsstand verwenden und beide
Logs derselben Sitzung sichern. Zuerst zehn getrennte Dodges beim Client mit
vorheriger Bewegung, dazwischen Ausdauer nachladen lassen; danach Gegenprobe
mit gehaltener Leertaste, Richtungswechsel unmittelbar vor dem Dodge und ein
Dodge aus dem Stand (natives Abbrechen bleibt erlaubt). Je Versuch Richtung,
Ausdauerverbrauch und -regeneration, Weite und Abbremsen beobachten und mit
dem Host vergleichen. Nullrichtungen/Schreibfehler und Client-Ruecksetzungen
zeitlich zuordnen. Nach Rundenwechsel wiederholen, um Welt-Neuzuordnung zu
pruefen. Erst mit erklaerter Ursache und zehn Dodges ohne Rutschen und mit
Ausdauer wie beim Host ist `dodge-client-live` erledigt.

**Ursache und Fix, 07.11.2026** (`dodge-client-fix`, Build `26187885`, ein PC, Mod über die Lobby, Netzprofil
Internet, Tag 1). Gemessen mit einem vorübergehenden Protokoll jedes `GA_Dash`-Ereignisses auf beiden Seiten
(`K2_ActivateAbility`, `K2_CommitExecute`, Rückruf der Aufgabe, `K2_OnEndAbility`) und lesend beim Host; die Dodges
löste ein Testskript aus (Richtungstaste halten, dann Leertaste).

- **[Belegt] Ursache:** Die Richtung, die der Host aus der Ortsänderung schätzt, weicht in beide Richtungen vom
  echten Dodge des Clients ab. (1) Der Client dodgt, die Schätzung ist veraltet (`Bewegungsalter=219 ms`): Der Host
  schreibt (0, 0), seine Instanz endet ohne `K2_CommitExecute`, der Client bucht die Kosten nur vorhergesagt und bekommt
  sie zurück; seine Dodge-Werte blieben dabei länger stehen (333 statt 135 ms). (2) Der Client bricht ab (sechs
  Aktivierungen ohne Commit, Eingabe (0, 0)), der Host schätzt „in Bewegung“, bucht Ausdauer und schiebt die Figur.
  In den Sitzungen vom 06.11. hing so schon der erste Client-Dodge genau 750 ms nach der Host-Meldung.
- **[Belegt]** Die Engine liefert die Eingabe des Clients beim Host nicht: Beschleunigung (`+0x338`) und Tempo der
  Client-Figur standen beim Host während Laufen und Dodge des Clients auf 0, die Laufrichtung im Controller ebenso.
  Eine native Quelle gibt es also nicht.
- **[Belegt]** Der Vorhersageschlüssel der Aktivierung (`CurrentActivationInfo.PredictionKeyWhenActivated.Current`)
  ist bei Client und Host gleich, er zählt je Sitzung hoch (1, 2, 3 …).
- **[Belegt] Abgebrochener Dodge:** Endet `GA_Dash` nicht über seine Aufgabe (der Host lehnt eine verkettete
  Aktivierung bei gehaltener Leertaste ab, oder die Aktivierung kommt beim Host erst nach dem Ende beim Client an,
  einmal 214 ms Verzug), stellt es Reibung und Schwerkraft nicht zurück. Der nächste Dodge übernimmt dann 0 und 20 als
  „vorher“. Das betrifft beim Client die eigene Figur und beim Host seine Kopie der Client-Figur.

**Fix (lokal zuerst, minimal geteilt):** Der Client meldet je eigener Aktivierung genau die Eingabe, die sein `GA_Dash`
liest, mit dem Vorhersageschlüssel (`0x500`, [Protokoll](CONTROL_PROTOCOL.md#dodge-richtung-mod-kompatibilität-8)). Der
Host trägt sie vor dem nativen Start ein; ist die Meldung noch unterwegs, hält er die Aktivierung höchstens 250 ms an.
Die Schätzung bleibt nur Rückfall. Kosten, Schub, Ende und Ausdauer bleiben nativ beim Host. Der Client setzt die
Dodge-Werte sofort zurück, sobald kein eigener Dodge mehr läuft, und vor jedem neuen Dodge; der Host seine Kopie der
Client-Figur nach 750 ms. Die alte Zeitgrenze von 750 ms bleibt beim Client als Rückfall.

**Im Spiel danach (ein PC, Internet-Profil):** 10 von 10 getrennten Client-Dodges kamen beim Host mit der Richtung des
Clients an (Schlüssel 1 bis 10), meist nach 15 bis 47 ms Halt, weil das Spielpaket die Meldung überholte. Kein
Rückfall auf die Schätzung, keine Rücksetzung beim Client. Nach zwei schnellen Dodges startete der Client keinen
dritten, die Ausdauer lud langsam nach, ohne zurückzuspringen. Richtung und Leertaste gleichzeitig: richtige Richtung.
Aus dem Stand: elf Aktivierungen mit (0, 0) auf beiden Seiten, kein Commit. Gehaltene Leertaste: eine vom Host
abgelehnte Aktivierung, Werte beim Client nach 164 ms statt 750 ms zurückgesetzt. Nebenbei stürzte der Client einmal
beim Öffnen einer Airdrop-Auswahl in `InventoryPanel.SetupInventory` ab (`0x5A8BB36`), das ist
`airdrop-dodge-absturz` und nicht dieser Weg. **Nicht geprüft:** zwei PCs über echtes Internet, Gefühl von Weite und
Abbremsen durch einen Menschen, mehr als ein Client.

### Tod des Clients (`tod-client`)

Gemeldet aus dem Live-Spiel auf zwei PCs am 07.11.2026: Beim Host blieb eine tote Client-Figur liegen, beim Client ließ sie
sich weiter steuern. Untersucht am selben Tag, Build `26495302`, ein PC, nur die Mod (`-NoSpike`, auf diesem Build gibt es
keinen Spike und keine Cheats), Tag 1. Ein Mensch spielte die Tode, eine lesende Probe las alle 0,5 s beide Figuren in beiden
Instanzen über die Reflexion (HP, Tempo-Attribut, `MaxWalkSpeed`, Ort, Tempo).

- **[Belegt] Tod heißt im Spiel niedergestreckt, nicht gesperrt.** Der Host setzt beim Tod das replizierte Attribut
  `UCommonAttributeSet.MovementSpeed` von 630 auf ein Zehntel (35 bis 63), die Figur kriecht. An der Todesstelle legt er den
  replizierten Lootspot `BP_EnvironmentObjectLootspot_ReviveVariant_C` an, dazu `BP_Revive_Player_Interactable_C` (nicht
  repliziert). Steht der andere Spieler lange genug dort, füllt sich ein Balken, die Figur steht mit halben HP wieder auf.
  Stirbt der letzte Spieler (`LastOneToDie`), folgt die Niederlage wie unter [Niederlage](#niederlage-niederlage).
- **[Belegt] Ursache des Bugs:** `MaxWalkSpeed` folgt dem Attribut nur beim Host (`OnMovementSpeedChanged_Server`). Beim
  Client kam 630/63 richtig an, seine Bewegungskomponente blieb aber auf dem Startwert 600. Er lief tot mit voller Geschwindigkeit
  weiter, und weil der Host die Position des Clients annimmt (`bServerAcceptClientAuthoritativePosition`), wanderte die
  kriechende Kopie beim Host mit (rund 500 Einheiten in wenigen Sekunden).
- **[Belegt] Dodge im Tod:** Der Host lehnt `GA_Dash` eines toten Spielers nativ ab (keiner der Dodges kam beim Host an).
  Die Vorhersage des Clients lief trotzdem: sieben Dodges in 0,1 s trugen die tote Figur rund 2500 Einheiten weit.
- **[Belegt] Nebenbei:** Auch lebend zeigte der Client 600 statt 630. Dieselbe Ursache gilt vermutlich für die Tempo-Unterschiede
  aus `opt-tempo-upgrades`; nach dem Fix trägt der Client auch die Trefferbremse (504, 353) mit.
- `IsDowned` (`BP_DD_Character_C`) stand auch bei lebenden Figuren auf 1 und taugt nicht als Merkmal; verlässlich sind die HP.

**In der Mod** (`src/Game/MovementPatches.cpp`, nur Client, eigene Figur), keine neue Nachricht, der Host entscheidet allein über
Tod und Wiederbelebung:

| Fall | Verhalten |
|---|---|
| Replizierter Wert von `MovementSpeed` ändert sich | `MaxWalkSpeed` der eigenen Figur auf diesen Wert, wie der native Delegate beim Host; nur bei Änderung, ungültige oder nicht initialisierte Werte bleiben unberührt |
| `GA_Dash.K2_ActivateAbility` der eigenen Figur bei replizierten HP 0 | Aktivierung nicht ausführen und nicht melden; die native Ablehnung des Hosts beendet sie |

Die Position des Clients nimmt der Host weiter an, auch im Tod: Das Kriechen ist natives Verhalten, und beide Seiten nutzen jetzt
dasselbe Tempo. Eine volle Sperre wäre eine Abweichung vom Spiel.

**Im Spiel danach** (drei Sitzungen, Mensch am Spiel):

| Prüfung | Ergebnis |
|---|---|
| Toter Client beim Client | kriecht mit 31 bis 63, in 22 s rund 80 Einheiten |
| Toter Client beim Host | Kopie am selben Ort, kein Mitwandern |
| Dodge im Tod | über 20 Aktivierungen zurückgehalten, keine Bewegung, keine beim Host |
| Einsammeln oder Auslösen im Tod | nichts eingesammelt (Mensch) |
| Wiederbelebung durch den Host | HP 50, Tempo 630, Laufen und Dodge normal, Host führte die Dodges aus |
| Niederlage (beide tot) | Wertung, „Zurück zur Basis“, beide verbunden in der `MetaBase`, kein Absturz |
| Gegenprobe | Airdrop des Clients, Dodges, Kanister, Rundenende, Lager mit Bestätigen bei beiden, Routenkarte Tag 2 |

- **[Belegt] Eigener Fehler, nicht von diesem Fix:** Rund 12 Minuten nach Missionsstart stürzte der Client ab
  (`EXCEPTION_ACCESS_VIOLATION reading 0x348`, Build `26495302` bei `0x15C0407`, Aufrufer `0x5870F4D`). `UReaperComponent`
  liest beim Start `ADDMissionGameMode` (`+0x188`) und bindet ohne Nullprüfung `OnNewPlayerSpawned` (`+0x340`); den GameMode gibt
  es beim Client nicht. Der Host stürzte 22 s später ab, nachdem der Client weg war. **Vermutung:** Der Sensenmann erscheint nach
  einer festen Zeit (`MinutesUntilReaper`), jeder Client stürzt dann ab. Das ist eine eigene Aufgabe.
- **Nicht geprüft:** zwei PCs über das Internet, Tod in Bosskämpfen, Wiederbelebung über Items (`ItemEffectReviveAndDestroy`),
  Tod des Hosts bei lebendem Client mit Wiederbelebung durch den Client, mehr als ein Client.

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

**In der Mod (`restrictor`, 05.11.2026):** Beides umgesetzt in `src/Game/CameraPatches.cpp`. Ein Null-Schutz an `0x5A17ACC` behandelt
einen fehlenden GameState wie einen Spieler (keine Bremse, Sprung nach `0x5A17B88`), und der Client trägt jede Sekunde den aktuellen
`ADDMissionGameState` in jeden Restrictor der aktuellen Welt nach („Restrictor: MissionGameState nachgetragen“).

| Test (ein PC, Mod mit Lobby, Netzprofil Internet, Tag 1) | Ergebnis |
|---|---|
| Feld beim Client dreimal auf null, sofort `D` für 1 s | Kein Absturz, Feld jeweils binnen 1 s nachgetragen |
| Feld 5 s lang immer wieder auf null (7-mal), dabei `D` 4 s gehalten | Kein Absturz, beide Instanzen laufen weiter |

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

### Remote-P2: geprüfte Anbindung und Grenze der lokalen UI

Stand 04.11.2026, Build `26187885`, nur Spike und Loopback. Die frühere Hypothese ist für den Host bestätigt: `ActivePlayers[0/1]` kann die aktuellen lokalen/Remote-Controller als schwache Verweise führen, `SetCoopEnabled(true)` liefert danach P2 und zwei aktive Spieler, ohne zweiten `ULocalPlayer`.

**Zusätzlich nötig und beobachtet:** Die native Prüfung eines lokalen P2-Geräts öffnet sonst ein blockierendes Controller-Beitrittsfenster. Ein auf den vollständigen aktuellen Listen-Host-Roster begrenzter Guard bei `0x59072AA` umgeht nur diese lokale P2-Gerätepflicht; P1-Geräteprüfung und Solo-Verlustpfad bleiben erhalten. Eine getrennt gesendete native `OnPlayerJoined`-Meldung aktualisiert die P2-Darstellung. Ein eigener veralteter Map-Eintrag wird bei fehlendem Remote-Roster mit dem nativen SparseArray-Entferner aufgeräumt und beim erneuten Beitritt ersetzt.

**Keine vollständige Coop-UI:** Rundenstatistik und Lager zeigen beim Host beide Spieler, die lokale Kontrollübergabe im Lager blockiert aber weiterhin. Der Client bleibt lokal Index 0; Host-Coop-Zustand/Router werden nicht repliziert. Netzwerk-P2 muss deshalb von lokaler Geräteidentität getrennt werden; UI-Anfragen und Ergebnisse brauchen den geplanten Kontrollkanal. Die Aufgaben `remote-spieler`, `eingaben`, `inventar-sync` und `inventar-ui` bleiben offen. Die Phase-1-Frage nach Tragfähigkeit und nötigen Patches ist beantwortet; kein neuer Vollrun-/Internet-Nachweis. [Probe, Gegenversuche, Tests und Übernahmegrenzen](../research/spikes/a-connect/RemoteP2.md).

**In der Mod (`remote-spieler`, 05.11.2026):** Die Bindung läuft jetzt ohne Befehl in `src/Game/RemoteP2Patches.cpp`, siehe
[Remote-Spieler als P2](../src/Game/README.md#remote-spieler-als-p2).

- **[Belegt]** Ohne `bCoopEnabled` genügen die `ActivePlayers`-Einträge: `GetPlayerController(1)` liefert den Remote-Controller,
  `GetActivePlayerCount()` 2. Lager, Routenkarte und ein Level-up-Airdrop laufen dabei wie ohne Mod.
- **[Belegt]** Mit `bCoopEnabled` hängt das Lager am Tagesende: beide Rucksäcke, „Bestätigen & Wechseln“, die Kontrolle geht nicht an P2.
  Mit dem Airdrop-Befund unten heißt das: Den Koop-Modus erst einschalten, wenn `airdrop-auswahl` und `inventar-ui` die Eingaben von P2
  über den Kontrollkanal liefern (`Patches::AllowRemoteCoop()`).
  **Nachtrag 06.11.2026:** Der Koop-Modus bleibt dauerhaft aus. Airdrop und Lager laufen ohne ihn über `inventar-lokal`,
  `bestaetigen-alle` und `rundenende-client`; `inventar-ui` ist damit ohne eigene Umsetzung erledigt.
- **[Belegt]** Verlassen und harter Abbruch des Clients entfernen nur die eigene P2-Bindung, `IsReconnectRequired` bleibt falsch; der
  Neubeitritt bindet den neuen Controller.

### Airdrop-Auswahl für beide Spieler (`airdrop-auswahl`)

Stand 05.11.2026, Build `26187885`, zwei Instanzen auf einem PC über die Steam-Lobby
der Mod, Tag 1, Level-up-Airdrops. Rohdaten und Probe auf `task/client-visuell`.

- **[Belegt] Ohne Koop-Modus:** Egal wer einsammelt, öffnet nur der Host das normale
  Inventar-Panel („Nimm 1 von 3 Items“), der Client sieht keine Auswahl. Die Weltpause
  kommt über die replizierten WorldSettings auch beim Client an. Die native
  Koop-Reward-Sitzung bleibt leer. Nach einer vom Client eingesammelten Kiste kann
  dessen Reward-Queue beim Host auf der alten Kiste hängen bleiben; der nächste
  Airdrop öffnete sich dann ohne Auswahl.
- **[Belegt] Mit Remote-Client als P2** (Spike `!p2 on`, siehe oben): Beim Airdrop
  startet die native Koop-Sitzung von `UCoopInventoryCoordinatorComponent`:
  `AwaitingClaims`, `eligible=[0,1]`, `RequiredClaims=2`, Host-Panel mit beiden
  Rucksäcken. Nach dem ersten Bestätigen wartet sie auf den Claim von Spieler 1, die
  Pause bleibt. Ein Host-Aufruf `ClaimReward(1, Choice)` schließt die Sitzung
  (`Completed`, `claimed=[0,1]`).
- **[Belegt] Grenzen:** Das Host-Panel zählt seine Bestätigung als Spieler 0, auch
  wenn das Item im P2-Rucksack liegt. Nach dem Code-Claim für Spieler 1 blieb das
  Host-Panel ohne Bedienmöglichkeit offen, und die Pause blieb;
  `AcknowledgeRewardUIClosed` lieferte `false`. Ohne Platzierung landete die
  P2-Wahl im Lager von Spieler 0. `TransferItem` mit Quelle `RewardChoice` und Ziel
  `P2Backpack` endete mit `InvalidRequest`.
- **[Hypothese]** Das Panel schließt nativ erst nach Übergabe der UI-Autorität an
  P2 (`RequestUIAuthorityTransfer`), die ohne lokales P2-Gerät blockiert, siehe
  Remote-P2.
- **[Belegt] Reward-Queue je Figur:** `URewardQueueComponent` hängt an der Figur
  (`BP_DD_Character_C.RewardQueue`, `+0x980`). `RegisterReward` reiht eine Kiste ein
  und ruft `OpenNextReward` nur, wenn `+0x108` (Kiste offen) 0 ist. `OpenNextReward`
  gibt die nächste Kiste mit `SetRewardCanBeOpened(true)` frei; solange die aktuelle
  (`CurrentRewardObject`, schwacher Zeiger `+0x10C`) lebt und nicht fertig ist,
  ändert es nichts. `InventoryPanel.HandleConfirmButton` ruft es nur für die Queue des
  Panels (`+0x6B8`), also die des Hosts. Nach einer vom Client geöffneten Kiste blieb
  deshalb die Queue von P2 belegt, mit zerstörter Kiste als aktueller; alle weiteren
  Kisten hatten `CanBeOpened=false`.
- **[Belegt] Client-Panel:** `CreateItemInstance` ist `BlueprintAuthorityOnly`
  (Flags `0x4020405`), `ProcessEvent` überspringt es beim Client; der native Aufruf
  geht. Die Inventarfunktionen (`AddToInventory`, `AddToBackpack`,
  `OccupyBackpackSlots`, `RemoveFromInventory` …) sind Server-RPCs. Eine nur lokale
  Instanz kommt beim Host als Null an, `AddToBackpack` stürzt dann ab
  (`0x5708419`). Beim Wählen schickt das Panel zuerst `AddToInventory` (Wahl), nach
  „Bestätigen“ `AddToBackpack` und `OccupyBackpackSlots` mit den Feldern.
- **[Belegt] Pause:** Das offene Auswahl-Panel des Hosts setzt eine Zeitanfrage beim
  `TimescaleManager` (`active_requester=WP_Inventory_Panel`) und eine UI-Sitzung mit
  `blocks_gameplay`; die Spielzeit steht. Wählt der Client zuerst, bleibt sie bis zum
  Bestätigen des Hosts stehen.
- **[Belegt] Host bestätigt zuerst (`airdrop-warten`, 05.11.2026):** Gemessen alle 200 ms
  auf beiden Seiten an vier Airdrops (Zeitanfragen des `TimescaleManager`, UI-Sitzungen,
  `TimeDilation` und `PauserPlayerState` der WorldSettings, `SecondsSinceMissionStart` und
  `FreezeTimeManager` des `DDMissionGameState`). Hält die Mod das Bestätigen zurück, bleiben
  Zeitanfrage und UI-Sitzung des Host-Panels stehen, die Rundenuhr stand auf beiden Seiten
  bis zur Wahl des Clients (etwa 37 s lang auf 324). Die Uhr-Sperre `FreezeTimeManager`
  bleibt bei Airdrops leer. Der Eindruck „die Uhr läuft weiter“ war im Speicher nicht zu sehen.
- **[Belegt] Verlorener Klick:** Das nachgeholte Bestätigen schloss das Host-Panel einmal
  nicht, der Host musste erneut klicken. `HandleConfirmButton` (`0x5A6CB20`) kehrt ohne Wirkung
  zurück, wenn gerade ein Item gezogen wird (`+0x790`) oder die Prüfung der Koop-Autorität
  scheitert. Die Mod holte das Bestätigen nur einmal nach. Jetzt wartet sie den Drag ab und
  versucht es alle 500 ms erneut, bis die Sitzung endet, höchstens zehnmal. Danach endete das
  Host-Panel beim ersten Versuch, ohne zweiten Klick; der Drag-Fall ist nur im Code abgesichert.
- **[Belegt] Client:** Nach seinem Bestätigen setzt der Client lokal `TimeDilation=1,0`,
  solange der Host noch wählt. Er bleibt trotzdem stehen, weil der replizierte
  `PauserPlayerState` die Welt beim Client pausiert. Wählt der Client zuerst, stand die Uhr
  ebenfalls bis zum Bestätigen des Hosts.
- Nebenbei: Öffnet der Client sein Pausenmenü, läuft der Host weiter (Uhr 451 bis 466). Das
  ist die bekannte getrennte Pause, Aufgabe `pausen-sync`.

### Eingaben getrennt (`eingaben`)

Stand 06.11.2026, Build `26187885`, ein PC, Mod über die Lobby, Godmode, Tag 1. Positionen und Gerätebindungen mit lokalen, nur
lesenden Skripten gemessen, Eingaben über `Game-Input.ps1` ins jeweilige Fenster.

**Ausgangslage, ohne Änderung gemessen:**

- **[Belegt]** Bewegung, Dodge, Klick/Zielen und Pausemenü wirken nur in der eigenen Instanz. `D` im Host-Fenster bewegt nur die
  Host-Figur, `A` im Client-Fenster nur die Client-Figur; mit Dodge ebenso (beim Host greift dabei der Dodge-Pfad aus
  `ausweichen-client`). Esc öffnet das Pausemenü nur beim Drückenden; Host-Eingaben bewegen den Host weiter, während der Client im
  Menü steht. Die Zielrichtung (`Hands`) wählt das Spiel automatisch nach Gegnern, sie ist auf beiden Seiten gleich repliziert.
- **[Belegt]** Gerätebindungen (`ULocalCoopSubsystem+0x150`): auf beiden Seiten nur Tastatur/Maus (Gerät 0) an Spieler 0, kein
  Entwurf. Router-Sitzungen haben Authority-Index 0. Der Host-Router hängt also keine Eingabe an den Remote-Slot.
- **[Belegt, Disassembly]** Die Lücke liegt im nativen lokalen Koop: `AssignPhysicalDevice` (`0x58D93B0`) erlaubt Index 0 und 1,
  `DraftAssignPhysicalDevice` (`0x58E5690`) -1 bis 1. Ein Gamepad oder eine weitere Tastatur ließe sich so an P2 binden, der beim Host
  der Remote-Spieler ist. `SpawnLocalPlayer(1)` (`0x5901E90`) gibt beim Host einen eingetragenen P2 nur zurück; beim Client oder
  ohne Bindung legt es über `CreateLocalPlayer` einen zweiten `LocalPlayer` an, beim Client also einen zusätzlichen Netzwerkspieler.

**Umsetzung:** `src/Game/LocalDevicePatches.cpp` hängt sich an diese drei nativen Einstiege. Im Netzwerkspiel (Host mit NetDriver
oder Client) lehnen sie Index ≥ 1 mit dem nativen Fehlerergebnis ab (`false` bzw. kein Controller); Index 0, das Lösen (-1) und das
Zurückgeben eines schon eingetragenen Remote-P2 beim Host bleiben. Ohne Netzwerk bleibt der lokale Koop des Spiels unverändert.
Nichts wird übertragen, keine neue Nachricht, `ModVersion` unverändert. Kein Ordner `src/Sync/Players`, weil nichts synchronisiert
wird. Regeln in `LocalDevicePolicy.h`, Unit-Tests in `tests/GameTests/PatchTests.cpp`. Die Bewegungs- und Dodge-Pfade aus `bewegung`
und `ausweichen-client` bleiben unberührt.

**Geprüft** (06.11.2026, `Start-Session.ps1 -Restart -Mod -Tunnel Lobby -Godmode -Brief`, `Run-Route.ps1 -Days 1 -StayLast`):

- 41 statt 38 Patches gesetzt. Bewegung (`D`/`A`) und Dodge (`W`/`S` mit Leertaste) je Seite: gleiche Wege wie ohne Änderung, die
  andere Figur bleibt stehen.
- Eine Wegwerf-Probe (nicht im Repo) rief auf dem Game Thread beider Instanzen `AssignPhysicalDevice(0, 1)`,
  `DraftAssignPhysicalDevice(0, 1)`, `SpawnLocalPlayer(1)`, dann `AssignPhysicalDevice(0, 0)` und `SpawnLocalPlayer(0)` auf.
  Client: P2-Zuordnung, Entwurf und lokaler Spieler abgelehnt (Log „Eingaben: … abgelehnt“), weiter ein `LocalPlayer`. Host:
  Zuordnung und Entwurf abgelehnt, `SpawnLocalPlayer(1)` lieferte den Remote-Controller aus `ActivePlayers[1]`, nichts angelegt.
  Index 0 ging auf beiden Seiten durch. Danach Bindungen weiter nur Gerät 0 an Spieler 0, Host und Client steuerten je ihre Figur.
- Gegenproben: Level-up-Airdrop, Client zuerst bestätigt (Knopf gelb), dann Host, beide Panels zu, Inventare gleich. Lager am Ende
  von Tag 1: Host bestätigt und wartet; nach Fortsetzen und Bestätigen beim Client öffnete der Host die Routenkarte. Ohne Probe keine
  „Eingaben“-Zeile und kein Fehler im Log, kein Absturz.
- Nicht geprüft: ein echtes Gamepad (keins am Test-PC) und das Beitrittsmenü des Spiels; der Weg dorthin endet in denselben drei
  Funktionen. Nebenbefund: `Run-Route.ps1` und `Travel-Day.ps1` bestätigen das Lager nur beim Host und bleiben deshalb seit
  `bestaetigen-rundenende` vor dem zweiten Tag stehen, bis der Client bestätigt. Behoben mit `skripte-rundenende`.

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

**Folgerungen der damaligen Analyse** (Stand 02.11.; die Host-Anbindung und die lokale Geräte-/UI-Grenze sind inzwischen [geprüft](#remote-p2-geprüfte-anbindung-und-grenze-der-lokalen-ui))

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
Die Umleitung gilt nicht für das Hauptmenü: Dort stürzte der Host mit Umleitung ab, im Original nicht. Die Rückkehr in die `MetaBase` nach einer Niederlage läuft mit derselben Umleitung, siehe [Niederlage](#niederlage-niederlage).

`TriggerEndRound` meldet das Rundenende nur an die Persistenz und löst keine Reise aus. `FinishMission` zeigt das normale Tagesende, auch wenn vorher `Lose` gesetzt wurde. Der echte Weg bei einer Niederlage steht unten.

### Niederlage (`niederlage`)

Stand 04.11.2026, Build `26187885`. Zwei Instanzen auf einem PC mit `Start-Session.ps1` und CrashGuard, ohne Godmode, Route mit Seed `20795` bis in die Mission an Tag 1. Beide Figuren starben über `!cheats` und `DD_DamagePlayerAtIndex 1000 <Index>` beim Host.

- **[Belegt] Ablauf beim Host:** Stirbt der letzte Spieler, ruft das Spiel `DDMissionGameInstance.TriggerEndRound` mit Niederlage auf, danach öffnet sich „Du bist gestorben!“ (`WP_Round_Stats_C`, `RoundEndPanel` mit Run-Wertung). Nach der Wertung erscheint „Zurück zur Basis“. Der Klick reist über `SeamlessTravelToLevel` in die `MetaBase`, die vorhandene Spike-Umleitung macht daraus `servertravel /Game/Maps/MetaBase?listen`.
- **[Belegt] Client:** Der Phasenadapter meldet „Rundenübersicht, Eingabe gesperrt“. Die Wertung selbst sieht nur der Host. Nach der Reise folgt der Client in die `MetaBase`.
- **[Belegt] Absturz ohne Patch:** Nach dem Klick stürzte der Host ab: `EXCEPTION_ACCESS_VIOLATION reading address 0x0` bei `0x584CD85`. Der Handler `0x584CD20` läuft beim Run-Ende für jeden Controller, ausgelöst über eine Delegate-Kette aus dem Inventarbereich (`0x5956E00`, `0x596DA03`). Er prüft den Besitzer bei `this+0xA8` auf die Controller-Klasse, ruft `APlayerController::GetLocalPlayer` (`0x2E706D0`) und ohne Nullprüfung `+0x330` auf dem Ergebnis auf. Der Remote-Controller hat keinen `LocalPlayer`. Das ist dieselbe Annahme wie bei den Spawn-Patches.
- **Spike-Patch** (`RunEnd.cpp`): Ohne `LocalPlayer` gilt wie im Spieler-Index-Helfer Index 1 (P2), danach läuft die Funktion normal weiter.
- **[Belegt] Mit Patch:** kein Absturz. Beide stehen mit 2 Spielern und vollen HP in der `MetaBase`, beide Controller werden getauscht (`source=swap`). Danach startete ein neuer Run mit `Start-Run.ps1` über Routenkarte bis in die Mission an Tag 1, mit vollständigen und gleichen Inventaren.
- **Nebenbefund:** In der `MetaBase` zeigt der Client seinen eigenen Meta-Stand (andere Zähler und Gebäude als der Host). Seit der [Nutzerentscheidung vom 06.11.2026](SYNC_REQUIREMENTS.md#nutzervorgabe-meta-fortschritt-des-hosts-06112026) soll fuer den gemeinsamen Run der Host-Stand gelten; die technische Umsetzung ist damit noch nicht belegt.
- **Nicht geprüft:** Niederlage an späteren Tagen oder in Bosskämpfen, nur ein Spieler tot (der andere lebt weiter), Niederlage über das Internet. Je Variante ein Lauf.

### Run-Ende bei Host und Client (`run-ende`)

Stand 10.11.2026, Build `26495302`, ein PC, Mod mit Spike über die Lobby, zwei Instanzen, Godmode bis zum Auslösen. Getestet per Cheat: Niederlage mit `!cheats` und `DD_DamagePlayerAtIndex`, Sieg mit `DD_SetCurrentDay 8` und `!finish`.

| Ende | Auslöser beim Host | Host | Client vorher | Client jetzt |
|---|---|---|---|---|
| Niederlage | `TriggerEndRound(Lose)` | „Du bist gestorben!“, Wertung, „Zurück zur Basis“ | Keine Anzeige, eingefrorene Mission, bis der Host reist | „Du bist gestorben!“, keine Knöpfe, wartet auf die Reise |
| Sieg | `FinishMission` am letzten Tag, setzt `LastRoundChangeReason` auf `Win` | „Du hast gewonnen!“, Wertung, „Zurück zur Basis“ oder Endlosmodus | Normales „Ende der Runde“ | „Du hast gewonnen!“, „Fortsetzen“ bleibt für den Endlosmodus |
| Abbruch | Pausemenü, „Zurück zum Hauptmenü“. Einen eigenen Menüpunkt zum Aufgeben gibt es nicht | Hauptmenü | Hauptmenü, Sitzung beendet | unverändert (`trennen`, `net-fehler`) |

- **[Belegt] Ursache der Lücke:** Bei einer Niederlage läuft `FinishMission` nicht, also meldete der Host nichts über `0x401`.
- **Lokal zuerst:** Lokal bleibt die native Übersicht des Clients, geöffnet wie bei jedem Rundenende mit `FinishMission` (`rundenende-client`). Geteilt wird nur der Ausgang als ein Byte in `0x401`, siehe [Kontrollkanal](CONTROL_PROTOCOL.md#rundenende-rundenende-client). Der Host entscheidet, keine zweite Autorität. Auch die Rückkehr wählt nur der Host, der Client folgt über `servertravel` (`ClientMayStartTravel`).
- **[Belegt, gescheitert] `TriggerEndRound(Lose)` beim Client:** Es öffnete keinen Bildschirm, und sein Meta-Stand im Mod-Ordner änderte sich. Darum sperrt `RunLossAllowed` eine Niederlage beim Client jetzt ganz.
- **[Belegt, gescheitert] Umschalten auf jeder Instanz der Klasse:** `RefreshAsGameOver` auf der Vorlage im `WidgetTree` des Pakets stürzte ab (Lesen `0x110`, keine Besitzer-Instanz). Es gelten nur Instanzen, die über ihre Outer an der `DDMissionGameInstance` hängen.
- **[Belegt] Darstellung:** Nach `FinishMission` schaltet die Mod die Übersicht einmal um: `RefreshAsGameOver` (nur Niederlage), `SetNegativeText` oder `SetPositiveText`, Überschrift tauschen, `PlayRunFinishedAnimation`. Das Spiel blendet die Knöpfe nach der Animation wieder ein, darum blendet die Mod sie bei einer Niederlage jede Sekunde erneut aus.
- **[Belegt] Lauf auf dem Endstand:** Frische Testordner. Niederlage an Tag 1, beide in der `MetaBase`. Neuer Run in derselben Sitzung, Rundenende und Lager bei beiden, Reise nach Tag 2: Inventare `complete` und `equal`, Desync-Melder OK. Sieg, beide in der `MetaBase`. Danach Abbruch über das Pausemenü, beide im Hauptmenü. Kein Absturz.
- **[Belegt] Spielstände:** Die vier normalen Spielstände im Testordner blieben über Niederlage, zweiten Run und Sieg Byte für Byte gleich. `Management_SaveSlot_0.sav` des Clients im Mod-Ordner blieb gleich, beim Host änderte er sich. Ein Client bekommt also keine Meta-Gutschrift, siehe [Nutzervorgabe](SYNC_REQUIREMENTS.md#nutzervorgabe-meta-fortschritt-des-hosts-06112026). Der Run-Slot des Clients wird weiter nur im Mod-Ordner geschrieben.
- **Grenzen:** Die Werte der Wertung sieht der Client als Nullen, weil die Run-Statistik nicht repliziert wird (`opt-ergebnis-werte`). Am Sieg-Bildschirm führt „Fortsetzen“ beim Client wie bisher ins Lager. Der Host entscheidet trotzdem über die Reise.
- **Nicht geprüft:** Zwei PCs und Internet, ein ausgespielter Endbosskampf, Niederlage im Bosskampf, Endlosmodus nach dem neuen Sieg-Bildschirm. Der Endlosmodus-Pfad des Clients ist bis auf die Überschrift unverändert ([`dauerlauf`](../research/spikes/dauerlauf/README.md)).
- **Testhinweis:** Nach einer Niederlage und nach dem Fokuswechsel zwischen den Fenstern ging der erste Klick auf einen Knopf der Übersicht verloren, der zweite wirkte. Das betrifft `Complete-RoundEnd` beim ersten Rundenende nach einer Niederlage. Mit `-RealSave` fand `Start-Run.ps1` die Routenkarte nicht.

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

### Rundenende über Tank und Bus (`rundenende-absturz`)

Stand 05.11.2026, Build `26187885`, ein PC, Mod mit Spike über die Lobby, Seed `20795`, Tag 1.

- **Rundenende:** Eine Runde endet regulär, wenn der Tank voll ist und die Spieler mit dem Bus flüchten (Nutzerangabe). Der Absturz
  „rund 12,5 Minuten nach Missionsstart“ aus dem [Desync-Lauf](DESYNC.md#langzeitmessung-im-spiel-05112026-nach-merge-von-main) lag
  davor und war deshalb kein Rundenende.
- **[Belegt] Rundenende über den Bus:** Tank beim Host per Speicherschreiben auf 118 von 120 (`UFuelMeter.CurrentFuel` am
  `GS_DDMissionState_C`, `+0xF8`), der Client übernahm den Wert über die Replikation. Der Nutzer sammelte einen Kanister (128) und flüchtete
  mit dem Bus. Host mit „Ende der Runde“, Client mit Spielansicht und Phasenhinweis, kein Absturz, Desync-Melder OK. Danach Lager,
  Routenkarte und Reise zu Tag 2: `check_round.py` sauber, 63 Kacheln auf beiden Seiten, Figuren am Bus.
- `DD_QuickFinishMission` beendet die Runde ohne Tank und Bus, wie `!finish`. Es ersetzt den Busablauf nicht.
- **Offen:** Der Tank ist je Teilnehmer getrennt gewertet, jeder PlayerState hat einen eigenen `UFuelMeter` (Maximum 100). Nach
  Nutzervorgabe ist er ein Gruppenwert, Folgeaufgabe `tank-gemeinsam`. Nachtrag: Der Tank am GameState ist schon der Gruppenwert, siehe [Tank als Gruppenwert](#tank-als-gruppenwert-tank-gemeinsam).

### Tank als Gruppenwert (`tank-gemeinsam`)

Stand 06.11.2026, Build `26187885`, ein PC, Mod mit Spike über die Lobby, Godmode, Tag 1. Der Nutzer steuerte beide Figuren.
Ergebnis: **Der Tank ist im Spiel schon ein Gruppenwert, die Mod braucht dafür keinen Code.**

- **[Belegt] Tankanzeigen zur Laufzeit:** In der Mission hat jede Seite einen `UFuelMeter` am `GS_DDMissionState_C` (Maximum 120) und
  je Spieler einen am `BP_DDPlayerState_C` (Maximum 100). Dazu kommen die Vorlagen der Klassen. Gelesen mit `fuel.py` und einer
  Zuordnung über den Outer. Der Tank am GameState startet bei 10.
- **[Belegt] Reflektierte Funktionen:** `DDMissionGameState.GetFuelMeter`, `FuelMeter.IncreaseFuel`, `GetLastLootClaimer`,
  `OnRep_CurrentFuel`, `FuelLoot.Loot`, `LootBlueprintFunctionLibrary.DropRequiredFuel`. Einen Cheat für Treibstoff gibt es nicht.
- **[Belegt] Kanister:** Nach `DD_OpenAllLootSpots` (nach `!cheats`) lagen rund 30 `BP_FuelLoot_C` über die ganze Karte verteilt.
- **[Belegt] Jeder Kanister zählt einmal für beide:** Mitschnitt aller Tanks auf beiden Seiten alle 0,25 s. Kanister des Hosts:
  GameState-Tank 10 → 20, Kanister des Clients: 20 → 30, jeweils bei Host und Client in derselben Sekunde. Die PlayerState-Tanks
  blieben bei 0, nichts zählte doppelt. Nach Auffüllen beim Host auf 118 (Speicherschreiben, der Client übernahm den Wert sofort) zwei
  weitere Kanister: 128, 138 auf beiden Seiten. Der Wert darf das Maximum übersteigen.
- **[Belegt] Voller Tank gibt die Busflucht frei:** Bei vollem Tank liefen beide Figuren zum Bus, die Runde endete. Host-Log
  „Rundenende: Host beendet die Runde, Client wird benachrichtigt“, Client-Log „Host hat die Runde beendet, Client öffnet
  Rundenübersicht und Lager selbst“; der Client zeigte „Ende der Runde“. Die Abfahrtsprüfung des Clients blieb unterdrückt
  (`bus-ende`). Damit ist auch das Rundenende über den echten Bus für `rundenende-client` belegt.
- **Vermutung:** Die PlayerState-Tanks sind ungenutzt oder für einen anderen Spielmodus. Beim Einsammeln änderten sie sich nicht.
- **Nebenbefund, nur Anzeige:** Die Rundenübersicht des Clients zeigt eigene, lokale Werte (Kills 0, „Tag 0/0“). Kein Hindernis für
  einen Run, Kandidat für die optionalen Optimierungen.
- **Werkzeug:** `fuel.py --fill` schreibt alle Tanks, auch einen PlayerState-Tank und die Vorlage der Klasse. Für den Test egal,
  gemeint ist nur der Tank am GameState.

### Busabfahrt nur beim Host (`bus-ende`)

Stand 06.11.2026, Build `26187885`, ein PC, Mod mit Spike über die Lobby, Godmode, Tank per Speicherschreiben fast voll.

- **[Belegt] Fehlerbild:** Im Coop-Test endete die Runde nur beim Client. Laut Spike-Trace rief der Client selbst
  `ExtractionPointBase.CanBusTakeOff` und 3 s später `DDMissionGameState.FinishMission` auf und stand danach auf einer Routenkarte, auf
  der er keinen Tag wählen konnte. Der Host blieb in der Mission.
- **[Belegt] Native Funktionen:** Über die `FNameNativePtrPair`-Tabelle gefunden. `CanBusTakeOff(EBusCantLeaveReason&)` liegt bei
  `0x59C3E10` (Thunk `0x56E3A70`), `CheckTakeOffCondition` bei `0x59C4930` ruft es bei `+0x1F` auf. `FinishMission(bool IsLoading)` liegt
  bei `0x58E84C0` (Thunk `0x56BB8C0`). Beide laufen nicht über `ProcessEvent`, deshalb ein Funktions-Hook.
- **Umsetzung** (`ClientPatches.cpp`, Patch „Busabfahrt und Rundenende nur beim Host“): Beim Netz-Client liefert `CanBusTakeOff` immer
  `false` mit Grund `None`, und `FinishMission` läuft nur noch mit `IsLoading`. Der Host entscheidet, der Client folgt über die Reise.
  Regeln `BusTakeOffMayDecide` und `MissionFinishAllowed` in `PatchPolicy.h`, mit Tests.
- **[Belegt] Lauf:** Der Client meldete dreimal „Abfahrtsprüfung beim Client unterdrückt“, der Host beendete die Runde mit dem Bus.
  Host über Rundenübersicht, Lager und Routenkarte nach Tag 2 (Welt 3), der Client folgte in dieselbe Welt mit 2 Spielern.
  Desync-Melder danach zweimal OK, kein Absturz.
- **Nachtrag:** Rundenübersicht und Lager sieht der Client seit `rundenende-client` selbst, siehe
  [Rundenübersicht und Lager beim Client](#rundenübersicht-und-lager-beim-client-rundenende-client).

### Rundenübersicht und Lager beim Client (`rundenende-client`)

Stand 06.11.2026, Build `26187885`, ein PC, Mod mit Spike über die Lobby, Godmode, Seed `20795`.

- **Ablauf:** Was lokal bleibt: Rundenübersicht, Lager, Routenkarte und das Ordnen des eigenen Inventars, alles mit dem nativen
  `FinishMission` des Clients. Geteilt wird nur, dass der Host die Runde beendet hat: `FinishMissionHook` merkt sich beim Host das
  Ende der aktuellen Welt, der Kontrollkanal bringt es als `0x401` zum Client, siehe [Kontrollkanal](CONTROL_PROTOCOL.md#rundenende-rundenende-client).
  Der Client ruft `FinishMission(false)` einmal selbst auf, vorbei an der Sperre aus `bus-ende`, nur wenn die Karten-Id zu seiner
  übernommenen Karte passt (`ClientMayFollowRoundEnd`). Das Ziel wählt weiter nur der Host: `SeamlessTravelToLevel` reist beim
  Client nicht mehr, außer ins Hauptmenü (`ClientMayStartTravel`). Die Inventaränderungen im Lager laufen über die nativen Server-RPCs,
  der Host entscheidet, das Ergebnis repliziert. Code in `MapPatches.cpp`, `ClientPatches.cpp`, `TravelPatches.cpp`, `MapSync.cpp`.
- **Spike:** Mit der Mod sperrt der Phasenadapter (`Mission.cpp`) den Client nur noch beim Warten auf den Host und während der Reise,
  nicht mehr in Rundenübersicht, Lager und Routenkarte.
- **[Belegt] Lauf:** Host-`!finish` an Tag 1. Der Client meldete 280 ms später „Client öffnet Rundenübersicht und Lager selbst“ und
  zeigte „Ende der Runde“, nach „Fortsetzen“ sein Lager mit eigenem Inventar. Er legte seine Pistole ins Lager, bestätigte und stand
  auf einer Routenkarte ohne wählbaren Knoten. Klicks auf Knotenposition und „Zurück“ lösten nichts aus. Danach Host über Lager und
  Routenkarte nach Tag 2: beide in derselben Welt mit 2 Spielern, Karte vom Host übernommen, `Get-Status -Inventory` vollständig und
  gleich, die Pistole des Clients auf beiden Seiten im Lager. Desync-Melder an Tag 2 dreimal OK, Uhr des Clients läuft, kein Absturz.
- **[Belegt] Zwischenstand im Lager:** Solange der Client sein Lager offen hatte, meldeten Desync-Melder und `inventory.py` das Inventar
  ungleich. Der Host hatte die Ablage übernommen (Pistole nicht mehr im Rucksack), beim Client stand sie noch mit lokalem Pivot im
  Rucksack. Nach der Reise war es gleich. Das ist die lokale Anzeige des offenen Panels, wie bei der offenen Airdrop-Auswahl.
- **[Belegt] Spielstand des Clients:** Das lokale `FinishMission` schreibt den Spielstand des Clients (`SaveSlot_DDR_0.sav` im
  Test-`UserDir`, 3093 → 7867 Bytes zum selben Zeitpunkt) mit eigenem Run-Seed, Tag 0, `EndOfDay` und dem Inventar. Der Client hatte
  den Spielstand schon beim Beitritt geschrieben. Ob dabei ein laufender eigener Run eines echten Spielstands verloren geht, ist nicht
  geprüft (Vermutung: ja, der Run-Slot wird überschrieben). Seit [`mod-saves`](#spielstände-der-mod-mod-saves) schreibt die Mod das nur noch in `SaveGames_DDCoop`.
- **Nicht geprüft:** Mehrere
  Rundenenden hintereinander, Itemsäcke im Lager, ein Client, der den Host im Lager überholt. Die Reisesperre des Clients griff im Lauf
  nicht, weil seine Routenkarte keinen Knoten anbot; sie ist nur in Tests belegt. Das Rundenende über den echten Bus ist seit
  [`tank-gemeinsam`](#tank-als-gruppenwert-tank-gemeinsam) belegt.

### Tagesablauf: Tag, Uhr, Tank, Nacht und Ziele (`tagesablauf`)

Stand 08.11. und 10.11.2026, Build `26495302`, ein PC, Mod mit Spike über die Lobby, Godmode, Seed `20795`. **Ergebnis: Der Tagesablauf
repliziert nativ, die Mod braucht dafür keinen neuen Sync-Pfad und keine neue Nachricht.** Neu ist nur die Messung.

- **Messung:** Die Testkanal-Anfrage `day` (`src/Game/DayView.cpp`, nur mit `-ddcoop_control`, schreibt nichts) zählt per Reflexion alle
  skalaren Felder des `GS_DDMissionState_C`, seiner Unterobjekte und deren Unterobjekte (Tank, Nacht, Gefahrenstufe, Wallets) und
  aller `DDObjective`-Instanzen auf, Structs eine Ebene tief. `tools/spike/day_state.py` vergleicht Host und Client nach Klasse und Feld,
  `Run-Route.ps1 -DayState [-DaySeconds n]` nach jeder Ankunft, spät am Tag und auf der Routenkarte.
- **[Belegt] Tag 1 über 300 s (08.11.):** Neun Messungen alle 30 s. `DayNum` 1, `SecondsSinceMissionStart` (Uhr im HUD) 60 bis 302 s,
  `FuelMeter.CurrentFuel` 0 → 10 (Kanister) bei `MaxRequiredFuel` 120, `NightModeComponent` (Nacht nach 150 s, Controller
  `AutoNightController`, `NightmodeStartedSeconds`) und `ManualNightController` auf beiden Seiten gleich, in jeder Messung.
  `check_round.py` sauber, Inventare gleich.
- **[Belegt] Ankunft Tag 1 (10.11., nach Merge von main bis #203):** Tag, Uhr, Tank, Nacht und alle Wallets gleich.
- **[Belegt] Missionsziele:** An Tag 1 gibt es auf keiner Seite eine `DDObjective`-Instanz. Das Ziel „Tank füllen“ ist die Tankanzeige
  selbst, die über `FuelMeter` repliziert. Boss-Ziele legt das Spiel nur beim Host an, die Mod baut die Leiste beim Client nach,
  siehe [Bosse](../src/Sync/Enemies/README.md).
- **[Belegt] Nur beim Host:** `ThreatLevel.GameMode`, `.GameState` und `.FreezeThreatLevelManager` (samt Unterobjekt) setzt nur der
  Server, der die Gefahr steuert. Beim Client null, kein Fehler. `day_state.py` blendet sie aus.
- **[Belegt] Schrott, 08.11., vor `lager-handel-client`:** `ScrapWalletComponent.CurrencyAmount` beim Host 1/2, beim Client 46/92. Der
  Stand vom 08.11. hatte die Schrott-Übertragung `0x309` aus #170 noch nicht. Am 10.11. war Schrott bei Ankunft auf beiden Seiten 0,
  später im Tag nicht erneut gemessen. `day_state.py` meldet eine Abweichung wieder als Fehler.
- **Nicht mit `-DayState` gemessen:** Tag 2 bis 5, Biomwechsel nach Tag 4 und Endlosmodus. Der Lauf vom 10.11. wurde nach Tag 1 auf
  Anweisung des Nutzers beendet, der den Test als erfolgreich abgenommen hat. Reise und Rundenende über mehrere Tage sind aus anderen
  Läufen belegt ([`dauerlauf`](../research/spikes/dauerlauf/README.md), 12 Tage mit Endboss und Endlosmodus ohne Absturz; `spiel-update`, `Run-Route.ps1 -Days 2` sauber); `DayNum` vergleicht
  der Desync-Melder ohnehin laufend.
- **Abgegrenzt:** Run-Info, Routenkarte und Gefahrenstufen-Anzeige des Clients liegen bei `opt-routenkarte`.

### Routenkarte beim Client (`routenkarte-client`)

Stand 07.11.2026, Build `26187885`, ein PC, Mod mit Spike über die Lobby, Godmode, Seed `20795`. Nur gelesen, kein Code geändert.
Gelesen wurde `FCurrentRunInfo` der GameInstance (`+0x2A0`): Run-Seed `+0x0`, `CurrentRoute` `+0x8` mit `MissionChain` `+0x2B0`,
Anzahl `+0x2B8`, `CurrentMissionIndex` `+0x2C0`, `IsEndOfMissionChain` `+0x2C8`, `ChosenMissionIndexes` `+0x328`; Tag aus
`ADDMissionGameState.DayNum` (`+0x390`).

| Zeitpunkt | Host | Client |
|---|---|---|
| MetaBase vor dem Run | Seed 21492 (eigener Spielstand), Route leer, Index −1 | Seed 20807 (eigener Spielstand), Route leer, Index −1 |
| Routenkarte in der MetaBase | Seed 20795, volle Karte mit Biomen und Knoten, Route noch leer | Seed 20807, sieht weiter die MetaBase, keine Karte |
| Tag 1 in der Mission | 1 Eintrag `Biome_Cities`, 11 × 11 Kacheln, Index 0, gewählt `[2]`, Tag 1 | Route leer, Index −1, Tag 1 |
| Routenkarte nach dem Rundenende | Index 0, Ende der Kette, Karte mit Bus und Spalte „Tag 2“ | Index 0, Ende der Kette, Kette leer; Karte nur mit Wolken und Hintergrund, Bus oben links mit „999“ |
| Tag 2 nach der Reise des Hosts | Eintrag `Biome_Cities`, 6 × 6 Kacheln, gewählt `[2, 2]`, Vorroute = Tag 1, Tag 2 | Route leer, Index 0, Tag 2 |

- **[Belegt] Ursache der leeren Karte:** Die Run-Info der GameInstance repliziert nicht. Der Client behält Run-Seed und leere Route
  aus seinem eigenen Spielstand, auch nach dem Wechsel in die Mission des Hosts. Sein lokales `FinishMission` (`rundenende-client`)
  setzt nur den Index von −1 auf 0 und das Ende der Kette, auf einer leeren Kette. Das Panel `WI_MissionChoicePanel_C` hat beim Client
  also nichts zu zeichnen. Das passt zur leeren Route aus dem [PCG-Befund](#pcg-vegetation-beim-client-pcg).
- **[Belegt] Nur Anzeige bei Karte und Tag:** Ein Klick des Clients auf die Knotenposition für Tag 2 bewirkte nichts, das Log zeigt
  keinen Reiseversuch, also auch keinen Eingriff von `ClientMayStartTravel`. Nach der Reise des Hosts stand der Client mit 2 Spielern
  in derselben Welt, `check_round.py` sauber, Tag 2 aus dem GameState wie beim Host, Rundenzähler im HUD auf beiden Seiten „2“.
- **[Belegt] Abweichung im HUD:** Die Zahl unter der Stoppuhr (Gefahrenstufe) zeigte an Tag 2 beim Host „2“, beim Client „0“.
  **[Vermutung]** Die Anzeige kommt aus dem aktuellen Routeneintrag (`GetDisplayThreatLevel(FRouteMission)`), der beim Client fehlt.
  Die eigentliche Gefahr steuert der Host, die Komponente `ThreatLevel` repliziert.
- **[Belegt] Routenkarte in der MetaBase** gibt es nur beim Host, der Client läuft dort weiter durch die Basis. Das ist kein Fehler
  der Karte, der Client wählt den Run ohnehin nicht.
- **[Vermutung] Weitere Folgen der fehlenden Run-Info:** Alles, was beim Client Seed, Route oder Biom der GameInstance liest, rechnet
  mit falschen Werten: `GetCurrentMissionSeed` (Run-Seed 20807 statt 20795), biomabhängige Darstellung beim Biomwechsel nach Tag 4
  (`OptionalPreviousBiome`), Torboss- und Endboss-Knoten, Endlosmodus. Die Karte selbst kommt über `MapSync` vom Host, der
  PCG-Graph über den Patch in `MapPatches.cpp`. Der frühere Lauf bis Tag 13 mit zwei Biomwechseln hatte saubere Einstiegskontrollen, ein gestörter
  Spielablauf ist bisher nicht belegt. Nicht geprüft: Biomwechsel, Bossknoten und Endlosmodus mit Blick auf die Routenkarte des Clients.
- **[Vermutung] Fehlbedienung:** Der Client sieht nicht, wohin die Reise geht, und klickt auf eine leere Karte, auf der auch „Zurück“
  nichts tut, siehe [Lauf in `rundenende-client`](#rundenübersicht-und-lager-beim-client-rundenende-client). Er kann den Ablauf
  nicht stören, versteht ihn aber nicht.
- **Lösungsweg, nicht umgesetzt:** Der Host schickt über den Kontrollkanal Run-Seed und gewählte Indizes, der Client setzt den Seed
  wie `DD_SetCurrentRunSeed` und baut `CurrentRoute` daraus nach, oder er übernimmt die Route direkt. Die Reisesperre bleibt, der
  Client sieht dann dieselbe Karte, wählen kann er weiter nicht. Bei echten Spielständen nur in den Mod-Spielständen
  ([`mod-saves`](#spielstände-der-mod-mod-saves)).

**Nachtrag 09.11.2026, zurückgestellt als `opt-routenkarte`:** Build `26495302`, ein PC, Mod mit Spike über die Lobby, Godmode,
Seed `20795`, frischer Spielstand (Tag 1 startet ohne Routenkarte). Kein Code geändert, nur gelesen und disassembliert. Auf
Wunsch des Nutzers verwaltet im ersten Release nur der Host die Routenkarte; der Client bekommt währenddessen Pause und
Eingabesperre (Sperre und volle Karte beim Client laufen später gemeinsam unter [`opt-routenkarte`](../TODO.md#optionale-optimierungen)).

- **[Belegt] Offsets gelten auch auf `26495302`:** `FCurrentRunInfo` an `GameInstance+0x2A0` wie oben. Host nach Tag 1: Seed 20795,
  `RouteMissionStructureIdentifier` `(0, 1)`, Kette 1, Index 0, Ende der Kette, `ChosenMissionIndexes` `[2]`,
  `LinkedNextMissionIndexes` `[0, 2]`, `Position` 2. Client: eigener Seed, Kennung `(0, 0)`, Kette leer, gewählt `[]`.
- **[Belegt] Missionsstruktur ist beim Client schon gleich:** `UMissionGenerationSubsystem.CachedMissionStructure` (`+0x30`,
  Elemente `FPossibleMissionGroup` zu `0x20` Bytes) hatte nach `DD_SetCurrentRunSeed 20795` beim Client dieselben 8 Gruppen wie beim
  Host (Cities, Cities, Suburbs, Suburbs, Woods, Woods, Meadows, Meadows mit 3/3/3/3/1/3/3/1 Routen).
- **[Belegt] Seed allein reicht nicht:** Nach `DD_SetCurrentRunSeed` beim Client, auch vor dem nächsten Rundenende gesetzt, blieb die
  Karte leer. `WI_MissionChoicePanel_C.SpawnedChoices` (`+0x448`, Anzahl `+0x450`) ist beim Client 0, beim Host 20.
- **[Belegt] Eigentliche Sperre ist eine Rollenprüfung im Spiel:** `UMissionChoicePanel::SetChoices` (Exec-Thunk `0x57545F0`,
  Implementierung `0x5ABD220`) hat genau einen nativen Aufrufer in der Funktion ab `0x5AB7090`, vermutlich
  `UBusPanel::OpenMapIfRequired` (BP-Klasse `WI_BusScreen_C`, `MissionChoicesPanel` an `+0x330`). Sie kopiert `CurrentRoute`
  (`GameInstance+0x2A8`) und prüft dann bei `0x5AB7183` `cmp byte ptr [rax+168h], 3` auf dem Ergebnis von vtable `+0x2C8`
  (`0x3FE2F10`, liefert den besitzenden PlayerController aus `+0x310`). `AActor::Role` liegt an `+0x168`, 3 ist `ROLE_Authority`.
  Nur mit Autorität ruft sie `SetChoices(GetMissionStructure(-1))` (`0x58EDDD0`). Beim Client ist der Controller
  `ROLE_AutonomousProxy`, das Panel bleibt leer, egal welche Run-Info er hat.
- **Weg für `opt-routenkarte`, nicht umgesetzt:** (1) Run-Info des Hosts übertragen (Seed, Strukturkennung, `ChosenMissionIndexes`,
  Index, Ende der Kette, Gefahrenstufen-Erhöhung) wie `MapSync` über den Kontrollkanal. (2) Beim Client die Rollenprüfung in
  `0x5AB7090` umgehen (Sprung `jne` bei `0x5AB718A`, 6 Bytes `0F 85`) oder nach dem Öffnen `SetChoices` selbst aufrufen. Ob Bus,
  Tagesspalte und gereiste Linien dann stimmen und ob `CurrentRoute.MissionChain` für HUD-Gefahrenstufe nachgebaut werden muss, ist
  nicht geprüft; der Versuch mit dem ausgehebelten Sprung wurde abgebrochen und nicht ausgewertet. Die Reisesperre
  (`ClientMayStartTravel`) muss bleiben, weil mit befülltem Panel Knoten klickbar werden.
- **[Belegt, Versuch `routenkarte-sperre`, 09.11.2026] Hook auf `OpenMapIfRequired` sperrt die Karte nicht:** Hook auf `0x5AB7090` (Referenz-RVA `0x05AB20C0`, eindeutig umrechenbar) im Ein-PC-Test mit Mod und Spike, Seed `20795`, Tag 1, `!finish`. Beim Client läuft die Funktion nur einmal, mitten in `FinishMission` (`FollowHostRoundEnd`), und der Hook unterdrückte sie dort. Die leere Karte (Wolken, Bus „999“, Knopf „Zurück“) ging trotzdem nach dem Bestätigen im Lager auf, ihr Öffnen läuft also über einen anderen Weg (vermutlich Blueprint oder Bus-Panel-Animation, nicht gefunden). Der Hook wurde verworfen, kein Code übernommen. Die Reisesperre `ClientMayStartTravel` hält Klicks auf Knoten beim Client weiter ab. Offen für eine spätere Sperre: den Öffner der Karte finden (`WI_BusScreen_C`, `MissionChoicesPanel` `+0x330`) oder das Panel beim Client per `SetVisibility` unbedienbar machen und einen Hinweis zeigen. Zur Umrechnung von Adressen des neuen Builds auf die Referenz: Muster der neuen Funktion in der alten Exe suchen (`tools/rva_map.py`-Hilfen `masked` und `find_unique`), dann Zeile in `RvaMap<build>.inc` und `patch_signatures.py generate --write`.

### Seltenheit und Aufwerten beim Client (`werkbank-client`)

Stand 06.11.2026, Build `26187885`, ein PC, Mod mit Spike über die Lobby, Godmode.

- **[Belegt] Seltenheit repliziert nicht:** `UInventoryItemInstance.ItemRarity` (`+0x118`) ist beim Host für jedes Item gesetzt
  (`Rarity_0_Common` usw.), beim Client für jedes replizierte Item null, auch für die eigenen. Lokale Anzeige-Instanzen der Airdrop-Auswahl
  haben sie, weil der Client sie selbst mit Seltenheit anlegt. Ohne Seltenheit wertet das Panel beim Ablegen auf ein gleiches Item nie
  auf (`UpgradeDroppedItem` liest `ItemRarity->NextUpgradableRarity`). Die Mod schickt sie jetzt für das Clientinventar (`0x308`).
- **[Belegt] Aufwerten nativ:** Ablegen und Werkbank enden in `UInventoryComponent::UpgradeItems`, nativ ohne `ProcessEvent`. Sie legt die
  Ausgabe mit `CreateItemInstance` an, setzt `Broken` zurück, legt sie mit `AddToInventory` ab und überträgt Modifikatoren der Eingaben.
- **[Belegt] Im Lager keine sichtbare Replikation:** Nach der Übernahme eines Edits mit Aufwertungen kamen die neuen Items des Hosts beim
  Client weder im Lager noch auf der Routenkarte an, er behielt seine lokalen Ausgaben. Nach der Reise in den nächsten Tag standen die
  replizierten Objekte in der Reihenfolge des Hosts da, die lokalen waren weg. Passt zu `rundenende-client`. Ablauf in
  [Werkbank und Aufwerten beim Client](../src/Game/README.md#werkbank-und-aufwerten-beim-client).

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

### Live-Test auf zwei PCs am 06.11.2026 (`livetest-befunde`)

Build `26187885`, Mod-Stand `main` bis `installer` (#131), zwei Haushalte über das Steam-Relay, Ping 32 ms, kein Paketverlust. Lobby,
MetaBase, Tag 1 und Tag 2 mit 14 Airdrops des Clients, Abbruch durch einen Absturz des Clients im Lager am Ende von Tag 2. Ausgewertet
sind das Mod-Log und der Speicher des Hosts, nur lesend; das Client-Log lag bei der Auswertung noch nicht vor.

- **[Gemeldet] Aufwerten beim Client geht nicht.** Erwartet: `UInventoryComponent::UpgradeItems` und `InventoryPanel.TryToUpgradeDroppedItem`
  sind laut SDK lokale Funktionen, kein Server-RPC. Die Mod deckt das Aufwerten noch nicht ab, siehe `werkbank-client`.
- **[Belegt, Host] Waffe des Clients nach der Itemauswahl nicht ausgerüstet.** Der Client wählte unter anderem Pistole, Messer und zweimal MP.
  Beim Host lagen danach Pistolen, Messer und Fahrradpumpe des Clients außerhalb des Rucksacks (`IsInBackpack` 0, Pivot aber gesetzt), im
  Rucksack nur Batterie, MP, Bohrer und Charakter-Item. `ADDCharacter.EquippedWeapons` (`+0x7B8`) der Client-Figur beim Host enthielt nur
  `Data_SMG`. Alle Edits übernahm der Host mit `vollständig=true`. Verdacht, nicht belegt: Während der Bearbeitung verwirft der Client alle
  fremden Server-RPCs (`InventoryEdit.cpp`, `Kind::Foreign`), und beim Umlegen nimmt der Host ein verschobenes Item mit
  `ForceRemoveAllEffects` heraus und setzt die Wirkung nur neu, wenn das Panel des Clients `ApplyAddToBackpackEffect` gerufen hatte.
- **[Belegt, Host] Inventar dauerhaft abweichend.** Ab der fünften Wahl (Tag 2) meldete der Desync-Melder nach fast jeder Übernahme
  `ABWEICHUNG Inventar` und wurde nicht wieder gleich. Vorher war der Abgleich nach jeder Übernahme binnen Sekunden wieder `OK`. Der Nutzer
  sah beim Client unsichtbare Items. Ob `opt-desync-pivot` einen Teil davon erklärt, ist offen.
  **Nachtrag (`inventar-client-abweichung`, 09.11.2026):** Siehe [Lager-Edit des Clients mit Reparatur und Aufwertung](#lager-edit-des-clients-mit-reparatur-und-aufwertung-inventar-client-abweichung).
- **[Gemeldet] Dodge-Reparatur beim Client wirkt nicht mehr.** Beim Host stand je Karte „Laufrichtung der Client-Figur für den Dodge beim
  Host eingetragen“, keine Ablehnung aus `eingaben`. Was beim Client fehlt, braucht sein Log.
- **[Belegt, Host] Absturz des Clients im Lager, Host reist weiter.** Rund 77 s nach „Client im Lager“ (Ende Tag 2) kamen keine Daten mehr
  vom Client, 3 s später meldete der Host „Client meldet sich nicht mehr aus dem Lager“ und bestätigte dann allein („alle bereit“), reiste
  nach Tag 3 und verlor erst dort die Verbindung (Timeout 4001). Absturzadresse und Client-Log fehlen noch.

**Zweite Sitzung am selben Abend, Rollen getauscht** (dieser PC Client, Ausgewertet: Client-Log, Client-Speicher, Absturzbericht):

- **[Belegt] Mehr als ein Mitspieler geht nicht.** Die Lobby nimmt bis zu 4 Mitglieder auf (`Session::kMaxMembers`), der Transport des
  Hosts lehnt aber jeden weiteren mit `Busy` ab („es gibt schon einen Mitspieler“), und Remote-P2, Airdrop-Auswahl, Inventar-Edit und
  Desync-Melder kennen genau einen Client. Ein Spiel zu dritt ist mit dem aktuellen Stand nicht möglich.
- **[Belegt] Vier Airdrops, alle Edits übernommen.** Das Panel des Clients ruft beim Ablegen einer Waffe jedes Mal
  `ApplyAddToBackpackEffect`, den der Host nachholt. Verworfen wird während der Bearbeitung aber `CharacterEffectManager.RemoveItemFromBackpack`;
  ob dadurch Effekte herausgenommener Items hängen bleiben, ist offen.
- **[Belegt, Client] Gegner weichen ab, im Lager auch das Inventar.** Ab 19:13:41 UTC (im Kampf, während einer Itemauswahl) meldete der
  Melder `ABWEICHUNG Gegner`, nach dem Rundenende im Lager alle 30 s `Gegner, Inventar`, bis zum Ende. Der Client hatte im Lager 116
  sichtbare `EnemyBase`, alle `SimulatedProxy` vom Host, keine lokal entstandenen. Die Zahl des Hosts lag nicht vor.
- **[Belegt] Dodge-Reparatur greift laufend.** „Dodge-Werte der eigenen Figur hingen, Reibung und Schwerkraft zurückgesetzt“ stand oft
  mehrmals je Minute, ohne sie rutschte der Client.
- **[Belegt] Client-Absturz, als der Host im Lager das Spiel beendete (Alt+F4).** `EXCEPTION_ACCESS_VIOLATION reading address 0xE0` bei
  `0x5AB513A`. Die Funktion ab `0x5AB5120` liest `UItemRepresentation.ItemInstance` (`+0xA28`) und davon ohne Prüfung `ItemData`
  (`UInventoryItemInstance +0xE0`). Darunter UMG/Slate und die Blueprint-VM, also ein Inventar-Widget, dessen Item beim Verbindungsabbau
  schon weg ist. Verdacht: derselbe Weg wie beim ersten Absturz im Lager, sobald dort ein Item-Widget sein Item verliert; vergleiche
  [Trennen](#trennen-ins-hauptmenü-trennen), wo ein fehlendes Item in `EndPlay` abstürzte.
  **[Belegt, Disassembly]** `0x5AB5120` ist eine `void`-Funktion, die nur die Größe des Widgets aus der Itemform setzt (je 66 px minus
  3), neun direkte Aufrufer. `lager-absturz-client` überspringt sie ohne Item, und der Host wartet bei stummem Client bis zum
  Verbindungsabbau, siehe [Lager am Rundenende](../src/Game/README.md).

### Live-Test auf zwei PCs am 07.11.2026 (`livetest-0711`)

Build `26495302`, Mod-Stand `main` `49e0194` (nach `update-inventar`, #155), nur Mod (`-Mod -Tunnel Lobby -NoSpike`), zwei Haushalte
über das Steam-Relay (Frankfurt), Ping 40 bis 43 ms, kein Paketverlust. Je PC eine Rolle, zwei Menschen. Ablauf mit
`Guided-Test.ps1` beim Client (16 Schritte, 14 ok, 2 mit Abweichung), beim Host Statusaufzeichnung und Mod-Log. Zeiten in MEZ.
Ausgewertet: Mod-Logs beider Seiten, Overlay-Verlauf, `crashes.py` beider Seiten. Keine Absturzberichte auf beiden PCs.

- **[Belegt] Alle Patch-Listen auf dem neuen Build.** Der Client lief ohne `-PartialPatches` und meldete „Patches: 43 gesetzt“. Der Host lief
  mit `-PartialPatches`, sein Log meldet „Teilinstallation (DDCOOP_PARTIAL_PATCHES), 13 von 13 Listen gesetzt“, also ebenfalls keine
  fehlende Liste. Der Client trat der Lobby ohne Host-Absturz bei (früher `0x5904DE8`), der Handshake bestätigte Build, Mod-Version und Rolle.
- **[Belegt] Drei Rundenenden ohne Absturz.** Tag 1, Tag 2 und nach dem Wiederbeitritt ein drittes. Der Client öffnete Rundenübersicht
  und Lager jeweils selbst, „Bestätigen wartet auf alle“. Der frühere Client-Absturz beim Rundenende auf `26495302` (`0x59677FD`) trat nicht auf.
- **[Belegt] Dodge des Clients.** 40 Dodges des Clients kamen beim Host mit Richtung an, keiner mit (0,0). Der Mensch am Client sah kein
  Rutschen, die Ausdauer sank wie beim Host, Richtungswechsel und gedrückt gehaltene Taste verhielten sich wie beim Host, beide Figuren
  bewegten sich beim jeweils anderen ohne Sprünge. Zwei Abbrüche durch den Host setzte der Client sofort zurück („Reparatur 1/2“).
  Hinweis: Der Host setzte 8-mal hängende Dodge-Werte der Client-Figur zurück, die Reparatur griff jedes Mal. Im Client-Log stehen
  Dodges oft paarweise im Abstand von etwa 70 ms, ohne dass der Mensch einen doppelten Dodge bemerkte; Ursache nicht untersucht.
- **[Belegt] Airdrops.** 5 Dodges in bzw. durch Airdrops auf beiden Seiten ohne Absturz. Der abgesicherte Fall in `SetupInventory`
  (kein Inventar) trat nicht auf, darum keine Aufrufer-RVA. Belohnungswahl beim Client: Edit vom Host übernommen, Item bei beiden gleich.
- **[Belegt] Inventar kurz abweichend.** Der Desync-Melder meldete dreimal `ABWEICHUNG Inventar`, jeweils während einer Airdrop-Auswahl
  oder beim Umlegen im Lager, und war nach etwa 10 s wieder `OK`. Keine dauerhafte Abweichung wie am 06.11.
- **[Gemeldet] Verkaufen und Reparieren im Lager gehen beim Client nicht**, beim Host schon. Items verschieben geht. Am Rundenende lassen
  sich beim Client nicht reparierte Items nicht ausrüsten. Offen, ob das Spiel kaputte Items auch beim Host sperrt. Neue Aufgabe `lager-handel-client`. **Nachtrag (`lager-handel-client`):** Behoben, das Spiel sperrt kaputte Items auch beim Host; siehe [Verkaufen und Reparieren beim Client](../src/Game/README.md#verkaufen-und-reparieren-beim-client).
- **[Belegt] Client im Lager hart beendet.** 11:43:41 Prozess des Clients beendet. Der Host erkannte den stummen Client um 11:43:50 und
  wartete; auch nach seinem eigenen Bestätigen (11:43:56) meldete er nicht „alle bereit“. Beim Verbindungsabbau (Timeout 11:43:58,
  Grund 4001) löste der Host die P2-Bindung und holte sein Bestätigen nach („Client nicht mehr im Lager“), reiste aber nicht ab: Laut
  Host-Log blieb er bis zum Wiederbeitritt im Lager, die Reise begann erst 11:44:45.
- **[Belegt] Wiederbeitritt in die laufende Mission.** Mit derselben Lobby-ID trat der neu gestartete Client direkt der laufenden Mission
  bei, nicht der MetaBase. Der Host reichte das Rundenende nach, der Client stand wieder im Lager („Bestätigen nachgeholt (alle bereit)“).
  Host-Log ab 11:44:29: Verbindung angenommen, Handshake bestätigt, Karte und Rundenende gesendet, P2 neu gebunden (`Spieler=2 gültig=true`),
  11:44:30 „Client im Lager“. Das vor dem Abbruch abgegebene Bestätigen des Hosts blieb gültig: Als der Client 11:44:43 bestätigte, reiste
  der Host 11:44:45 sofort zum nächsten Tag. Darum brauchte der zweite Abbruchtest ein weiteres Rundenende (Tank gefüllt, Busflucht).
  `Start-Session.ps1 -Role Client` wartet danach weiter auf die MetaBase und muss abgebrochen werden.
- **[Belegt] Host beendet im Lager.** Rundenende 11:47:01, beide im Lager, keiner bestätigte. 11:47:24 beendete der Host über das Menü:
  Spielstand im Mod-Ordner gespeichert, 11:47:25 Hauptmenü, 11:47:27 Lobby geschlossen und Transport beendet. Der Host-Prozess lief bis
  zum Schließen von Hand (11:51) ohne Absturzbericht. Der Client meldete
  „Spiel läuft ohne Host“, „Der Host hat die Sitzung verlassen“ und landete ohne Absturz im Hauptmenü.

### Gate G2 auf zwei PCs am 07.11.2026 (`integration-g2`)

Build `26495302`, Mod-Stand `main` `8705c9a`, nur Mod (`-Mod -Tunnel Lobby -NoSpike`, Host mit `-PartialPatches`), zwei Haushalte
über das Steam-Relay, Ping 40 bis 47 ms, kein verworfenes Paket. Run per `Start-Run.ps1`, danach eine Mission auf `DDDynamic_Map`
von 11:54:55 bis nach 12:04:53 (Log-Zeit), also rund zehn Minuten mit Laufen, Kampf, Dodges und Airdrops. Ausgewertet: Mod-Log und
Absturzberichte des Hosts, Rückmeldung der Menschen an beiden PCs.

- **[Belegt] Stabil.** Keine Trennung während der Mission, keine Absturzberichte beim Host, beide Menschen meldeten keine Probleme.
- **[Belegt] Kein dauerhafter Desync.** Der Desync-Melder meldete 44-mal `OK` und sechsmal kurz `ABWEICHUNG` (fünfmal Inventar, zweimal
  Gegner, einmal beides), jeweils innerhalb weniger Sekunden wieder `OK`, wie am 07.11. bei Airdrop-Auswahl und Umlegen.
- **Ergebnis:** Gate G2 bestanden, die Runde war stabil, ohne dauerhaften Desync und spielbar. Offen für Phase 5 bleibt `update-welt`.

### Live-Test auf zwei PCs am 10.11.2026 (`livetest-1011`)

Build `26495302`, Mod-Stand `main` `8a0c493` (Mod-Version 9), zwei Haushalte über die Lobby, Ping 43 bis 54 ms, kein verworfenes
Paket. Ein PC Client, der andere Host. Ausgewertet sind nur das Mod-Log und der Absturzbericht des Clients sowie, nur lesend, sein
Speicher; das Host-Log lag nicht vor. Drei Rundenenden, 18 Airdrop- und Level-up-Auswahlen, 20 Edits des Clients (19 übernommen,
1 abgelehnt), Desync-Melder 77-mal OK, 28-mal Abweichung (fast alle während offener Auswahl oder Lager, danach wieder OK).

- **[Belegt, Client] Bestätigt:** Lobby-Beitritt über die Steam-Einladung, Handshake, Karte nach etwa 2 s, Pausen bei jeder Auswahl
  (`pausen-sync`), Airdrop- und Level-up-Edits samt neuer Felder und Aufwertung im Bearbeitungsmodus, Lager wartet auf beide
  (`bestaetigen-rundenende`), Busabfahrt mit vollem Tank. Für `lager-handel-client`: 5 Verkäufe in einem Edit vom Host übernommen,
  Schrott danach beim Client auf dem Stand des Hosts (80). Reparatur und Ausrüsten nicht in einem übernommenen Edit geprüft.
- **[Belegt, Client] Großer Lager-Edit abgelehnt (`lager-edit-ablehnung`).** 17:39 MEZ: 6 Verkäufe, Reparatur einer Granate, 2
  Aufwertungen und 4 neue Felder in einem Edit, vom Host nach 0,3 s abgelehnt. Wiederherstellung `vollständig=false` (Revolver
  abweichend, „Felder neu angezeigt, 13 statt 17“). Danach liefen zwei Aufwertungen „außerhalb einer Bearbeitung … läuft nativ“ nur
  beim Client, Verkäufe wurden verworfen, das Bestätigen war wegen zu vieler Items gesperrt, bis der Mensch Items ins Lager legte.
  **[Gemeldet]** Angebaute Felder, Luftpumpe und die violette Schrotflinte fehlten danach; die Rucksackgrafik stand neben den Feldern,
  die Felder blieben nutzbar. Der Ablehnungsgrund steht nur im Host-Log.
- **[Belegt, Client] Absturz beim Bestätigen im dritten Lager (`lager-bestaetigen-absturz`).** 17:56:50 „Client bereit, wartet auf den
  Host“, 17:56:51 „Bestätigen nachgeholt (alle bereit)“, Sekundenbruchteile später `EXCEPTION_ACCESS_VIOLATION`: Ausführung an Adresse
  0 (Aufruf über einen leeren Funktionszeiger), aufgerufen aus `DDSurvivors-Win64-Shipping.exe+0x5a71965`, darüber Rücksprünge in
  `DDCoop.dll` (`+0x283950`, `+0x2843f8`, `+0x28ce10`, `+0x9d75d`, `+0x86051`, `+0xc849e`). **Vermutung:** Das nachgeholte native
  Bestätigen (`ReleaseRoundEndConfirm`) trifft ein Panel, das schon abgebaut wird. Der Crash-Reporter blieb gesperrt, Bericht nur lokal.
- **[Belegt, Client] Schrott springt beim Aufsammeln (`opt-schrott-anzeige`).** 764 Korrekturen „Schrott: Stand des Hosts übernommen“,
  in Gruppen alle etwa 270 ms, lokal kurz bis 185 statt 9; der Endstand stimmt.
- **[Belegt, Client] Takt.** 1025 Meldungen „Takt: … hielt den Frame … auf“ (Busbalken, Patches, Sync, Sync-Fortschritt, Boss-Leiste,
  meist 8 bis 13 ms), siehe `opt-client-takt`.
- **[Gemeldet]** Die Client-Figur schaut beim Client nur nach hinten (`opt-blickrichtung-client`), die Laufanimation stimmt nicht
  (`opt-laufanimation-client`), Warnanzeigen sprintender und spuckender Zombies fehlen (`opt-gegner-indikatoren`), Sound-Fehler der
  Level-up-Kiste im Endbildschirm (`opt-levelup-sound`), der Client stempelt das geschaffte Level nicht ab (`opt-rundenstempel`).
- **Nicht geprüft:** `fortschritt` (Giftwolke, Item-Effekte, Werte nach Upgrade), Reparatur und Ausrüsten für `lager-handel-client`.

### Client-Darstellung im Live-Spiel am 07.11.2026 (`todo-client-optik`)

Build `26495302`, Mod-Stand `main` `971819d`, zwei PCs über die Lobby, Ping etwa 46 ms. Gemeldet von den beiden Spielern, dazu nur
lesend der Speicher des Clients (Objekt- und Funktionsliste, keine Eingaben, keine Änderung). Der Host-PC wurde nicht gelesen.

- **[Gemeldet] Schadenszahlen fehlen beim Client.** **[Belegt, Client]** Die Anzeige läuft über `DamageNumberActor.ShowDamageText`
  (nativ, kein Netz-RPC); beim Client gab es 0 `DamageNumberActor` und 0 `DamageNumberWidget`. Daneben gibt es die Einstellung
  `BP_EnableDamageNumbersSetting` (`DDGameUserSettings.GetEnableDamageNumbers`). **Vermutung:** Die Zahlen entstehen beim Schaden, und
  den rechnet nur der Host. Lokal zuerst: Der Client zeigt sie selbst, wenn die replizierte Gesundheit eines Gegners sinkt.
- **[Gemeldet] Körperteile sterbender Zombies fehlen beim Client.** Eine eigene Klasse dafür war nicht zu finden (gesucht: Gib, Gore,
  Dismember, BodyPart, Corpse). **Vermutung:** Sie entstehen im Todesablauf beim Host, wie Blut und Treffereffekte (`opt-blut`).
- **[Gemeldet] Der Lootbalken fehlt beim Client** (Fortschritt beim Durchsuchen von Kisten, Tonnen und Säcken). **[Belegt, Client]**
  `BP_EnvironmentObjectLootspot_C` hat `OnLootingUpdate`, `OnRep_IsLooting`, `ExecuteLooting` und `TryPlayFinishedLootingSound`; beim
  Client gab es 75 davon und 29 `WBP_LootWidget_C`. **Vermutung:** Der Host treibt den Balken in `OnLootingUpdate`, beim Client kommt nur
  das replizierte `IsLooting` an. Lokal zuerst: Balken beim Client aus `IsLooting` und der Lootdauer selbst laufen lassen. Behoben und
  genauer belegt unter [Lootbalken beim Client](#lootbalken-beim-client-opt-lootbalken): Auch `Progress` repliziert, ein eigener Timer ist unnötig.
- **[Gemeldet] Kein Geräusch, wenn der Client mit dem Dash gegen ein Objekt prallt.** Seit `ausweichen-client` führt der Host den Dodge
  des Clients nativ aus. **Vermutung:** Der Aufprall und sein Sound laufen deshalb nur beim Host.
- **[Belegt, Client] Inventar weiter abweichend.** Der Desync-Melder des Clients meldete ab spätestens 12:44 (Log-Zeit, UTC) alle 30 s
  `ABWEICHUNG Inventar`, siehe `inventar-client-abweichung`.

Alle vier gemeldeten Punkte verhindern keinen Run und stehen deshalb unter den optionalen Optimierungen.

### Steam-Lobby über das Internet (`lobby-live`)

Ausgewertet aus den Mod-Logs beider Sitzungen vom 06.11.2026 (siehe oben), ohne eigenen Termin. Zwei Steam-Konten in zwei Haushalten,
Route jeweils Steam-Relay `fra`, Ping 32 ms.

- **[Belegt] Beitritt nur über die Einladung bis in die MetaBase.** Zweite Sitzung, Client: „Einladung über Steam angenommen“,
  Lobby betreten, P2P über Relay verbunden, „Protokoll: Build, Mod-Version und Gegenrolle bestaetigt“, Tunnel steht, „Welt: MetaBase,
  Client, 2 Spieler“, alles binnen 1,6 s. Der Handshake des [Kontrollkanals](CONTROL_PROTOCOL.md) ist damit über das Internet
  bestätigt; der Desync-Melder meldete 6 s später `OK` für alle Bereiche. Das deckt auch den ersten Fall von `lobby-auth-live`
  (eingeladener Freund verbindet), die Aufgabe bleibt aber wegen der beiden anderen Fälle offen.
- **[Belegt] Weitere Beitritte.** Zwei Beitritte ohne Einladungsvermerk (Lobby-ID) und einer nach Neustart des Hosts verliefen genauso.
  Auf dem Host-PC der ersten Sitzung stand „Lobby erstellt“, „P2P bereit, nur Lobby-Mitglieder“ und 12 s später der Client über Relay.
- **[Belegt] Host verlässt.** Zweimal „Der Host hat die Sitzung verlassen“ (Grund 1001 `ddcoop`): Der Client trennt im Spiel, landet im
  Hauptmenü und kann sofort einer neuen Lobby beitreten. Ein Beitritt zur geschlossenen Lobby endet mit „Die Lobby gibt es nicht mehr,
  der Host hat sie geschlossen“ ohne Absturz.
- **[Belegt] Host nach Verbindungsverlust.** Erste Sitzung: „Verbindung zum Mitspieler verloren“, danach „Lobby geschlossen: Das Spiel
  ist im Hauptmenü“. Auslöser war allerdings der Absturz des Clients, kein geordnetes Verlassen.
- **Nicht geprüft:** Start über `+connect_lobby` bei geschlossenem Spiel und geordnetes Verlassen des Clients aus Sicht des Hosts. Das
  war der Rest in `lobby-live-rest`, belegt am 07.11.2026, siehe [Lobby-Abnahme](#lobby-abnahme-auf-zwei-pcs-am-07112026-lobby-auth-live-lobby-live-rest).

### Lobby-Abnahme auf zwei PCs am 07.11.2026 (`lobby-auth-live`, `lobby-live-rest`)

Build `26495302`, Mod-Stand `main` `e0fdacd` beim Host, beim Client derselbe Stand plus `lobby-auth-live` (nur Skripte und eine Logzeile,
gleiche Mod-Version). Nur Mod, kein Spike. Zwei Konten in zwei Haushalten, Route Steam-Relay `fra`, Ping 39 bis 44 ms. VitaminStack Host
mit `Start-Session.ps1 -Role Host -Mod -Tunnel Lobby -NoSpike` und **einer** Lobby für alle drei Teile, dieser PC Client. Zeiten in UTC
aus den Mod-Logs beider Seiten. Kein Absturz auf beiden Seiten, der Host lief durchgehend in der MetaBase.

| Teil | Ablauf | Ergebnis |
|---|---|---|
| B: Beitritt und geordnetes Verlassen | Client `-Role Client -Tunnel Lobby -NoSpike -Peer <Lobby-ID>`, dann `Guided-Test.ps1` mit `guided/lobby-abnahme.txt` (5 von 5 ok), „Verlassen“ über das Overlay | 11:33:37 Host: „eingehende Verbindung eines berechtigten Kontos angenommen“, Handshake, MetaBase mit 2 Spielern, Desync `OK`. 11:35:29 Client: Transport beendet, `disconnect`, 0,2 s später Hauptmenü, Overlay „Sitzung verlassen“, Erstellen und Beitreten wieder frei. Host 11:35:35: Grund 1002, „Der Mitspieler hat die Sitzung verlassen“, „Welt: MetaBase, Host, 1 Spieler“, P2-Bindung gelöst, Lobby bleibt offen |
| C: Start über `+connect_lobby` | Client-Spiel geschlossen, `Start-Session.ps1 -Role Client -Mod -Tunnel Lobby -NoSpike -SteamLaunch`. Host lädt über das Overlay ein, der Mensch nimmt die Einladung in Steam an | Steam startete das Spiel aus seiner Installation mit `+connect_lobby <Lobby-ID>`. Client-Log 11:38:54 „Start über Steam-Einladung (+connect_lobby, Kommandozeile)“, 11:38:56 Steam bereit und Beitritt, 11:38:58 P2P über Relay, 11:38:59 „MetaBase, Client, 2 Spieler“, ohne weiteren Klick. Host 11:39:04 berechtigtes Konto angenommen. Danach erneut „Verlassen“: Host 11:41:07 Grund 1002, 1 Spieler, Lobby offen |
| A: Konto außerhalb der Lobby | Client nicht in der Lobby, `-Role Client -Mod -Tunnel Mod -NoSpike -Peer <SteamID64 des Hosts>` | Host 11:42:18 „Verbindung von einem Konto außerhalb der Lobby, warte bis zu 5000 ms auf die Lobby“, 11:42:23 „eingehende Verbindung abgelehnt“. Client: Grund 1005, „Der Host hat die Verbindung abgelehnt: nicht Mitglied seiner Lobby“. Der zweite Versuch (11:42:30) endete gleich, dann Sitzung beendet. Kein „angenommen“, der Host blieb bei 1 Spieler, der Platz frei |

- **[Belegt] Alle drei Fälle von `lobby-auth-live`.** Eingeladener Freund verbindet (Teil C, wie schon am 06.11.), ein Konto außerhalb der
  Lobby bekommt 1005 und keinen Platz (Teil A), der Host trennt nach dem Verlassen der Lobby und gibt den Platz frei (Teile B und C).
  Der Weg über `RevokeAfterMs` (Konto verschwindet aus der Lobby, Verbindung bleibt) lief live nicht, weil „Verlassen“ Transport und Lobby
  zusammen schließt; er bleibt durch `AuthorizationTests.cpp` abgedeckt.
- **[Belegt] `lobby-live-rest`.** Start durch Steam bei geschlossenem Spiel bis in die MetaBase ohne Klick, geordnetes Verlassen zweimal
  mit freiem Platz beim Host, ohne Absturz und ohne Rest-Lobby beim Client.
- **Hinweis zum Werkzeug.** `-SteamLaunch` legt die Mod für den Test in die Steam-Installation, weil Steam nie `PinnedGameDir` startet,
  und läuft dort ohne eigenes `-UserDir`; die Mod speichert trotzdem nur in `SaveGames_DDCoop`. `Stop-Session.ps1` nahm die Mod danach
  wieder heraus. Ein Probelauf ohne Einladung meldete richtig „ohne +connect_lobby“.

### Lager-Edit des Clients mit Reparatur und Aufwertung (`inventar-client-abweichung`)

Lokaler Test am 09.11.2026 (Build 26495302, `Start-Session.ps1 -Role Both -Mod -Tunnel Lobby -NoSpike`), Mensch spielte den Client.

- **[Belegt, Host-Absturz] Zurückrollen schickte Server-RPCs an den Host.** Nach „Edit mit 1 Aufwertungen ungültig, Bearbeitung
  zurückgenommen“ beim Client stürzte der Host rund 0,4 s später ab (`EXCEPTION_ACCESS_VIOLATION reading address 0xf0` im Spielcode,
  Aufruf über den durchgereichten `ProcessEvent`-Hook). `RevertClient` setzte `Phase::Idle` vor `Apply`. Verschachtelte Server-RPCs der
  nativen Inventarfunktionen (zum Beispiel `CharacterEffectManager.*`) liefen dadurch wieder zum Host und nannten die nur beim Client
  vorhandene Aufwertungs-Ausgabe. **Fix:** eigene Phase `Reverting`, in der fremde RPCs verworfen und eigene Inventar-RPCs nur lokal
  laufen; `Idle` erst nach `Apply`. Welche Funktion beim Host las, zeigt der Stapel nicht; nach dem Fix kein Absturz mehr.
- **[Belegt] Ganzes Lager-Edit abgelehnt, Inventar am nächsten Tag zurückgesetzt.** Der Client reparierte ein kaputtes Item (Lighter,
  Pistole) und wertete es im selben Edit auf. Ein verbrauchtes Item übernahm im Edit den Ausgangszustand aus `Session.States`, also
  „kaputt“. Der Host zählte die Reparatur nicht, `CanItemsBeUpgradedTogether` lehnte das bei ihm noch kaputte Item ab, und damit fiel das
  ganze Edit samt Verkäufen und Verschiebungen weg. **Fix:** Der Client merkt sich lokal reparierte Items (`Session.Repaired`) und meldet
  verbrauchte davon als repariert. Der Host bucht solche Eingaben als Reparatur, löscht das Kaputt-Flag nur für die native Prüfung kurz,
  repariert sie vor `UpgradeItems` nativ und bucht den Schrott wie jede Reparatur.
- **[Belegt] Nachtest.** Reparatur und Aufwertung im selben Edit: „Aufwertung 1 geprüft (Ablegen)“, „1 von 1 verkauft, 1 von 1 repariert“,
  „vollständig=true“, danach `Desync: OK … Inventar`. Am nächsten Tag hatte der Client die aufgewertete Waffe. Eine spätere kurze
  `ABWEICHUNG Inventar` fiel in eine offene Airdrop-Auswahl: Beim Client standen die drei lokalen Wahlinstanzen (`IsReward`) im Inventar.
- **Abgegrenzt.** Der dauerhafte Fehlalarm durch die Rasterposition von Items außerhalb des Rucksacks bleibt bei `opt-desync-pivot`.

### Sekündlicher Ruckler beim Host (`host-lagspike`)

Ein-PC-Test am 09.11.2026 (Build 26495302, `Start-Session.ps1 -Mod -Tunnel Lobby -NoSpike`), Mensch spielte beide Seiten. Gemeldet war
etwa einmal pro Sekunde ein kurzer FPS-Einbruch beim Host, „wie ein Herzschlag“.

- **[Belegt] Ursache: Suchen über das ganze Objekt-Array im Sekundentakt.** Neue Zeitmessung im Game-Thread-Takt
  (`GameThread::SlowScope`, Log `Takt: <Name> hielt den Frame <ms> ms auf` ab 8 ms) zeigte beim Host in der MetaBase rund 75 ms für
  die Patch-Ticks in einem Frame (Karte, Spieler, Projektile, Bewegung) und 25 ms für den Desync-Abgleich. Jeder dieser Abgleiche lief
  einzeln über alle Einträge von `GObjects`.
- **[Belegt] Teuer je Objekt waren drei Dinge:** `Memory::Rva` suchte für `GObjects` und `GNames` bei jedem Zugriff in Adresstabelle und
  Signaturliste (jetzt `Memory::CachedRva`), `IsA` verglich die Klassenkette Namen für Namen als Text (jetzt Ergebnis je Klasse gemerkt,
  geprüft über den Namensindex der Klasse), und `CachedInventory` in `RewardPatches.cpp` verwarf sein Ergebnis bei Inventaren mit
  `Outer == PlayerState` und suchte jedes Mal neu.
- **[Belegt] Sekunden-Abgleiche über `ULevel::Actors`:** Spielerfiguren (`PlayerPatches`, `MovementPatches`), Projektile, Airdrops und
  Kamera/Controller des Clients lesen jetzt die Actor-Liste des Levels (`Objects::ForEachLevelActor`) statt des Objekt-Arrays.
- **Ergebnis:** Beim Host im Run kein Abschnitt mehr über rund 5 ms, der Mensch spürt keinen Ruckler mehr. Unit-Tests bestehen.
- **Rest beim Client:** Fortschritts-Anzeige (Widget-Suche je Sekunde) rund 12 ms und Boss-Leiste rund 10 ms, siehe `opt-client-takt`.

### Charakterwahl in der MetaBase (`charakter-wahl`)

Stand 10.11.2026, Build `26495302`, SDK, Disassembly und Live-Test auf einem PC. Auslöser: Beim Host erscheinen immer dieselben Spielfiguren.

- **[Belegt, SDK]** `UCharacterSelectionManager` (`UAbstractPersistentGameInstanceSubsystem`, `IRunPersistable`) hält `CharacterDefinitions`, `SelectedCharacters` (`TMap<int32, UPlayerCharacterDefinition*>`, Schlüssel Spielerindex) und die SaveGame-Felder `SelectedCharacterID`, `SelectedCharactersJson`. Er lebt in der GameInstance und repliziert nicht. Funktionen: `SetSelectedCharacter(PlayerIndex, Def)`, `GetSelectedCharacter`, `HasExplicitSelection`, `GetUnusedCharacterDefinitionOrRandom(Seed)`, Delegate `OnCharacterSelectionChanged`.
- **[Belegt, SDK]** `UCharacterSelectPanel` kennt zwei Spieler: `SelectedButtonsPerPlayer` (Index -> Knopf), Marker P1/P2 (`SetSelectionChipForPlayer`, `RefreshAllPlayerChips`), `HandleCharacterSelectAuthorityChanged`. Im Spiel wählt P2 am selben PC.
- **[Belegt, SDK]** `UCharacterDefinitionApplier` (Komponente an `ADDCharacter`) hält `CharacterSelectionManager` und `AppliedCharacterDefinition`, `ApplyInitialCharacterDefinition` wendet die Wahl an, `CharacterSelectionChanged(PlayerIndex, Def)` reagiert auf Änderungen. `AWandererManager` zeigt die Figuren in der MetaBase (`SpawnCharacter`, `OnCharacterChanged`, `OnPlayerJoined`).
- **[Belegt, früherer Befund]** Beim Host liefert `GetPlayerIndex` für den Controller des Clients Index 1 ([Remote-P2](#remote-p2-geprüfte-anbindung-und-grenze-der-lokalen-ui)).
- **[Belegt, Disassembly]** Die Vermutung „Index 1 beim Host“ stimmt nicht: Der Applier nimmt den Spielerindex aus `Pawn.Controller.Player` (vfunc `+0x330` des LocalPlayers), ohne LocalPlayer gilt Index 0. Eine Remote-Figur bekommt darum auf jeder Seite die eigene Wahl dieser Seite: beim Host die Figur des Clients, beim Client der Proxy des Hosts. Fehlt der Eintrag, fällt er auf `CharacterDefinitions[0]` zurück. `CharacterSelectionChanged` greift nur, wenn der Controller-Index zum gemeldeten passt, und erreicht eine Remote-Figur nie. Die eigentliche Arbeit macht die interne Funktion `0x5840DF0` (Definition vergleichen, Asset asynchron laden, Rest im Callback). Eine Definition steuert auch Rucksack, Startausrüstung, Loadout-Stufen und Klassen-Override, ist also mehr als Optik.
- **[Belegt]** Betritt die Remote-Figur beim Host die Kapsel eines MetaBase-POI, lief `TriggerCollisionActionForPlayer` mit dem Remote-Controller: kein Panel, aber `IgnoreMoveInput` blieb an beiden Controllern des Hosts stehen, der Host konnte sich nicht mehr bewegen.
- **Umsetzung:** Panel, Marker und Wahl im eigenen Manager bleiben nativ lokal. Jede Seite schickt nur den Namen der eigenen Definition (etwa `CD_BananaPerson`, Kontrolltyp `0x800`, bei Änderung und alle 2 s, damit Verlust, Beitritt und Reise ihn nachholen). Jede Seite wendet die Wahl der Gegenseite mit `0x5840DF0` auf die Remote-Figur an: beim Spawn über einen Hook auf `ApplyInitialCharacterDefinition` (`0x5837220`), danach per Sekunden-Abgleich. Die Spielwerte der Client-Figur rechnet wie bisher der Host, jetzt mit der Definition des Clients. Doppelte Wahl lehnt die Mod nicht ab: Jede Seite entscheidet nur über die eigene Figur, eine gemeinsame Autorität ist dafür nicht nötig. Beim Host übergeht `FilterRemotePoiOverlap` Betreten und Verlassen eines POI durch die Remote-Figur, der Client öffnet das Panel bei sich selbst. Lesewerkzeug: `tools/spike/characters.py <pid>`; `Start-Session.ps1 -RealSave` startet mit einer Kopie des echten Spielstands, weil ein frischer keine Figurenauswahl freigeschaltet hat.
- **Live-Test 10.11.2026, ein PC, zwei Instanzen:** Beide starten mit `CD_PraesidentKnabe`, die Remote-Figur wird beim Spawn mit der Wahl der Gegenseite angewendet. Der Client wählt in der MetaBase `CD_BananaPerson`, rund 0,4 s später wendet der Host sie auf die Client-Figur an, vom Menschen im Bild bestätigt. Der Host blieb beweglich, der Filter griff beim Betreten und Verlassen des POI. **Nicht geprüft:** zwei PCs, Reise und erneuter Beitritt mit gehaltener Wahl, Startausrüstung der gewählten Figur im Run. Rest: Der Sekunden-Abgleich hielt einmal 10 bis 11 ms den Frame auf (volle Objektsuche nach dem Applier), siehe `opt-charakter-takt`.

## Systeme, die die Mod respektieren muss

- **[Belegt]** Bestenlisten und Erfolge: `PixelLeaderboards`, `DDLeaderboardSettings`, `PixelAchievements`, im Mod-Betrieb gesperrt, siehe unten
- **[Belegt]** Telemetrie und Berichte: `DDBalanceTelemetrySettings`, `DDCorruptedSaveAutoReportSettings`, `DDBugReportForm`, `PixelsplitFeedbackForm`, im Mod-Betrieb abgeschaltet, siehe unten
- **[Belegt]** Spielstände: `PixelsplitSaveSystem`, `LocalPlayerSaveGame`
- **[Belegt]** Zeitsteuerung: `PixelsplitTimescaleSystem`. Für Online-Spiel relevant, weil Pausen bei beiden gelten müssen.

### Erfolge und Bestenlisten (`online-sperren`)

Stand 04.11.2026, Build `26187885`, Steamworks-Laufzeit 1.64.

- **[Belegt]** Die Exe importiert `steam_api64.dll` nicht statisch. Das Online-Subsystem lädt sie zur Laufzeit und holt sich die Schnittstellen über ihre Versionsnamen, für Erfolge, Statistiken und Bestenlisten `STEAMUSERSTATS_INTERFACE_VERSION013`. Alle Schreibwege enden dort, die Methoden liegen in `steamclient64.dll`.
- **Mod:** `Core::SteamStats` sperrt die schreibenden Methoden von `ISteamUserStats`, unabhängig von Spiel-Offsets: `SetAchievement`, `IndicateAchievementProgress`, `SetStatInt32`, `SetStatFloat`, `UpdateAvgRateStat`, `StoreStats`, `UploadLeaderboardScore`, `AttachLeaderboardUGC`. Den vtable-Platz jeder Methode liest die Mod aus dem Code der flachen Exporte (`SteamAPI_ISteamUserStats_*`) und hängt sich mit MinHook an die Methode selbst. Lesen und Bestenlisten anzeigen bleibt erlaubt.
- **Zeitpunkt:** Die Mod hängt sich an `SteamInternal_FindOrCreateUserInterface`, über das das Spiel die Schnittstelle anfordert. Jede Statistik-Schnittstelle ist gesperrt, bevor das Spiel sie bekommt; scheitert das Sperren oder ist die Version unbekannt, bekommt das Spiel keine. Ist `steam_api64.dll` beim Start der Mod noch nicht geladen, richtet ein Hook auf `LoadLibraryExW` das beim Laden ein; scheitert es dort, schlägt das Laden fehl, das Spiel läuft dann ohne Steam. Gibt es die Schnittstelle schon, sperrt die Mod sie sofort. Steht die Sperre beim Start nicht, startet die Mod nicht (`Core::Protection`). Der Start von Steam wird nie aufgehalten.
- **Gescheiterter erster Ansatz:** Ein Hintergrund-Thread versuchte alle 50 ms, die Schnittstelle zu holen und zu sperren. Dazwischen hätten Schreibaufrufe durchrutschen können, und bei Fehlern lief die Mod ungeschützt weiter (Review zu #75). Der zwischengespeicherte Zugriff `SteamAPI_SteamUserStats_v013` blieb dabei einmal dauerhaft leer.
- **Test:** zwei Instanzen auf einem PC mit `Start-Session.ps1 -Mod`, Route mit Seed `20795` bis zum Rundenende an Tag 1. Auf beiden Seiten lud das Spiel `steam_api64.dll` rund 80 ms nach dem Start der Mod, die Sperre stand beim Laden. Der Host versuchte rund 7 s später, die Statistiken zu speichern (`StoreStats`), und wurde abgewiesen. Das Spiel lief normal, kein Absturz. Ob es vor dem Start der Mod Schreibversuche gab, ist nicht gemessen; die Mod sieht nur Aufrufe ab ihrer Sperre. Unit-Tests mit nachgebauter Steam-Laufzeit decken späten Steam-Start, den ersten Schreibaufruf, eine schon vorhandene Schnittstelle, gescheiterte Hooks, unbekannte Einsprünge und fremde Versionen ab.
- **Nicht geprüft:** ein echter Erfolg und ein Bestenlisten-Eintrag am Run-Ende. Absichtlich ausgelöst habe ich keinen: Wäre die Sperre unwirksam, stünde der Erfolg dauerhaft im Steam-Konto.
- **[Vermutung]** Das Spiel speichert seinen Erfolgsfortschritt auch im eigenen Spielstand (`AchievementSubsystem`, `PersistedAchievementData`). Läuft ein Mod-Run auf dem normalen Spielstand, könnte das Spiel solche Erfolge später ohne Mod nachmelden. Seit [`mod-saves`](#spielstände-der-mod-mod-saves) laufen Mod-Runs nur auf eigenen Ständen.
- **Gesperrt seit `weekly-sperre`:** die Weekly-Challenge über `ddrweekly.deadlydays.com`, wie Erfolge und Bestenlisten (Entscheidung `weeklymode`). Wie das Menü ohne Antwort reagiert, ist noch nicht im Spiel geprüft.

### Telemetrie und Berichte (`telemetrie-aus`)

Stand 04.11.2026, Build `26187885`. Neben dem Crash-Reporter (siehe [Crash-Reporter](../research/spikes/a-connect/README.md#sperre-in-mod-und-spike-crash-reporter)) schickt das Spiel Daten über das eingebaute libcurl an diese Ziele:

| Weg | Ziel | Schalter |
|---|---|---|
| Balance-Telemetrie (`BalanceTelemetrySubsystem`, Bosskämpfe und Runden) | `ServerBaseURL` aus `DDBalanceTelemetrySettings`, im Spiel `pixelsplit.games` | `bEnabled`, `bAttachSaveFile` |
| Spielstand-Analytics | `https://pixelsplit.games/gamelytics/ddr/save_analytics.php` (POST) | – |
| Bug-Report-Formular (`DDBugReportForm`, `FeedbackManagerSubsystem`) | `https://api.codecks.io/user-report/v1/create-report` | – |
| Automatische Berichte über beschädigte Spielstände (`DDCorruptedSaveReportSubsystem`) | Codecks | `bEnableAutomaticCorruptedSaveReports` |

- **[Belegt]** Alle drei Schalter stehen ab Werk auf an.
- **[Belegt]** Die Mod schaltet sie am Klassen-Defaultobjekt ab (`Game::TelemetryAdapter`) und lässt zusätzlich die Namensauflösung für `codecks.io`, `pixelsplit.games`, `deadlydays.layerapps.com` und `ddrweekly.deadlydays.com` scheitern (`Core::ReportHosts`). Die zweite Schicht hängt nicht an Spiel-Offsets und deckt auch Analytics und Bug-Report-Formular ab, die keinen Schalter haben.
- **[Belegt]** Die HTTP-Anfragen des Spiels laufen über diese Namensauflösung: Im Menü fragt das Spiel `ddrweekly.deadlydays.com` an, und der Hook sieht es.
- **Nicht gesperrt:** Twitch-Anbindung (nur auf Wunsch des Spielers).
- **Gesperrt seit `layerapps-sperre` (04.11.2026):** `deadlydays.layerapps.com`.
  - **[Belegt]** Der Host fragt den Dienst direkt beim Run-Ende an, kurz vor `pixelsplit.games`. Gesehen beim Test zu `patches` nach einer Niederlage.
  - **Vermutung:** Das Spiel meldet dort das Run-Ergebnis. Was es sendet, ist nicht untersucht. Die Sperre folgt der Regel, dass Mod-Runs keine unnötigen Daten senden.
  - Gesperrt ist nur dieser Name samt Unternamen, nicht `layerapps.com` insgesamt.
  - **[Belegt]** Mit der Sperre lief das Run-Ende normal (Niederlage an Tag 1, „Zurück zur Basis“, beide in der MetaBase), danach startete ein neuer Run bis in die Mission.

Test: ein PC, zwei Instanzen mit `Start-Session.ps1 -Mod`, MetaBase und Tag 1 bis zum Rundenende. Auf beiden Seiten meldete die Mod den bestandenen Selbsttest (`api.codecks.io` scheitert ohne Netzanfrage) und die drei abgeschalteten Schalter. Weitere Anfragen gab es im Lauf nicht, das Spiel lief normal weiter, kein Absturz. Das Bug-Report-Formular habe ich bewusst nicht abgeschickt: Wäre die Sperre unwirksam, ginge ein Bericht an Pixelsplit.

### Spielstände der Mod (`mod-saves`)

Stand 07.11.2026, Build `26187885`, ein PC, Mod mit Spike über die Lobby, Godmode, Test-`UserDir` beider Instanzen.

- **[Belegt, Laufzeit]** Der Spielstand liegt im Ordner `<Saved>\SaveGames` (mit `-UserDir` darunter, sonst `%LOCALAPPDATA%\DDSurvivors\Saved`).
  Dateien: `Management_SaveSlot_0.sav`, `SaveSlot_DDR_<n>.sav` je Slot, `WeeklyChallenge.sav`, `EnhancedInputUserSettings.sav`, Sicherungen unter
  `Backups\<Name>_bak.sav`. Beim Start prüft der `UPixelSaveManager` je Stand `_tmp`, `_bak`, `_bak2`, `_bak3` und den Stand selbst.
  Gespeichert wird in Schritten: `_bak` schreiben, `_tmp` schreiben, den Stand löschen, `_tmp` auf den Stand verschieben. In den echten
  Ordner legt Steam `steam_autocloud.vdf`, er wird also über Steam Cloud abgeglichen.
- **[Belegt, Strings]** Der SaveManager baut Dateinamen teils selbst (`.sav`, `*.sav`, `%s__%s__%s.sav`, `SaveSlot_DDR_`) und arbeitet direkt mit
  Dateien, neben `UGameplayStatics`/`ISaveGameSystem` (`%sSaveGames/%s.sav`). Eine reine Slot-Umleitung (`SwitchToNamedSaveSlot`,
  `GetSaveSlotNameForIndex`) hätte Sicherungen und Archivkopien nicht erfasst.
- **Mod:** `Game::SaveRedirect` hängt sich an die Datei-APIs von Windows (`kernelbase`: `CreateFileW`, `CreateFile2`, `DeleteFileW`,
  `GetFileAttributes(Ex)W`, `SetFileAttributesW`, `FindFirstFile(Ex)W`, `CreateDirectoryW`, `RemoveDirectoryW`, `MoveFileExW`,
  `MoveFileWithProgressW`, `ReplaceFileW`, `CopyFileW`, `CopyFileExW`, `CopyFile2`; `kernel32`: `MoveFileW`). Diese Breitzeichen-Funktionen
  importiert die Exe; die CRT endet ebenfalls dort. Jeder Pfad unter `<Saved>\SaveGames` geht nach `<Saved>\SaveGames_DDCoop`, der Ordner selbst
  eingeschlossen. Keine Spiel-Offsets. Umgeleitet werden die Ordner aus `-UserDir`, aus `%LOCALAPPDATA%\DDSurvivors` und neben `Binaries`,
  damit mit Mod kein normaler Stand erreichbar bleibt. Pfadlogik in `SavePaths.h` mit Unit-Tests.
- **Sicherheit:** Die Umleitung sitzt direkt nach `Core::Hooks::Initialize`, vor allen anderen Teilen der Mod. Fehlt eine Umleitung, nimmt die Mod
  die übrigen zurück und startet nicht; das Spiel läuft dann normal auf dem normalen Stand. Lässt sich ein Pfad mit `SaveGames` nicht auflösen,
  lehnt die Mod den Aufruf ab (`ERROR_ACCESS_DENIED`), statt den normalen Stand zu berühren. Ohne Mod-DLL, mit `enabled=false` und bei
  unbekanntem Build gibt es keine Umleitung.
- **Erster Start:** Fehlt `SaveGames_DDCoop`, legt die Mod den Ordner an und kopiert die `.sav`-Dateien der obersten Ebene des normalen Ordners
  einmal hinein (`CopyFileW` ohne Überschreiben, vor den Hooks). Der Meta-Fortschritt bleibt so erhalten. Abschaltbar mit
  `copy_normal_saves=false`. Danach nutzt die Mod nur ihre eigenen Stände, auch allein gespielt. Mod-Runs landen nie im normalen Stand,
  das erledigt auch die Vermutung zu Erfolgen unter [Erfolge und Bestenlisten](#erfolge-und-bestenlisten-online-sperren).
- **[Belegt] Lauf:** Normale Stände beider Instanzen vorher und nachher mit Größe, Zeitstempel und SHA-256 festgehalten. Start mit
  `Start-Session.ps1 -Restart -Mod -Tunnel Lobby -Godmode -Brief`: beide kopierten 2 Stände. Die Hooks saßen rund 2 s vor dem ersten Zugriff des
  Spiels (Hooks 0,6 s nach dem Start der Mod). Alle Zugriffe des Spiels (prüfen, lesen, schreiben, löschen, verschieben) meldeten den
  Mod-Ordner, keiner einen fremden Ordner. `Run-Route.ps1 -Days 2 -StayLast`: Tag 1, Rundenende beim Client mit Rundenübersicht, Lager und
  Reise nach Tag 2, alle Prüfungen sauber, Inventare gleich. Beim Rundenende schrieb der Client `SaveSlot_DDR_0_bak.sav` und
  `SaveSlot_DDR_0_tmp.sav`, löschte `SaveSlot_DDR_0.sav` und verschob `_tmp` darauf, alles im Mod-Ordner (3093 → 7866 Bytes, wie der Befund
  unter [Rundenende](#rundenübersicht-und-lager-beim-client-rundenende-client), dort noch im normalen Stand). Der Host schrieb seinen Stand im
  Mod-Ordner (19228 → 26373 Bytes). Alle sechs normalen Dateien beider Instanzen blieben Byte für Byte gleich, mit unveränderten Zeitstempeln.
- **[Belegt] Wiederladen:** Zweiter Start mit Mod (nach dem Steam-Update aus der eingefrorenen Kopie, siehe [Spielversion](#unterstützte-spielversion)):
  beide meldeten „Mod-Ordner vorhanden“, kopierten nichts und lasen `Management_SaveSlot_0.sav`, `SaveSlot_DDR_0.sav` und
  `WeeklyChallenge.sav` nur aus dem Mod-Ordner, also die von der Mod geschriebenen Stände (Client 7866, Host 26373 Bytes). Kein Zugriff
  außerhalb, normale Dateien unverändert.
- **[Belegt] Ohne Mod:** `Start-Session.ps1 -Restart -Godmode -Brief` ohne `-Mod` (keine `DDCoop.dll`, kein `version.dll`, kein Mod-Log):
  Das Spiel schrieb wie im Original in den normalen Ordner (Host `SaveSlot_DDR_0.sav` 19228 → 19433 Bytes, Sicherungen rückten auf
  `_bak`/`_bak2`). Der Mod-Ordner blieb Byte für Byte gleich.
- **Nicht geprüft:** Rennen beim Start: Die Mod startet auf dem Thread des Loaders parallel zum Spiel. In allen Läufen saßen die Hooks rund
  2 s vor dem ersten Zugriff. Ein Lesen vor den Hooks wäre harmlos, ein Schreiben in den normalen Stand vor den Hooks ist nicht beobachtet
  und nicht ausgeschlossen. Solo-Spiel mit Mod nur über denselben Codeweg abgeleitet (die Umleitung kennt keine Rolle). Ein echter
  Spielstand unter `%LOCALAPPDATA%` wurde absichtlich nicht berührt.

## Allgemeines

- Entwickler laut SteamSpy: Pixelsplit. Version 1.0 mit fertigem Local-Coop erschien am 29.10.2026, also kurz vor Projektstart.
  Es ist mit weiteren Patches zu rechnen.
- Zu Mod-Support, Workshop oder offiziellem Online-Coop wurde nichts gefunden. Das heißt nicht, dass nichts geplant ist.
