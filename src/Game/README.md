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
| `Reflection.h` | Löst Felder (`FProperty`: Offset, Größe, Typ) und Funktionen (`UFunction`) von Spielklassen zur Laufzeit über die Engine-Reflexion auf, einmal je Start gecacht |
| `WorldAdapter.h` | Muster-Adapter: Karte, Rolle (allein, Host, Client) und Spielerzahl |
| `DesyncAdapter.h` / `DesyncSnapshot.h` | Lesende Diagnosebilder, kanonische HP-/Inventar-Hashes, explizit unbekannte Felder; [Messumfang und offene Abnahme](../../docs/DESYNC.md) |
| `Objects.h` | Suche im Objekt-Array (`FindInstance`, `FindObject`, `IsA`) und Aufruf über `ProcessEvent` |
| `TutorialAdapter.h` | Schaltet die Tutorial-Hinweise als Host und Client ab, Prüfung einmal je Sekunde |
| `TelemetryAdapter.h` | Schaltet im Mod-Betrieb Balance-Telemetrie und automatische Berichte über beschädigte Spielstände ab und sperrt die Telemetrie-Adresse über `Core::ReportHosts` |
| `SaveRedirect.h` / `SavePaths.h` | Trennt die Spielstände: Datei-APIs von Windows leiten alles unter `Saved\SaveGames` nach `Saved\SaveGames_DDCoop` um, ohne Spiel-Offsets; reine Pfadlogik in `SavePaths.h`, ohne Spiel getestet. Siehe [Spielstände](../../docs/GAME_ANALYSIS.md#spielstände-der-mod-mod-saves) |
| `Patches.h` | Die Spielfehler-Fixes für eine Mission zu zweit, siehe [Patches](#patches) |
| `CodeCave.h` | Werkzeuge für Patches mitten in Funktionen: Code-Höhle nahe der Exe, Bytes prüfen, Sprünge und vtable-Einträge schreiben |
| `LocalDevicePolicy.h` | Reine Regeln für `LocalDevicePatches.cpp`: welche Gerätezuordnung und welcher lokale Spawn im Netzwerkspiel abgelehnt wird, ohne Spiel getestet |
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
2. **Neue Feldzugriffe auf Spielklassen über den Helfer `Reflection::Property`**, neue Funktionssuchen über `Reflection::Function`,
   nicht über feste Offsets oder eine eigene Suche im Objekt-Array. Die `FieldSpec` nennt Paket, Klasse, Feldname, den erwarteten
   `FProperty`-Typ (etwa `IntProperty`, `BoolProperty`, `StrProperty`) und die Größe. Liefert der Helfer nichts, bleibt nur der
   betroffene Adapter oder Patch aus, der Helfer hat das einmal geloggt. Nie auf einen festen Wert zurückfallen. Ein Bitfeld-`bool`,
   ein statisches Array oder ein Offset außerhalb der Klasse gilt als unpassend. Beispiele: `TutorialAdapter`, `TelemetryAdapter`.
   Feste Offsets in `Offsets.h` bleiben für Engine-Grundlayouts (Objekt-Array, Namen, `UObject`, `UStruct`, `FField`, `FProperty`),
   heiße Pfade, Felder ohne Reflexionsdaten (etwa in Blueprint-Structs oder Engine-Interna) und bestehenden Code. Solche Werte in
   `docs/OFFSETS.md` belegen und in `SdkChecks.cpp` gegen das SDK prüfen.
3. Spielspeicher nur über `Memory::Read`/`ReadPointer` lesen. Ein Lesefehler liefert `nullopt`, nie einen Absturz.
4. Nur auf dem Game Thread aufrufen. Lesefunktionen prüfen `Core::GameThread().IsDrainThread()`. Andere Threads reichen Arbeit mit
   `Core::GameThread().Post` herein.
5. Laufende Beobachtung als Tick über `GameThread::AddTick` in `Game::Start` registrieren. Logs nur bei Änderungen, nicht in jedem Takt.
6. Reine Logik (wie `RoleFrom`) als freie Funktion herausziehen und in `tests/GameTests` ohne Spiel testen.

Schreibende Zugriffe und Hooks folgen demselben Muster, siehe [Patches](#patches).

## Patches

Die Fixes, ohne die keine Mission zu zweit läuft, kommen aus dem Spike (Aufgabe `patches`). Herkunft und Belege jeder Stelle stehen in
der [Patch-Tabelle des Spikes](../../research/spikes/a-connect/README.md#patches). Die RVAs stehen als Konstanten am Anfang der jeweiligen Datei.

Alle RVAs im Quelltext gelten für den Referenz-Build `26495302` (seit `spiel-update`, vorher `26187885`, siehe
[Offsets](../../docs/OFFSETS.md#referenz-build-26495302-spiel-update)). `At(Rva)` und `Memory::Rva` rechnen sie über `AddressMap` auf einen künftigen
weiteren Build um (Tabelle `RvaMap<build>.inc` aus [`tools/rva_map.py`](../../tools/README.md#rva_mappy)). Fehlt eine Adresse, liefern sie 0 und
protokollieren sie einmal; die Byte-Prüfungen schlagen dann fehl und keine Patch-Liste wird gesetzt. Neue RVAs immer als Referenz-RVA
eintragen und für jeden weiteren Build in dessen Tabelle nachziehen. Feld-Offsets (`Offsets.h`) werden nicht umgerechnet.

**Teilinstallation für die Portierung.** Mit `DDCOOP_PARTIAL_PATCHES=1` (`Start-Session.ps1 -PartialPatches`) setzt die Mod auf einem anderen
Build als `26495302` jede Patch-Liste (eine `*Patches.cpp`) für sich ganz oder gar nicht; Ticks laufen nur für gesetzte Listen. Das Log nennt
ausgelassene Listen (`Liste … ausgelassen`) und die Summe. Nur zum Testen einzelner Portierungen: Hooks einer gesetzten Liste können Code einer
ausgelassenen erreichen. Im Referenz-Build und ohne Schalter bleibt es bei allen Patches oder keinem.

| Datei | Seite | Inhalt |
|---|---|---|
| `PlayerPatches.cpp` | Host | Spawn und Spielerindex ohne `LocalPlayer`, Loot- und Gegner-Spawner ohne Spieler, `SpawnedPlayers` bei Registrierung und Controller-Tausch, Abgleich je Sekunde als Rückfall, Client-Position annehmen, Busplatz schont alle Spielerfiguren, Run-Ende ohne `LocalPlayer` |
| `ClientPatches.cpp` | Client | Refresh, Attribut-Tick, AttributeSet-Suche, Kartengenerator-Schritt, Figurenkomponente und ThreatLevel ohne Host-Objekte; Exp-Aggregation wartet auf AttributeSets; Blockaden nur beim Host; NetGUID-Pfade bleiben; SneakApproach wartet auf Mesh und PlayerState; keine lokale PowerUp-Kopie; Niederlage entscheidet der Host (`run-ende`), Client schaltet seine Rundenübersicht auf das Run-Ende um |
| `MovementPatches.cpp` | beide | Client: eigene Figur übernimmt das Lauftempo aus dem replizierten Attribut `MovementSpeed` und dodgt bei HP 0 nicht ([Tod des Clients](../../docs/GAME_ANALYSIS.md#tod-des-clients-tod-client)), eigene Figur bekommt nach haengendem Dodge Reibung und Schwerkraft zurueck, fremde Figur folgt dem replizierten Ort bei unbekannter Unterlage oder fehlendem Modus. Host: Richtung des Client-Dodges aus der Meldung des Clients (`0x500`, Schlüssel der Aktivierung), Schätzung nur als Rückfall; [Dodge](../../docs/GAME_ANALYSIS.md#dodge-des-clients-beim-host-ausweichen-client), [Zwei-PC-Befund und Fix](../../docs/GAME_ANALYSIS.md#dodge-im-zwei-pc-test-dodge-client-live) |
| `CameraPatches.cpp` | Client | Kamera-Spieler aus der `PlayerArray` statt aus dem GameMode, Bodenfix, gemeinsame Kamera als ViewTarget je Sekunde; Bewegungs-Restrictor ohne GameState stürzt nicht ab und bekommt ihn nachgetragen, siehe [Restrictor](../../docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) |
| `InventoryPatches.cpp` | beide | Inventar-Unterobjekte über den klassischen Replikationspfad, Timer des Achievement-Detektors beim `EndPlay` löschen, Kartengenerator-Schritt nur für die aktuelle Welt; [Itemsack-Vergabe](../../research/spikes/itemsack-absturz/README.md) nur auf Authority außerhalb einer Client-Welt, beim Host [für alle Spieler](#itemsack-für-alle-spieler); abschaltbare Inventar-Diagnose in `RewardPatches.cpp` |
| `MapPatches.cpp` | beide | Gleiche Karte: Der Host meldet Seed, Profil, Größe, Tags und PCG-Graph seiner Erzeugung, der Kontrollkanal bringt sie zum Client (`Sync/World/MapSync.cpp`), dessen Erzeugung wartet darauf; lokale Kopien replizierter Child Actors unterdrückt, gleiche Seeds der PCG-Instanzen. Siehe [Gleiche Karte in der Mod](../../docs/GAME_ANALYSIS.md#gleiche-karte-in-der-mod-missionsstart) |
| `TravelPatches.cpp` | Host | Reise ins Spiel wird `servertravel`, vorher Zeitanfragen abräumen; allein und ins Hauptmenü reist das Spiel wie gewohnt |
| `WidgetPatches.cpp` | Client | `WP_UI` wartet auf den GameState; Beobachter nach jedem `ProcessEvent` |
| `RemoteP2Patches.cpp` | Host | Remote-Spieler als P2 in `ActivePlayers` des Coop-Subsystems, ohne zweiten `LocalPlayer`; beim Verlassen nur die eigene Bindung entfernen; lokale P2-Gerätepflicht nur bei vollständiger Bindung ausnehmen. Koop-Modus erst nach `AllowRemoteCoop()`, siehe [Remote-Spieler](#remote-spieler-als-p2) |
| `HealthBarPatches.cpp` | Client | Lebensleisten nach repliziertem `OnRep_Health` und `OnRep_MaxHealth` |
| `EnemyTelegraphPatches.cpp` | beide | Warnanzeigen der Gegnerangriffe: Host meldet Kreise (`SpawnCircleTelegraph`) und Sprint-Striche (`UTackleAbilityExecution`) über `0x900`, der Client zeigt dasselbe Niagara-System bzw. treibt die eigene Komponente des Gegners; siehe [Warnanzeigen](../../docs/GAME_ANALYSIS.md#warnanzeigen-der-gegner-beim-client-opt-gegner-indikatoren) |
| `LocalDevicePatches.cpp` | beide | Im Netzwerkspiel bleiben lokale Geräte bei Spieler 0: keine Gerätezuordnung oder kein Entwurf für P2, kein zweiter `LocalPlayer`; ohne Netzwerk unverändert, siehe [Eingaben getrennt](../../docs/GAME_ANALYSIS.md#eingaben-getrennt-eingaben) |

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
die Airdrop-Auswahl wartet auf den Claim von P2. Gelöst wurde beides ohne Koop-Modus (`inventar-lokal`, `bestaetigen-alle`,
`rundenende-client`), der Modus bleibt deshalb aus. Zurückschalten ist nicht vorgesehen: `SetCoopEnabled(false)` entfernt laut Analyse vorher einen vorhandenen P2.

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
  Clients kommt seit `inventar-lokal` als Teil eines Inventar-Edits (`0x303`), siehe [Inventar lokal bearbeiten](#inventar-lokal-bearbeiten).
- **Client:** Die Mod legt lokale Anzeige-Instanzen über den nativen `CreateItemInstance` an (die UFunction ist `BlueprintAuthorityOnly`,
  über `ProcessEvent` liefert sie beim Client nichts) und öffnet das native Panel mit `SetRewardItems`. Solange es offen ist, laufen
  Inventar-RPCs lokal, siehe [Inventar lokal bearbeiten](#inventar-lokal-bearbeiten).
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

**Abnahme `airdrop-rucksack-live` (06.11.2026, Build `26187885`, ein PC, Lobby, Godmode, `tools/spike/guided/airdrop-rucksack.txt`).**
Die oben fehlende gemeinsame Transaktion liefert inzwischen `inventar-lokal` (Edit beim Bestätigen), die sofortige Feldanzeige
`ausruestung-felder`; deren Läufe deckten zwei Airdrops, Umsortieren und gleiche Inventare ab. In diesem Lauf: Item in den Rucksack
gezogen, ohne zu bestätigen, das Panel blieb offen, keine automatische Bestätigung, kein Geister-Item. Die übrigen Schritte (Wirkung im
Kampf, zweiter Airdrop) wurden nicht mehr einzeln geführt, der Nutzer erklärte die Aufgabe nach eigener Prüfung ohne Befund für erledigt.

**Bestätigen-Logik, lesend geprüft (06.11.2026).** Gewünscht ist: Bei jeder Itemauswahl (Airdrop und Rundenende mit Lager) drücken alle
Spieler Bestätigen, der Knopf leuchtet gelb, und erst wenn alle samt Host bestätigt haben, setzt der Host Spiel oder nächste Runde fort.
Stand im Code:

- **Airdrop, teilweise:** Bestätigt der Host zuerst, hält `ConfirmHook` sein Bestätigen zurück (`Host.HeldPanel`), das Spiel bleibt
  pausiert, und `TickHost` holt es nach, sobald die Wahl des Clients da ist. Bestätigt der Client zuerst, schließt sein Panel sofort
  lokal (`ClientConfirm`), das Spiel läuft weiter, sobald der Host bestätigt. Es gibt keine gelbe Markierung und keine Anzeige, wer schon
  bestätigt hat. Der Client sieht nach seinem Bestätigen nur das pausierte Spiel.
- **Nur ein Client:** `RemoteReady`, `Host.ClientPick` und `ClientPending` gibt es je einmal, gewartet wird nur auf P2.
- **Rundenende, fehlt:** Zum Zeitpunkt der Prüfung sah der Client Rundenübersicht und Lager noch nicht; das liefert inzwischen
  `rundenende-client`, das gemeinsame Bestätigen dort folgt als `bestaetigen-rundenende`. Das native „Bestätigen & Wechseln“ des
  Koop-Modus wartet zwar auf P2, hängt aber mit dem Remote-Spieler, darum bleibt der Koop-Modus aus.

**Alle bestätigen (`bestaetigen-alle`, 06.11.2026).** Ersetzt beim Airdrop das sofortige Schließen. Format in
[Kontrollkanal](../../docs/CONTROL_PROTOCOL.md#alle-bestätigen-mod-kompatibilität-5), Mod-Version 5.

- **Lokal:** Bestätigen markiert den Spieler nur als bereit. `ConfirmHook` ruft das native `HandleConfirmButton` nicht auf, färbt den
  Bestätigen-Knopf (`MainButton`, ein `DefaultFocusableButton` mit `ParentPanel` `+0x3A8` = Panel) über `UserWidget.SetColorAndOpacity`
  gelb, das Panel bleibt offen und bedienbar. Weitere Klicks ändern nichts. Ein Zurücknehmen per zweitem Klick gab es im ersten
  Lauf; ohne sichtbare Färbung klickte der Host erneut und hob sein Bereit auf, während der Client gerade bestätigte. Darum entfernt.
- **Geteilt:** Nur das Bereit des Clients (`0x305`, Sitzung und Revision) und dessen Echo im Sitzungsbild (Bit 3). Das Bereit des
  Hosts ist das vorhandene „Host wartet“ (Bit 2).
- **Entscheidet:** Der Host. Erst wenn das Sitzungsbild „Host wartet“ und das eigene Bereit zeigt, löst der Client sein natives
  Bestätigen aus und schickt den Edit (`inventar-lokal`). Nach der Übernahme holt der Host sein zurückgehaltenes Bestätigen nach
  (`airdrop-warten`), beide Panels schließen, das Spiel läuft weiter. Ohne Mod beim Client bleibt alles wie im Solo-Spiel.
- **Abbruch:** Eine neue Revision (Reroll) braucht ein neues Bereit; fehlt beim Auslösen die Wahl, nimmt der Client sein Bereit zurück.
  Schließt die Sitzung, setzt die Mod die Farbe zurück.

**Geprüft** (06.11.2026, Build `26187885`, ein PC, Lobby, Godmode, `tools/spike/guided/bestaetigen-alle.txt`): Zwei Airdrops, einmal
Host zuerst, einmal Client zuerst. Beide Knöpfe gelb, das Spiel blieb pausiert, bis beide bestätigt hatten, danach schlossen beide
Panels ohne weiteren Klick (`Bestätigen des Hosts nachgeholt (Versuch 1)`), Edits übernommen, darunter einer mit 3 neuen Feldern. Der
Nutzer bestätigte den Ablauf. Nicht geprüft: Reroll während eines Bereit, mehr als ein Client (es gibt nur P2). Das Rundenende mit
Lager folgt als `bestaetigen-rundenende`, sobald `rundenende-client` dem Client das Lager zeigt.

**Lager am Rundenende (`bestaetigen-rundenende`, 06.11.2026).** Dasselbe Verfahren für das Lager nach der Rundenübersicht. Das
Lager ist dasselbe `InventoryPanel` (`WP_Inventory_Panel_C`) wie beim Airdrop, sein Bestätigen läuft also durch denselben Hook. Format
in [Kontrollkanal](../../docs/CONTROL_PROTOCOL.md#lager-am-rundenende-mod-kompatibilität-6), Mod-Version 6.

- **Lokal:** Bestätigen im Lager färbt den Knopf gelb und hält das native `HandleConfirmButton` zurück. Weitere Klicks ändern nichts.
  Inventar und Lager bleiben bedienbar. Die Rundenübersicht, das Lager und das Ordnen des Inventars laufen wie in
  `rundenende-client` beim Client lokal.
- **Geteilt:** Der Client meldet sein Rundenende (`Game::Map::RoundEnd::Id`, nur wenn er es selbst geöffnet hat) und sein Bereit
  (`0x306`), der Host seinen Stand (`0x307`: Host bereit, Bereit des Clients).
- **Entscheidet:** Der Host. Er wartet nur, wenn ein Client mit der Mod verbunden ist, dasselbe Rundenende geöffnet hat und noch
  nicht bereit ist. Sind beide bereit, holen beide ihr Bestätigen einmal nativ nach, der Host kommt zur Routenkarte und wählt den Tag.
  Ist der Client schon bereit, läuft das Bestätigen des Hosts sofort.
- **Nie hängen:** Hat der Client die Runde nicht nachgezogen (andere Karte, kein GameState), wartet niemand. Trennt sich der Client,
  kommt 2 s lang keine Meldung von ihm oder fehlt ihm 5 s lang ein Stand des Hosts, holt die wartende Seite ihr Bestätigen nach. Die
  2 s sind nötig, weil `disconnect` beim Client nur das Spiel trennt; die Verbindung der Mod bleibt, der Client meldet nur nichts mehr. Bleibt das Nachholen ohne Wirkung,
  läuft ein zweiter Klick direkt nativ. Ein neues Rundenende (neue Welt) setzt alles zurück, auch die Farbe.
- Regeln `HostHoldsRoundEndConfirm`, `ClientHoldsRoundEndConfirm` und `ClientMayReleaseRoundEnd` in `PatchPolicy.h`, mit Tests;
  Laufzeit in `RewardPatches.cpp`, Nachrichten in `src/Sync/Inventory/RewardSync.cpp`, Rundenende aus `MapPatches.cpp`.

**Geprüft** (06.11.2026, Build `26187885`, ein PC, Lobby, Godmode, Rundenende per `!finish`): Zwei Rundenenden hintereinander.
Tag 1 Host zuerst: Knopf des Hosts gelb, der Host blieb 23 s im Lager, bis der Client bestätigte, dann beide weiter
(„Lager: Bestätigen nachgeholt“), der Host auf der Routenkarte. Tag 2 Client zuerst: Knopf des Clients gelb, nach dem Bestätigen des
Hosts beide weiter. Reise bis Tag 3, Inventare gleich, Desync-Melder OK, kein Absturz. Das Rundenende über den Bus nimmt beim Client
denselben Weg (`tank-gemeinsam`).

**Trennen während des Wartens** (gleicher Tag, zweiter Lauf): Host bereit, dann `disconnect` beim Client. Im ersten Versuch wartete der
Host weiter, weil die Verbindung der Mod bestehen blieb. Seit dem 2-s-Ablauf: „Lager: Client meldet sich nicht mehr aus dem Lager“,
„Bestätigen nachgeholt (Client nicht mehr im Lager)“, der Host auf der Routenkarte. Mit diesem Build danach erneut Client zuerst
geprüft, Reise bis Tag 2, Inventare gleich, kein Absturz.

**Stummer Client und Absturz im Lager** (`lager-absturz-client`, 06.11.2026): Vorher hörte der Host nach 2 s ohne Meldung auf zu warten
und bestätigte als „alle bereit“, auch wenn der Client abgestürzt war (Live-Test 06.11.). Jetzt meldet der Client das Verlassen des
Rundenendes ausdrücklich (`0x306` mit Id 0, 3 s lang). Nur dann gibt der Host sofort frei. Bleibt der Client nur stumm, schreibt der
Host „Client stumm, Host wartet auf ihn bis zum Verbindungsabbau“ und gibt erst frei, wenn die Verbindung abläuft. Bestätigt der Host
ohne Client im Lager, steht „ohne Client im Lager“ statt „alle bereit“ im Log. Dazu fängt `InventoryPatches.cpp` den Absturz in der
Größenberechnung eines Item-Widgets ab, das sein Item verloren hat (`0x5AB5120`, Lesefehler `0xE0`).

Geprüft am selben Abend, ein PC, Lobby, Godmode, Rundenende per `!finish`: Zwei Rundenenden bis Tag 3, Inventare gleich, kein Absturz.
Host bereit, dann Client hart beendet: Der Host wartete 5 s, meldete den stummen Client und reiste 9 s später nach dem
Verbindungsabbau (5003) allein weiter, kein „alle bereit“. Host bereit, dann `disconnect` beim Client: Der Host gab binnen 4 s frei,
ohne die Wartezeit. Beide im Lager, der Host schließt sein Fenster (wie Alt+F4): Der Client kam ohne Absturz ins Hauptmenü, der
Widget-Wächter griff dabei aber nicht, der Absturz vom Live-Test ließ sich so nicht nachstellen. **Offen:** zwei PCs, und ob der
erste Absturz im Lager (Ende Tag 2) denselben Weg nahm; dafür fehlen Client-Log und Absturzbericht dieser Sitzung.

**Verworfen:** Zuerst nutzte die Mod die native Koop-Reward-Sitzung (Koop-Modus nur für den Airdrop, Client wählt im Overlay, Host ruft
`ClaimReward(1, …)`). Das Host-Panel zeigte beide Rucksäcke, nach dem Claim des Clients ging die UI-Autorität an P1 und der Host hing.
Die Auswahl im Overlay gefiel nicht, der Nutzer wollte für beide das normale Panel.

### Inventar lokal bearbeiten

Aufgabe `inventar-lokal`, Code in `InventoryEdit.cpp` (Laufzeit), `InventoryEditPlan.h` (reine Logik mit Tests in
`tests/GameTests/InventoryEditTests.cpp`), `RewardPatches.cpp` (Bestätigen und Host-Übernahme) und `src/Sync/Inventory/RewardSync.cpp`,
Format in [Kontrollkanal](../../docs/CONTROL_PROTOCOL.md#inventar-lokal-mod-kompatibilität-4). Ersetzt die Fernsteuerung über Wahl
(`0x301`) und Ablage (`0x302`).

- **Lokal beim Client:** Solange das Airdrop-Panel offen ist, führt `RouteClientRpc` die Server-RPCs des Inventars (`AddToBackpack`,
  `AddToInventory`, `OccupyBackpackSlots`, `UnoccupyBackpackSlots`, `RemoveFromBackpack`, `RemoveFromInventory`,
  `Server_CreateSlotsFromBagItem`) und der Items (`SetZeroPivotTile`, `UpdateIsInBackpack`, `UpdateIsInStash`, `UpdateIsReward`,
  `UpdateIsTrash`) mit der nativen Logik lokal aus: `ProcessEventLocal` nimmt für den Aufruf `FUNC_Net` weg. Ablegen, Drehen, Sortieren,
  Rucksackteile und neue Felder sind damit sofort sichtbar. `RemoveFromInventory` zerstört beim Client nie, damit sich ein verbrauchter
  Rucksack zurücklegen lässt. `ApplyAddToBackpackEffect` merkt sich der Client nur, der Host löst ihn nach dem Ablegen aus. Alle
  anderen Server-RPCs (`FUNC_NetServer`, Client-RPCs und Multicasts vom Host laufen normal) verwirft der Client, bis der Host geantwortet hat, auch die anderer Klassen (Ausrüsten, Waffen); nur Bewegung
  geht weiter. Sie nennen oft Objekte, die es nur beim Client gibt: In den ersten beiden Spieltests stürzte der Host beim Bearbeiten ab
  (Lesezugriff `0xF0`), erst am Effekt der gewählten Belohnung, dann nach dem Verschieben des Revolvers. Verworfene RPCs stehen je
  Bearbeitung einmal im Log.
- **Geteilt:** Beim Bestätigen ein Edit: nur die geänderten Items als Endzustand (Index im Ausgangsbild und Prüfsumme des
  `ItemData`-Namens, vorhanden, im Rucksack, Papierkorb, Drehung, Pivot, Felder), die gewählte Belohnung und die neu entstandenen
  Felder, dazu Sitzung und Revision der Auswahl. Keine Objektzeiger, keine Zwischenschritte.
- **Entscheidet:** Der Host. Er ordnet die Items über Index und Prüfsumme dem Clientinventar zu, prüft Form, Sitzung und freie oder
  neue Felder, legt die Wahl mit `CreateAndAddItem` an und führt den Plan (`Plan`) in einem Durchgang mit den nativen Funktionen aus:
  erst aus dem Rucksack nehmen, entfernen, Felder anlegen, dann ablegen. Das Ergebnis repliziert nativ zum Client.
- **Ablehnung und Abbruch:** Der Client hält das Ausgangsbild, bis die Antwort (`0x304`) kommt. Bei Ablehnung, geschlossenem Panel
  ohne Wahl oder Verbindungsverlust rollt `RevertClient` mit demselben Plan lokal zurück: verbrauchte Items wieder hinein, lokale
  Belohnungen und neue Felder (`DestroySlotInstance`) wieder hinaus. Bis zur Antwort wiederholt der Client den Edit jede Sekunde, der
  Host übernimmt jede Kennung nur einmal.
- **Diagnose:** Logzeilen `Inventar lokal: …`, mit `DDCOOP_TRACE_INVENTORY=1` jeder einzelne Aufruf.

**Geprüft** (06.11.2026, Build `26187885`, ein PC, Lobby, Godmode, `tools/spike/guided/inventar-lokal.txt`): Der Client legte
Rucksackteile, verschob und drehte Revolver, Pistole, Schrotflinte und Baseballschläger, neue Felder erschienen sofort. Vier Edits,
darunter einer mit drei neuen Feldern und ein reines Umsortieren ohne Wahl, übernahm der Host jeweils vollständig. Danach meldete
`Get-Status -Inventory` beide Inventare vollständig und gleich. Tank per Speicherschreiben auf 118 von 120, echtes Rundenende über den Bus
bis Tag 2, dort gekämpft: kein Absturz, Inventare stabil. Der letzte Vergleich in Tag 2 wich nur um die drei lokalen
Anzeige-Instanzen einer gerade offenen Auswahl ab (`reward=1`), das ist so gewollt. Die ersten beiden Läufe stürzten beim Host ab, siehe
oben; behoben. Danach im Spiel nicht mehr geprüft: die Begrenzung des Verwerfens auf `FUNC_NetServer`, zuvor verwarf der Client auch
`ClientSetViewTarget`, `ClientEffectApplied` und Gameplay-Cues vom Host. Abbruch und Ablehnung sind nur in Tests und über das
Schließen ohne Wahl (`Ausgangsstand wiederhergestellt, vollständig=true`) belegt, eine Ablehnung durch den Host kam im Spiel nicht vor.
Offen und nicht Teil dieser Aufgabe: Beim Rundenende sieht der Client noch die alte Sperre statt Rundenübersicht und Lager
(`rundenende-client`).

**Neue Felder sofort sichtbar (`ausruestung-felder`, 06.11.2026).** Fehlerbild: Legt der Client einen Rucksack-Block an, entstanden die
Felder lokal (Client-Inventar 11 statt 8 Slots, Log `Server_CreateSlotsFromBagItem lokal, 3 Felder`), das Panel zeigte sie aber erst,
nachdem der Host etwas repliziert hatte. Ursache: Die Feld-Widgets hängen an einem nativen Delegate am `InventoryComponent` (`+0x110`,
nicht im SDK benannt), den nur `OnRep_Slots` auslöst (Implementierung `0x596D970` ruft nur dessen `Broadcast`). Beim Client läuft
`OnRep_Slots` nur bei Replikation vom Host. Jetzt ruft der Client nach der lokalen Feldanlage und nach dem lokalen Zurückrollen mit
`DestroySlotInstance` `OnRep_Slots` mit dem alten Feldstand selbst auf (`NotifySlots`), also derselbe Weg wie bei der Replikation.
Logzeile `Inventar lokal: Felder neu angezeigt, 11 statt 8`.

**Geprüft** (06.11.2026, Build `26187885`, ein PC, Lobby, Godmode, `tools/spike/guided/ausruestung-felder.txt`): Vor dem Fix
nachgestellt (Felder unsichtbar), mit dem Fix erscheinen sie beim Anlegen sofort. Wahl eines anderen Items nimmt den Block wieder heraus.
Der Host übernahm den Edit mit 3 neuen Feldern, `Get-Status -Inventory` und der Desync-Melder meldeten die Inventare gleich, kein
Absturz. Nicht in diesem Lauf geprüft: Rundenende danach; der Nutzer schloss die Aufgabe vorher ab. Der Befund „Felder nach dem
Rundenende wieder im Lager“ stammt aus dem Test vor `inventar-lokal`. Seitdem übernimmt der Host neue Felder im Edit, und der
`inventar-lokal`-Lauf blieb über das Rundenende bis Tag 2 gleich. Das Tab-Inventar lässt sich im Run nicht öffnen, die Anzeige außerhalb
des Airdrop-Panels ist daher nicht eigens angesehen.

### Waffen aus der Itemauswahl des Clients (`waffe-client-auswahl`)

Im Live-Test am 06.11.2026 wirkten Waffen, die der Client in der Itemauswahl tauschte, im Spiel nicht. Drei Ursachen:

1. **Entfernstil vertauscht.** Beim Übernehmen nahm der Host ein Item mit `RetainEffects` (2) aus dem Rucksack, wenn es ihn
   verließ, und mit `ForceRemoveAllEffects` (0), wenn es nur verschoben wurde. Das native Panel macht es umgekehrt. Folge: Eine
   abgelegte Waffe schoss weiter, eine verschobene verlor ihre Wirkung. `RemovalStyle()` in `InventoryEdit.cpp` folgt jetzt dem Panel.
2. **Kein Auffrischen der Wirkungen beim Host.** Legt der Host selbst ab, folgt auf `OccupyBackpackSlots` ein vollständiges
   Auffrischen beider Figuren (`RemoveAllEffects`, `RemoveEffectsFromItem`, `ApplyEffectsFromItem` je Item, im Trace belegt). Beim
   Übernehmen eines Client-Edits fehlte es. Eine neue Waffe wirkte erst nach dem nächsten Levelaufstieg, der ebenfalls auffrischt.
   `ApplyClientEdit` ruft jetzt `InventoryComponent.RefreshItemEffects(false, GetEffectManager())` für das Inventar des Clients und
   das des Hosts. Das zusätzliche `ApplyAddToBackpackEffect` nach dem Ablegen ist entfallen: `AddToBackpack` ruft es beim Host
   selbst, der zweite Aufruf legte die Rucksackwirkung doppelt an.
3. **Waffenslots beim Client während der Bearbeitung.** Belegte Hände (`UDDCharacterAttributeSet.OccupiedHands`) setzt nur der
   Host über die Itemwirkungen. Der Client verwirft deren RPCs während der Bearbeitung, also gab eine abgelegte Waffe keinen Slot
   frei und das Panel ließ keine neue zu. Das war schon in `inventar-lokal` (`f42b6d6`) so, kein Rückschritt. Jetzt rechnet der
   Client lokal vor: Ausgangswert + (Hände der Rucksackitems jetzt - beim Start), je Item über
   `InventoryEffectHelperLibrary.GetHandsRequiredForItem`. Er schreibt nur `CurrentValue` und setzt den eigenen Hände-Balken über
   `HandBarDisplay.UpdateCurrentHandsBar`. Kein `OnRep_OccupiedHands`, sonst rechnet das ASC den Wert aus seinen Modifikatoren
   neu. `RefreshHandsBar` zeichnet den Balken nicht neu (im Spiel geprüft). Verbindlich bleibt der Host: Nach dem Übernehmen kommt
   sein Wert über die Replikation, bei Ablehnung oder Abbruch stellt `RevertClient` den Ausgangswert wieder her. Das Attributset
   gehört dem PlayerState, nicht der Figur.

**Geprüft** (07.11.2026, Build `26187885`, ein PC, Lobby, Godmode, Trace `DDCOOP_TRACE_INVENTORY=1`, vom Nutzer gespielt):
Waffen ablegen, tauschen und wieder ausrüsten in der Itemauswahl des Clients; der Hände-Balken des Clients folgt sofort
(Log „Hände-Vorschau 2 statt 1“ und zurück). Nach dem Bestätigen übernahm der Host den Edit vollständig; `tools/spike/weapons.py` zeigte beim Host für beide Figuren genau die Rucksackwaffen ausgerüstet (Client: abgesägte Schrotflinte und Pistole), die Rucksäcke gleich wie beim Client. Ein Rückbau auf `f42b6d6`
zeigte denselben Fehler bei den Waffenslots. Nicht geprüft: zwei PCs über das Internet, Ablehnung eines Edits mit Hände-Vorschau.

### Werkbank und Aufwerten beim Client

Aufgabe `werkbank-client`, Code in `RewardPatches.cpp` (Hook auf `UpgradeItems`, Host-Prüfung, Seltenheiten), `InventoryEdit.cpp`
(Aufwertungen im Edit), `InventoryEditPlan.h` (reine Logik mit Tests in `tests/GameTests/InventoryEditTests.cpp`) und
`src/Sync/Inventory/RewardSync.cpp`. Format in [Kontrollkanal](../../docs/CONTROL_PROTOCOL.md#aufwerten-und-seltenheiten-mod-kompatibilität-7).

**Befund vorher** (06.11.2026, Build `26187885`): Ablegen auf ein gleiches Item (`InventoryPanel.UpgradeDroppedItem`) und die Werkbank
rufen beide `UInventoryComponent::UpgradeItems` (`0x5980AE0`) direkt auf, keine UFunction über `ProcessEvent`. Die Funktion legt die
Ausgabe mit `CreateItemInstance` an und nimmt die Eingaben über die Inventar-RPCs heraus. Beim Client hieß das: Der Host entfernte die
Eingaben, die Ausgabe gab es nur beim Client. Dazu kam es aber gar nicht, denn `UInventoryItemInstance.ItemRarity` (`+0x118`) repliziert
nicht: Beim Client steht dort für jedes replizierte Item null, das Panel wertet ohne Seltenheit nie auf. Im Lager tauschte es nur die
beiden Pistolen. Der Host wertete im selben Lauf seine eigenen Pistolen ohne Fehler auf.

- **Lokal beim Client:** Das native Panel wertet auf, wie im Solo-Spiel. `UpgradeHook` lässt `UpgradeItems` nur innerhalb einer lokalen
  Bearbeitung laufen (`inventar-lokal`), die Inventar-RPCs der Aufwertung laufen dann lokal. Beim Airdrop ist das die Bearbeitung der
  Auswahl. Im Lager beginnt die erste Aufwertung eine Bearbeitung (`LagerEdit`), die bis zum Bestätigen im Lager läuft; vorher bleibt das
  Lager wie in `rundenende-client` bei den nativen RPCs. `RecordUpgrade` merkt sich Eingaben, Ausgabe, Daten und Seltenheit.
- **Geteilt:** Beim Bestätigen trägt der Edit (`0x303`) zusätzlich die Aufwertungen: Eingaben als Quellen (Ausgangs-Item, Wahl oder eine
  frühere Ausgabe), Objektnamen von Ausgabe-`ItemData` und Seltenheit und der Endstand der Ausgabe. Dazu schickt der Host jede Sekunde bei
  Änderung (sonst alle 5 s) die Seltenheiten des Clientinventars (`0x308`); der Client setzt damit nur `ItemRarity` seiner replizierten
  Items, nicht während einer Bearbeitung.
- **Entscheidet:** Der Host. Er prüft jede Aufwertung, bevor sich etwas ändert: Ablegen nur mit zwei Eingaben gleicher Daten, Ausgabe mit
  denselben Daten in `URarity.NextUpgradableRarity` (`+0x58`) einer Eingabe, und nativ `CanItemsBeUpgradedTogether`. Sonst muss sie zu
  einem Rezept aus `DT_RecipeTable` passen (Zutaten nach Daten und, falls vorgegeben, Seltenheit; Ausgabe mit der vorgegebenen Seltenheit
  oder der einer Zutat). Dann ruft er `UpgradeItems` selbst am Clientinventar auf und setzt die Ausgaben wie jedes andere Item des Edits.
  Die Eingaben entfernt `UpgradeItems`, nicht der Plan.
- **Ablehnung und Abbruch:** Wie beim Edit: Der Client rollt zurück, die lokalen Ausgaben kommen wieder heraus. Verlässt der Client das
  Lager ohne Bestätigen (neues Rundenende), rollt er ebenso zurück. Bis zur Antwort bleibt sein Bestätigen im Lager zurückgehalten, und er
  meldet sich erst danach bereit, damit der Host nicht vor der Übernahme reist.
- **Freischaltung:** `OnetimeUnlock_Workbench` ist eine Meta-Freischaltung des eigenen Spielstands. Ob der Client die Werkbank sieht,
  entscheidet sein Spielstand, wie bei Figuren. Der Host prüft nur die Aufwertung selbst.
- **Diagnose:** Logzeilen `Aufwertung beim Client …` (mit Aufrufer-RVA), `Inventar lokal: Aufwertung …`, `Lager: …`, `Seltenheiten: …`.
  Aufwertungen beim Client außerhalb einer Bearbeitung laufen nativ weiter und stehen als Warnung im Log.

**Geprüft** (06.11.2026, Build `26187885`, ein PC, Lobby, Godmode, Items über die Spike-Testhilfe `!give`, Werkbank über
`DD_SetMetaUpgrade OnetimeUnlock_Workbench 1`):

- Lager Tag 1, Ablegen: Der Client legte eine Pistole (Gewöhnlich) auf die zweite, das Panel zeigte „Ungewöhnlich“. Der Host prüfte
  („Ablegen“), wertete nativ auf und übernahm vollständig. `inventory.py`: eine Pistole (Ungewöhnlich), vollständig und gleich. Beide
  bestätigten, Routenkarte, `Get-Status -Inventory` gleich.
- Lager Tag 2, Werkbank und Ablegen in einer Bearbeitung: Abgesägte Schrotflinte aus Schrotflinte und Messer (Ungewöhnlich), danach zwei
  Pistolen zu einer. Ein Edit mit 2 Aufwertungen, der Host prüfte „Rezept“ und „Ablegen“ und übernahm vollständig. Solange Lager und
  Routenkarte offen waren, zeigte der Client seine lokalen Ausgaben, die Ausgaben des Hosts kamen nicht an (dieselbe Grenze wie
  `rundenende-client`: im Lager repliziert das Inventar sichtbar erst nach der Reise). Nach der Reise auf Tag 3 hatte der Client die
  replizierten Objekte des Hosts in derselben Reihenfolge, Seltenheiten gesetzt, `inventory.py` gleich, nichts doppelt, nichts fehlte.
- Gegenproben: Airdrop an Tag 3 (Wahl übernommen, Bestätigen des Hosts nachgeholt) und Lager an Tag 3 ohne Aufwertung (Host zuerst) wie
  bisher, kein Absturz, keine Aufwertung außerhalb einer Bearbeitung. Danach meldete `Get-Status -Inventory` nur die bekannte
  Rasterposition einer ins Lager gelegten Pistole (`opt-desync-pivot`), sonst gleich.

**Nicht live geprüft:** Aufwerten mit einer Airdrop-Belohnung als Eingabe (das Angebot hatte kein passendes Item; im Airdrop-Panel lassen
sich Lager-Items nicht aufeinanderlegen), verkettete Aufwertungen, eine Ablehnung durch den Host und Aufwertungen am Tagesende
(`EndOfDayUpgradeManager`). Diese Fälle decken die Unit-Tests ab. Das Tagesende läuft beim Host nativ am replizierten Inventar; beim
Client gab es in allen Läufen keinen Aufruf von `UpgradeItems` außerhalb einer Bearbeitung.

### Verkaufen und Reparieren beim Client

Aufgabe `lager-handel-client`, Code in `RewardPatches.cpp` (Hook auf `HandleItemSold`, Beginn der Bearbeitung bei `RepairItem`,
Host-Prüfung und Buchung, Schrott), `InventoryEdit.cpp` (`RepairItem` lokal, Zustand `Broken`, Zurückrollen), `InventoryEditPlan.h`
(reine Logik mit Tests in `tests/GameTests/InventoryEditTests.cpp`) und `src/Sync/Inventory/RewardSync.cpp`. Format in
[Kontrollkanal](../../docs/CONTROL_PROTOCOL.md#verkaufen-reparieren-und-schrott-mod-kompatibilität-9).

**Befund vorher** (07.11.2026, Build `26495302`, Analyse der Exe, Adressen dieses Builds):

- `ItemRepresentation.ConfirmSell` (`0x5AA7E50`) holt das Schrottkonto (`ADDMissionGameState.ScrapWallet`, `+0x3C0`), schreibt den Preis
  mit `AbstractCurrency::AddAmount` **lokal** gut und ruft `UInventoryComponent::HandleItemSold` (`0x596BA70`, nativ, kein RPC). Das nimmt
  das Item über die Inventar-RPCs heraus: `RemoveFromBackpack`, `UnoccupyBackpackSlots`, `RemoveFromInventory` mit `SoldForScrap` (2).
- Reparieren läuft im Takt des Item-Widgets: Ist der Haltebalken voll, vergleicht es `CurrencyAmount` (`+0x100`) mit
  `GetDynamicRepairPrice`. Bei zu wenig Schrott blinkt es rot, sonst zieht `ConfirmRepair` (`0x5AA7CA0`) den Preis **lokal** ab und
  ruft den Server-RPC `InventoryComponent.RepairItem`.
- `ScrapWalletComponent` repliziert nicht. Beim Client stand ein eigener Stand, Gutschrift und Abzug blieben lokal. Der Host entfernte
  ein vom Client verkauftes Item ohne Schrott, eine Reparatur blieb an zu wenig Schrott hängen.
- **Kaputte Items sperrt das Spiel selbst**, auch beim Host: Ein kaputtes Item lässt sich nicht in den Rucksack legen, nur verkaufen
  oder reparieren (im Spiel geprüft). Ausrüsten geht also erst nach einer Reparatur.

**Umsetzung:**

- **Lokal beim Client:** Das native Panel verkauft und repariert wie im Solo-Spiel. Im Lager beginnt der erste Verkauf
  (`SoldHook` vor `HandleItemSold`) oder die erste Reparatur (`RepairItem` in `FilterRewardEvent`) eine lokale Bearbeitung
  wie die erste Aufwertung (`BeginLagerEdit`). Darin laufen die Entfern-RPCs lokal (das Item wird nicht zerstört), `RepairItem` lokal
  (`Broken` wird heil, das Item lässt sich sofort ablegen). Gutschrift und Abzug des Panels sind nur Vorschau.
- **Geteilt:** Der Edit (`0x303`) trägt je Item den Zustand kaputt; verkauft steht schon als `RemoveContext` drin. Dazu schickt der Host
  seinen Schrott (`0x309`), der Client übernimmt ihn außerhalb einer Bearbeitung. So stimmt auch die native Vorprüfung beim Client.
- **Entscheidet:** Der Host. Vor jeder Änderung prüft er jede Aktion am eigenen Bild: verkauft nur ein Ausgangs-Item ohne
  Belohnungsflag, repariert nur ein bei ihm kaputtes Item, nie heil zu kaputt. Preise rechnet er selbst, `TradeBalance` prüft den
  Schrott (erst Verkäufe, dann Reparaturen). Dann führt der Plan die nativen Funktionen aus: `RepairItem` vor dem Ablegen, Verkauf als
  Entfernen mit `SoldForScrap`. Erst danach bucht er einmal mit `AddAmount` (`Handel: … gebucht`).
- **Ablehnung:** Der Client rollt wie bei jedem Edit zurück; reparierte Items setzt er wieder kaputt (`StepKind::Break`, nur lokal),
  verkaufte kommen zurück. Den Schrott gleicht danach der nächste Stand des Hosts an.
- **Außerhalb einer Bearbeitung** verwirft der Client `HandleItemSold` und `RepairItem` (Warnung im Log), der Host verwirft ein direktes
  `RepairItem` am Clientinventar. Ohne Lager gibt es beim Client keinen Verkauf, bei dem der Host keinen Schrott bucht.
- **Grenze:** Die Chance auf eine kostenlose Reparatur (`FreeRepairChance`) würfelt nur das Panel des Clients; der Host bucht immer den
  Preis. Die Vorschau des Clients gleicht der Stand des Hosts danach an.

**Geprüft** (07.11.2026, Build `26495302`, ein PC, Lobby, nur Mod, ein Mensch spielte; kaputte Items per Speicherschreiben beim Host,
Tank mit `fuel.py --fill`):

- Lager Tag 1: Der Client verkaufte eine Pistole (+12) und reparierte einen Baseballschläger (18). Der Host prüfte „1 Verkäufe,
  1 Reparaturen“, buchte einmal (Schrott 16 zu 10) und übernahm vollständig. Danach `Get-Status -Inventory` gleich, Schrott bei beiden
  10, die Pistole bei beiden weg, der Schläger bei beiden heil im Rucksack.
- Lager Tag 2, Ablehnung: Der Client reparierte Schrotflinte und Granate mit lokal erhöhtem Schrott. Der Host lehnte ab (27 reichen
  nicht), der Client stellte den Ausgangsstand her (beide wieder kaputt, die Schrotflinte aus dem Rucksack) und übernahm 27.
  Inventare gleich.
- Der Spiegel übernahm Schrott aus der Mission laufend (auch lokal eingesammelten Schrott des Clients überschrieb der Stand des Hosts).
- Gegenproben im selben Lauf: zwei Airdrops mit Wahl des Clients übernommen, Bestätigen im Lager an beiden Tagen mit Reise.
- Der Reparaturversuch mit 16 Schrott scheiterte beim Client zu Recht (Preis 18). Der Tooltip zeigte 10, das ist nicht der native
  Reparaturpreis. **Vermutung:** Er zeigt einen anderen Wert.

**Nicht live geprüft:** Werkbank als Gegenprobe (ohne Spike lässt sie sich auf `26495302` nicht freischalten), Verkaufen im
Airdrop-Panel, Verkaufen mehrerer Items auf einmal (`SellStashItems`) und `FreeRepairChance`. Die Unit-Tests decken die Regeln ab.

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
`DD_SetCurrentRunSeed`; Ersatz mit Antworten ist der Testkanal `src/Mod/Control.cpp`, Aufgabe `mod-steuerung`), `Cheats.cpp` (CheatManager, `!load`), Godmode, `!finish` und `!round`
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
