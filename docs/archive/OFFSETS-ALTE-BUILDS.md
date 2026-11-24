> **Archiv.** Historischer Stand, nicht aktuell. Gilt nur, wenn eine aktuelle Doku darauf verweist.

# Offsets alter Spiel-Builds

Abschnitte aus [OFFSETS.md](../OFFSETS.md), die nur für Builds vor `26786272` gelten oder den Umstieg dokumentieren. Werte hier nicht für den unterstützten Build verwenden.

## Referenz-Build 26495302 (`spiel-update`)

Stand 09.11.2026. `Game::AddressMap::ReferenceBuild` ist `26495302`, `Core::SupportedBuilds`, `Common.ps1`, `game_build.py` und der
Installer kennen nur noch diesen Build. Alle RVAs in `src/` und `research/spikes/a-connect` wurden mit der Tabelle aus `update-basis`
(98 Einträge der Mod, 7 des Spikes) und 18 weiteren, mit `tools/rva_map.py` eindeutig umgerechneten Adressen aus Kommentaren ersetzt. Die
Tabelle selbst ist entfernt; für alte Werte weiter unten: `git show 0cb3d80:src/Game/RvaMap26495302.inc` (Referenz-RVA `26187885` -> `26495302`).

- Verzweigungen nach dem alten Build entfernt: `PanelIsDragging` fest `0x788`, Prüfungen von `HandleConfirmButton` und
  `RepresentationSize` nur noch mit den Bytes von `26495302`. `PatchSignatures.inc`: Zeilen von `26187885` entfernt, die von `26495302` gelten für alle.
- Offline gegen die Exe von `26495302`: `tools/expect_check.py` 74 `ok`, 2 `uebersprungen`; `tools/patch_signatures.py check` 89 `ok`.
- Spieltest ein PC, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -Godmode`: Host und Client `Signaturen: 89 Adressen geprüft`, `Patches: 44 gesetzt`,
  `Spiel-Build 26495302`, Handshake bestätigt, Spike ohne fehlende Adresse. `Run-Route.ps1 -Days 2 -Godmode` Exitcode 0: MetaBase, Tag 1 (159/159 Kacheln),
  Tag 2 (63/63), je 2/2 Spieler, Inventar gleich, kein `[ERROR]` im Mod-Log.
- Drei Adressen nur aus Kommentaren ließen sich nicht umrechnen (Exec-Thunks von `ReceiveExp` und `HandleItemSold`, Absturzstelle in
  `RepresentationSize`); die Kommentare nennen sie nicht mehr.

## Build 26786272 (`update-basis-26786272`)

Zwischenstand vor dem Umstieg (drei Builds parallel, RVAs dieses Abschnitts als Referenz-RVA `26495302` mit Umrechnung), abgelöst durch
[Referenz-Build 26786272](../OFFSETS.md#referenz-build-26786272-spiel-update-26786272). Stand 20.11.2026, Patch 1.0.3 vom 19.11.2026 (Version laut `CrashReportClient.ini` des Spiels: `1.0.3-main-10811c16`). `26495302` bleibt
Referenz-Build, `26669225` bleibt unterstützt. Für `26786272` rechnet `Game::AddressMap` die Referenz-RVAs über
`src/Game/RvaMap26786272.inc` um. Exe: TimeDateStamp `0x260774D7`, SizeOfImage `0x0B750000`; `.text` ist gewachsen
(`0x0828081C` statt `0x0827BCFC` auf `26669225`). Alles unten ist auf einem PC gemessen, zwei Instanzen, ohne echte Eingaben.

| | 26495302 | 26669225 | 26786272 | Beleg |
|---|---|---|---|---|
| GObjects | `0x0AA67B80` | `0x0AA69B80` | `0x0AA6FC80` | rva_map (Code-Verweis) und Dumper-7-SDK |
| GNames | `0x0A999B40` | `0x0A99BB40` | `0x0A9A1C40` | rva_map und SDK |
| GWorld | `0x0AAEA770` | `0x0AAEC770` | `0x0AAF2870` | rva_map und SDK |
| ProcessEvent | `0x0160C220` | `0x0160C220` | `0x0160C220` | rva_map (eindeutig) und SDK |

- **Code-Adressen:** 124 Adressen aus `src/`. `tools/rva_map.py` rechnet von `26495302` aus 110 um (103 eindeutig, 3 vtables, 3 Globale,
  1 über den Funktionsanfang). Gegenprobe: dieselben 110 von `26669225` aus über dessen Tabelle umgerechnet, alle 110 Ergebnisse gleich.
  Die übrigen 14 sind von Hand bestimmt und in der Tabelle mit `// von Hand` und Beleg vermerkt; **keine Adresse fehlt**:

  | Referenz-RVA | 26786272 | Bedeutung | Beleg |
  |---|---|---|---|
  | `0x056676F5` | `0x05667FB5` | Blockade-Thunk (`ClientPatches.cpp`) | einziger `jmp` auf `BlockadeLoadingFinished` `0x058BD200`, dessen 70 Befehle maskiert gleich sind |
  | `0x05707870` | `0x05708220` | `Hands.PlayReloadSound` | 37 eindeutige Befehlsfenster, alle 38 Befehle maskiert gleich |
  | `0x057DBA40`, `0x057DC810`, `0x057DCD60`, `0x057DCE70`, `0x057DD080` | `0x057DCB80`, `0x057DD950`, `0x057DDEA0`, `0x057DDFB0`, `0x057DE1C0` | `OnRep_Armor`, `OnRep_Health`, `OnRep_MaxArmor`, `OnRep_MaxHealth`, `OnRep_MaxStamina` | je 20 bis 30 eindeutige Fenster, alle Befehle maskiert gleich, Verschiebung `+0x930` gegen `26669225` |
  | `0x058B1130` | `0x058B25E0` | `UTackleAbilityExecution::EndTackle` | Funktionsanfang aus `.pdata`, 111 Fenster; **Rumpf geändert** (siehe unten) |
  | `0x058CE280` | `0x058CFD10` | `ShopPanelItem::TryBuyItem` | eindeutig von `26669225` aus, alle 12 Befehle gleich; Prüfbytes wie `26669225` |
  | `0x05998D70`, `0x05998D91` | `0x0599B984`, `0x0599B9A5` | `LootPawnRead`, `LootNoRootTarget` (`PlayerPatches.cpp`) | Bytefolge `mov rax,[rbx+1B8h]`/`test`/`je`/`movups [rax+210h]` genau einmal in `.text`; **Funktion umgebaut** (siehe unten) |
  | `0x05A90940` | `0x05A941D0` | `ClientRefreshFunction` | 179 eindeutige Fenster, 639 von 656 Befehlen gleich |
  | `0x093A1F98` | `0x093A7EF8` | vtable `BackpackSlot` | 82 von 84 Einträgen umgerechnet, alle gleich |
  | `0x094C6A78` | `0x094CD068` | vtable Achievement-Detektor | 162 von 164 Einträgen umgerechnet, alle gleich |

- **Geänderter Code an Patch-Stellen** (drei Stellen, je mit eigener Prüfung für `26786272`):
  - `EndTackle`: Prolog `sub rsp,28h` statt `38h`, die Funktion räumt ein neues Feld `+0x26C` ab und ruft für den alten Mittelteil
    `0x058AE870`. Signatur `(Execution, bool)` und `TackleBlocked` `+0x268` unverändert; der Hook hängt am Anfang und ruft das Original.
    Eigene Prüfbytes in `CheckEndTackle`.
  - Loot-Spawner: Die Funktion nutzt andere Register und Stack-Plätze (`r12` statt `rsi`, `[rbp+20h]` statt `[rbp-20h]`), die gepatchten
    sieben Bytes `48 8B 83 B8 01 00 00` und die Wirkung des Sprungziels (Standardort laden) sind gleich. Der Patch bleibt unverändert,
    die Signaturen von `LootPawnRead` und `LootNoRootTarget` haben dafür Platzhalter bekommen.
  - `UItemRepresentation.ItemInstance` liegt bei `+0xA38` statt `+0xA28` (zwei neue Felder davor, siehe SDK-Vergleich). Das ist der einzige
    fest eingetragene Feld-Offset der Mod, der sich verschoben hat: `RepresentationItem()` in `InventoryPatches.cpp` wählt den Wert je
    Build, `CheckRepresentationItem` belegt ihn je Build mit den Bytes `mov rax,[rsi+0A28h]` bzw. `[rsi+0A38h]`. Ohne die Änderung fiel
    die ganze Liste Inventory aus (gemessen, erster Spieltest: 17 von 18 Listen; dass der Client-Prozess dort nach gut einer Minute in
    der Mission ohne Absturzbericht endete, hängt vermutlich damit zusammen, siehe Spieltest nur mit Mod).
- **Offline gegen die drei Exes:** `tools/expect_check.py` je Build 101 `ok`, 3 `anderer-build`, 2 `uebersprungen`, keine `abweichend`,
  keine `fehlt`. `tools/patch_signatures.py check` je Build 113 `ok`. Drei Signaturzeilen sind dafür angepasst (`EndTackle`,
  `LootPawnRead`, `LootNoRootTarget`). `expect_check.py` kennt jetzt `// Build <id>, <id>` für Prüfungen, die für mehrere Builds gelten.
- **SDK-Vergleich** (`py tools/sdk_diff.py`, SDK von `26669225` gegen SDK von `26786272`, dieses am 20.11.2026 per `inject.py` im Host
  in `DDDynamic_Map` erzeugt, 2.494 Dateien): 351 unverändert, nichts verschoben, verschwunden oder mit geänderter Signatur;
  `UInventoryPanel` wächst von `0xA18` auf `0xA20`, `URecipePanel` von `0x4B0` auf `0x4C0`; `URecipeButton` meldet das Werkzeug als neu, im SDK von `26669225` steht die Klasse aber schon (`0x8A0`, jetzt `0x8B0`). Vier Einträge
  `unbekannt` wie bei `26669225` (ordnet das Werkzeug nicht zu). `Engine_classes.hpp`, `Engine_structs.hpp`, `CoreUObject_classes.hpp`,
  `UMG_classes.hpp`, `GameplayAbilities_classes.hpp` und `DDSurvivors_structs.hpp` sind bytegleich. In `DDSurvivors_classes.hpp` haben
  sich diese nativen Klassen geändert (Feld für Feld aus beiden SDKs verglichen, dazu die davon abgeleiteten Blueprint-Klassen
  `WP_Inventory_Item_C`, `WP_Inventory_Panel_C`, `WP_RecipeButton_C`, `WP_Recipe_Panel_C`, `WBP_PlayerControlSwapper_C`, `WP_Tooltip_Effect_C`):

  | Klasse | Änderung |
  |---|---|
  | `UItemRepresentation` | neu `UpgradableMask` `0xA00`, `GlitterMask` `0xA08` (vorher im Blueprint `WP_Inventory_Item_C`), `DeactivatedIcon` `0xAB0`, `DeactivatedIconMaterial`, `DeactivatedIconMID`, `DeactivatedBackgroundMID`; Felder ab `0xA00` `+0x10`, ab `0xAB0` `+0x30` |
  | `UInventoryPanel` | neu `RecipePanelButtonText` `0x7F8`, Felder dahinter `+0x8`; neue Funktionen `RefreshItemBadgesAfterEffectUpdate`, `CanCraft`, `GetWorkbenchTitle` |
  | `URecipePanel`, `URecipeButton` | `URecipePanel`: neu `ReadOnlyHint` `0x428`, `RecipePanelTitle` `0x430`, elf Felder dahinter `+0x10`; `URecipeButton`: Größe `0x8A0` → `0x8B0`, reflektierte Felder unverändert |
  | `UTackleAbilityExecution` | neu `bClampTargetsToBossArena` `0x101`, zwei Bool-Felder dahinter `+1`; neue Funktion `ProjectTackleTarget` |
  | `UMaterialAnimator` | neu `OverrideMaterialInstance` `0x128`, vier Felder dahinter `+0x8` |
  | `UOrbitingItemManager`, `UPlayerControlSwapper` | Größe `+0x8` bzw. `-0x8` |
  | Funktionsbibliothek Inventar | neu `GetLargestBuffValueSum`, `IsItemSwitchingOffItem`; `GetHandsRequiredForItem` mit geänderter Parameterliste |

  Die Mod nutzt aus diesen Klassen fest nur `UItemRepresentation.ItemInstance` (oben) sowie `UInventoryPanel` `+0x370` und `+0x788`
  (beide vor der Einfügung, unverändert) und `UTackleAbilityExecution` `+0x238`/`+0x268` (unverändert). `GetHandsRequiredForItem` ruft die
  Mod nicht auf. **[Vermutung]** `IsItemSwitchingOffItem` und die `Deactivated…`-Felder gehören zu einer neuen Mechanik, bei der ein Item
  ein anderes abschaltet; ob sie beim Client richtig ankommt, ist nicht geprüft (`update-26786272-run`).
- **SdkChecks:** `SdkChecks.cpp` baut gegen das SDK von `26786272` ohne Fehler (nach der üblichen
  [Nacharbeit](GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) an `Assertions.inl`); `Offsets.h` gilt damit für alle drei Builds. Grenze wie
  immer: Offsets, die nur als Zahl im Code stehen, erfasst das nicht. Die 238 fest eingetragenen Werte in `src/` sind deshalb gegen die
  Liste der geänderten Klassen oben abgeglichen, nicht einzeln gemessen.
- **Dumper-7 auf `26786272`:** Dump mit dem Stand des Submoduls plus lokalem Fix aus `dumper7-image-ende` (nicht im Repo). Mit dem Fix
  brauchte der Dump rund zehn Minuten statt unter einer; die Stichprobe des Dumper-Threads stand in `FindStringInRange` →
  `IsBadReadPtr`. **[Vermutung]** Die zusätzliche Prüfung ruft für jeden Kandidaten `VirtualQuery`; im Fork besser erst
  `IsInAnySection` prüfen. Ohne den Fix auf diesem Build nicht versucht.
- **Spike:** `Builds.cpp` erkennt `26786272` am TimeDateStamp `0x260774D7` und rechnet über `RvaMap26786272.inc` und vier eigene
  Einträge um (`UpdateTag` `0x04B7E230`, Tag `State.Invincible` `0x0AC70218`, die beiden Stellen in `SerializeNewActor` wie auf
  `26669225`; je von beiden alten Builds aus gleich umgerechnet). `tools/spike/ddmem.py` kennt die Globalen.
- **Spieltest `26786272`** (eingefrorene Kopie, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map`, ohne
  `-PartialPatches`): Host und Client `Signaturen: 113 Adressen geprüft`, `Teilinstallation auf Build 26786272, 18 von 18 Listen gesetzt,
  0 Patches ausgelassen`, beide mit 2 Spielern in der Mission. `!wave`, `!give ItemData_Pistol`, danach `Inventar: vollstaendig, gleich`,
  `Test-Godmode.ps1` bestanden, Desync-Melder `OK Tag, Phase, Spieler, HP, Gegner, Inventar`, Schadenszahlen 194 vom Host gesendet und
  194 beim Client angekommen, `!finish`: Rundenende beim Client geöffnet, Lager bei beiden. Kein `[ERROR]`, kein Absturz.
  `Test-Desync.ps1` lief im laufenden Kampf in den Zeitablauf (Feld Gegner wechselt zwischen offen und OK); nicht weiter untersucht.
- **Nur mit Mod, wie bei Spielern** (`-Mod -Tunnel Lobby -NoSpike`, ohne `-PartialPatches`, Endstand dieses Branches): `18 von 18 Listen
  gesetzt, 0 Patches ausgelassen`, Lobby, Reise über den Testkanal in `DDDynamic_Map`, dort dreieinhalb Minuten mit Kampf (65
  Schadenszahlen gesendet und empfangen) und einer Pause der Weltzeit: beide Instanzen laufen, Inventar gleich, kein `[ERROR]`, kein
  Absturzordner. Im ersten Lauf ohne die Inventory-Liste endete der Client-Prozess an derselben Stelle (kurz nach der Pause der
  Weltzeit); damals war zusätzlich Dumper-7 im Host geladen. **[Vermutung]** Ursache war die fehlende Inventory-Liste; mit ihr trat das
  Ende nicht mehr auf, ein Lauf mit Liste und Dumper fehlt als Gegenprobe.
- **Gegenprobe:** derselbe Ablauf auf `26669225` (`18 von 18 Listen gesetzt, 0 Patches ausgelassen`) und `26495302`
  (`Patches: 57 gesetzt`), je Inventar gleich, Rundenende bei beiden, kein `[ERROR]`, kein Absturz.
- **Nicht geprüft:** Lauf über die Routenkarte und mehrere Tage, Händler, Werkbank und Rezept-Panel (dort hat 1.0.3 Klassen geändert),
  Bosse, Sitzung nur mit Mod über Tage, zwei PCs, Installation in die Steam-Kopie. Siehe `update-26786272-run`.

## Build 26669225 (`update-basis-1-0-2`)

Stand 16.11.2026, Patch 1.0.2 vom 14.11.2026. `26495302` bleibt Referenz-Build (`Game::AddressMap::ReferenceBuild`, `Protocol::GameBuild`,
`$SupportedBuild`, `game_build.BUILD`). `26669225` läuft parallel: Für ihn rechnet `Game::AddressMap` die Referenz-RVAs über
`src/Game/RvaMap26669225.inc` um, erzeugt mit [`tools/rva_map.py`](../../tools/README.md#rva_mappy). Exe: TimeDateStamp `0xE9CA3AB7`,
SizeOfImage `0x0B74A000`, `.text` gleich groß (`0x827C000`). Die Portierung der offenen Adressen ist je Patch-Datei aufgeteilt, siehe
[Wartung](../../TODO.md) (`update-1-0-2-*`).

| | 26495302 | 26669225 | Beleg |
|---|---|---|---|
| GObjects | `0x0AA67B80` | `0x0AA69B80` | rva_map (Code-Verweis) und Dumper-7-SDK |
| GNames | `0x0A999B40` | `0x0A99BB40` | rva_map und SDK |
| GWorld | `0x0AAEA770` | `0x0AAEC770` | rva_map und SDK |
| ProcessEvent | `0x0160C220` | `0x0160C220` | rva_map (eindeutig) und SDK, Index `0x4A` unverändert |
| FName::AppendString (nur SDK) | `0x0143DAD0` | `0x0143DAD0` | SDK |

- **Code-Adressen:** 117 Adressen aus `src/`: 102 eindeutig, 3 vtables (`ItemVtable`, `InventoryVtable`, `MapGenLambdaVtable`), 3 Globale,
  1 über den Funktionsanfang (`ScheduleCall`), 8 fehlen (Code geändert). Code bis `0x024E95F0` liegt unverändert, danach wächst die
  Verschiebung von `-0x30` (Engine) auf bis zu `+0xED0` (Spielcode).

  | Datei, Liste | Fehlende Referenz-RVA | Bedeutung | Folgeaufgabe |
  |---|---|---|---|
  | `ClientPatches.cpp`, Client | `0x056676F5` | Blockade-Thunk, reflektierter Einsprung auf `BlockadeLoadingFinished` | von Hand nachgetragen: `0x05667E85`, der einzige `jmp` auf `BlockadeLoadingFinished` (`0x058BC360`) im neuen Build; Spieltest offen, `update-1-0-2-client` |
  | `HealthBarPatches.cpp`, HealthBar | `0x057DC810`, `0x057DCE70`, `0x057DD080` | `OnRep_Health`, `OnRep_MaxHealth`, `OnRep_MaxStamina` | von Hand nachgetragen, Code unverändert, siehe [Lebensleisten](#lebensleisten-auf-26669225-update-1-0-2-lebensleisten) |
  | `InventoryPatches.cpp`, Inventory | `0x093A1F98`, `0x094C6A78` | vtable `BackpackSlot`, vtable Achievement-Detektor | in `freigabe-1-0-2` von Hand nachgetragen, Rest `update-1-0-2-inventar` |
  | `RewardPatches.cpp`, Reward | `0x058CE280` | `ShopPanelItem::TryBuyItem` | von Hand nachgetragen: `0x058CE890`, Ziel des Exec-Thunks `0x0579B580`. Der Anfang ist geändert: Die Funktion lädt die Instanz nach `rax` und kehrt auch ohne deren `ItemData` (`+0xE0`) oder dessen Feld `+0xE8` zurück, danach gleich; eigene Prüfbytes je Build in `CheckTryBuy`, gemeinsame Signatur. Spieltest offen, `update-1-0-2-handel` |
  | `WeaponSoundPatches.cpp`, WeaponSound | `0x05707870` | `Hands.PlayReloadSound` | von Hand nachgetragen: `0x05708000`, 400 Befehle maskiert gleich; Spieltest offen, `update-1-0-2-waffensound` |

- **Stand 16.11.2026 nach `update-1-0-2-client`, `update-1-0-2-waffensound` und `update-1-0-2-handel`:** Alle Code-Adressen sind eingetragen. `tools/expect_check.py` auf `26669225`: 95 `ok`, 1 `anderer-build`, 2 `uebersprungen`, kein `fehlt`; `tools/patch_signatures.py check`: 106 `ok` auf beiden Builds. Die Zahlen im nächsten Punkt sind der Stand davor.
- **Spieltest 16.11.2026 mit diesem Stand** (ein PC, eingefrorene Kopie `26669225`, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -NoSpike`, ohne echte Eingaben, Reise über den Testkanal nach `DDDynamic_Map`): Host und Client melden `Signaturen: 106 Adressen geprüft` und `Teilinstallation auf Build 26669225, 18 von 18 Listen gesetzt, 0 Patches ausgelassen` (vorher 4 ausgelassen). Lobby-Beitritt ohne Absturz, Handshake bestätigt, beide mit 2 Spielern in der Mission, 90 s gelaufen: kein `[ERROR]`, kein Absturzbericht, Desync-Melder auf beiden Seiten `OK Tag, Phase, Spieler, HP, Gegner, Inventar`. **Nicht geprüft:** Blockade, Kauf beim Händler, Verkauf im Lager und Nachladesound selbst; die drei Hooks sind gesetzt, ausgelöst wurde keiner.
- **Offline gegen die Exe von `26669225`:** `tools/expect_check.py` 89 `ok`, 6 `fehlt`, 2 `uebersprungen` (lokale `Site`), keine `abweichend`;
  `tools/patch_signatures.py check` 100 `ok`, 6 `fehlt`. Die beiden vtables prüft nur das Spiel (`ExpectVirtual`), `expect_check.py` kennt sie nicht.
- **SDK-Vergleich:** Beide SDKs am 16.11.2026 per `inject.py` im Host erzeugt, beide Instanzen in `DDDynamic_Map` nach direkter Reise:
  `26495302` 2.503 Dateien (Sitzung mit Spike, Dumper-7 aus dem Submodul), `26669225` 2.502 Dateien (ohne Spike, Dumper-7 mit lokalem Fix,
  siehe unten; es fehlt nur `GE_MetaBaseSpeed`, ein dort nicht geladenes Blueprint). `py tools/sdk_diff.py --old … --new …`:
  **337 unverändert, nichts verschoben, verschwunden oder mit geänderter Signatur**, Exitcode 0. 5 Einträge meldet das Werkzeug als
  `unbekannt`, weil es sie nicht zuordnet; von Hand in beiden SDKs geprüft und gleich: `UCharacterMovementComponent.MaxWalkSpeed` `0x288`,
  `ADDCharacterBase.CommonAttributeSet` `0x700`, `UCharacterSelectionManager.GetSelectedCharacter(int32)`, `SetVisibility` von
  `USceneComponent` und `UWidget` (`Engine_classes.hpp` und `UMG_classes.hpp` sind in beiden SDKs bytegleich).
- **Dateivergleich der SDKs:** `Engine_classes.hpp`, `Engine_structs.hpp`, `CoreUObject_classes.hpp`, `UMG_classes.hpp`,
  `GameplayAbilities_classes.hpp` und `DDSurvivors_structs.hpp` sind bytegleich. In `DDSurvivors_classes.hpp` hat sich genau eine native
  Klasse geändert: `UMemoSubsystem` wächst von `0x158` auf `0x170`, neu `bPendingBossMemoBackfill` `0x130`, `CachedMemoDataTable`
  `0x130` → `0x138`. Die Mod nutzt die Klasse nicht. Daneben unterscheiden sich einige Blueprint-Dateien (unter anderem
  `BP_DD_Character_parameters`, `OptionsMenu`, `BP_ManagerBoss_Stage*`, `WBP_AchievementNotification`); nicht einzeln ausgewertet,
  die von der Mod genutzten Blueprint-Felder deckt `sdk_diff.py` ab.
- **SdkChecks:** `SdkChecks.cpp` baut gegen beide SDKs ohne Fehler (nach der üblichen
  [Nacharbeit](GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) an `Assertions.inl`); `Offsets.h` gilt damit für beide Builds. Grenze wie
  immer: Offsets, die nur als Zahl im Code stehen, und nicht reflektierte Felder erfasst weder der Vergleich noch der Build.
- **Geänderter Quelltext:** `PlayerPatches.cpp` prüfte den Kamera-Folger mit `At(CameraFollowTick + 0x0E)`; die Summe steht in keiner
  Tabelle, darum fiel auf `26669225` die ganze Liste Player aus und der Host stürzte beim Beitritt des Clients an `0x5905A88` ab
  (`SpawnNullSite`, Referenz `0x5904DE8`, derselbe Absturz wie in `update-basis` ohne Player-Liste). Jetzt `At(CameraFollowTick, 0x0E)`;
  die Bytes sind in beiden Builds gleich. Die Teilinstallation loggt zusätzlich jede gesetzte Liste (`Patches: Liste <Name> gesetzt`).
- **Spieltest `26669225`** (ein PC, eingefrorene Kopie, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -NoSpike -PartialPatches`, ohne echte
  Eingaben): Host und Client melden `Spiel-Build 26669225`, `Signaturen: 6 von 106 Adressen gesperrt`, `Teilinstallation …, 17 von 18 Listen
  gesetzt, 4 Patches ausgelassen`. Ausgelassen ist die Liste Inventory; Client, HealthBar, Reward und WeaponSound sind gesetzt, aber je
  ohne den Patch mit der fehlenden Adresse und ohne Tick. Crash-Reporter gesperrt, Handshake „Build, Mod-Version und Gegenrolle bestaetigt“,
  Lobby-Beitritt ohne Absturz, Reise über den Testkanal (`travel /Game/Maps/DDDynamic_Map`) mit beiden Spielern in der Mission.
- **Ohne `-PartialPatches`** (Stand `update-basis-1-0-2`, überholt durch die [Freigabe](#freigabe-von-build-26669225-für-spieler-freigabe-1-0-2)): Die Mod setzte auf `26669225` keinen Patch (`Patches: keiner gesetzt`). Vor dieser
  Aufgabe blieb die Mod dort wegen des unbekannten Fingerabdrucks ganz aus; mit dem Fingerabdruck liefe sie ohne Patches und böte die Lobby
  an, ein Beitritt ließe den Host abstürzen. Darum sperrt `Game::Patches::CoopBlocked` auf jedem Nicht-Referenz-Build ohne gesetzte Patches
  das Erstellen und Beitreten (Overlay, Einladung, Testkanal), Log `Patches: Spiel-Build 26669225 ist noch nicht portiert, Lobby erstellen
  und beitreten gesperrt`, Overlay-Text „Diese Spielversion wird noch nicht unterstützt, Coop gesperrt“. Der Referenz-Build ist davon
  ausgenommen. Im Spiel geprüft: Start ohne Schalter, beide Instanzen bleiben im Hauptmenü, „Lobby erstellen“ über den Testkanal wird abgelehnt, kein Absturz. **Trotzdem nicht veröffentlichen, bevor die Portierungen fertig sind:** Installer und `SupportedBuilds` nehmen
  `26669225` schon an.
- **Client-Abstürze mit Teilinstallation** (zwei Beobachtungen, Ursache nicht belegt): Im ersten Lauf verlor der Client den Host (dessen
  Absturz kam vom Dumper, siehe unten) und stürzte eine Sekunde später an `0x597A8FA` ab: Lesen von `0x340`, also `this+0x318+0x28` mit
  leerem `this`, Aufrufer `0x5976AE9`, `0x5960F42`. Die Bytes um die Stelle sind in `26495302` bei `0x5979B4A` gleich (Verschiebung
  `+0xDB0`); die Stelle ist in dieser Datei bisher nicht beschrieben. Im zweiten Lauf endete der Client-Prozess direkt nach „Rundenende: an
  den Client gesendet“ ohne Absturzbericht. **[Vermutung]** Beides folgt aus der fehlenden Inventory-Liste (Inventar-Unterobjekte
  replizieren nicht), wie die Client-Abstürze auf `26495302` vor `update-inventar`; zu prüfen in `update-1-0-2-inventar`. Ein Coop-Tag
  auf `26669225` ist erst nach dieser Portierung sinnvoll.
- **Dumper-7 auf `26669225`:** Der Stand des Submoduls (`2570b5b`) stürzt beim Dump im Host ab. Belegt: Stack in der Suche nach
  `ByteProperty` für GNames (`FindFNameGetNamesOrGNames_Windows` → `FindByStringInAllSections`, mit der PDB des Dumpers aufgelöst),
  Lesezugriff auf eine Adresse mit Seitenanfang `…A000`; zieht man SizeOfImage `0x0B74A000` ab, ergibt sich eine auf 64 KB ausgerichtete
  Basis. **[Vermutung]** Der Zeichenkettenvergleich in `FindStringInRange` liest über das Image-Ende, wenn ein als `lea` gelesener Kandidat
  in dessen letzte Bytes zeigt. Dazu passt: Mit einer lokalen Kopie, die vor dem Vergleich prüft, ob auch das Ende der Zeichenkette lesbar
  ist, lief der Dump durch (einmal versucht). Auf `26495302` tritt der Absturz nicht auf. Der Fix ist nicht im Repo und nicht im Fork
  (`dumper7-image-ende`).
- **Spike (Stand `update-basis-1-0-2`, überholt):** kannte nur `26495302` und setzte auf `26669225` keine Patches, Sitzungen dort liefen nur mit `-NoSpike`.
- **Spike auf `26669225` (`update-1-0-2-spike`, 17.11.2026):** `Builds.cpp` erkennt den Build am TimeDateStamp `0xE9CA3AB7` und rechnet
  über `RvaMap26669225.inc` um. Von 80 Adressen im Spike-Quelltext stehen 76 in der Tabelle der Mod, die übrigen vier sind mit
  `rva_map.py` umgerechnet: `SerializeNewActor` `0x02D09D90` → `0x02D09D60`, `NewActorArchetypeCheck` `0x02D0A626` → `0x02D0A5F6`,
  `UpdateTag` `0x04B7D9D0` → `0x04B7E190` (alle eindeutig), Tag `State.Invincible` `0x0AC680D8` → `0x0AC6A0D8` (global).
  **Spieltest** (ein PC, eingefrorene Kopie `26669225`, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -Godmode -Map
  /Game/Maps/DDDynamic_Map`, ohne echte Eingaben): Spike geladen, beide Instanzen mit 2 Spielern in der Mission, Mod `18 von 18 Listen
  gesetzt, 0 Patches ausgelassen`. `!give ItemData_Pistol` legt das Item in beide Inventare, `Get-Status.ps1 -Inventory` meldet
  vollständig und gleich, `Test-Godmode.ps1` bestätigt Godmode für beide Spieler samt Client-Replikaten, `!finish` beendet die Runde:
  Client öffnet Rundenübersicht und Lager, Host meldet „Client im Lager“, beide Prozesse leben, keine Absturzberichte. Gegenprobe auf
  `26495302` mit demselben Stand: Start, `!give`, Inventarvergleich und Godmode wie zuvor. **Nicht geprüft:** `Run-Route.ps1 -Days 2`
  (braucht echte Maus- und Tastatureingaben), der Spike ohne Mod auf `26669225` (dann setzt er seine eigenen Patches) und die übrigen
  Spike-Befehle.
  `tools/spike/ddmem.py` kennt die Globalen von `26669225`.
- **Regression `26495302`:** `Start-Session.ps1 -Build -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map`: Host und Client
  `Signaturen: 106 Adressen geprüft`, `Patches: 54 gesetzt`, `Spiel-Build 26495302`, beide mit 2 Spielern in `DDDynamic_Map`, kein `[ERROR]`,
  kein Absturz, auch nach dem SDK-Dump im Host. Dreimal gelaufen, zuletzt mit dem Endstand dieses Branches.

### Inventar-Patches gegen die Patchnotes 1.0.2 und 1.0.3 (`inventar-patchnotes-1-0-2`)

Stand 21.11.2026. Ergebnis: **Alle sieben Patches der Liste Inventory bleiben nötig, keiner wird durch Patch 1.0.2 oder 1.0.3 überflüssig,
keiner wurde entfernt.** Quelle der Patchnotes: Steam-Ankündigungen „Patch 1.0.2“ (14.11.2026) und „Patch 1.0.3“ (19.11.2026).

**Verfahren (offline, kein Spieltest):** Die Exes der eingefrorenen Kopien `26495302` (1.0.1), `26669225` (1.0.2) und `26786272` (1.0.3)
verglichen. Je gepatchter Stelle die ganze Funktion ab ihrem Anfang bis zum Füllbyte disassembliert (Capstone), Sprung-/Aufrufziele und
RIP-relative Verschiebungen maskiert, Befehlsfolge verglichen; dazu die gepatchten vtable-Einträge aus `.rdata` gelesen. Adressen der alten
Builds aus den Tabellen `RvaMap26669225.inc` und `RvaMap26786272.inc` (Verlauf, siehe [Referenz-Build](../OFFSETS.md#referenz-build-26786272-spiel-update-26786272)).
**Grenze:** „gleich“ heißt, die gepatchte Funktion selbst ist unverändert. Aufrufer und aufgerufene Funktionen sind damit nicht verglichen
(Ausnahme: der Schritt des Kartengenerators). Ob der Auslöser eines Absturzes an anderer Stelle weggefallen ist, belegt das nicht.

| Patch (Name in `List`) | Stelle, RVA `26495302` / `26669225` / `26786272` | Vergleich | Ergebnis |
|---|---|---|---|
| Itemsack-Vergabe nur beim Host, für alle Spieler | `AItemLoot::Loot` `0x59AF200` / `0x59AFFC0` / `0x59B1F50` | 135 Befehle, in allen drei gleich | weiter nötig: Coop-Regel der Mod (wer vergibt), kein Spielfehler; das Spiel prüft die Rolle weiter nicht |
| Schrott beim Aufsammeln nur beim Host buchen | `AScrapLoot::Loot` `0x59AF660` / `0x59B0420` / `0x59B23B0` | 100 Befehle, gleich | weiter nötig: Coop-Regel, weiter keine Rollenprüfung vor `AddAmount` |
| Aufsammelklang beim Client je Objekt einmal | `ALootBase::Loot` `0x59AF3F0` / `0x59B01B0` / `0x59B2140` | 35 Befehle, gleich | weiter nötig: Coop-Folge (Objekt bleibt beim Client bis zum Zerstören des Hosts) |
| Item-Widget ohne Item: Größe nicht setzen | Größe des Item-Widgets `0x5ABA0F0` / `0x5ABAFC0` / `0x5ABD6C0` | 43 Befehle; 1.0.2 gleich, 1.0.3 nur Feld-Offset `+0xA28` → `+0xA38` | weiter nötig: `ItemInstance`, `ItemData` (`+0xE0`) und Form (`+0xE8`) werden weiter ohne Prüfung gelesen |
| Inventar-Unterobjekte replizieren | vtable-Einträge `+0x200` von `BackpackSlot` und Item, `+0x3D8` des Inventars | zeigen in allen drei Builds auf `DefaultNetworkingSupport` `0x1512430` (2 Befehle, gleich) bzw. `ReadyForReplication` des Inventars (`0x596D8C0` / `0x596E670` / `0x59702D0`, Sprung auf dieselbe Engine-Funktion) | weiter nötig: Das Spiel überschreibt `IsSupportedForNetworking` für beide Klassen weiter nicht |
| Timer des Achievement-Detektors beim EndPlay löschen | `EndPlay` des Detektors `0x57D0C60` / `0x57D1470` / `0x57D1DA0` (vtable `+0x3F0`), Timer-Anlage in der Funktion um `ScheduleCall` (`0x57CB400` / `0x57CBB80` / `0x57CC4C0`) | 15 bzw. 139 Befehle, gleich; `SetTimerForNextTick`, `FindTimer`, `ClearTimer` gleich | weiter nötig: `EndPlay` löscht den Timer weiter nicht. Die Achievement-Fixes aus 1.0.2 (Memos, Bombe) haben diese beiden Funktionen nicht geändert |
| Kartengenerator-Schritt nur für die aktuelle Welt | Lambda `Execute` `0x59CFBA0` / `0x59D0960` / `0x59D2920`, `ExecuteIfSafe` `0x59CFD50` / `0x59D0B10` / `0x59D2AD0`, Schritt `0x59DDE60` / `0x59DEC20` / `0x59E0BE0` | 3, 7 und 237 Befehle, gleich | weiter nötig: Das Lambda ruft den Schritt weiter mit rohem Generator ohne Prüfung |

**Die drei Punkte der Patchnotes 1.0.2:**

- *Absturz nach dem Schließen des Inventars, wenn eine Waffe aufgewertet oder verkauft wurde:* kein Gegenstück in `InventoryPatches.cpp`.
  Kein Patch der Liste greift beim Schließen, Aufwerten oder Verkaufen ein. Welche Funktion das Spiel dafür geändert hat, ist nicht
  bestimmt. Verkauf und Bestätigen gehören zu `RewardPatches.cpp` (dort `TryBuyItem` mit geändertem Anfang, siehe Tabelle oben,
  `update-1-0-2-handel`) und sind nicht Teil dieser Aufgabe.
- *Seltene Abstürze bei Belohnungskisten, Händlern, Rezepten und Item-Vorschau:* Der einzige thematisch nahe Patch ist „Item-Widget ohne
  Item“. Seine Funktion ist in 1.0.2 unverändert, der Fix des Spiels liegt also nicht dort. **[Vermutung]** Der Fix betrifft einen anderen
  Weg (Vorschau/Tooltip); der Fall der Mod (Widget verliert sein Item beim Verbindungsabbau im Lager) ist ein Coop-Fall, den das Spiel
  allein nicht hat.
- *Absturz beim Schließen einer Twitch-Kisten-Abstimmung ohne Serververbindung:* kein Gegenstück in der Mod.

**Patchnotes 1.0.3, soweit Inventar:** deaktivierte Items mit rotem Abzeichen, Rezeptliste im Pausen-Inventar und bei Kisten, weniger
Ruckeln und Speicherzuwachs im Inventar, Absturz bei vielen Kisten hintereinander mit großem Rucksack, Hammer-Aufwertung und
Werkbank-Zutaten. Kein Gegenstück in `InventoryPatches.cpp`; die einzige Folge an einer Patch-Stelle ist der verschobene Feld-Offset
`UItemRepresentation.ItemInstance` (`0xA38`, in `update-basis-26786272` nachgezogen).

**Laufzeit:** `tools/expect_check.py --file InventoryPatches.cpp` auf `26786272`: 10 `ok`, keine Abweichung (die vtables prüft nur das
Spiel). In den Mod-Logs der Live-Tests vom 17.11. (`26669225`) und 21.11.2026 (`26786272`) steht je Instanz „klassische Replikation der
Unterobjekte bereit“, beim Client die Zeilen der Itemsack-, Schrott- und Klang-Regel; die Schutzzeilen „Item-Widget ohne …“ und
„Kartengenerator: Schritt für eine alte Welt ausgelassen“ kommen in beiden nicht vor. Diese beiden Schutzpfade wurden dort also nicht
ausgelöst; dass sie nötig bleiben, stützt sich auf den unveränderten Code, nicht auf einen neuen Absturz ohne Patch.

**Nicht geprüft:** Ein Lauf ohne einzelne Patches (Gegenprobe im Spiel) und die Funktionen, die das Spiel für die drei Punkte tatsächlich
geändert hat.

## Freigabe von Build 26669225 für Spieler (`freigabe-1-0-2`)

Nutzerentscheidung vom 16.11.2026: Das Update 1.0.2 ist für das Projekt nicht ausschlaggebend, `26669225` wird für Spieler geöffnet,
ohne die restlichen Portierungen abzuwarten. Die Entwicklungskopien wechseln **nicht**: `26495302` bleibt Referenz, `PinnedGameDir`
unverändert (`wechsel-1-0-2` bleibt offen).

- **Teilinstallation von selbst:** `Game::Patches::PartialInstallAllowed` gilt auf einem weiteren Build mit eigener Tabelle in
  `AddressMap` jetzt ohne Schalter. Auf `26669225` setzt die Mod jede Liste für sich und lässt einzelne Patches mit fehlender Adresse
  aus. `DDCOOP_PARTIAL_PATCHES` bleibt für Builds ohne Tabelle. Der Referenz-Build ist unverändert (alle Patches oder keiner).
- **Sperre neu gefasst:** `CoopBlocked` sperrt Lobby erstellen und beitreten auf einem weiteren Build, wenn kein Patch steht, das Setzen
  abbrach oder eine der Kernlisten Player und Client fehlt. Belegt ist der Host-Absturz ohne die Player-Liste; die Client-Liste ist eine
  vorsichtige Wahl ohne eigenen Beleg.
- **Inventar-vtables von Hand:** `SlotVtable` `0x093A1F98` → `0x093A2EF8` und `DetectorVtable` `0x094C6A78` → `0x094C7B88`. Verfahren:
  alle Einträge der alten vtable als Funktionen umrechnen (`rva_map.map_code`) und in `.rdata` der neuen Exe die Stelle suchen, an der die
  meisten passen. Slot: 82 von 84 Einträgen umgerechnet, alle 82 gleich, `+0x200` → `0x1512430`. Detektor: 162 von 164 umgerechnet, alle
  162 gleich, `+0x3F0` → `DetectorEndPlay` `0x57D1470`. Die nächstbesten Kandidaten haben 81 bzw. 159 Treffer (andere Klassen mit
  geerbten Einträgen). `rva_map.py` scheiterte, weil es nur die ersten sechs Einträge nimmt und dort einer nicht umrechenbar ist.
- **Stand auf `26669225`:** `Teilinstallation auf Build 26669225, 18 von 18 Listen gesetzt, 4 Patches ausgelassen`. Ausgelassen:

  | Ausgelassener Patch | Folge für Spieler **[Vermutung aus der Aufgabe des Patches, auf `26669225` nicht im Spiel geprüft]** | Folgeaufgabe |
  |---|---|---|
  | Blockade nur beim Host | Blockade-Ladeende läuft beim Client nativ, Verhalten an Blockaden offen | `update-1-0-2-client` |
  | Lebens- und Ausdauerleisten beim Client | Leisten des Clients aktualisieren sich nicht wie auf `26495302` | erledigt in `update-1-0-2-lebensleisten`, der Patch ist gesetzt |
  | Kaufen beim Händler nur in lokaler Bearbeitung | Kauf des Clients beim Händler ohne Host-Buchung, Abweichung möglich | `update-1-0-2-handel` |
  | Nachladesound des Hosts für den Client | Client hört das Nachladen des Hosts nicht | `update-1-0-2-waffensound` |

- **Spieltest 16.11.2026** (ein PC, eingefrorene Kopie `26669225`, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -NoSpike`, **ohne**
  `-PartialPatches`, ohne echte Eingaben, Reise über den Testkanal):
  - Vor den vtables: 17 von 18 Listen, Lobby-Beitritt und Mission erreicht, zehn Minuten in `DDDynamic_Map` ohne Absturz, beide
    Prozesse leben, Desync-Melder dauerhaft `offen Inventar`.
  - Mit den vtables: 18 von 18 Listen, Lobby-Beitritt und Mission erreicht, Desync-Melder `OK Tag, Phase, Spieler, HP, Gegner,
    Inventar` bei Host und Client (zwischendurch `offen HP`, das sich wieder schließt), kein Absturz in rund 90 Sekunden.
  - `tools/expect_check.py --build 26669225`: 89 `ok`, 6 `fehlt`, 2 `uebersprungen` (die vtables prüft das Werkzeug nicht).
  - Sitzungsende mit 18 von 18 Listen, beide in `DDDynamic_Map`: `Test-NetEnd.ps1` besteht alle Fälle (Host verlässt, Client verlässt,
    Client beendet, Host beendet, Neubeitritt per Lobby-ID), `Abstuerze: keine neuen`. Der Client-Absturz nach Hostverlust an
    `0x597A8FA` aus `update-basis-1-0-2` trat damit nicht mehr auf (ein Lauf).
- **Nicht geprüft:** Rundenende, Lager, Routenkarte, zweiter Tag, Händler und Kampf auf `26669225`. Ohne Spike gibt es dort kein
  `!finish`, ein Rundenende braucht echte Eingaben oder zwei Menschen; zehn Minuten Leerlauf in der Mission lösten keines aus. Das
  Ende des Client-Prozesses nach dem Rundenende aus `update-basis-1-0-2` (Lauf ohne Inventory-Liste) ist darum nicht nachgeprüft.
- **Zustand `Active`:** Mit 18 von 18 Listen meldet `DDCoopPatchState` `Active`, obwohl 4 einzelne Patches fehlen; das ist dieselbe
  Regel wie bei per Signatur ausgelassenen Patches im Referenz-Build. Die Lücken stehen nur im Log.
- **Regression `26495302`:** `Start-Session.ps1 -Build -Mod -Tunnel Lobby`, beide `Patches: 54 gesetzt`, Mission erreicht, Desync OK,
  kein Fehler im Mod-Log.
- **Widerspruch zu `update-startpruefung`:** Die Aufgabe verlangt, dass eine Teilinstallation nicht als erfolgreicher Start gilt. Auf
  `26669225` ist sie jetzt der Normalfall; das ist dort beim Umsetzen zu entscheiden.

## Lebensleisten auf 26669225 (`update-1-0-2-lebensleisten`)

Stand 16.11.2026. `HealthBarPatches.cpp` ist unverändert, nur `RvaMap26669225.inc` bekam drei Zeilen `// von Hand`.

| Funktion | 26495302 | 26669225 |
|---|---|---|
| `OnRep_Health` | `0x057DC810` | `0x057DD020` |
| `OnRep_MaxHealth` | `0x057DCE70` | `0x057DD680` |
| `OnRep_MaxStamina` | `0x057DD080` | `0x057DD890` |

- **Der Code hat sich nicht geändert.** Die Angabe „Code geändert" bzw. „Prolog geändert" für diese drei Adressen stimmte nicht: Alle
  OnRep-Funktionen der Attribut-Sets sind gleich gebaut, `rva_map.py` sucht mit höchstens 32 Befehlen, und so weit sind `OnRep_MaxHealth`
  und `OnRep_MaxStamina` (beide `lea r8,[rdi+0x30]`) nach dem Maskieren gleich, `OnRep_Health` hat einen Zwilling bei `0x5885BD0`
  (neu `0x58861A0`). Das gilt schon im Referenz-Build. `OnRep_Stamina` (`+0x50`) ist als einzige eindeutig.
- **Beleg der Zuordnung:** Mit 60 Befehlen (bis in den Kaltpfad mit dem Attributnamen) ist jedes Muster in beiden Exes genau einmal
  vorhanden und trifft die Zeile der Tabelle. Der Kaltpfad nennt im neuen Build dieselben Namen und Längen (`Health` 6, `MaxHealth` 9,
  `MaxStamina` 10), der Rumpf liest dieselben Felder (`+0xA0`, `+0x30`, `+0x30`) und `ASC+0x8D8`. Die Verschiebung ist `+0x810` wie bei
  `OnRep_Stamina` und `DetectorEndPlay`, die statischen Attribut-Zeiger liegen `+0x2000` wie `GObjects`.
- **Broadcast weiter nötig:** Die Funktionen sind Befehl für Befehl gleich geblieben (nur `SetBaseAttributeValueFromReplication`, kein
  eigener Broadcast). Ein Vergleichslauf ohne die drei Adressen auf `26669225` belegt dazu nichts: Der Boss traf zwar (HUD des
  Clients 80), in den Bildern ist aber nur eine Figur in der Arena und nicht zuzuordnen, wessen Leiste zu sehen ist.
- **Offline:** `expect_check.py --build 26669225` 92 `ok`, 3 `fehlt`, 2 `uebersprungen`; `patch_signatures.py check` 103 `ok`, 3 `fehlt`
  (Blockade, Nachladesound, Händlerkauf), Referenz-Build 106 `ok`. Die vorhandenen Signaturen passen ohne Änderung.
- **Spieltest `26669225`** (ein PC, eingefrorene Kopie, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -NoSpike`, Reise über den Testkanal
  nach `DDDynamic_Map`, ohne echte Eingaben, drei Läufe): Host und Client `Signaturen: 3 von 106 Adressen gesperrt`, `Liste HealthBar
  gesetzt` ohne „ausgelassen", `18 von 18 Listen gesetzt, 3 Patches ausgelassen`, beim Client `Lebensleisten: erster Broadcast beim
  Client, 1 Empfänger`. Fensterbilder des Clients ab Sekunde 0 der Mission, im längsten Lauf rund 2,5 Minuten: HUD-Leiste 100 und gefüllt, Lebens- und
  Ausdauerleiste über beiden Figuren gefüllt, wie beim Host; der Boss erschien in diesen Läufen nicht. Kein Absturz.
- **Nicht geprüft:** Änderung der Leisten nach Schaden mit dem Patch auf `26669225` (in den drei Läufen mit Patch griff der Boss auch in rund 2,5 Minuten
  nicht an, ohne Spike gibt es keinen Schadensbefehl) und der echte Weg in die erste Karte über die Routenkarte, er braucht Eingaben.
  Der Live-Befund vom 16.11.2026 („Lebens- und Rüstungsleiste des Clients in der ersten Karte lange leer und schwarz") ließ sich mit
  der direkten Reise auch ohne den Patch nicht nachstellen; dort waren die Leisten von Beginn an gefüllt. **[Vermutung]** Er folgt aus
  dem fehlenden Patch wie in [hp-leisten](GAME_ANALYSIS.md#lebensleisten-beim-client-hp-leisten) (Leiste entsteht vor der Replikation
  und bleibt ohne Broadcast auf 0 %). Für die Rüstung gab es damals keinen Hook, nachgetragen in [Rüstung](../OFFSETS.md#rüstung-opt-astronaut-ruestung).
- **Regression `26495302`:** `Start-Session.ps1 -Build -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map`: beide `Signaturen: 106 Adressen
  geprüft`, `Patches: 54 gesetzt`, Broadcast beim Client, Leisten gefüllt, kein Absturz.

## Rundenende auf 26669225 (`rundenende-1-0-2`)

**Stand 16.11.2026:** Ursache behoben und auf einem PC im Spiel bestätigt (beide Reihenfolgen, Reise bis Tag 3). Offen bleibt die
Gegenprobe auf zwei PCs mit Airdrop und einem Lager-Edit mit Aufwertung und Verkauf, wie im Live-Test.

- **Befund** (Live-Test 16.11.2026, zwei PCs, Mod-Stand nach `freigabe-1-0-2`): Der Host musste jede Airdrop-Auswahl doppelt bestätigen.
  Am ersten Rundenende blieb sein Lager offen, Bestätigen ohne Wirkung. Der Client hatte bestätigt, sein Lager-Edit war übernommen,
  er loggte `Lager: Bestätigen nachgeholt (alle bereit)` und stand allein auf der Routenkarte. Der Run ging nicht weiter.
- **Ursache:** Auf `26669225` fehlt `ShopPanelItem::TryBuyItem` (`0x058CE280`). Die Teilinstallation ließ darum nicht nur den
  Händler-Patch aus, sondern auch den Tick der ganzen Liste Reward (`Liste Reward gesetzt, ohne Tick (Adresse fehlt)`). `ConfirmHook`
  stand aber und hielt das Bestätigen des Hosts zurück; nachgeholt wird es nur in `Rewards::Tick`. Beim Airdrop lief erst der zweite
  Klick nativ, weil die Wahl des Clients dann schon da war. Das ist belegt: Der fehlende Tick steht im Log,
  der Doppelklick folgt aus dem Code. Im Lager gibt es diesen Ausweg nicht: Nach dem ersten Klick ändern weitere nichts.
  **[Vermutung]** Dass das Lager des Hosts genau daran hing, ist nicht belegt: Dieser Weg lässt den Knopf gelb, gemeldet wurde
  „nicht gelb“. Ohne Host-Log und ohne Spieltest ist eine zweite Ursache im Lager nicht ausgeschlossen. Der Client hängt nicht am Tick, sein Bestätigen lösen Nachrichten des Hosts aus.
- **`HandleConfirmButton` ist in 1.0.2 unverändert:** `0x5A71900` und `0x5A72720` sind Befehl für Befehl gleich, ebenso die Aufrufe bis
  in die dritte Ebene (314 Funktionspaare verglichen, Adressen ausgeblendet). Die einzige inhaltliche Abweichung liegt in
  `0x5AA26A0` → `0x5AA34C0` (**[Vermutung aus dem Code]** Aufbau eines Item-Widgets, neuer früher Ausstieg ohne Itemdaten; nicht im Spiel geprüft).
- **Fix:** `PatchList::TickOptional` nennt Stellen, ohne die der Tick einer Liste auskommt. Für Reward ist das `TryBuyItem`: `Rewards::Tick`
  benutzt die Adresse nicht, nur Prüfung, Setzen und Hook des Händler-Patches. Der Händler-Patch bleibt ausgelassen
  (`update-1-0-2-handel`), der Tick läuft. Erwartete Logzeile auf `26669225`: `Patches: Liste Reward gesetzt` ohne den Zusatz.
- **Belegt:** Client-Log des Live-Tests, Code, Unit-Test `Patches: ein Patch mit gesperrter Stelle fällt allein aus, ohne Check und Apply`.
  Das Host-Log des Live-Tests lag nicht vor. Darin wären zu erwarten: `Liste Reward gesetzt, ohne Tick (Adresse fehlt)` und
  `Lager: Host bereit, wartet auf den Client` ohne folgendes `Lager: Bestätigen nachgeholt`.
- **Spieltest 16.11.2026** (ein PC, eingefrorene Kopie `26669225`, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -NoSpike`, ohne echte
  Eingaben, Reise über den Testkanal): Host und Client melden `Patches: Liste Reward gesetzt` ohne Zusatz, weiter 18 von 18 Listen und
  4 ausgelassene Patches. Der Tick läuft beim Host (`Reward-Queue Host: neue Welt, belegt false, aktuelle Kiste keine`, dasselbe für den
  Remote-Spieler). Rund 75 Sekunden in `DDDynamic_Map`, beide Prozesse leben, Desync-Melder bei beiden `OK Tag, Phase, Spieler, HP,
  Gegner, Inventar`, kein neuer Fehler im Mod-Log.
- **Spieltest Rundenende 16.11.2026** (ein PC, eingefrorene Kopie `26669225`, `Run-Route.ps1 -Start -NoSpike -Days 1 -StayLast`, echte
  Klicks über `Game-Input.ps1` mit Zustimmung des Nutzers, Rundenende über „Full Tank“ des Dev-Tabs):
  - **Host zuerst, Tag 1:** ein Klick beim Host, Knopf gelb, `Lager: Host bereit, wartet auf den Client`. 15 s später bestätigt der
    Client: `Lager: Client bereit`, in derselben Millisekunde `Lager: Bestätigen nachgeholt (Client bereit)` beim Host, 0,2 s danach
    `Lager: Bestätigen nachgeholt (alle bereit)` beim Client. Host auf der Routenkarte für Tag 2, kein zweiter Klick.
  - **Reise zu Tag 2:** Knoten angeklickt, beide in `DDDynamic_Map` (`MGP_Forest_Roadstops`).
  - **Client zuerst, Tag 2:** `Lager: Client bereit, wartet auf den Host`, 6 s später ein Klick beim Host: `Lager: Host bestätigt,
    alle bereit`, 0,2 s danach `Lager: Bestätigen nachgeholt (alle bereit)` beim Client. Host auf der Routenkarte für Tag 3.
  - **Reise zu Tag 3:** beide in `DDDynamic_Map`, Desync-Melder bei beiden `OK Tag, Phase, Spieler, HP, Gegner, Inventar`. Kein
    Absturz, kein neuer Fehler im Mod-Log.
  - **Host wartet, Client bearbeitet sein Lager (zweiter Lauf, Tag 1):** Host bestätigt zuerst (`Lager: Host bereit, wartet auf den
    Client`), der Client verschiebt danach die Pistole im Rucksack (lokale Bearbeitung) und bestätigt: `Lager: Client bestätigt nach
    lokaler Bearbeitung, Edit …`, beim Host `Edit … übernommen, 1 Items, … vollständig=true`, 0,5 s später `Lager: Bestätigen
    nachgeholt (Client bereit)`, Host auf der Routenkarte für Tag 2.
- **Nicht geprüft:** Airdrop-Auswahl im Spiel (Host zuerst, erwartet `Bestätigen des Hosts nachgeholt (Versuch 1)`), ein Lager-Edit mit
  Aufwertung, Verkauf und Reparatur wie im Live-Test (dort 8 Items, 2 Aufwertungen, 3 Verkäufe; hier ein verschobenes Item), zwei PCs.
  Der gemeldete nicht gelbe Knopf ist damit nicht erklärt; mit laufendem Tick trat kein Hänger auf. `TickStation` lief ohne Stations-Bearbeitung.
- **Rundenende ohne Spike und ohne Eingabe:** Stehen beide Figuren am Bus (Ankunft), beendet ein voller Tank die Runde sofort. „Full
  Tank“ des Dev-Tabs lässt sich als Fensternachricht an das Overlay schicken (`[DDCoopOverlay]::Click(<Overlay>, 111)` aus
  `Common.ps1`, Godmode `112`). Eine Direktreise mit `Send-GameTravel` in `DDDynamic_Map` landet dagegen in `MGP_HQ8_Boss` ohne Bus.
- **Derselbe Mechanismus, nicht Teil dieser Aufgabe:** Auch die Ticks der Listen HealthBar (Busbalken) und WeaponSound blieben auf
  `26669225` aus, solange ihre Adressen fehlten (nachgetragen in `update-1-0-2-lebensleisten` und `update-1-0-2-waffensound`). Ein hängender Ablauf ist
  dort nicht belegt.

## Build 26495302 (`update-basis`)

Stand 07.11.2026, historisch. Bis zum Umstieg am 09.11.2026 blieben die Werte Referenz-RVAs von `26187885`. Für `26495302` rechnet `Game::AddressMap` sie über
`src/Game/RvaMap26495302.inc` um, erzeugt mit [`tools/rva_map.py`](../../tools/README.md#rva_mappy). Exe: TimeDateStamp `0x65BFBBFB`, SizeOfImage `0x0B748000`.

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
  Beitritt unverändert an `0x5904DE8`; behoben mit `update-client`, das die Player-Liste portiert hat.

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
- Rundenende Tag 1 (Tank per `tools/spike/fuel.py` auf 118/120, letzter Kanister und Busflucht vom Menschen): Der Host beendet die Runde
  und benachrichtigt den Client („Rundenende: an den Client gesendet“). Direkt danach stürzt der Client bei `0x59677FD` ab (`0xC0000005`).
  Das ist `+0x7D` in der Inventarschleife `0x5967780` und entspricht dem bekannten Referenz-Absturz `0x596362D`
  (`0x59635B0` + `0x7D`, siehe [Spike-Inventar](../../research/spikes/a-connect/README.md#inventar-ursache-teilfix-und-verbleibende-grenze)):
  Beim Client fehlen Inventar-Items, weil die Inventory-Liste (`Inventar-Unterobjekte replizieren`) auf `26495302` noch nicht gesetzt ist.
  Kein neuer Fehler der vier Dateien; Wiederholung des Coop-Tags nach `update-inventar`.

### Inventar, Belohnung, Lebensleisten (`update-inventar`)

Von Hand nachgetragen in `RvaMap26495302.inc` (Zeilen „von Hand“). Native Funktionen über ihre Exec-Thunks (Namenspaar
`{char*, FNativeFuncPtr}` in `.rdata`, Thunk ruft bzw. springt in die Funktion), vtables über den Verweis im Konstruktor.

| Name | 26187885 | 26495302 | Weg | Änderung im Code |
|---|---|---|---|---|
| `OnRep_Health` | `0x57DC840` | `0x57DC810` | Exec-Thunk `0x5696EE0` | keine, nur verschoben |
| `OnRep_MaxHealth` | `0x57DCEA0` | `0x57DCE70` | Exec-Thunk `0x5697040` | keine |
| `InventoryPanel.HandleConfirmButton` | `0x5A6CB20` | `0x5A71900` | Exec-Thunk `0x5712FB0` | neuer Prolog, Panel-Felder −8 |
| `InventoryPanel.SetupInventory` | `0x5A8BAB0` | `0x5A90940` | Exec-Thunk `0x571BC20` | keine; auch `ClientRefreshFunction` aus `update-client` |
| Größe des Item-Widgets | `0x5AB5120` | `0x5ABA0F0` | alle 8 übertragbaren Aufrufer | umgeordnet, liest `+0xA28` weiter ungeprüft (`+0x41`) |
| vtable `BackpackSlot` | `0x939CE08` | `0x93A1F98` | Konstruktor `0x5658829` | 84 Einträge wie vorher, `+0x200` → `0x1512430` |
| vtable Achievement-Detektor | `0x94C1758` | `0x94C6A78` | `0x57C3A50` | 164 Einträge, `+0x3F0` → `DetectorEndPlay` `0x57D0C60` |
| `InventoryComponent::HandleItemSold` (`lager-handel-client`) | `0x5967900` | `0x596BA70` | Exec-Thunk `0x57128B0` bzw. `0x57133D0` | keine, Prolog gleich |

- `ScheduleCall` (`0x57CB5A5`, über den Funktionsanfang) ist derselbe Befehl, nur der rel32 hat sich verschoben. Die Prüfung vergleicht
  jetzt das Aufrufziel (`SetNextTick`) statt der rel32-Bytes und gilt damit für beide Builds.
- `UInventoryPanel`: Felder ab `0x398` liegen 8 Byte weiter vorn, `IsDragging` `0x790` → `0x788` (SDK und Befehl `cmp byte ptr
  [rdi+0x788],0` in `HandleConfirmButton +0x4A`). `RewardPatches.cpp` liest den Offset je Build und prüft genau diesen Befehl.
  `InventoryComponent` `+0x370` ist unverändert.
- Unverändert laut beiden SDKs: alle übrigen lokalen Offsets der sechs Dateien (`InventoryComponent`, `InventoryItemInstance`,
  `InventoryItemData`, `URarity.NextUpgradableRarity`/`RarityName`, `BackpackSlot`, `CommonAttributeSet`, `OccupiedHands` `0x110`,
  `HandBarDisplay.CachedCharacter`, `DefaultFocusableButton.ParentPanel`, `BP_DD_Character_C.RewardQueue`, `ItemRepresentation +0xA28`,
  `AController`, `UGameInstance`, `UPlayer`) und die Parameter-Structs aller 77 per Name gerufenen UFunctions.
- Statische Gegenprobe aller `Expect`/`ExpectVirtual` der sechs Dateien gegen die Exe: `26187885` ohne Abweichung; auf `26495302`
  vor der Portierung 3 Abweichungen (Detektor-Timer, Item-Widget, `HandleConfirmButton`), die die neuen Prüfungen je Build abdecken.
- Patchnotes 1.0 und 1.0.1 geprüft: Kein Patch der sechs Dateien wird überflüssig. Die Lebensleisten brauchen weiter den Broadcast beim
  Client (`OnRep_Health` unverändert), das Item-Widget liest weiter ungeprüft. Die Hände-Vorschau nutzt `GetHandsRequiredForItem` und
  übernimmt die neue Zählung (zwei Einhänder = zwei Hände) nativ. P2-Duplikat ins Lager, Reroll-Loot-Level und Kisten zum Bus sind
  native Fixes ohne Gegenstück in der Mod.
- Spieltest 07.11.2026 auf `26495302` (ein PC, Lobby, `-NoSpike -PartialPatches`): 11 von 13 Listen gesetzt, Inventory, HealthBar und
  Reward ohne Fehlermeldung, ausgelassen nur Player und Client (`update-client`). Absturz beim Beitritt unverändert `0x5904DE8`. Die
  Abnahme beim Client (Airdrop, Belohnungswahl, Lager) stand deshalb aus; `update-client` ist inzwischen gemergt.

### Welt-Patches (`update-welt`)

Stand 07.11.2026, Dateien `MapPatches.cpp`, `TravelPatches.cpp`, `ExpPatches.cpp`, `ProjectilePatches.cpp`, `MovementPatches.cpp`,
`CameraPatches.cpp`, `CodeCave.cpp`. Es war keine Codeänderung nötig, die Tabelle `RvaMap26495302.inc` aus `update-basis` und
`update-client` deckt alle Adressen dieser Dateien ab.

- `tools/expect_check.py` gegen die Exe von `26495302` (TimeDateStamp `0x65BFBBFB`): 11 von 11 prüfbaren Prüfungen der sieben Dateien `ok`,
  keine `abweichend`, keine `fehlt`. Die übrigen Patches der Dateien lesen ihre Adressen zur Laufzeit (lokale `Site`), die prüft nur das Spiel.
- Spieltest ein PC, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -NoSpike`: Host und Client melden `Patches: 43 gesetzt` (ohne
  `-PartialPatches`) und `Signaturen: 88 Adressen geprüft`, keine Fehlermeldung. `Run-Route.ps1 -Start -Mod -Tunnel Lobby -Days 2`
  (mit Spike) endet mit Exitcode 0: MetaBase, Tag 1 (159/159 Kacheln) und Tag 2 (63/63 Kacheln) bei beiden `sauber`, 2/2 Spieler,
  Inventar gleich. `SeamlessTravelToLevel` läuft ohne Absturz, der Client setzt die gemeinsame Kamera
  (`Kamera: gemeinsame Kamera gesetzt`), der Host beobachtet den Dash der Clients (`GA_Dash`).
- Ohne Spike geht `Run-Route.ps1` nur bis Tag 1 (`!finish` fehlt).
- Nachtest 09.11.2026, ein PC, `Start-Session.ps1 -Build -Mod -Tunnel Lobby -NoSpike`, 44 Patches, Mensch spielte den Client: XP-Bälle
  erscheinen beim Client (fünfmal `XP-Baelle: 1 Host-Ausstoss(e) angezeigt`, sichtbar leicht verzögert), der Client-Dash am Kamerarand
  bleibt in der gemeinsamen Kamera. Kein Absturz. Damit ist `update-welt` bestätigt.

### Spike (`spike-build-26495302`)

Stand 07.11.2026. Der Spike (`research/spikes/a-connect`) nennt weiter die RVAs von `26187885` und rechnet sie zur Laufzeit mit `Rva()` aus
`Builds.cpp` um: erst die eigenen Einträge, dann die Tabelle der Mod (`src/Game/RvaMap26495302.inc` wird eingebunden). Der Build kommt aus dem
TimeDateStamp der Exe, ein unbekannter Build setzt keine Patches. Eine Adresse ohne Eintrag liefert `kMissingRva`, `Expect` meldet sie im Log
(`Adresse fehlt oder nicht lesbar`) und der Patch bleibt aus.

| Stelle | 26187885 | 26495302 | Weg |
|---|---|---|---|
| `SerializeNewActor` | `0x02D09B90` | `0x02D09D90` | rva_map (eindeutig) |
| Archetyp-Prüfung darin | `0x02D0A426` | `0x02D0A626` | rva_map (eindeutig) |
| `UpdateTag` (Cheats) | `0x04B7D7D0` | `0x04B7D9D0` | rva_map (eindeutig) |
| Tag `State.Invincible` (Global) | `0x0AC61FE8` | `0x0AC680D8` | rva_map (global) |

- GObjects, GNames und GWorld stehen in der Tabelle des Spikes (Werte oben). Alle übrigen Code- und Datenadressen des Spikes stehen schon in der Tabelle der Mod.
- Relative Sprünge (`call`/`jmp rel32`) prüft `ExpectBranch` auf ihr Ziel statt auf feste Bytes, denn die Verschiebung ändert sich je Build.
- Importeinträge (`CreateProcessW`, `sendto`, `recvfrom`) liegen in jedem Build anders und werden in der Importtabelle der Exe gesucht (`FindImportSlot`).
- Offline geprüft: alle `Expect`/`ExpectBranch` des Spikes (51 mit Adresse, 1 über den Parameter in `HealthBars.cpp`, dort beide Aufrufer geprüft) passen auf die Exe von `26495302` (Muster, Abstände wie im Quelltext).
- Spieltest ein PC, zwei Instanzen: Spike allein und Spike mit `-Mod -Tunnel Lobby` laufen bis `DDDynamic_Map` (2 Spieler), Patches „aktiv“, „Karte vom Host“, „PCG vom Host“, Busplatz, Lebensleisten, Run-Ende und Lastmessung gesetzt, `!wave` wirkt, kein Absturz.
