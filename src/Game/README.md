# Game

Einzige Schicht mit Spielzugriff. Enthält Adapter auf Spielklassen und später die Hook-Definitionen, je Spielsystem eine Datei
(zum Beispiel `PlayerAdapter`, `EnemyAdapter`, `InventoryAdapter`). Bei einem Spiel-Update wird vor allem dieser Ordner angepasst. Darf `Core` nutzen.

## Bausteine

| Datei | Aufgabe |
|---|---|
| `Game.h` | `Game::Start` registriert die Adapter-Ticks und startet den Takt, aufgerufen von `Mod.cpp` |
| `GameThread.h` | Takt auf dem Game Thread, siehe unten |
| `Offsets.h` | Alle Offsets und RVAs als Konstanten, Herkunft in [docs/OFFSETS.md](../../docs/OFFSETS.md) |
| `Memory.h` | Geschützte Lesezugriffe (`Read<T>`, `ReadPointer`), Schreiben von Werten (`WriteBytes`) und Objektnamen (`NameOf`) |
| `SdkChecks.cpp` | Prüft `Offsets.h` beim Bauen gegen das Dumper-7-SDK, ohne SDK leer |
| `WorldAdapter.h` | Muster-Adapter: Karte, Rolle (allein, Host, Client) und Spielerzahl |
| `DesyncAdapter.h` / `DesyncSnapshot.h` | Lesende Diagnosebilder, kanonische HP-/Inventar-Hashes, explizit unbekannte Felder; [Messumfang und offene Abnahme](../../docs/DESYNC.md) |
| `Objects.h` | Suche im Objekt-Array (`FindInstance`, `FindObject`, `IsA`) und Aufruf über `ProcessEvent` |
| `TutorialAdapter.h` | Schaltet die Tutorial-Hinweise als Host und Client ab, Prüfung einmal je Sekunde |
| `TelemetryAdapter.h` | Schaltet im Mod-Betrieb Balance-Telemetrie und automatische Berichte über beschädigte Spielstände ab und sperrt die Telemetrie-Adresse über `Core::ReportHosts` |
| `Patches.h` | Die Spielfehler-Fixes für eine Mission zu zweit, siehe [Patches](#patches) |
| `CodeCave.h` | Werkzeuge für Patches mitten in Funktionen: Code-Höhle nahe der Exe, Bytes prüfen, Sprünge und vtable-Einträge schreiben |
| `RemoteP2Policy.h` | Reine Logik für den Remote-Spieler als P2: `ActivePlayers`-Map prüfen und aufräumen, Geräte-Wächter, ohne Spiel getestet |
| `PatchPolicy.h` | Reine Regeln der Patches (SpawnedPlayers-Menge, Wächter-Code, Reise, Busplatz), ohne Spiel getestet |

## Game Thread

`GameThread::Start` sucht im Hintergrund das `UnrealWindow` des eigenen Prozesses, hängt sich in dessen Fensterprozedur und setzt dort einen
Timer (16 ms). Unreal pumpt diese Nachrichten auf dem Game Thread zwischen zwei Frames. In jedem Takt laufen erst
`Core::GameThread().Drain()`, dann die mit `AddTick` registrierten Ticks. Ein Tick, der selbst Nachrichten pumpt, löst keinen zweiten aus.
Mit Host und Client im Spiel geprüft, auch über den Wechsel auf Tag 1. Der Spike hängt sich an dasselbe Fenster, beide laufen nebeneinander.

## SDK

Das Dumper-7-SDK liegt nie im Repo. Ist `Dumper7SdkRoot` in `config/Local.props` gesetzt und vorhanden, definiert `Game.vcxproj`
`DDCOOP_WITH_SDK` und bindet die SDK-Header als externe Includes ohne Warnungen ein. Genutzt werden nur Header, keine SDK-Quellen.
`SdkChecks.cpp` vergleicht dann jeden Wert aus `Offsets.h` per `static_assert` mit `offsetof` im SDK. Ein falscher Wert bricht den Build.
Die CI und Mitwirkende ohne SDK bauen dieselbe Mod, nur ohne diese Prüfung. Beim Start steht im Log, welcher Fall gilt.

Warum keine SDK-Typen im Laufzeitcode: Die Mod muss überall bauen, auch in der CI. Und gegen ein SDK eines anderen Builds würde sie still
falsche Layouts verwenden, statt laut zu scheitern.

## Muster für neue Adapter

1. Header ohne SDK- und Windows-Typen, mit einfachen Structs wie `WorldInfo`. So können `Sync` und Tests ihn nutzen.
2. Neue Offsets in `Offsets.h` eintragen, in `docs/OFFSETS.md` belegen und in `SdkChecks.cpp` gegen das SDK prüfen.
3. Spielspeicher nur über `Memory::Read`/`ReadPointer` lesen. Ein Lesefehler liefert `nullopt`, nie einen Absturz.
4. Nur auf dem Game Thread aufrufen. Lesefunktionen prüfen `Core::GameThread().IsDrainThread()`. Andere Threads reichen Arbeit mit
   `Core::GameThread().Post` herein.
5. Laufende Beobachtung als Tick über `GameThread::AddTick` in `Game::Start` registrieren. Logs nur bei Änderungen, nicht in jedem Takt.
6. Reine Logik (wie `RoleFrom`) als freie Funktion herausziehen und in `tests/GameTests` ohne Spiel testen.

Schreibende Zugriffe und Hooks folgen demselben Muster, siehe [Patches](#patches).

## Patches

Die Fixes, ohne die keine Mission zu zweit läuft, kommen aus dem Spike (Aufgabe `patches`). Herkunft und Belege jeder Stelle stehen in
der [Patch-Tabelle des Spikes](../../research/spikes/a-connect/README.md#patches). Die RVAs stehen als Konstanten am Anfang der jeweiligen Datei.

| Datei | Seite | Inhalt |
|---|---|---|
| `PlayerPatches.cpp` | Host | Spawn und Spielerindex ohne `LocalPlayer`, Loot- und Gegner-Spawner ohne Spieler, `SpawnedPlayers` bei Registrierung und Controller-Tausch, Abgleich je Sekunde als Rückfall, Client-Position annehmen, Busplatz schont alle Spielerfiguren, Run-Ende ohne `LocalPlayer` |
| `ClientPatches.cpp` | Client | Refresh, Attribut-Tick, AttributeSet-Suche, Kartengenerator-Schritt, Figurenkomponente und ThreatLevel ohne Host-Objekte; Exp-Aggregation wartet auf AttributeSets; Blockaden nur beim Host; NetGUID-Pfade bleiben; SneakApproach wartet auf Mesh und PlayerState; keine lokale PowerUp-Kopie |
| `MovementPatches.cpp` | Client | Nach dem Dodge: eigene Figur bekommt Reibung und Schwerkraft zurück, fremde Figur folgt dem replizierten Ort, wenn ihre Unterlage beim Client fehlt oder sie ohne Modus hängt, siehe [Dodge beim Client](../../docs/GAME_ANALYSIS.md#dodge-beim-client-bewegung) |
| `CameraPatches.cpp` | Client | Kamera-Spieler aus der `PlayerArray` statt aus dem GameMode, Bodenfix, gemeinsame Kamera als ViewTarget je Sekunde; Bewegungs-Restrictor ohne GameState stürzt nicht ab und bekommt ihn nachgetragen, siehe [Restrictor](../../docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) |
| `InventoryPatches.cpp` | beide | Inventar-Unterobjekte über den klassischen Replikationspfad, Timer des Achievement-Detektors beim `EndPlay` löschen, Kartengenerator-Schritt nur für die aktuelle Welt; [Itemsack-Vergabe](../../research/spikes/itemsack-absturz/README.md) nur auf Authority außerhalb einer Client-Welt, beim Host [für alle Spieler](#itemsack-für-alle-spieler); abschaltbare Inventar-Diagnose in `RewardPatches.cpp` |
| `MapPatches.cpp` | beide | Gleiche Karte: Der Host meldet Seed, Profil, Größe, Tags und PCG-Graph seiner Erzeugung, der Kontrollkanal bringt sie zum Client (`Sync/World/MapSync.cpp`), dessen Erzeugung wartet darauf; lokale Kopien replizierter Child Actors unterdrückt, gleiche Seeds der PCG-Instanzen. Siehe [Gleiche Karte in der Mod](../../docs/GAME_ANALYSIS.md#gleiche-karte-in-der-mod-missionsstart) |
| `TravelPatches.cpp` | Host | Reise ins Spiel wird `servertravel`, vorher Zeitanfragen abräumen; allein und ins Hauptmenü reist das Spiel wie gewohnt |
| `WidgetPatches.cpp` | Client | `WP_UI` wartet auf den GameState; Beobachter nach jedem `ProcessEvent` |
| `RemoteP2Patches.cpp` | Host | Remote-Spieler als P2 in `ActivePlayers` des Coop-Subsystems, ohne zweiten `LocalPlayer`; beim Verlassen nur die eigene Bindung entfernen; lokale P2-Gerätepflicht nur bei vollständiger Bindung ausnehmen. Koop-Modus erst nach `AllowRemoteCoop()`, siehe [Remote-Spieler](#remote-spieler-als-p2) |
| `HealthBarPatches.cpp` | Client | Lebensleisten nach repliziertem `OnRep_Health` und `OnRep_MaxHealth` |

**Alle oder keiner.** `Patches::Install` läuft in `Game::Start` vor dem ersten Takt. Es prüft zuerst jede Stelle (Bytes, vtable-Einträge)
und setzt erst dann alle Patches. Passt eine Stelle nicht, setzt es keinen und protokolliert die Stelle; das Spiel läuft dann wie ohne Mod.
Ganze Funktionen hängt `Core::Hooks` (MinHook) ein, Stellen mitten in einer Funktion springen in Code-Höhlen aus `CodeCave.h`.
Was Spielobjekte braucht (`Default__DDMissionGameMode`, Funktionsnamen), holt `Patches::Tick` auf dem Game Thread nach.

**Mod und Spike.** Die Mod exportiert `DDCoopPatchState` (0 offen, 1 alle gesetzt, 2 keiner, 3 ein Teil). Findet der Spike die Mod mit 1 oder 3,
setzt er seine eigenen Fassungen nicht und meldet seinen Phasenbeobachter über `DDCoopObserveProcessEvent` an. Seit `missionsstart`
gilt das auch für Karte und PCG (`MapGen.cpp`, `Pcg.cpp`). Testhilfen und Messung bleiben im Spike.

### Remote-Spieler als P2

Aus der Spike-Probe [`remote-p2`](../../research/spikes/a-connect/RemoteP2.md), Aufgabe `remote-spieler`. Der Host trägt in jedem Takt den
aktuellen lokalen und den Remote-Controller als Schlüssel 0 und 1 in `ULocalCoopSubsystem::ActivePlayers` ein, mit dem nativen Map-Add und
schwachen Verweisen. Danach liefert `GetPlayerController(1)` den Remote-Controller und `GetActivePlayerCount()` 2. Nur beim Listen-Host mit
genau einem `LocalPlayer` und höchstens zwei aktuellen Controllern, sonst ändert die Mod nichts. `Patches::RemoteP2Bound()` meldet den Zustand.

**Koop-Modus bleibt aus.** `SetCoopEnabled(true)` und die native Beitrittsmeldung `OnPlayerJoined(1, …)` laufen erst, wenn
`Patches::AllowRemoteCoop()` gerufen wurde. Mit Koop-Modus wartet das Lager am Tagesende auf „Bestätigen & Wechseln“ von P2 und hängt, und
die Airdrop-Auswahl wartet auf den Claim von P2. Das lösen `airdrop-auswahl` und `inventar-ui` über den Kontrollkanal, dann schalten sie
den Modus ein. Zurückschalten ist nicht vorgesehen: `SetCoopEnabled(false)` entfernt laut Analyse vorher einen vorhandenen P2.

**Geprüft** (05.11.2026, Build `26187885`, `Start-Session.ps1 -Build -Mod -Tunnel Lobby`, Seed `20795`, ein PC):

- Ohne Koop-Modus: In MetaBase und an Tag 1 bis 3 nach jedem Wechsel `p1=true p2=true aktiv=2`. Lager, Routenkarte und ein Level-up-Airdrop,
  vom Nutzer eingesammelt, liefen wie ohne Mod. Inventare gleich, kein Absturz.
- Mit Koop-Modus (Gegenprobe): Grüne P2-Markierung über der Client-Figur, kein Controller-Fenster. `Test-NetEnd.ps1 -Cases b,d`: Bindung
  beim Verlassen und beim harten Abbruch entfernt (`aktiv=1`, `reconnect=0`), nach dem Neubeitritt neu gesetzt. Das Lager am Ende von
  Tag 1 hing mit beiden Rucksäcken bei „Bestätigen & Wechseln“, deshalb der Standard aus.

**Geprüft** (04.11.2026, Build `26187885`, `Start-Session.ps1 -Build -Mod`, Seed `20795`): MetaBase, dann Tag 1 bis 5 über die echte Route,
danach Niederlage und „Zurück zur Basis“. Kein Absturz, die Inventare waren nach jedem Wechsel gleich. `SpawnedPlayers` kam bei jedem Wechsel
über Registrierung und Tausch in Ordnung, der Abgleich je Sekunde hatte nichts zu tun. Gegriffen haben im Log: Reise, Zeitanfragen, Busplatz,
Controller-Tausch, Client-Position, Kamera, PowerUp, Lebensleisten. Die übrigen Schutzstellen (HUD, Exp, Blockade, alter Kartengenerator)
wurden in diesem Lauf nicht gebraucht; ihre Logik ist mit `tests/GameTests/PatchTests.cpp` und aus den Spike-Läufen belegt.

### Airdrop-Auswahl

Aufgabe `airdrop-auswahl`, Code in `RewardPatches.cpp` und `src/Sync/Inventory/RewardSync.cpp`, Format in
[Kontrollkanal](../../docs/CONTROL_PROTOCOL.md#airdrop-auswahl-mod-kompatibilität-3). Der Koop-Modus bleibt aus, jeder wählt im
normalen Einzelspieler-Panel.

- **Host:** `RewardChest.OpenAirdropChoices` gibt eine vom Remote-Spieler geöffnete Kiste an die Figur des Hosts (`RewardCharacter`).
  Der Hook auf `InventoryComponent.SetRewardItems` liest die Choices mit, `RewardSync` schickt sie als Sitzungsbild. Drückt der Host
  „Bestätigen“, hält der Hook auf `InventoryPanel.HandleConfirmButton` das Bestätigen zurück, bis der Client gewählt hat. Die Wahl des
  Clients legt der Host mit `CreateAndAddItem` in dessen Inventar, mit der gemeldeten Ablage (`0x302`) per `AddToBackpack` und
  `OccupyBackpackSlots` auf dieselben Rucksackfelder.
- **Client:** Die Mod legt lokale Anzeige-Instanzen über den nativen `CreateItemInstance` an (die UFunction ist `BlueprintAuthorityOnly`,
  über `ProcessEvent` liefert sie beim Client nichts) und öffnet das native Panel mit `SetRewardItems`. Inventar-RPCs mit diesen Instanzen
  hält `FilterRewardEvent` zurück; das erste Ablegen einer Choice gilt als Wahl, die lokalen Instanzen fliegen direkt aus `MyItems`.
- **Reward-Queue:** Kisten laufen über die `RewardQueue` der öffnenden Figur, das Panel schaltet aber nur die Queue des Hosts weiter.
  Ohne Eingriff blieb die Queue von P2 nach einer vom Client geöffneten Kiste belegt, danach ging keine Kiste mehr auf. Die Mod ruft
  beim Host ohne offene Auswahl jede Sekunde `OpenNextReward` für P2 auf.

**Geprüft** (05.11.2026, ein PC, Lobby-Tunnel, Tag 1, vom Nutzer eingesammelt): Beide sehen ihre Auswahl, ob Host oder Client die Kiste
öffnet; mehrere Airdrops nacheinander öffnen sich wieder; ohne Absturz, Inventare gleich. Wählt der Client zuerst, wartet das Spiel auf
den Host. Bestätigt der Host zuerst, bleibt das Spiel bis zur Wahl des Clients pausiert; das nachgeholte Bestätigen wartet einen Drag ab und wird wiederholt, bis das Panel schließt (`airdrop-warten`). Die Wahl des
Clients landet im Lager statt im Rucksack: Er merkt sich die Ablage, sie kommt aber nicht beim Host an (`airdrop-rucksack`).

**Nachprüfung `airdrop-rucksack` (05.11.2026).** `TickClient` konsumierte die Ablage vor dem
Senden der Wahl, auch wenn noch keine Sitzungskennung vorlag. Jetzt wird zuerst die Wahl gesendet,
danach nur die Ablage derselben bestätigten Sitzung; fehlgeschlagene Sendeversuche behalten die Nachricht.
Die Game-Schicht bindet die gemerkte lokale Instanz zusätzlich an die Sitzung.

Im laufenden Tag-1-Test wurden die ersten zwei Choices beim Host tatsächlich in den Rucksack gelegt.
Der dritte Versuch scheiterte an einem für den Host noch belegten Feld `(1, 1)`. Screenshot und
Inventarvergleich bestätigten Client-Geisterdarstellung sowie einen abweichenden Pivot des vorher
verschobenen Revolvers. Ursache im Auswahl-Lebenszyklus: `AddToInventory` läuft schon beim
Drag-Beginn. Die Mod bestätigte daraufhin automatisch und entfernte lokale Instanzen, obwohl
das native Panel `HandleConfirmButton` mit gesetztem `IsDragging` (`+0x790`) sofort abbricht.
Die Entfernung lief damit während eines noch aktiven Drags.

Die automatische Bestätigung ist entfernt: Erst der echte Bestätigen-Klick veröffentlicht die
Wahl. Während eines Drags wird auch die Mod-Bestätigung verworfen. Die Host-Ablage nutzt die
nativen Setter für Drehung und Pivot statt direkter Feldschreibzugriffe. **Noch nicht live geprüft:**
dieser zweite Fix, erneutes Umsortieren und die Wirkung des gewählten Items im Kampf. Die Abnahme
bleibt offen; laufende Nutzerinstanzen wurden nicht ungefragt neu gestartet. Ein unabhängiger
Upstream-Sync der Pivot-/Drehungswerte bereits vorhandener Items ist hiermit nicht nachgewiesen.

**Zweite Gegenprobe:** Der Nutzer bestätigte die Wirkung des Revolvers im Kampf. Nach dem ersten
Airdrop meldeten beide Desync-Messungen wieder vollständige Gleichheit. Beim Öffnen des zweiten
Airdrops fehlte der Revolver dagegen im Client-Rucksackbild. Lesender Vergleich im offenen Panel:
Item, `IsInBackpack`, Pivot `(0, 1)` und beide belegten Slots waren korrekt vorhanden. Die echte
replizierte Instanz hatte aber `CachedBackpackLocalPosition=(0, 0)`, die lokale Anzeigeinstanz der
ersten Wahl `(0, 66)`. Dieser Pixelpositions-Cache ist nicht repliziert. `SetupInventory` verwendet
ihn zum Zeichnen; damit überlagert die Pistole den Revolver. `SetZeroPivotTile` schreibt nativ nur
das Pivot-Feld und repariert den Cache entgegen der ersten Annahme nicht.

Der Client-Hook nach `SetupInventory` richtet deshalb echte ausgerüstete Items mit vorhandener
Repräsentation über das native `SnapRepresentationToTile` aus ihren replizierten Pivot-Feldern aus.
Das setzt UI und Cache gemeinsam, ohne Slots, Itembesitz, Effekte oder Host-Inventar zu verändern.
Lokale Reward-Choices, fremde Inventare und laufende Drags bleiben unberührt. Sichtbare Gegenprobe
dieses zusätzlichen UI-Fixes steht noch aus. Der erste geführte Test wurde mit der gemeldeten
Abweichung beendet, nicht als bestanden gewertet.

Build und automatisierte Tests nach dem zusätzlichen UI-Hook bestanden: Core 36, UI 15,
Game 28, Net 62 und Python 124; auch Diff- und Aufgabenstatus-Prüfung sind sauber.
Das belegt noch nicht die native Anzeige im erneut geöffneten Airdrop-Panel.

**Dritte Gegenprobe:** Der Nutzer bestätigt den jetzt sichtbaren Revolver beim zweiten Airdrop,
meldet aber fehlende Nutzung im Kampf sowie abweichende/neue Equipmentfelder. Lesende Messung
im offenen zweiten Panel: Die Host-Waffeninstanz des Client-Inventars besitzt einen WeaponActor;
ihre nativen Zähler stehen auf 6 Angriffen und 5 Treffern. Das belegt bisherige hostseitige Nutzung,
nicht die Sichtbarkeit beim Client oder fortgesetzte Nutzung nach dem zweiten Panel. Die Getter
`WeaponInstanceBase.GetTimesAttacked/GetTimesHit` lesen `+0xB8/+0xBC`.

Die acht aktuellen Slot-Koordinaten und ihre Belegung für den Client-Spieler stimmen auf Host
und Client überein. `InventoryComponent.PlacedBagWrappers` (`+0x150`) ist dagegen nicht repliziert:
Host-Clientinventar 8 Wrapper, lokales Clientinventar 1 Wrapper; `HasCreatedDefaultBackpack`
(`+0x148`) ist beim Host gesetzt, beim Client nicht. Die Hintergrunddarstellung ist daher nicht
mit der logischen Feldmenge gleichzusetzen. Ein frischer Bag-Drop mit anschließendem Vergleich
der neu angelegten Slots und ihrer Nutzbarkeit steht noch aus. Nicht als rein optisch abgenommen.

**Rückmeldung und Feld-Gegenprobe:** Der Nutzer korrigiert die Waffenmeldung: Der Revolver wird
im Kampf korrekt angezeigt. Kein weiterer Waffen-Fix aus diesem Bericht nötig. Neue Equipmentfelder
bleiben fehlerhaft. Im offenen fünften Airdrop hat dasselbe Client-Inventar auf dem Host 17 Slots,
auf dem Client 14; die zusätzlichen Host-Koordinaten sind `(3, -1)`, `(2, 0)`, `(3, 0)`.
Die Auswahl ist dabei noch nicht bestätigt: Ob diese Differenz danach aufgelöst wird, ist offen.
Sitzung 2 vergab einen Rucksack als normales Lager-Item ohne Placement-Nachricht. Die Feldanlage
läuft separat über `Server_CreateSlotsFromBagItem`, nicht über den sitzungsgebundenen Wahl-/Ablagepfad.
Damit fehlt eine gemeinsame Transaktion für Bag-Verbrauch, Feldanlage und bestätigtes Client-Bild.
Weitere reine Pixelkorrekturen schließen diese Lücke nicht. Zielgrenze: lokale UI/Vorschau,
ein zusammengehöriger bestätigter Inventarvorgang mit stabilen Item-Identitäten und anschließender
Zustandsabstimmung; Effekte und Vergabe bleiben hostautoritativ. Das ist noch kein implementierter Fix.

**Verworfen:** Zuerst nutzte die Mod die native Koop-Reward-Sitzung (Koop-Modus nur für den Airdrop, Client wählt im Overlay, Host ruft
`ClaimReward(1, …)`). Das Host-Panel zeigte beide Rucksäcke, nach dem Claim des Clients ging die UI-Autorität an P1 und der Host hing.
Die Auswahl im Overlay gefiel nicht, der Nutzer wollte für beide das normale Panel.

### Itemsack für alle Spieler

Nutzervorgabe 06.11.2026: Ein aufgesammelter Itemsack zählt wie ein Airdrop für alle, jeder Spieler bekommt das Item. Die Vergabe bleibt
beim Host, der Client lässt seine lokale Vergabe weiter aus. `ItemLootHook` (`InventoryPatches.cpp`) merkt sich beim Host vor der nativen
Vergabe die Items aller Spielerinventare (über die `PlayerArray` und `UInventoryComponent.OwningPlayerState`, `+0x228`). Danach bekommt
jedes andere Inventar eine Kopie jedes neuen Items über `CreateAndAddItem`, mit derselben `ItemData`, Seltenheit und Beschädigung wie das
Original, wie `ClaimForClient` bei der Airdrop-Auswahl. Das Item repliziert nativ. Nur wenn genau ein Inventar etwas bekommen hat
(`ShareItemLoot` in `PatchPolicy.h`, mit Tests), damit nichts doppelt verteilt wird.

**Geprüft** (06.11.2026, Build `26187885`, Lobby, ein PC, Godmode): Host sammelte einen Rucksack-Block, der Client Rollerblades, beide
landeten bei beiden Spielern (`Itemsack: … auch an Spieler i vergeben, angelegt=true`). Im Lager nach der Busflucht lieferte
`inventory.py` für Host- und Client-Sicht `complete` und `equal`. Über die Reise in den nächsten Tag ist es nicht geprüft.

**Diagnose für `inventar-lokal`:** Mit der Umgebungsvariable `DDCOOP_TRACE_INVENTORY=1` schreibt die Mod jeden `ProcessEvent`-Aufruf an
Inventar, Item, Rucksackfeld, Reward-Queue und Inventar-Widgets als `InvTrace C|H Klasse.Funktion [Net]` ins Log, ohne Animations- und
Abfrageaufrufe, höchstens 20000 Zeilen. Ohne die Variable kostet sie nur einen Vergleich je Aufruf. Erste Spur: Schon beim Öffnen der
Airdrop-Auswahl ruft der Client an jeder Choice `UpdateIsReward` als Server-RPC.

## Ohne Spike

Im Release gibt es keinen Spike. Stand `ohne-spike` (05.11.2026, Build `26187885`): Die Mod läuft allein, Lobby-Beitritt, MetaBase und
Mission an Tag 1 ohne Absturz, geprüft mit `Start-Session.ps1 -NoSpike` und `Run-Route.ps1 -Start -NoSpike -Days 1 -StayLast`,
siehe [Nur Mod, ohne Spike](../../docs/TESTING.md#nur-mod-ohne-spike). Der Absturz beim Beitritt ohne Spike (Host `0x5901CE8`,
Client `0x56B16D9`) stammte aus der Zeit vor `patches`, heute setzt die Mod beide Fixes beim Spielstart.

**Für den Betrieb nötig, aber nur im Spike.** Das läuft dort über Dateien in `%TEMP%\ddspike`, also nur auf einem PC. In die Mod
gehört es mit Übertragung über den [Kontrollkanal](../../docs/CONTROL_PROTOCOL.md), das ist Arbeit der genannten Aufgaben, nicht ein
Kopieren des Spike-Codes.

| Spike-Datei | Eingriff | Ohne ihn | Aufgabe |
|---|---|---|---|
| `Mission.cpp`, `RoundPhase*.h`, Beobachter in `Widgets.cpp` | Rundenübersicht, Lager und Routenkarte des Hosts beim Client mit Anzeige und Eingabesperre | Client bleibt in der Missionsansicht, während der Host im Lager oder auf der Routenkarte ist | `pausen-sync`, `tagesablauf` |

**Schon in der Mod**, der Spike lässt seine Fassung aus, sobald `DDCoopPatchState` 1 meldet: `Players.cpp`, `Client.cpp`, `Camera.cpp`,
`Inventory.cpp`, `Travel.cpp`, `Widgets.cpp` (HUD wartet auf den GameState), `MapObjects.cpp` (PowerUp-Kiste), Busplatz aus `TestEnv.cpp`,
`HealthBars.cpp`, `RunEnd.cpp`, `MapGen.cpp` und `Pcg.cpp` (Werte über den Kontrollkanal statt über Dateien). Dazu ohne Gegenstück im Spike-Schalter: Sperre des Crash-Reporters (`Core/CrashReports`) und
Tutorial-Hinweise (`TutorialAdapter`, im Spike `Cheats.cpp`). `Tunnel.cpp` ersetzt der Transport aus `src/Net`.

**Nur Testhilfe oder Messung**, kommt nicht in die Mod: Befehlsdatei und Konsolenbefehle (`Main.cpp`, etwa `open`, `servertravel`,
`DD_SetCurrentRunSeed`; Ersatz mit Antworten ist `mod-steuerung`), `Cheats.cpp` (CheatManager, `!load`), Godmode, `!finish` und `!round`
aus `Mission.cpp`, `!wave`, `Trace.cpp`, `Load.cpp`, `Hits.cpp`, Diagnose-Log aus `TestEnv.cpp`, die `RemoteP2.cpp`-Probe und die
zweite Sperre des Crash-Reporters in `CrashReports.cpp`.

**Geprüft** (05.11.2026, ein PC, zwei Läufe): `spike.dll` war in keiner Instanz geladen, `DDCoop.dll` in beiden, je 30 Patches gesetzt.
Lobby-Beitritt in rund 4 s, beide in der MetaBase mit 2 Spielern. Im ersten Lauf ging der Host über `Game-Input.ps1` und den Startring in die Mission,
im zweiten über `Run-Route.ps1`. In beiden Läufen reisten beide Instanzen nach `DDDynamic_Map`. Im Mod-Log griffen Reise, Busplatz,
Controller-Tausch, Client-Position, Kamera und PowerUp. Bewegung des Clients kam beim Host an (270 Einheiten in 1,5 s, gleiche Position
auf beiden Seiten). Im zweiten Lauf starben beide Figuren ohne Eingabe nach gut 3 Minuten. Der Host zeigte „Du bist gestorben“, der
Client blieb in der Missionsansicht (fehlende Rundenphase, siehe oben). Nach „Zurück zur Basis“ waren beide wieder in der MetaBase. Kein
Absturz, keine Absturzberichte. Nicht geprüft: die Knotensuche auf der
Routenkarte, weil beide Test-Spielstände ohne Routenkarte in den ersten Run fuhren.
