# Offsets für Build 26786272

Der Quelltext (`src/`, Spike, `tools/spike`) nennt seit `spiel-update-26786272` (21.11.2026) RVAs von Steam-Build `26786272` (Patch 1.0.3),
Exe `DDSurvivors-Win64-Shipping.exe`; die Mod unterstützt nur diesen Build. **Die Abschnitte dieser Datei sind zu ihrem jeweiligen Stand
entstanden und nennen die RVAs des damaligen Builds** (`26495302`, davor `26187885`); sie sind nicht umgeschrieben. Umrechnung:
[Referenz-Build 26786272](#referenz-build-26786272-spiel-update-26786272). Feld-Offsets sind gleich geblieben, außer wo vermerkt.
Nach einem Spiel-Update müssen sie neu bestimmt werden, siehe [Spiel-Updates](../CONTRIBUTING.md#spiel-updates).
Sie sind hier zum Nachschlagen gesammelt, damit niemand sie erneut suchen muss. Wer neue findet, trägt sie hier ein.

RVAs sind relativ zur Image-Base `0x140000000`. Die Patchstellen stehen als Konstanten am Anfang der jeweiligen
[Spike-Datei](../research/spikes/a-connect/README.md#dateien), ihre Bedeutung in der [Patch-Tabelle](../research/spikes/a-connect/README.md#patches).
Die Spielfehler-Fixes nutzt auch die Mod, mit denselben Werten am Anfang von `src/Game/*Patches.cpp`, siehe [Patches der Mod](../src/Game/README.md#patches).

## Referenz-Build 26786272 (`spiel-update-26786272`)

Stand 21.11.2026, Nutzerentscheidung vom selben Tag: Die Mod unterstützt nur noch den neuesten Spiel-Build. `Game::AddressMap::ReferenceBuild`,
`Protocol::GameBuild`, `Core::SupportedBuilds`, `Common.ps1`, `game_build.py`, `expect_check.py`, Spike und Installer kennen nur noch
`26786272`; `26495302` und `26669225` sind entfernt, ebenso beide Tabellen `RvaMap*.inc`. Auf einem älteren Build bleibt die Mod aus
(`Unbekannter Spiel-Build`), der Installer lehnt ihn ab.

- **Umgeschrieben:** 155 Adressen in 41 Dateien (`src/`, `research/spikes/a-connect`, `tools/spike`, zwei Tests): 128 über die Tabelle aus
  `update-basis-26786272` (124 der Mod, 4 des Spikes), 26 weitere aus Kommentaren und Tools mit `tools/rva_map.py` (23 eindeutig, je eine
  über den Funktionsanfang, über Befehlsfenster der Funktion und als vtable) und eine von Hand (Aufruf von `PlayReloadSound` in der
  Nachladelogik, `0x05A48CC5`). Die Tabelle (Referenz-RVA `26495302` -> `26786272`) liegt nur im Verlauf des Pull Requests 268, nicht auf
  `main`: `git fetch origin pull/268/head`, dann `git show 87a91a7:src/Game/RvaMap26786272.inc`.
- **Gemessene Werte in Kommentaren:** Wo ein Kommentar einen Messwert eines früheren Builds nennt, steht der alte Build weiter dabei
  (`ItemAbilityPatches.cpp`, Koordinaten des Startrings in `Start-Run.ps1`). Die Kopfzeilen der Werkzeuge unter `tools/spike` nennen jetzt
  `26786272`, ohne dass jedes einzeln gelaufen ist, siehe „Nicht geprüft“.
- **Nicht umgeschrieben:** die Markdown-Dateien (diese Datei, `GAME_ANALYSIS.md`, die READMEs unter `src/` und im Spike). Ihre Abschnitte
  sind datierte Befunde mit den Adressen des damaligen Builds; teils stammen sie noch von `26187885`.
- **Verzweigungen nach Build entfernt:** `TryBuyItem` und `EndTackle` prüfen nur noch die Bytes von `26786272`, `RepresentationItem` ist
  fest `0xA38`.
- **Signaturen (`signaturen-nachziehen`):** `PatchSignatures.inc` ist geleert und für den einen Build neu erzeugt: 115 Adressen, auch die
  beiden bisher fehlenden aus `CharacterPatches.cpp`; die Namen kommen jetzt alle vom Werkzeug. `patch_signatures.py check --strict` 115
  `ok`, `sites` ohne Abweichung (dafür `InventoryPatches.cpp` nachgezogen und `DamageNumbersSites` umbenannt), ein zweites
  `generate --write` ändert die Datei nicht. `expect_check.py` 101 `ok`, 2 `uebersprungen`.
- **SdkChecks:** Build gegen das SDK von `26786272` ohne Fehler.
- **Spieltest** (ein PC, eingefrorene Kopie `26786272`, ohne echte Eingaben, 21.11.2026):
  - Mod und Spike (`Start-Session.ps1 -Build -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map`): Host und Client
    `Signaturen: 115 Adressen geprüft`, `Patches: 57 gesetzt`, `Spiel-Build 26786272`, Gegnerwelle, Inventar gleich,
    `Test-Godmode.ps1` bestanden, Rundenende bei beiden, kein `[ERROR]`, kein Absturz.
  - Nur Spike, ohne Mod: beide Instanzen `Patch: aktiv`, Karte und PCG vom Host, keine Zeile `Adresse fehlt` oder `unerwartete Bytes`,
    Inventar gleich.
  - Nur Mod (`-NoSpike`): `Patches: 57 gesetzt`, Lobby, Reise in die Mission, zweieinhalb Minuten, Inventar gleich, kein Absturz.
- **Nicht geprüft:** die lesenden Python-Werkzeuge unter `tools/spike` einzeln (keines nennt einen Offset in den verschobenen Bereichen
  von `UInventoryPanel`, `UItemRepresentation`, `URecipePanel` oder deren Blueprints, ihre Feld-Offsets liegen in Klassen, die laut
  SDK-Vergleich unverändert sind; gelaufen sind `ddmem.py`, `inventory.py`, `godmode_state.py` über die Skripte oben), Befehle des Spikes
  außer `!wave`, `!finish`, `!god`, alles Weitere wie in [Build 26786272](#build-26786272-update-basis-26786272) (`update-26786272-run`).
- **Entwicklungskopien:** Nach dem Merge stellt jeder mit `Freeze-GameBuild.ps1 -Build 26786272 -Activate` um (`wechsel-26786272`).
  Branches von vor dem Umstieg brauchen bis zu ihrem Rebase die alte Kopie.

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
[Referenz-Build 26786272](#referenz-build-26786272-spiel-update-26786272). Stand 20.11.2026, Patch 1.0.3 vom 19.11.2026 (Version laut `CrashReportClient.ini` des Spiels: `1.0.3-main-10811c16`). `26495302` bleibt
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
`src/Game/RvaMap26669225.inc` um, erzeugt mit [`tools/rva_map.py`](../tools/README.md#rva_mappy). Exe: TimeDateStamp `0xE9CA3AB7`,
SizeOfImage `0x0B74A000`, `.text` gleich groß (`0x827C000`). Die Portierung der offenen Adressen ist je Patch-Datei aufgeteilt, siehe
[Wartung](../TODO.md#wartung) (`update-1-0-2-*`).

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
  und bleibt ohne Broadcast auf 0 %). Für die Rüstung gab es damals keinen Hook, nachgetragen in [Rüstung](#rüstung-opt-astronaut-ruestung).
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
- **Derselbe Mechanismus, nicht Teil dieser Aufgabe:** Auch die Ticks der Listen HealthBar (Busbalken) und WeaponSound bleiben auf
  `26669225` aus, bis `update-1-0-2-lebensleisten` und `update-1-0-2-waffensound` ihre Adressen nachtragen. Ein hängender Ablauf ist
  dort nicht belegt.

## Build 26495302 (`update-basis`)

Stand 07.11.2026, historisch. Bis zum Umstieg am 09.11.2026 blieben die Werte Referenz-RVAs von `26187885`. Für `26495302` rechnet `Game::AddressMap` sie über
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
  (`0x59635B0` + `0x7D`, siehe [Spike-Inventar](../research/spikes/a-connect/README.md#inventar-ursache-teilfix-und-verbleibende-grenze)):
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
| `HandleConfirmButton` in `26495302` | `0x5A71900`, Aufruf `0x5A71960` → `0x5AF5030` | lädt `[[Panel+0x798]+0x3E0]` und springt über dessen VTable-Slot `+0x2A8`; bei freigegebenem Panel Sprung nach 0 (`lager-bestaetigen-absturz`) |
| `UObject.InternalIndex` | `+0x0C` | Platz im Objekt-Array; `Objects::RefTo`/`IsLive` vergleichen ihn mit `FUObjectItem.Object` |
| `InventoryPanel.SetupInventory` | `0x5A8BAB0` | liest `InventoryComponent` bei `+0x370`; positioniert aus `InventoryItemInstance.CachedBackpackLocalPosition` (`+0x150`, FVector2D, nicht repliziert) |
| `DefaultFocusableButton.ParentPanel` | `+0x3A8` | Panel des Knopfs (UUserWidget); der Bestätigen-Knopf des Inventar-Panels heißt `MainButton`, gefärbt über `UserWidget.SetColorAndOpacity` (`bestaetigen-alle`) |
| `InventoryPanel.SnapRepresentationToTile` | `0x5A92030` (Build `26495302`: `0x5A96EF0`, Aufrufer `0x5A6A23A`, `0x5A8105E`, Exec-Thunk `0x571C283`) | richtet UI und Pixelpositions-Cache an einem Feld aus (Feld mal 66 px, Ausgleich nur bei Drehung 1 und 3); der native Ablage-Weg übergibt nicht den Pivot, vermutlich die obere linke Ecke der belegten Fläche; reflektierte Parameter: ItemRepresentation `+0`, FIntVector2 `+8`, CanvasPanel `+0x10`, Größe `0x18` |
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
| `InventoryComponent::HandleItemSold` | `0x5967900` (`26495302`: `0x596BA70`), Exec-Thunk `0x57128B0` | `(this, Item)`, nativ; `RemoveFromBackpack`, `UnoccupyBackpackSlots`, `RemoveFromInventory(SoldForScrap)`, `RefreshItemEffects` (`lager-handel-client`) |
| `ShopPanelItem::TryBuyItem` (`opt-haendler-client`) | `26495302`: `0x58CE280`, Exec-Thunk `0x579ADE0` | `(this)`, nativ; Preis aus `CostDisplay` (`+0x850`, Wert `+0x384`), zahlt lokal über einen Funktor des Panels, dann `AddToInventory` mit `ItemInstance` (`+0x868`); `ManuallyCallClearAndAdd` `+0x848`. Offsets aus dem Code dieses Builds |
| `InventoryPanel.ConfirmAllStagedIngredients` | `26495302`: `0x5A5F060`, Exec-Thunk `0x570B460` | Rezept bestätigen, ruft `UpgradeItems` (`opt-werkbank-station`); nicht gepatcht |
| `ItemRepresentation.ConfirmSell`, `.ConfirmRepair` | `0x5AA2F60`, `0x5AA2DB0` (`26495302`: `0x5AA7E50`, `0x5AA7CA0`) | buchen Schrott lokal über `AddAmount`, dann `HandleItemSold` bzw. Server-RPC `RepairItem` |
| `ADDMissionGameState.ScrapWallet`, `UAbstractCurrency.CurrencyAmount` | `+0x3C0`, `+0x100` | nicht repliziert; `AddAmount` begrenzt auf 0 bis INT32_MAX und löst `OnCurrencyChanged` aus |
| `UAbstractCurrency::AddAmount` (nativ) | `26495302`: `0x586B2B0`, Exec-Thunk `0x5662130` | `(this, Delta)`; schreibt `+0x100`, Broadcast `+0xC8` (`opt-schrott-anzeige`) |
| `AScrapLoot::Loot_Implementation`, `ALootBase::Loot_Implementation` | `26495302`: `0x59AF660`, `0x59AF3F0` | `(ILootableInterface* = Actor + 0x2A8, ULootAttractor*)`; ScrapLoot liest `ScrapAmount` (`+0x2D0`), bucht ohne Rollenprüfung über `AddAmount` und springt in die Basis (Klang-RPC, Delegate). Beim Client gesperrt (`opt-schrott-anzeige`) |
| `UInventoryItemInstance.GetDynamicRepairPrice` | `26495302`: `0x5965960` | Seltenheit `+0x54` mal Faktor, mal `DDGameStatsSubsystem +0xD0 / 8 + 1` |

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
| `DDMissionGameInstance.TriggerEndRound` (Implementierung) | `0x5907ED0` | `(this, ERoundChangeReason)`: `NewDay` 1, `Win` 2, `Lose` 3. Aufrufer `FinishMission` und Exec-Thunk. Hook `run-ende`: Host meldet die Niederlage, Client führt sie nie aus |
| `DDMissionGameInstance.TriggerStartRun` | `0x5908150` | Exec-Thunk `0x56CB140`; ruft zuerst `PrepareForNewRun`, am Ende `0x591F1D0` am `RunPersistencyManager` (`opt-metabase-inventar`) |
| `DDMissionGameInstance.PrepareForNewRun` | `0x58FB3E0` | Exec-Thunk `0x56C6BF0`; leert `PendingGameStateComponentTravelStates` (`+0x248`, Helfer `0x58FEA50`), `+0x298` (Wort, samt `LastRoundChangeReason`), Seed aus `+0x398`. Leert den Run-Container nicht |
| `DDMissionGameInstance.FinalizeRunStart` | `0x58E9D80` | Exec-Thunk `0x56BC230`, nur aus Blueprints |
| `URunPersistencyManager::OnRoundChanged` | `0x5936BE0` | Exec-Thunk `0x57974E0`. `NewDay`: Container `+0x80` neu, `RunDataState` (`+0x88`) `EndOfDay`, Persistables serialisieren, speichern. `Win`/`Lose`: Container leeren (`0x591F280`), `OriginalVersion` (`+0x148`) −2, speichern. Einziger Aufrufer ist der Thunk. `26669225`: `0x5937870`. Hook der Mod: Bei einem Netz-Client läuft es nicht (`client-runstand-abbruch`) |
| `URunPersistencyManager::ResavePersistable` | `0x593EBA0` | Exec-Thunk `0x57989A0`, dazu fünf native Aufrufer (`0x5854E11`, `0x5854E9F`, `0x59076BE`, `0x595DA0E`, `0x5A02907`). `(this, TScriptInterface&)`: Nur wenn `RunDataState` (`+0x88`) nicht 0 ist, serialisiert die Komponente in den Container (`+0x80`) und speichert. `26669225`: `0x593F830`. Hook der Mod wie bei `OnRoundChanged` |
| Native-Tabelle `{Name, Exec-Thunk}` von `URunPersistencyManager` | Thunks `0x5792500` bis `0x579B3B0` | 18 Einträge, darunter `RegisterPersistable` (Rumpf `0x593CE40`), `LoadRunData` (`0x5934760`), `SaveRun`, `TryContinueRun`, `IsSavedRunCoop` (`0x5934010`). `ERunDataState` im Spielstand: ohne Feld (leer), `BeginOfDay`, `EndOfDay` |
| `URunPersistencyManager.ClearRunData` | `0x591FD50` | Exec-Thunk `0x57927A0`; nativ nur von `DD_CleanRunSave` (`0x58DF610`) aufgerufen |
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
| `UGameplayAbility` | Schlüssel der Aktivierung (`CurrentActivationInfo` `0xD4` + `PredictionKeyWhenActivated` `0x04` + `FPredictionKey.Current`, int16; beim Client und beim Host gleich, `dodge-client-live`), bIsActive | `0xD8`, `0x388` |
| `UAbilityTask_ApplyRootMotionConstantForce` | FinishVelocityMode, WorldDirection, Strength, Duration | `0x88`, `0xD0`, `0xE8`, `0xEC` |
| `BP_DD_Character_C` | Attributset mit Ausdauer; darin MaxStamina, Stamina (`FGameplayAttributeData`, CurrentValue je `+0x0C`) | `0x7D8`; `0x30`, `0x50` |
| `APlayerController` | CheatManager, CheatClass | `0x410`, `0x418` |
| `UBossSpawnLookup` | BossSpawnInfos (je `0x68`: BossID `0x00`, IsEndBoss `0x08`, BossEnemyType `0x10`, SpawnPointClass `0x38`) | `0x28` |
| `UDDBossObjective` | MaxHealth `0xD0` (int64, per Reflexion aufgelöst, `BossBarAdapter`); `UDDObjectiveWidget.ObjectiveManager` `0x378`; `UCommonAttributeSet` MaxHealth `0x30`, Health `0xA0` (FGameplayAttributeData, Wert bei `+0xC`) | Build `26495302` gemessen, `gegner-alle` |
| `FBossSpawnInfo.BossEnemyType` (weicher Verweis) | Paketpfad als FName bei Eintrag `+0x18`, Assetname bei `+0x20` (Build `26495302` gemessen, `gegner-alle`); Zeiger `LoadObjectByPath(Paket.Asset)` lädt den `EnemyType` | |
| `UEnemyType` | EnemyBlueprint (`TSoftClassPtr`) | `0x128` |
| `ACharacter` | CharacterMovement, BasedMovement, ReplicatedMovement-Block | `0x330`, `0x340`, `0x398` |
| `ACharacter` (Spiel) | AttributeSet | `0x7D8` |
| `ADDCharacterBase` | CommonAttributeSet | `0x700` |
| `UCommonAttributeSet` | Health (`FGameplayAttributeData`, CurrentValue bei `+0xC`) | `0xA0` |
| `UCommonAttributeSet` | MovementSpeed (`FGameplayAttributeData`, BaseValue `+0x8`, CurrentValue `+0xC`; `tod-client`) | `0xE8` |
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

## Ausdauerbalken (`opt-ausdauerbalken`)

Build `26495302`, Exec-Thunks `0x56C6000` und `0x56C54E0`, im Spieltest vom 10.11.2026 belegt.

| Stelle | Wert |
|---|---|
| `OnRep_Stamina` | `0x57DDE40`, Prolog wie `OnRep_Health` |
| `OnRep_MaxStamina` | `0x57DD080`, Prolog wie `OnRep_Health` |
| `UDDCharacterAttributeSet` | `MaxStamina` `0x30`, `OnMaxStaminaChanged` `0x40`, `Stamina` `0x50`, `OnStaminaChanged` `0x60` |

## Rüstung (`opt-astronaut-ruestung`)

Stand 18.11.2026, beide Builds offline aus der Exe gelesen: `OnRep_*` über die Exec-Thunks der `FNameNativePtrPair`-Tabelle (gleiche Tabelle
wie `OnRep_Health`), Feld-Offsets aus den Eigenschaftsparametern der Reflection (`FPropertyParamsBase`, Wort `ArrayDim | Offset << 16`).
Felder und Delegates sind auf beiden Builds gleich. Befund in [Rüstung beim Client](GAME_ANALYSIS.md#rüstung-beim-client-opt-astronaut-ruestung).

| Stelle | 26495302 | 26669225 |
|---|---|---|
| Exec-Thunk `OnRep_Armor` | `0x5696C20` | `0x56973B0` |
| Exec-Thunk `OnRep_MaxArmor` | `0x5696F90` | `0x5697720` |
| `UCommonAttributeSet::OnRep_Armor` | `0x57DBA40`, Prolog wie `OnRep_Health`, liest `+0x1B0` | `0x57DC250` |
| `UCommonAttributeSet::OnRep_MaxArmor` | `0x57DCD60`, Prolog wie `OnRep_Health`, liest `+0x190` | `0x57DD570` |
| `UCommonAttributeSet` | `MaxArmor` `0x190`, `OnMaxArmorChanged` `0x1A0`, `Armor` `0x1B0`, `OnArmorChanged` `0x1C0` | gleich |
| `UCommonAttributeSet::PostAttributeChange` (vtable-Platz 88, `+0x2C0`) | `0x57DF690` | `0x57DFE50` |
| darin `call FGameplayAttribute::SetNumericValueChecked` (Klemmung auf ein gesunkenes Maximum, für Rüstung und Leben) | `0x57DF8EB` | `0x57E00AB` |
| `FGameplayAttribute::SetNumericValueChecked` | `0x4BDDD80` | `0x4BDE540` |
| `UCommonAttributeSet::GetArmorAttribute` (statisches Attribut, Name „Armor“) | `0x57D48A0` | `0x57D50B0` |
| `UCommonAttributeSet::PreAttributeChange` (vtable-Platz 87) | – | `0x57E14A0` |
| `UAbilitySystemComponent::ActiveGameplayEffects` | `ASC+0x8D8`, Elemente `GameplayEffects_Internal` bei `+0x130`, je `0x328` Byte, `Spec` `+0x20`, `PredictionKey` `+0x270` | gleich |

Stand 19.11.2026: Platz 88 und die Aufrufstelle aus der vtable des laufenden `26669225` gelesen und disassembliert, auf `26495302` über
denselben Befehlsablauf gefunden, `tools/rva_map.py` bestätigt die Zuordnung (`eindeutig`). Die Elementgröße der aktiven Effekte stammt
aus der Reflection (`ActiveGameplayEffect`, `PropertiesSize`) im laufenden Spiel.

- Ein zweiter Satz Delegates `OnHealthChanged` `0x40` bis `OnMaxArmorChanged` `0x90` (Leben, Ausdauer, Rüstung, je Wert und Maximum) gehört zu
  einer anderen Klasse mit eigener Delegate-Signatur; welche, ist offline nicht bestimmt. **[Vermutung]** Ein Hörer am ASC für die HUD-Leiste.

## Schadenszahlen (`opt-schadenszahlen`)

Build `26495302`, Disassembly und Live-Speicher, Befund in [Schadenszahlen beim Client](GAME_ANALYSIS.md#schadenszahlen-beim-client-opt-schadenszahlen).

| Stelle | Wert |
|---|---|
| Schadenszahl-Handler des `ADDPlayerController` | `0x5A22DB0(Controller, FDamageInformation*)`, Prolog `48 8B C4 48 89 58 18 55 56 57`; Aufrufer im Schadenspfad `0x5816538`, `0x5816C42` |
| `FDamageInformation`, Konstruktor | `0x56591B0`, Prolog `48 89 5C 24 10 48 89 4C 24 08` |
| `TagMagnitudes` freigeben | `0x12BC260`, Prolog `48 89 5C 24 08 57 48 83 EC 30`; `FMemory::Free` `0x12F85E0`, Prolog `48 85 C9 74 2E 53 48 83 EC 20` |
| `FDamageInformation` (0xA0) | `Damage` `0x0`, schwache Zeiger `DamagedActor` `0x4`, `DamagedAvatarActor` `0xC`, `InstigatedBy` `0x14`, `DamageCauser` `0x1C`, `SourceObject` `0x24`, `HitDirection` `0x30`, `DamageFlags` `0x48`, `TagMagnitudes` `0x50` |
| `ADDPlayerController` | `DamageNumberClass` `0x940` (`BP_FloatingText_C`), `EnableDamageNumbersSettingInstance` `0x950`, letzter Ort `0x8C0` |
| `ShowDamageText` | Exec-Thunk `0x569BCD0`, Implementierung `0x5A94C80`, Pool-Rückruf `0x5A093F0` |

## Dev-Tab (`opt-dev-tab`, Entwicklerwerkzeug)

Feldoffsets aus dem SDK, Aufrufe per `ProcessEvent`. Im Spieltest vom 09.11.2026 belegt.

| Stelle | Wert |
|---|---|
| `AEnemySpawner` | `SelectionComponent` `0x3E8`; `SpawnSpecificEnemy` Parameter 0x90 Byte, `FTransform` bei `0x10`, Rückgabe `0x88` |
| `UEnemySpawner_SelectionComponent` | `FallbackEnemyType` `0xE0` |
| `AEnemyBase` | `Type` (`UEnemyType*`) `0x9B0` |
| `UInventoryItemData` | `MinimumItemRarity` `0xF0`, `IsBag` `0x178`, `IsHiddenItem` `0x190` |
| `ALootSpot` | `ItemLootTemplate` `0x2E0` |
| `BP_EnvironmentObjectLootspot_C` | `Widget` `0x3A8`, `Progress` `0x3C0` (double, Net), `IsLooting` `0x3C8` (RepNotify); Funktion `Update Progress Indicator(double)` mit Leerzeichen im Namen ([Lootbalken](GAME_ANALYSIS.md#lootbalken-beim-client-opt-lootbalken)) |
| `UAbstractProgressWidget` | `Progress` `0x328` |
| `UObject` | `InternalIndex` `0x0C` |
| `UGameplayCueManager::ExecuteGameplayCue_NonReplicated` | `0x4BC58D0`, statisch (Ziel, Tag als FName in einem Register, Parameter); ohne Rollenprüfung ([Körperteile](GAME_ANALYSIS.md#körperteile-beim-client-opt-koerperteile)) |
| `UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor` | Exec `0x4BE04C0`; mit ASC nur bei `AActor::Role` `0x168` = Autorität |
| `ADDCharacterBase` | `StartupAbilities` `0x708` (TArray) |
| `UEnemyDyingAbility` | `ChaosDestructionCueTag` `0x3A8` (FGameplayTag), `DestroyDelay` `0x3B0` |
| `UChaosDestructionProxyGameplayCue` | `ProxyActorClass` `0x40` |
| `UClass` | `ClassDefaultObject` `0x118` |
| `FGameplayCueParameters` | Größe `0xD8`, `Location` `0x70` |
| `LootBlueprintFunctionLibrary.SpawnSpecificItemLoot` | Parameter 0x50 Byte, Ort `0x18`, Vorlage `0x38`, Rückgabe `0x48` |

## Blickrichtung (`opt-blickrichtung-client`)

Build `26495302`, SDK und Disassembly, im Spieltest vom 11.11.2026 belegt ([Blickrichtung](GAME_ANALYSIS.md#blickrichtung-beim-client-opt-blickrichtung-client)).

| Stelle | Wert |
|---|---|
| `ADDCharacter` | `Hands` `0x7B0` (Net), `DirectionalMeshComponent` `0x7E0` (beim Client leer) |
| `BP_DD_Character_C` | `DirectionalCharacterMesh` `0x8F0` |
| `UDirectionalCharacterMesh` | `UseHandsDirection` `0x700`, `Mode` `0x703`, `Hands` `0x750`, `VelocityProvider` `0x768`, `FacingRight`/`FacingDown` `0x7F0`/`0x7F1`, gemerkter Winkel `0x7F4` |
| `AHands` | Blickwinkel (float, Grad, nicht repliziert) `0x31C`, `ParentCharacter` `0x320` (beim Client leer) |
| `USceneComponent` | `ComponentToWorld.Rotation` (FQuat, 4 double) `0x1F0` |
| Richtung des Mesh aus `Hands+0x31C` | `0x586BE20` |
| Drehung der Hände zum Ziel (schreibt `+0x31C`) | `0x593FDD0`, erreicht über `ProxyRotateTowards`-Implementierung `0x581A1E0` (VTable `0xA20`), Exec-Thunk `0x56C6D50` |
| Autoritätszweig von `ADDCharacter` (setzt `+0x7E0`, `+0x7B0`, `SetParentCharacter`) | `0x58022A0` |
| `Hands::SetParentCharacter` | Implementierung `0x5942290`, Exec-Thunk `0x571B240` |

## Laufanimation (`opt-laufanimation-client`)

Build `26495302`, [Befund](GAME_ANALYSIS.md#laufanimation-beim-client-opt-laufanimation-client).

| Stelle | Wert |
|---|---|
| `BP_DD_Character_C` | `PlayerAnimationVelocityProvider` `0x910` (nicht `0x768`: das ist `UDirectionalCharacterMesh.VelocityProvider`) |
| `ADDCharacter` | `WobbleAnimation` `0x780` |
| `UWobbleAnimationComponent` | `Progress` `0x258`, `MaxInputVelocity` `0x25C`, `VelocityToProgressFactor` `0x260`, `MaxSpeed` `0x264`, `OverrideVelocity` `0x268`, `OverrideVelocityToProgressFactor` `0x26C`, `VelocityProvider` `0x288`, `bCanStretch` `0x308` |
| `UPlayerAnimationVelocityProvider` (0xF8 Byte) | Schalter „manuell“ `0xC0` (bool), gemessenes Tempo `0xC8` (FVector), letzter Ort `0xE0` (FVector); Owner `0xA8` |
| RVA `0x5848450` | natives `UPlayerAnimationVelocityProvider::GetVelocity`: manuell → `+0xC8`, sonst `Owner->GetVelocity()` (vtable `+0x340`); gerufen nur vom Exec-Thunk `0x5770E80`, vom Richtungs-Mesh (`0x586BF17`) und vom Wackeln (`0x5B1BF2B`) |
| RVA `0x5857890` | natives `SetManualTrackingActive`: setzt `+0xC0`, `+0xE0` = Ort der Root-Komponente (`+0x210`), dann vtable `+0x410`; einziger Aufrufer ist der Exec-Thunk `0x57761F0` |

## Schuss-Animation (`opt-schuss-animation`)

Build `26495302`, SDK, im Spieltest vom 13.11.2026 gelesen ([Befund](GAME_ANALYSIS.md#schuss-animation-und-geisterwaffen-opt-schuss-animation)).
Das SDK stammt vom Vorgänger-Build; die Werte unten lieferten im Spiel plausible Listen, Klassen und Namen.

| Stelle | Wert |
|---|---|
| `AHands` | `InnerRotationContainer` `0x2C8`, `AutoFire` `0x378` (Net), `ChildActorComponents` `0x3A0`, `CurrentRangedWeapons` `0x3B0`, `CurrentMeleeWeapons` `0x3C0` (alle drei Listen beim Client leer) |
| `ADDCharacter` | `EquippedWeapons` `0x7B8` (nicht repliziert) |
| `UWeaponInstanceBase` | `InventoryItemInstance` (weak) `0x38`, `WeaponData` `0x40`, `ChildActorComponent` `0x50`, `WeaponActor` `0x58` |
| `UInventoryComponent` | `MyItems` `0x120`, `CharacterReference` `0x220`, `WeaponLookup` (TMap Item → Waffeninstanz, Element 0x18) `0x238` |
| `UChildActorComponent` | `ChildActor` `0x258` (Net) |
| `USceneComponent` | `AttachParent` `0xD0`, `AttachSocketName` `0xD8`, `AttachChildren` `0xE8`, `RelativeLocation`/`Rotation`/`Scale3D` `0x148`/`0x160`/`0x178`, `bVisible` `0x1A8` Bit 5, `bHiddenInGame` `0x1A9` Bit 3, `ComponentToWorld.Translation` `0x210` |
| `UDDCharacterAttributeSet` | `ExtraHands` `0x100` (`BaseValue` `+0x8`, `CurrentValue` `+0xC`) |
| `InventoryPanel.UpgradeDroppedItem` | `0x5A9D370`, Exec-Thunk `0x571E400`; ruft `UpgradeItems` (`0x5984CD0`) bei `0x5A9D71E` |
| Exec-Thunks | `RemoveFromBackpack` `0x5718980`, `UnoccupyBackpackSlots` `0x56627D0`, `RemoveEquippedWeapon` `0x56C6F80` (Implementierung `0x581C860`), `AddEquippedWeapon` `0x56B9050` (`0x57FE920`) |

## Charakterwahl (`charakter-wahl`)

Build `26495302`, Disassembly und SDK, im Spieltest vom 10.11.2026 belegt.

| Stelle | Wert |
|---|---|
| `UCharacterDefinitionApplier::ApplyInitialCharacterDefinition` | `0x5837220`, Prolog `40 57 48 83 EC 20 80 B9 10 01` |
| Definition anwenden (intern, aus `CharacterSelectionChanged`) | `0x5840DF0(Applier, Definition)`, Prolog `48 89 5C 24 10 48 89 74 24 18` |
| `UCharacterDefinitionApplier` | `AppliedCharacterDefinition` (`TSoftObjectPtr`, Assetname bei `+0x10`) `0xE8` |
| `UCharacterSelectionManager` | `CharacterDefinitions` `0x58`, `SelectedCharacters` (`TMap<int32, Def*>`) `0x68`, `SelectedCharacterID` `0xB8` |
| Spielerindex des Appliers | vfunc `+0x330` von `Pawn.Controller.Player`, ohne LocalPlayer Index 0 |

## Warnanzeigen der Gegner (`opt-gegner-indikatoren`)

Build `26495302`, Disassembly der Exec-Thunks (`UFunction::ExecFunction` `+0xD8`) und SDK. Im Spieltest vom 11.11.2026 belegt, siehe
[Warnanzeigen der Gegner](GAME_ANALYSIS.md#warnanzeigen-der-gegner-beim-client-opt-gegner-indikatoren).

| Stelle | Wert |
|---|---|
| `UEnemyAttackAbility::SpawnCircleTelegraph` | `0x58908C0(Ability, System, const FVector*, Delay, Lifetime, Size, AutoDestroy, Pool, Attached)`, Thunk `0x56EDBE0`, Prolog `48 8B C4 48 89 58 08 48 89 70 18 57`; direkte Aufrufer `0x586ADCB`, `0x586AE86`, `0x58914BA` |
| `UTackleAbilityExecution::ActivateLineTelegraph` | `0x58A3DB0(Execution)`, Thunk `0x57AF280` (`jmp`), Prolog `48 89 5C 24 08 57 48 83 EC 70`; nur aus Blueprints |
| `UTackleAbilityExecution::CalculateLineTelegraphEndPoint` | `0x58A9D30(Execution, const FVector*, Percentage)`, Thunk `0x57AFC40`, Prolog `40 53 48 81 EC D0 00 00 00` |
| `UTackleAbilityExecution::EndTackle` | `0x58B1130(Execution, bool)`, Thunk `0x57B0950`, Prolog `40 53 57 48 83 EC 38` |
| `UTackleAbilityExecution::ExecuteTackle` | `0x58B1F40(Execution, float)`, Thunk `0x57B09E0`, Prolog `48 89 5C 24 10 57 48 83 EC 60`; blendet zuerst `LineTelegraph` aus (vfunc `+0xAD0`) |
| `UEnemyAttackAbility` | `SizeVariableName` `0x440`, `LifetimeVariableName` `0x448`, `DelayVariableName` `0x450` (FName), `CachedTelegraphAttachComponent` `0x468` |
| `UTackleAbilityExecution` | `SwitchTelegraphColorPercentage` `0x108`, `LockTelegraphColor` `0x10C`, `DefaultTelegraphColor` `0x11C`, `LastTargetingPercentage` `0x208`, `LineTelegraph` `0x238`; ohne Reflexion: `0x268` (gesetzt: kein Endpunkt), `0x26A` (Farbe gewechselt), Niagara-Parameternamen `0x398` (Endpunkt), `0x3A0` (Farbe), `0x3A8` (Breite) |
| `UActorComponent` | `OwnerPrivate` `0xA8` (ohne Reflexion), `bIsActive` `0x9A` Bit 4 |
| `AProjectile` | `OptionalProjectileTelegraph` `0x378` |
| `UNiagaraComponent` | `Asset` `0x580`; vfunc `+0xA78` setzt einen Float-Parameter (wie `SetVariableFloat`) |
| `NiagaraFunctionLibrary.SpawnSystemAtLocation` | Parameter 0x68 Byte, Ort `0x10`, Rotation `0x28`, Skala `0x40`, `bAutoDestroy` `0x58`, Rückgabe `0x60` |

## Boss-Parzelle (`endboss-spawn`)

Build `26495302`, im laufenden Spiel gemessen (Tag-8-Karte `MGP_Burger_Boss_Nature`, `BP_Parcel_BurgerBoss_01_C`) und per Frida
beobachtet, 13.11.2026. Der BossSelector liegt gegenüber dem SDK verschoben. Siehe
[Boss erscheint nicht](GAME_ANALYSIS.md#endboss-erscheint-nicht-endboss-spawn).

| Stelle | Wert |
|---|---|
| `ABossMapTileParcel` | `StartFightZone` `0x370`, `BossSelector` `0x378`, `NightController` `0x380`, `BossComponent` `0x388`, `BossSpawnPoint` `0x390`, `ArenaBounds` `0x398`, `SequenceDelay` `0x3A0`, `LoadingSubSystem` `0x3D0`, `GameState` `0x3D8`, `BossProgress` `0x3E0`, Kampf gestartet `0x3E8` (bool, ohne Reflexion) |
| `UBossSelectorComponent` | `DA_BossSpawnLookup` `0xC8`, `NumBossesBeforeEndBoss` `0xD0` (3), `BossProgressSubsystem` `0xD8`, `SpecialRunSubsystem` `0xE0`, `EndlessModeSubsystem` `0xE8`, `BossfightSpawnPoint` `0xF0`, `EnemySpawnBounds` `0xF8`, `BossProxyLocation` `0x108`, `BossSpawnerLocation` `0x110`, Kandidaten `0x1E8` (TArray, je `0x68`) |
| `ABossfightSpawnPoint` | `SpawnedBoss` `0x498` |
| `ADDMissionGameState` | `OnMissionStarted` `0x328` (TArray `FScriptDelegate` je `0x10`: Objektindex, FName), `OnMissionFinishing` `0x338`, `SecondsSinceMissionStart` `0x38C` |
| `ABossMapTileParcel::OnBossfightTriggerZoneBeginOverlap` | Thunk `0x5666A90`, Implementierung `0x59AFFF0` (vtable `0x778`); ohne `BossfightSpawnPoint` kein Kampf |
| `ABossMapTileParcel::OnMissionStarted` | Thunk `0x56677B0`, Implementierung `0x59B09A0` (vtable `0x770`); plant einen Timer, dessen Lambda (`0x599FD30`) mit `rcx = Parcel+0x378` die Selector-Funktion `0x58C3370` ruft, die `BossfightSpawnPoint` setzt |
| `ABossMapTileParcel::OnLoadingFinished` | Thunk `0x56676C0`, nativ leer (`0x468AAB0`, vtable `0x768`) |
| Statischer FName `OnMissionStarted` | global `0x4AC558E0`, angelegt in `0x11E4170` (UTF-16-Text `0x9387630`) |

## Kamera-Folger und Controller-Liste (`host-absturz-start`)

Build `26495302`, Disassembly und Klassenregistrierung, 13.11.2026. Siehe
[Host-Absturz beim Rundenstart](GAME_ANALYSIS.md#host-absturz-beim-rundenstart-host-absturz-start).

| Stelle | Wert |
|---|---|
| `UAttachToCameraComponent` | Größe `0xE0`, Vtable `0x93A09D0`, Versatz (FVector) `0xC0`, Ziel (CameraManager) `0xD8`, Actor `0xA8` |
| `UAttachToCameraComponent::TickComponent` | `0x585AEA0` (Vtable `0x400`), liest `[Ziel+0x1B8]` ohne Nullprüfung |
| `UAttachToCameraComponent::BeginPlay` | `0x583B2D0` (Vtable `0x3E8`), setzt das Ziel einmal aus `GetFirstPlayerController()` |
| `UAttachToCameraComponent` Registrierung | `GetPrivateStaticClass` `0x5655980`, Name `0x93A0EF0`, `InternalConstructor` `0x5658430`, Konstruktor `0x5830DC0` |
| `UWorld::GetFirstPlayerController` | `0x31B9600` |
| `UWorld` | `PlayerControllerList` `0x268` (TArray `TWeakObjectPtr`, Num `0x270`) |
| `APlayerController` | PlayerCameraManager `0x360` (wie oben), Player `0x348` |
| Wetter mit Kamera-Folger | `BP_Rain_C` unter `/Game/RetroParticles/Particles/Systems/WeatherEffects/BP_Rain`, dazu `BP_Snow_C`, `BP_BaseWeatherEffect_C` |
| Client-Absturz bei der dritten Reise (eigener Fehler) | RIP `0x163B33F` lesend an `0xD8`, Aufrufer `0x58DE4B1` |

## Aufsammelsound (`opt-item-sound`)

Build `26495302`, Disassembly, siehe [Aufsammelsound](GAME_ANALYSIS.md#aufsammelsound-von-benzin-und-items-beim-client-opt-item-sound).

| Stelle | Wert |
|---|---|
| `ALootBase::Loot_Implementation` | `0x59AF3F0(LootableInterface, Looter)`, Actor = Interface − `0x2A8`; ruft das Ereignis `Client_PlaySound` (`0x572F970`, Aufruf `0x59AF417`) und gibt das Objekt an den Pool zurück |
| `AItemLoot::Loot_Implementation` | `0x59AF200`, ruft zuerst die Basis `0x59AF3F0` |
| `0x59AF480` | ruft zuerst die Basis `0x59AF3F0`, dann `0x5834880` mit einem Wert aus `this+0x28`; **Vermutung:** `AFuelLoot::Loot_Implementation` (Tank) |
| `LootBase.Client_PlaySound` | NetClient-RPC, beim Client vom Netz über `exe+299CFFB` (ProcessEvent), lokal aus `0x59AF417` |
| `LootBlueprintFunctionLibrary.DropRequiredFuel` | Parameter 0x30 Byte: `World` `0x0`, `FuelClass` `0x8`, `Location` `0x10`, `Radius` `0x28`, `AdditionalFuel` `0x2C` (Spike-Testhilfe `!fx fuel`) |

## Bedrohungsstufe (`opt-bedrohung-sound`)

Build `26786272`, Speicher gelesen und Reflexion zur Laufzeit, siehe [Bedrohungsstufe beim Client](GAME_ANALYSIS.md#bedrohungsstufe-beim-client-opt-bedrohung-sound).
Die Mod nutzt davon nur die Funktionen, die Offsets sind Belege.

| Was | Wert |
|---|---|
| `UThreatLevel` (Komponente am GameState, 0x1C0) | `OnThreatLevelChanged` `0xC0`, `OnThreatLevelFreezeChanged` `0xD0`; Stufe ohne Reflexionsdaten bei `0x1B8` (`int32`, beim Client −1) |
| `ThreatLevel.GetCurrentThreatLevel` | native Funktion, Rückgabe `int32` |
| `WBP_Stopwatch_C.CustomEvent` | Blueprint-Ereignis `(int32 Old, int32 New)`, 8 Bytes Parameter, an `OnThreatLevelChanged` gebunden |
| `WBP_Stopwatch_C.ThreatLevel`, `DangerLevel_Bind` | `0x430` (Komponente), `0x3D8` (`FString`, Zahl unter der Uhr) |

## Waffensounds (`opt-waffen-sound`)

Build `26495302`, SDK und Disassembly, siehe [Waffensounds beim Client](GAME_ANALYSIS.md#waffensounds-beim-client-opt-waffen-sound).
Die Mod hängt nur am Event-Wrapper von `PlayReloadSound` (Host), sonst nutzt sie die Felder und reflektierten Funktionen.

| Stelle | Wert |
|---|---|
| `UWeaponData` | `ClipSize` `0x6C`, `ProjectileClass` `0x80`, `ProjectileCount` `0x88`, `WeaponActorToSpawn` `0xB0`, `FireWeaponSoundEvent` `0xB8`, `ReloadWeaponSoundEvent` `0xC0` |
| `AHands` | `DefaultReloadSound` `0x330`, `AutoFire` `0x378` (Net), `CurrentRangedWeapons` `0x3B0`; ohne Reflexion: Feuern aktiv `0x318` |
| `UWeaponInstanceBase` | `WeaponData` `0x40` (ohne Reflexion gelesen in `FireIfPossible`) |
| `AHands::FireIfPossible` | `0x59278C0(Hands)`, Thunk `0x570D150`; startet bei `0x5927FE0` den Schusssound über `0x47B98D0` |
| `AHands::PlayReloadSound` (Event-Wrapper) | `0x5707870(Hands, Override)`, Prolog `48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B FA`; ohne Blueprint-Überschreibung `jmp [vtable+0x740]`, sonst `ProcessEvent`; Aufrufer `0x5A45F5C` (Nachladen der Waffeninstanz, `WeaponData+0xC0` als Override) |
| `AHands::PlayReloadSound_Implementation` | `0x5939AA0(Hands, Override)`, vfunc `+0x740`, Thunk `0x5666D80`; `Override`, sonst `DefaultReloadSound`, am Ort der Root-Komponente |
| `AWeapon::TriggerWeaponShotFX_Implementation` | `0x5B1C760(Weapon)`, vfunc `+0x738` (`BP_Pistol_C`), Thunk `0x5667BE0`; Multicast, unzuverlässig |
| `UFMODBlueprintStatics::PlayEventAtLocation` | nativ `0x47B98D0(WorldContext, Event, const FTransform*, bAutoPlay)`; Parameter der UFunction 0x80 Byte, `Location` `0x10`, `bAutoPlay` `0x70`, Rückgabe `0x78` |

## Item-Fähigkeiten (`client-items-wirkung`)

Felder aus dem SDK beider Builds (`26495302`, `26669225`, gleich), eines ohne Reflexion. Keine Code-Stelle, kein Patch der Exe.

| Stelle | Wert |
|---|---|
| `UInventoryItemData` | `ItemEffectWrappers` `0x118` (`TArray<FItemEffectWrapper>`, Element `0xB0`) |
| `FItemEffectWrapperBase` | `Type` `0x00`, `StandardEffectTemplate` `0x08`, `GameplayEffect` `0x20`, `GameplayAbility` `0x48`, `ActivateGameplayAbilityOnGive` `0x50`, `TriggerableGameplayAbility` `0x51` |
| `FItemEffectContext` | `Character` `0x00`, `Item` `0x08`, `InventoryComponent` `0x18` |
| `UDDGameplayAbility` | `StartupInputTag` `0x3A0` |
| `UGameplayAbility` | `InstancingPolicy` `0xCF`, `NetExecutionPolicy` `0x198` (0 LocalPredicted, 1 LocalOnly, 2 ServerInitiated, 3 ServerOnly), `AbilityTriggers` `0x1A8`; ohne Reflexion: `CurrentActorInfo` `0x328` (Zeiger, auf beiden Builds gemessen) |
| `FGameplayAbilityActorInfo` (`0x48`) | `OwnerActor` `0x08`, `AvatarActor` `0x10`, `AbilitySystemComponent` `0x20` (schwache Zeiger: Index, Serial) |
| `UAbilitySystemComponent` | `OwnerActor` `0x3F8`, `AvatarActor` `0x400`, `ActivatableAbilities.Items` `0x520` (`TArray<FGameplayAbilitySpec>`, Element `0x108`) |
| `FGameplayAbilitySpec` | `Handle` `0x0C`, `Ability` `0x10`, `Level` `0x18`, `ActiveCount` `0x28`, `NonReplicatedInstances` `0x88`, `ReplicatedInstances` `0x98` |
| `DDGameplayAbilityWithState.SetupAbility` | Parameter `0x150`: `ActorInfo` `0x00` (`0x48`), `Spec` `0x48` (`0x108`) |
| `UGA_DelayedHeal_C` | `CachedCharacter` `0x3B0`, `CachedAbilityComponent` `0x3B8`, `CachedCommonAttributes` `0x4E0`, `OldHealth` `0x4E8`, `PulsesRemaining` `0x4F0` |
| `UItemShapeDefinition` | `Tiles` `0x28` (`TArray<FIntVector2>`), genutzt von `!equip` |

## Roboterarm (`client-roboterarm`)

Felder aus dem SDK von Build `26786272`, im Spiel gelesen. Keine Code-Stelle, kein Patch der Exe.

| Stelle | Wert |
|---|---|
| `UDDCharacterAttributeSet` | `ExtraHands` `0x100`, `OccupiedHands` `0x110` (`FGameplayAttributeData`: `BaseValue` `+0x8`, `CurrentValue` `+0xC`), Outer ist der PlayerState |
| `UInventoryItemInstance` | `ItemData` `0xE0`, `Broken` `0x110`, `Rarity` `0x118`, `IsInBackpack` `0x180`, `IsInStash` `0x181`, `IsReward` `0x182` |
| `FItemEffectWrapperBase` | `ForceGameplayEffectLevel` `0x28`, `ForceScalingGameplayEffectLevel` `0x29`, `GameplayEffectLevel` `0x2C`, `ScalingGameplayEffectLevel` (`UFloatData*`) `0x30`; `Type` 1 ist `GameplayEffect` |
| `URarityScalingFloatData` | `RarityMappings` `0x40`, `UsePlayerLevelScaling` `0x50` |
| `UGameplayEffect` | `Modifiers` `0x428` (`TArray<FGameplayModifierInfo>`, Element `0x338`) |
| `FGameplayModifierInfo` | `Attribute.AttributeName` (FString) `0x00`, `ModifierOp` `0x38` (0 additiv), `ModifierMagnitude` `0x40`: `MagnitudeCalculationType` `+0x0` (0 ScalableFloat), `ScalableFloatMagnitude.Value` `+0x8`, `.Curve.CurveTable` `+0x10`, `.Curve.RowName` `+0x18` |
| `UAbilitySystemComponent` | `ActiveGameplayEffects.GameplayEffects_Internal` `0x8D8 + 0x130` (`TArray<FActiveGameplayEffect>`, Element `0x328`, `Spec.Def` `+0x20`, `bIsInhibited` `+0x29C`); beim Client nur für den eigenen PlayerState gefüllt |
| `UCharacterEffectManager` | `EffectedSpecifications` `0xC0`, `EffectedBackpackSpecifications` `0xD0` (`FEffectedSpecification`, Element `0xF0`: `ItemInstance` `0x00`, `EffectWrapper` `0x08`, `EffectedItem` `0xB8`), `Character` `0xE0` |
| `APlayerState` | `PlayerId` `0x2AC`, `PawnPrivate` `0x320` |
| `UHandBarDisplay` | `CachedCharacter` `0x348`; `UpdateCurrentHandsBar(int32)`, `UpdateMaxHandsBar(int32)` |
| `InventoryEffectHelperLibrary.GetFreeHands` | Exec-Thunk `0x57105B0`, Implementierung `0x5968520` (liest `ExtraHands` `+0x100` und `OccupiedHands` `+0x110` des Attributsets), weiterer direkter Aufrufer `0x5A40C2E`; Parameter `0x10`: `Character` `0x00`, `ReturnValue` (int32) `0x08` |
| `FloatData.GetContextualValueForRarity` | Parameter `0x10`: `Rarity` `0x00`, `PlayerLevel` `0x08`, `ReturnValue` (float) `0x0C` |
| `DataTableFunctionLibrary.EvaluateCurveTableRow` | Parameter `0x30`: `CurveTable` `0x00`, `RowName` `0x08`, `InXY` `0x10`, `OutResult` `0x14` (0 gefunden), `OutXY` `0x18`, `ContextString` `0x20` |

## Zeitanfragen und Weltzeit (`boss-client-einfrieren`)

Build `26495302`, Disassembly und Speicherlesen am 19.11.2026. Ablauf siehe
[Stillstand des Clients nach dem Bosstod](GAME_ANALYSIS.md#stillstand-des-clients-nach-dem-bosstod-boss-client-einfrieren). Die Mod
ruft `ClearAllRequests` und liest die Felder über Namen (Reflection), die RVAs dienen nur der Analyse.

| Funktion | RVA | Hinweis |
|---|---|---|
| `UTimescaleManager::RemoveRequest` | `0x46A5920` | Exec-Thunk `0x46A4BA0`. Entfernt den Eintrag; ist die Liste danach leer (`+0x50` gleich `+0x7C`), setzt es `ActiveRequester` auf null und wendet die Standardanfrage `+0x9C` an. Sonst Neuwahl, aber nur, wenn der Entfernte der aktive Anfragende war |
| Neuwahl der aktiven Anfrage | `0x46A6160` | `ActiveRequester = null`, beste Anfrage suchen, ohne Treffer Rückkehr ohne Anwenden |
| Beste gültige Anfrage suchen | `0x46A5F70` | Prüft je Eintrag den Weak-Zeiger (`0x1643700`), entfernt ungültige, sonst höchste Priorität (`+0x0C` im Element) |
| Anfrage anwenden | `0x46A62C0` | Faktor zwischen `AWorldSettings` `+0x3EC` und `+0x3F0` begrenzen, Zeitfaktor setzen (`0x2A8D080`), bei Pause oder kleinstem Faktor die Pause setzen (`0x2A8CFF0`), Zustand in `+0x98` |
| `UTimescaleManager::ClearAllRequests` | Exec-Thunk `0x46A4A30` | ruft `0x46A5520` |
| `UTimescaleManager::RequestTimescale` | Exec-Thunk `0x46A4C30` | **[Vermutung]** native Funktion `0x46A5B80` (liegt direkt hinter `RemoveRequest`, vergleicht die Priorität mit der aktiven Anfrage) |

| Klasse | Feld | Offset |
|---|---|---|
| `UTimescaleManager` | ActiveRequester, TimeRequests (Daten, Plätze), Belegungsbits (inline, ausgelagert), freie Plätze | `0x40`, `0x48`, `0x50`, `0x58`, `0x68`, `0x7C` |
| `UTimescaleManager` | Pausenzustand, Standardanfrage (`FTimeRequest`) | `0x98`, `0x9C` |
| Element von `TimeRequests` | Weak-Zeiger (Objektindex, Seriennummer), Faktor, Priorität, Pause | `0x00`, `0x04`, `0x08`, `0x0C`, `0x10` (Größe `0x1C`) |
| `UWorld` | TimeSeconds, UnpausedTimeSeconds, RealTimeSeconds, AudioTimeSeconds | `0x858`, `0x860`, `0x868`, `0x870` (`double`). **[Vermutung]** Zuordnung nach der üblichen Reihenfolge und dem Verhalten: Bei pausierter Welt steht `0x858`, `0x868` läuft immer. Auf `26669225` dieselben Offsets gemessen |
| `UObject` | Index im Objekt-Array | `0x0C` (`int32`), damit löst `pause_state.py` den Weak-Zeiger auf, ohne die Seriennummer zu prüfen |

Die Felder `0x48`, `0x50`, `0x58`, `0x68` und `0x7C` gelten nach der Kontrolle am Rundenende (je zwei Anfragen, `GS_DDMissionState_C`
und `WP_Round_Stats`) auch auf `26669225`. Ein verwaister Eintrag erscheint in `pause_state.py` mit dem Namen des Objekts, das den
Platz im Objekt-Array inzwischen belegt (gesehen: eine `Function`, ein `BorderSlot`).

## Boss-Leiste aller Bosse (`opt-endboss-leiste`)

Per Reflexion im laufenden Spiel gelesen, auf beiden Builds (`26495302`, `26669225`) gleich. Die Mod nutzt davon nur die bisherigen Felder
aus `gegner-alle` (oben); der Rest ist Befund für `tools/spike/boss_hp.py` und spätere Arbeit an der Phasenanzeige.

| Stelle | Wert |
|---|---|
| `UDDObjective` | `OnProgressChanged` `0x28`, `TargetValue` `0x68`, `CurrentValue` `0x70` (int64), `CachedWorld` `0x78` |
| `UDDBossObjective` | `TrackedBosses` `0xA0`, `WeakBossComponent` `0xB0`, `PhaseMarkers` `0xB8` (`TArray<float>`, Anteil am Maximum), `bUseAlternateBar` `0xC8`, `MaxHealth` `0xD0`; UFunction `OnPhaseChanged(LastPhase, NewPhase)` |
| `UDDObjectiveSubsystem` | `CurrentObjective` `0x60`, `ObjectiveInstances` `0x68`, `ObjectiveTypeToInstanceMap` `0x78` |
| `UBossComponent` | `OnNextPhase` `0xC0`, `HealthCeilValue` `0xE0`, `EnemyBase` `0x118`, `BossAIController` `0x180` (nur Host), `PhasePercentages` `0x220` (`TArray<float>`) |
| `UDDEnemyAttributeSet` | `Phase` `0x40` (`FGameplayAttributeData`, Wert bei `+0xC`) |
| Phasenmarken je Boss | SpikeBoss 0,5; Ballerina 0,5; Frankenstein 0,65; BlobBall 0,7; HQ-Endboss 0,8 und 0,4 |
| `CreateAndStartObjective` (Thunk) | `26669225`: `0x56BAB70`, ruft `0x58DED20`, das ruft `0x163B300` mit einem `FStaticConstructObjectParameters`-artigen Block (Klasse bei `+0x0`, Outer bei `+0x8`); dort liest `+0x163B33F` `[Klasse+0xD8]`. **[Vermutung]** `StaticConstructObject_Internal` und `ClassFlags`, ohne Symbole nicht belegt |
