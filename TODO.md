# TODO und Roadmap

Diese Datei ist die führende Aufgabenliste. Wer etwas übernimmt, reserviert es über einen Branch `task/<kennung>`, siehe [CONTRIBUTING.md](CONTRIBUTING.md#aufgaben-reservieren). Die Übersicht als Graph steht in [docs/ROADMAP.md](docs/ROADMAP.md).

**Vorgehen:** Das größte Risiko ist die Frage, ob native UE-Replikation im Spiel überhaupt noch funktioniert. Deshalb
wird das zuerst mit kleinen Spikes geprüft (Phase 1), bevor echte Mod-Infrastruktur entsteht.

| Priorität | Bedeutung |
|---|---|
| P0 | Blockiert andere Aufgaben |
| P1 | Nötig für das nächste Gate |
| P2 | Wichtig, aber später |
| Optional | Feinschliff, erst wenn die Mod lauffähig ist |

**Fokus: erst spielbar.** Vorrang hat, dass zwei Spieler einen Run durchspielen können. Kleine Fehler, die das nicht verhindern, werden nur unter
[Optionale Optimierungen](#optionale-optimierungen) festgehalten und erst behoben, wenn die Mod lauffähig ist.

## Wo kann ich anfangen?

Diese Aufgaben lassen sich unabhängig voneinander angehen. Oben stehen die, die die Spielbarkeit am meisten blockieren.
Für alle reicht die [Einrichtung](docs/SETUP.md), wenn nichts anderes dabeisteht.

**Spielzeit** sagt, wie sehr die Aufgabe laufende Testsitzungen braucht. Pro PC läuft nur eine Sitzung, Aufgaben mit „keine“ oder „wenig“ eignen sich also für parallele Arbeit.

**Gruppe** fasst zusammen, was thematisch zusammengehört. Verschiedene Gruppen lassen sich gleichzeitig bearbeiten. Innerhalb einer Gruppe gilt: `niederlage` kommt nach `rundenende`, `remote-p2` nach `spawnedplayers`, weil beide `Players.cpp` ändern.

Die Pakete für die Zeit nach Phase 1 stehen unter [Spätere Arbeitspakete](#spätere-arbeitspakete), alles, was nicht parallel geht, unter [Zentrale Arbeit](#zentrale-arbeit).

| Kennung | Gruppe | Aufgabe | Art | Spielzeit | Einstieg |
|---|---|---|---|---|---|
| `tag3` | Run | Host-Absturz bei der Reise auf Tag 3 klären, danach Inventar beim Client abschließen | Reverse Engineering, Spike | viel | [Inventar](research/spikes/a-connect/README.md#inventar-ursache-teilfix-und-verbleibende-grenze), [Abstürze auswerten](docs/TESTING.md#abstürze-auswerten), `tools/spike/inventory.py` |
| `rundenende` | Run | Rundenende, Lager und Routenkarte beim Client | Reverse Engineering, Spike | viel | [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf), `!finish`, `Mission.cpp` |
| `spawnedplayers` | Spieler | Remote-Controller sauber in `SpawnedPlayers` eintragen | Spike | mittel | `tools/spike/players.py`, `Players.cpp` |
| `remote-p2` | Spieler | Remote-Controller als P2 testen | Spike | mittel | [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf) |
| `karte-client` | Welt | Beim Client `MapSeed` und `MapGenProfile` des Hosts setzen, Duplikate zählen und unterdrücken | Spike | mittel | [Karte und replizierte Kartenobjekte](docs/GAME_ANALYSIS.md#karte-und-replizierte-kartenobjekte) |
| `treffer` | Spieler | Treffer des Clients direkt belegen | Spike | mittel | `AttributeSet` in [Offsets](docs/OFFSETS.md), Mission über das [Spielmenü](docs/TESTING.md#mission-über-das-spielmenü) |
| `last` | Technik | Last messen (Spike D) | Test | viel | Mission über das Menü, Bandbreite und Framerate bei voller Gegnerzahl |
| `tunnel` | Technik | Steam-Tunnel (Spike E) | C++, ohne Reverse Engineering, Steamworks SDK nötig | wenig | [Transport](docs/ARCHITECTURE.md#transport) |
| `crash-reporter` | Technik | Crash-Reporter im Mod- und Testbetrieb zuverlässig abschalten | Reverse Engineering | wenig | [Crash-Reporter](research/spikes/a-connect/README.md#crash-reporter) |
| `andockpunkte` | Technik | Proxy-DLL und Hook-Weg im laufenden Spiel erproben | C++, Spike | mittel | [Andockpunkte](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885), [Spike](research/spikes/andockpunkte/README.md) |
| `pausen` | Logik | Pausen bei Level-up und Inventar nachweisen | Spike | mittel | [Pausen-Spike](research/spikes/pausen/README.md) |
| `blueprint-logik` | Logik | Kontrollfluss und Events der Blueprints klären | Reverse Engineering | mittel | [Stichprobe](research/spikes/blueprint-logik/README.md) |
| `trennen` | Run | Client-Absturz beim Trennen klären (`0x5975A6A`) | Reverse Engineering, Spike | mittel | [Abstürze auswerten](docs/TESTING.md#abstürze-auswerten) |
| `niederlage` | Run | Echte Niederlage und Rückkehr in die `MetaBase`, erst nach `rundenende` | Spike | viel | [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf) |
| `tick-absturz` | Technik | Einmaligen Client-Absturz im Missions-Tick klären (`0x5A17ACC`) | Reverse Engineering | viel | [Bekannte Abweichungen](research/spikes/a-connect/README.md#bekannte-abweichungen) |
| `boss-dump` | Technik | SDK in einem Bosskampf dumpen | Reverse Engineering | mittel | [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) |

**Nicht erneut machen.** Das ist erledigt und dokumentiert:

- Iris oder klassisch: klassische Replikation, siehe [Spike](research/spikes/a-connect/README.md#ergebnis)
- Offsets und Patches für Build `26187885`, siehe [Offsets](docs/OFFSETS.md)
- Verbindung, Spawn, Bewegung und Schießen laufen mit den Patches, siehe [Spike](research/spikes/a-connect/README.md#patches)
- Weg vom Menü in die Mission, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf)
- Coop-Ablauf, Karten-Erzeugung, Gegner-Pooling und Kamera-Optionen sind analysiert, siehe [Spielanalyse](docs/GAME_ANALYSIS.md)
- Weitere Einzelprüfungen in den Inventar-Schleifen: Nach jeder stürzt der Client an der nächsten Stelle ab
- Eine höhere `NetPriority` des `GameState` hilft nicht gegen den HUD-Hänger, `OnRep_Slots` zurückzuhalten hilft nicht beim Inventar

## Spätere Arbeitspakete

Diese Pakete laufen unabhängig voneinander, sobald ihre Voraussetzung auf `main` ist. Vorher nicht reservieren. Jedes Paket arbeitet in seinem eigenen Ordner.
In `src/Game` bekommt jedes Paket eine eigene Adapter-Datei, siehe [Arbeitsbereiche](CONTRIBUTING.md#arbeitsbereiche). Die einzelnen Punkte stehen weiter in den Phasen unten.

**Welle 1: nach `grundgeruest`**

| Kennung | Paket | Ordner | Spielzeit |
|---|---|---|---|
| `overlay` | Overlay mit Status, Lobby-Knöpfen und Debug-Anzeige | `src/UI` | wenig |
| `deploy` | Deploy-Skript, das die Mod ins Spiel kopiert | `tools` | wenig |
| `ci` | Testprojekt und CI für Code ohne SDK-Abhängigkeit | `tests`, `.github` | keine |

**Welle 2: nach Gate G1 und `grundgeruest`**

| Kennung | Paket | Ordner | Spielzeit |
|---|---|---|---|
| `steam-lobby` | Lobby erstellen, Freunde einladen, beitreten | `src/Net` | wenig |
| `transport` | Tunnel aus Spike E als richtiges Modul | `src/Net` | wenig |
| `net-fehler` | Timeout, Abbruch, Host verlässt das Spiel, erst nach `transport` | `src/Net`, `src/UI` | wenig |

**Welle 3: nach `patches` und `remote-spieler`, Ziel ist Gate G2**

| Kennung | Paket | Ordner | Spielzeit |
|---|---|---|---|
| `missionsstart` | Gemeinsamer Missionsstart, gleiche Karte | `src/Sync/World` | viel |
| `kamera-code` | Entschiedene Kamera umsetzen | `src/Sync/World`, `src/Game` | mittel |
| `bewegung` | Bewegung, Dash und Blickrichtung | `src/Sync/Players` | mittel |
| `eingaben` | Eingaben des `CoopInputRouter` vom Remote-Spieler trennen | `src/Sync/Players` | mittel |
| `kampf` | Waffen, Projektile, Treffer, Schaden, Tod | `src/Sync/Combat` | mittel |
| `gegnertyp` | Ein Gegnertyp inklusive Spawn und Tod | `src/Sync/Enemies` | mittel |

`bewegung` und `eingaben` teilen sich einen Ordner, deshalb vorher die Dateien absprechen.

**Welle 4: nach Gate G2, die Pakete mit Kontrollkanal erst nach `protokoll`**

| Kennung | Paket | Ordner | Kontrollkanal | Spielzeit |
|---|---|---|---|---|
| `gegner-alle` | Alle Gegner, Bosse und Boss-Phasen | `src/Sync/Enemies` | nein | viel |
| `fortschritt` | Loot, XP, Level-ups, Fähigkeiten, Status-Effekte | `src/Sync/Combat` | ja | viel |
| `inventar-sync` | Rucksack, Stash, Werkbank, Reward-Queue | `src/Sync/Inventory` | ja | viel |
| `inventar-ui` | Inventar-UI beim Remote-Spieler, erst nach `inventar-sync` | `src/Sync/Inventory`, `src/UI` | ja | mittel |
| `pausen-sync` | Pausen und blockierende UI beider Spieler | `src/Sync/Players` | ja | mittel |
| `tagesablauf` | Tageszyklus, Treibstoff, Missionsziele, Etappen | `src/Sync/World` | ja | viel |
| `run-ende` | Run-Ende und Rückkehr zur Meta-Basis | `src/Sync/World` | ja | viel |
| `vfx-sound` | VFX und Sound beim Client, P2 | `src/Sync/Combat` | nein | mittel |
| `reconnect` | Wiederverbinden während eines Runs, P2 | `src/Net`, `src/Sync` | ja | viel |

`tagesablauf` und `run-ende` teilen sich einen Ordner, deshalb vorher die Dateien absprechen.

**Welle 5: vor dem Release, nach `game-adapter`**

| Kennung | Paket | Ordner | Spielzeit |
|---|---|---|---|
| `online-sperren` | Bestenlisten und Erfolge online sperren | `src/Game` | wenig |
| `telemetrie-aus` | Telemetrie, Bug- und Crash-Reports im Mod-Betrieb abschalten | `src/Game` | wenig |
| `mod-saves` | Mod-Spielstände vom normalen Spielstand trennen | `src/Game` | mittel |
| `installer` | Installer oder einfache Installationsanleitung | `tools`, `docs` | keine |

Zwei P2-Punkte haben bewusst noch kein Paket: die Herausforderung `WeeklyMode`, weil zuerst das Team entscheidet, und mehr als 2 Spieler, das kommt erst nach dem Release.

## Zentrale Arbeit

Diese Arbeit eignet sich nicht für paralleles Abarbeiten. Sie legt Schnittstellen fest, greift ineinander oder lässt sich nur als Ganzes testen. Es arbeitet immer nur eine Person oder ein Agent daran, reserviert wird wie gewohnt.

**Technik**

| Kennung | Arbeit | Warum zentral | Blockiert |
|---|---|---|---|
| `grundgeruest` | Solution, `Loader` und `Core` mit Logging, Config, Hooks, Dispatcher und Versionsprüfung | Alle Module bauen auf Struktur und Schnittstellen auf | Allen Mod-Code |
| `game-adapter` | SDK-Einbindung und Muster für die Adapter in `src/Game` | Einheitlicher Aufbau, den alle Adapter übernehmen | `patches`, Welle 3, Welle 5 |
| `patches` | Spike-Patches als geprüfte Hooks übernehmen | Die Patches greifen ineinander, eine Mission läuft erst mit allen zusammen | Welle 3 |
| `remote-spieler` | Remote-Spieler als P2 in die Coop-Logik einbinden | Grundlage für jeden Spieler-Sync, baut auf `remote-p2` auf | Welle 3 |
| `protokoll` | Handshake, Kontrollkanal und Nachrichtenformat | Eine Protokollversion für alle, nach dem [Vertrag](docs/SYNC_REQUIREMENTS.md#kontrollkanal-und-datenmodell) | Welle 4 mit Kontrollkanal |
| `integration-g2` | Vertical Slice zusammenführen und zehn Minuten zwischen zwei Haushalten testen | Prüft das Zusammenspiel aller Pakete, zwei Menschen nötig | Welle 4 |
| `integration-run` | Kompletten Run zu zweit über Internet durchspielen und Fehler zuordnen | Prüft das Zusammenspiel aller Pakete | Release |
| `release` | Versionierung, Abschlusstest und Veröffentlichung | Ein Stand, eine Freigabe | – |
| `spiel-update` | Bei einem Spiel-Update Offsets, Signaturen und Patches anpassen | Betrifft alles auf einmal, alle Spieltests warten darauf | Alle Spieltests |

**Entscheidungen und Außenwirkung**

Das macht das Team selbst, dafür gibt es keine Reservierung.

| Thema | Blockiert |
|---|---|
| Gate G1: Weg A, Weg B oder Stopp | Alles außer `grundgeruest` und Welle 1 |
| Gate G2: Testrunde abnehmen | Welle 4 |
| Kamera entscheiden | `kamera-code` |
| Aufteilung auf die Arbeitsbereiche, danach `CODEOWNERS` | – |
| Update-Regel und gemeinsame Build-ID | Verlässliche Spieltests |
| Meta-Fortschritt: Wessen Stand zählt? | `run-ende` |
| `WeeklyMode` unterstützen oder online sperren | – |
| Pixelsplit informieren und Rückmeldung berücksichtigen | `release` |
| Dumper-7-Fix dem Dumper-7-Projekt vorschlagen | – |

**Gemeinsame Dateien**

Diese Dateien nutzen alle, sie bleiben deshalb klein und ändern sich nur in kurzen eigenen PRs:

- `research/spikes/a-connect/Spike.h` und `Main.cpp`
- Öffentliche Schnittstellen in `src/Core` und `src/Net`
- Protokollversion und Nachrichtenformat, nur über `protokoll`

## Grundthemen bis Gate G1

Diese Themen müssen geklärt sein, bevor die eigentliche Umsetzung beginnt. Die einzelnen Aufgaben stehen in [Phase 1](#phase-1--analyse-und-spikes).
Die Reihenfolge ist die Arbeitsreihenfolge: Thema 1 entscheidet über den Weg, die übrigen legen fest, wie gebaut wird.

| # | Thema | Geklärt, wenn | Stand |
|---|---|---|---|
| 1 | **Netzwerkcode** | Zwei Instanzen sind in derselben Welt, Bewegung und Schießen funktionieren, Iris oder klassisch steht fest | Technisch geklärt: klassische Replikation, zwei Spieler in derselben Mission, Bewegung beidseitig, Schüsse des Clients wirken beim Host (indirekt belegt). Nur mit Patches, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| 2 | **Run-Ablauf** | Der Weg vom Menü in die Mission und zurück ist beschrieben, inklusive dessen, was beim Client laufen darf | Weitgehend geklärt, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf). Hinweg läuft mit Patches, der Client erreicht Tag 2, die Reise auf Tag 3 endet noch mit einem Host-Absturz. Offen: echte Niederlage und Rückkehr in die `MetaBase` |
| 3 | **Karte** | Beide Seiten haben dieselbe Karte, ohne doppelte Objekte | Seed gefunden. Im echten Ablauf ist die Karte des Clients eine andere (17 statt 144 bis 167 `MapTile`). Bodenkacheln sind nicht repliziert und in der `MetaBase` je Seite verschieden, das bricht die Client-Bewegung ohne Fix |
| 4 | **Remote-Spieler** | Ein Netzwerkspieler funktioniert in der Coop-Logik als P2 | Coop-Ablauf analysiert, der Test als P2 ist offen, siehe [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf) |
| 5 | **Kamera und Sichtbereich** | Gemeinsame oder getrennte Kamera ist entschieden, und das Gegner-Spawning passt dazu | Analysiert, gemeinsame Kamera läuft beim Client mit Patch und ist die Empfehlung. Es fehlt nur die Entscheidung des Teams, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) |
| 6 | **Spiellogik ohne Replikation** | Für XP, Währungen, Rewards, Pausen und UI ist je ein Sync-Weg festgelegt | Als Entwurf festgelegt, siehe [Sync-Bedarf](docs/SYNC_REQUIREMENTS.md). Nichts davon ist umgesetzt oder im Netz getestet |
| 7 | **Technische Andockpunkte** | Proxy-DLL gewählt, Hook-Weg über `ProcessEvent` und Signaturen an einem Beispiel erprobt | `version.dll` im isolierten Ladebeispiel und drei Signaturen geprüft. Laden im Spiel und Hook-Laufzeittest offen, siehe [Andockpunkte](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885) |

## Optionale Optimierungen

Bekannte kleine Fehler, die die Spielbarkeit nicht verhindern. Sie werden angegangen, sobald die Mod lauffähig ist.
Rückt ein Punkt die Spielbarkeit doch in Gefahr, wandert er in die passende Phase.

- [ ] Klären, warum der Host Remote-Figuren ohne clientautoritative Position fast nicht bewegt. Clientautoritative Position reicht für den Betrieb
- [ ] `MovementSpeed` und Beschleunigung: Upgrades kommen beim Client nicht an (Host 819 und 2866, Client 600 und 2048)
- [ ] Gegner-Zähler weichen ab (30 beim Host, 48 beim Client), vermutlich bleiben tote Gegner als Objekte stehen
- [ ] Blut- und Treffereffekte erscheinen nur beim Host
- [ ] Tutorial-Hinweise laufen auf beiden Seiten getrennt
- [ ] Host zeigt an Tag 2 ein anderes Figurenporträt

## Phase 0 – Projekt-Setup

Ziel: Alle können auf demselben Spielstand und mit derselben Toolchain arbeiten.

- [x] P0 Öffentliches GitHub-Repo anlegen
- [x] P0 Freunde als Mitwirkende einladen
- [x] P0 Branch-Schutz für `main` aktivieren
- [ ] P0 Team-Aufteilung auf die [Arbeitsbereiche](CONTRIBUTING.md#arbeitsbereiche) festlegen, danach `CODEOWNERS` anlegen
- [x] P0 SDK für UE 5.8 erstellen. Dumper-7 brauchte dafür einen Fix, siehe [Analyse](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58)
- [x] P0 SDK zusätzlich während einer laufenden Mission dumpen, damit die Blueprint-Klassen enthalten sind
- [ ] P2 SDK auch in einem Bosskampf dumpen, weil diese Klassen im Mission-Dump weitgehend fehlen
- [ ] P1 Dumper-7-Fix dem Dumper-7-Projekt vorschlagen, damit alle Helfer ihn ohne eigenen Patch bekommen. Bis dahin liegt der getestete Stand als Submodul unter `third_party/Dumper-7`, gebaut mit `tools/dumper7/Build-Dumper7.ps1`, upstream ist der Fehler als Issue #544 bekannt, siehe [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58)
- [x] P0 Visual-Studio-Solution mit einem Projekt je Modul anlegen, `config/Local.props` einbinden
- [ ] P1 Festlegen, wie das Team mit Spiel-Updates umgeht ([Update-Regel](CONTRIBUTING.md#spiel-updates))
- [ ] P1 Klären, ob alle das Spiel auf derselben Build-ID halten können

## Phase 1 – Analyse und Spikes

Ziel: Fakten für Gate G1 sammeln. Spike-Code ist Wegwerfcode und landet nicht in `src/`, sondern unter `research/spikes/`.

**Analyse**

- [x] P0 Config aus den Paks lesen: `IpNetDriver`, `DefaultPlatformService=Steam`, Standard-Maps, siehe [Analyse](docs/GAME_ANALYSIS.md#konfiguration)
- [x] P0 RPC-Liste aus dem neuen Dump gegen den Juli-Dump vergleichen: 3 RPCs entfernt, keiner hinzugekommen
- [x] P0 Replizierte Properties und `bReplicates` der Kernklassen erfassen: Der Kern repliziert, siehe [Analyse](docs/GAME_ANALYSIS.md#replikation-zur-laufzeit)
- [x] P0 `bReplicates` der Blueprint-Actors prüfen: 218 von 482 Blueprint-Actor-Klassen replizieren, vor allem über ihre native Basisklasse
- [x] P1 Klären, ob zur Laufzeit Iris oder die klassische Replikation aktiv ist: klassisch mit `ActorChannel`
- [x] P0 Run-Ablauf nachvollziehen: `TriggerStartRun`, `FinalizeRunStart`, Ladebildschirm und Wechsel zwischen den Etappen, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf)
- [ ] P0 Inventar beim Client lösen: Teilfix für fehlende Unterobjekt-Replikation in `Inventory.cpp`, ohne Null-Schutzsprünge zweimal bis Tag 2 getestet; Inventare beider Spieler vollständig und replizierte Werte identisch. Weiterreise auf Tag 3 endet noch beim Host im Callback `0x57CC395`/`0x57CC398`, Zusammenhang ungeklärt; kein kompletter Run belegt. Siehe [Inventar-Untersuchung](docs/GAME_ANALYSIS.md#inventar-unterobjekte-untersuchung-inventar)
- [ ] P0 Rundenende, Lager und Routenkarte beim Client abbilden: Host-Folge bis Routenkarte erneut geprüft; Client hat keine Route und erhält weder Rundengrund noch Endphasen-UI. Bestehende Funktionen sind keine RPCs, lokales Nachspielen und Panelpräsenz als Sync verworfen; Client-Diagnosebefehle jetzt gesperrt. Eigene rein anzeigende Phasenübertragung und Eingabesperre bleiben offen, siehe [Untersuchung](docs/GAME_ANALYSIS.md#rundenende-befund-und-offene-phasenübertragung-rundenende)
- [ ] P0 Neue Remote-Controller nach jedem Kartenwechsel sauber in `SpawnedPlayers` eintragen, der Spike ersetzt den Eintrag nur direkt
- [x] P1 Client-Absturz beim Trennen klären (`0x5975A6A`), wenn der Host ins Hauptmenü geht: Folge fehlender Inventar-Items in `UInventoryComponent::EndPlay`. Mit Inventar-Teilfix kein Absturz, ohne ihn reproduziert, siehe [Trennen](docs/GAME_ANALYSIS.md#trennen-ins-hauptmenü-trennen)
- [x] P1 Einmaligen Client-Absturz im Missions-Tick klären (`0x5A17ACC`, Lesen von `0x2C8`), trat bisher nur einmal auf: `MissionGameState` des Bewegungs-Restrictors war null, Absturz erst bei Bewegungseingabe, nachgestellt. Ursache des Null-Werts vermutlich ein später GameState, siehe [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz)
- [ ] P1 Echte Niederlage nachvollziehen: Alle Spieler tot, Rundenende, Rückkehr in die `MetaBase`, dabei den Client mitnehmen
- [x] P0 Coop-Ablauf nachvollziehen: Wie ruft das Spiel `SetCoopEnabled` und `SpawnLocalPlayer` auf, wie entsteht P2, und kommt der Code mit einem Controller ohne lokalen Spieler zurecht? P2 ist ein zweiter `ULocalPlayer` aus `SpawnLocalPlayer(1)`. Index-Abfragen fallen ohne `LocalPlayer` sauber zurück, 5 Stellen nicht, siehe [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf)
- [ ] P0 Remote-Controller als P2 testen: `SetCoopEnabled(true)` beim Host, Eintrag in `ActivePlayers` ohne `CreateLocalPlayer`, Wirkung auf Coop-UI und Eingabe-Router prüfen, siehe [Folgerungen](docs/GAME_ANALYSIS.md#coop-ablauf)
- [ ] P0 Kamera entscheiden: Was macht `AllPlayerFramingCamera`, und wie wirkt sich das Spawning an den Viewport-Rändern bei getrennten Kameras aus? Analyse und Optionen stehen in [Kamera und Sichtbereich](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich), Empfehlung: gemeinsame Kamera. Offen ist nur noch die Entscheidung des Teams
- [ ] P1 Technische Andockpunkte klären: geeignete Proxy-DLL, Hook-Weg über `ProcessEvent`, Signaturen für native Funktionen. `andockpunkte`: `version.dll` im isolierten Ladebeispiel und drei eindeutige Byte-Signaturen geprüft; Hook-Wege dokumentiert. Spiel-Laden und neuer Hook-Laufzeittest noch offen, siehe [Architektur](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885).
- [x] P0 Karten-Erzeugung verstehen: Wer startet den `MapGenerator`, und ergibt derselbe Seed dieselbe Karte? Start über `MapGenProfileInjectorAndBuildTrigger` auf beiden Seiten. Gleicher Run-Seed und gleiche Mission ergeben dieselbe Karte. Der Client hat keine Missionsdaten, deshalb `MapSeed=1` und eine gescheiterte Karte, siehe [Karte und Seed](docs/GAME_ANALYSIS.md#karte-und-seed)
- [x] P0 Klären, wie Karte und replizierte Kartenobjekte beim Client ohne Duplikate entstehen. Replizierte Kartenobjekte sind Child Actors der nicht replizierten Parzellen. Empfehlung: Client erzeugt die Karte selbst und unterdrückt lokale Kopien replizierter Klassen, siehe [Karte und replizierte Kartenobjekte](docs/GAME_ANALYSIS.md#karte-und-replizierte-kartenobjekte)
- [ ] P0 Spike: Beim Client `MapSeed` und `MapGenProfile` des Hosts setzen, Duplikate zählen und lokale Kopien replizierter Klassen unterdrücken
- [ ] P1 Doppelte `BP_ChaosDestructable_PowerUp_C` beim Client: Jede Kiste spawnt sie auch lokal
- [x] P1 Enemy-Spawning und `PoolManager` verstehen: Wie verhalten sich wiederverwendete Gegner bei Replikation? Tote Gegner werden zerstört, nicht gepoolt. `RecycleArray` versetzt lebende, entfernte Gegner. Der `PoolManager` betrifft Projektile und Effekte, siehe [Gegner-Spawning und Pooling](docs/GAME_ANALYSIS.md#gegner-spawning-und-pooling)
- [x] P1 Pausen-Logik klären: `pausen`: Inventar im Pausemenue auf Host und Client geoeffnet/geschlossen, verschachtelte Sitzungen und gleichzeitige Pausen gemessen; Host-Weltpause repliziert, Client-Wunsch nicht. Kontrollierte XP-Aufnahme bis Level 2 ohne anhaltende Pause; XP/Level bleiben beim Client unveraendert. Benoetigter Host-/Session-Sync und Testgrenzen im [Pausen-Spike](research/spikes/pausen/README.md#nachtrag-03112026-isolierte-ablaeufe). Analyse, kein implementierter Sync.
- [x] P1 Sync-Bedarf der nicht replizierten Komponenten festlegen: `LevelSystemComponent`, Wallets, `RerollCurrency`, `CoopInventoryCoordinatorComponent`. `sync-bedarf`: [Matrix und Kontrollvertrag](docs/SYNC_REQUIREMENTS.md) fuer alle Komponenten, XP, Waehrungen, Rewards, Pausen und UI festgelegt; Architekturentwurf, kein implementierter oder im Netz getesteter Sync. Offene Semantik-/Integrationsfragen sind dort benannt.
- [ ] P1 Blueprint-Logik auslesen: Was läuft dort bei Spawns, Effekten und Events, und was davon muss der Client sehen? `blueprint-logik`: [SDK-/Default-/Scriptreferenz-Stichprobe](research/spikes/blueprint-logik/README.md) fuer Spawner, Projektil, PowerUp-Kiste, Airdrop, Impact-Effekte und HUD dokumentiert. Vollstaendiger Kontrollfluss und konkrete Host-/Client-Eventtests noch offen; keine decompilierten Funktionskoerper im Repo.
- [ ] P1 Pixelsplit über das öffentliche Projekt informieren: Ist offizieller Online-Coop geplant, und wie stehen sie zu der Mod?

**Spikes**

- [x] P0 **Spike A – Verbindung:** Beide kommen in dieselbe Welt, auch auf einem PC, siehe [Ergebnis](research/spikes/a-connect/README.md)
- [x] P0 **Spike B – Spieler:** Client stabil in der Mission, Bewegung beidseitig, Client-Schüsse wirken beim Host, siehe [Ergebnis](research/spikes/a-connect/README.md)
- [ ] P0 Treffer des Clients direkt belegen, etwa über die Lebenspunkte eines Gegners, bisher nur aus Sterbeort und Reichweite geschlossen
- [x] P1 Mission über den echten Weg durch das Spielmenü mit den Patches starten, siehe [Ergebnis](research/spikes/a-connect/README.md)
- [x] P0 Crash-Reporter im Mod- und Testbetrieb zuverlässig abschalten, bisher hilft nur ein Skript. Testabstürze dürfen nicht bei Pixelsplit landen. Mod und Spike lassen den Start von `CrashReportClient.exe` scheitern, mit echten Abstürzen geprüft; `CrashGuard` bleibt erste Schicht für die Zeit vor dem Laden, siehe [Crash-Reporter](research/spikes/a-connect/README.md#sperre-in-mod-und-spike-crash-reporter)
- [ ] P1 **Spike C – Mission:** Lädt eine prozedurale Mission beim Client identisch? Direkt geladen: 17 `MapTile` je Seite und gleiches Bild am Start, kein Vergleich der ganzen Karte
- [ ] P1 **Spike D – Last:** Bandbreite und Framerate bei voller Gegnerzahl messen
- [ ] P1 **Spike E – Tunnel:** Spielverkehr über Steam P2P tunneln (siehe [Architektur](docs/ARCHITECTURE.md#transport))
- [x] P1 Testaufbau klären: Zwei Instanzen laufen auf einem PC unter demselben Steam-Konto, je mit eigenem `-UserDir=`

### Gate G1 – Technikentscheidung

Entscheiden und in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#entscheidungen) festhalten:

- **Weg A (native Replikation)**, wenn Spike A und B funktionieren und Spike D tragbar ist.
- **Weg B (eigene Snapshot-Synchronisierung)**, wenn native Replikation unbrauchbar ist.
- **Projekt stoppen oder verkleinern**, wenn keiner der Wege tragbar ist, etwa bei Logik, die fast nur in Blueprints liegt und nicht netzwerkfähig ist.

## Phase 2 – Fundament

Ziel: Stabile Basis, auf der alle parallel arbeiten können. Ab hier sind die Arbeitsbereiche weitgehend unabhängig.

- [x] P0 `Loader`: Proxy-DLL lädt die Mod beim Spielstart. Im Spiel mit Host und Client geprüft
- [x] P0 `Core`: Logging in Datei, Config, Hook-Framework
- [x] P0 `Core`: Dispatcher für Aufrufe auf dem Game Thread, `Drain` ruft erst `game-adapter` auf
- [x] P0 `Core`: Versionsprüfung – bei unbekannter Build-ID Mod sauber deaktivieren statt abstürzen
- [ ] P0 `Game`: Adapter-Schicht für die ersten Klassen
- [ ] P1 `UI`: Einfaches Overlay mit Status, Lobby-Knöpfen und Debug-Anzeige
- [x] P1 `tools`: Deploy-Skript, das die Mod ins Spiel kopiert, `tools/Deploy-Mod.ps1`
- [x] P1 `tests`: Testprojekt und CI für Code ohne SDK-Abhängigkeit

## Phase 3 – Session und Verbindung

Ziel: Zwei Spieler finden sich über Steam und verbinden sich ohne Portfreigabe.

- [ ] P0 Steam-Lobby erstellen, Freunde einladen, beitreten (`OnlineSubsystemSteam` oder direkt Steamworks)
- [ ] P0 Transport-Tunnel aus Spike E als richtiges Modul umsetzen
- [ ] P0 Handshake: Spiel-Build und Mod-Version müssen bei beiden gleich sein
- [ ] P0 Verlässlichen Kontrollkanal nach dem [Vertrag](docs/SYNC_REQUIREMENTS.md#kontrollkanal-und-datenmodell) bauen. Rundenende, Pausen, XP und Rewards brauchen ihn
- [ ] P1 Saubere Fehlerbehandlung: Timeout, Abbruch, Host verlässt das Spiel
- [ ] P2 Wiederverbinden während eines Runs (Hinweis: `OnCoopReconnectRequiredChanged`)

## Phase 4 – Vertical Slice

Ziel: Ein spielbarer Ausschnitt von vorne bis hinten. Zwei Spieler starten eine Mission, kämpfen gegen einen Gegnertyp und sehen dasselbe.

- [ ] P0 [Spike-Patches](research/spikes/a-connect/README.md#patches) als geprüfte Hooks nach `src/Game` übernehmen, ohne sie läuft keine Mission zu zweit
- [ ] P0 Remote-Spieler als P2 in die Coop-Logik einbinden, obwohl er kein lokaler Spieler ist
- [ ] P0 Gemeinsamer Missionsstart, gleiche Karte bei beiden
- [ ] P0 Entschiedene Kamera umsetzen, auch beim Client
- [ ] P0 Bewegung, Dash und Blickrichtung
- [ ] P0 Waffen, Projektile, Treffer, Schaden, Tod
- [ ] P0 Ein Gegnertyp inklusive Spawn und Tod
- [ ] P1 Eingaben aus dem lokalen `CoopInputRouter` sauber vom Remote-Spieler trennen
- [ ] P1 Beim Client `MissionGameState` des `InViewportMovementRestrictor` nachtragen, wenn der GameState erst nach dessen `BeginPlay` ankommt, sonst Absturz bei `0x5A17ACC`, siehe [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz)

### Gate G2 – Spielbarkeit

Eine zehnminütige Testrunde über echtes Internet zwischen zwei Haushalten: stabil, ohne Desync und spielbar.
Falls nicht, Ursachen beheben, bevor weitere Spielinhalte synchronisiert werden.

## Phase 5 – Vollständiger Run

Ziel: Ein kompletter Run ist zu zweit online spielbar.

- [ ] P0 Alle Gegner, Bosse und Boss-Phasen
- [ ] P0 Loot, XP, Level-ups, Fähigkeiten und Status-Effekte (GAS)
- [ ] P0 Inventar: Rucksack, Stash, Werkbank, Reward-Queue (`CoopInventoryCoordinator`)
- [ ] P0 Inventar-UI beim Remote-Spieler
- [ ] P0 Pausen und blockierende UI beider Spieler über den Kontrollkanal abstimmen
- [ ] P0 Tageszyklus, Treibstoff, Missionsziele, Fahrt zwischen den Etappen
- [ ] P1 Run-Ende und Rückkehr zur Meta-Basis
- [ ] P1 Vorgehen für Meta-Fortschritt: Wessen Stand zählt, was bekommt der Client?
- [ ] P2 VFX und Sound beim Client (FMOD)
- [ ] P2 Wöchentliche Herausforderung (WeeklyMode) entweder unterstützen oder online sperren

## Phase 6 – Release

- [ ] P0 **Bestenlisten und Erfolge online sperren**, damit Mod-Runs keine Ranglisten verfälschen
- [ ] P0 **Telemetrie, Bug-Reports und Crash-Reports im Mod-Betrieb abschalten**, damit Pixelsplit keine Mod-Fehler als Spielfehler bekommt
- [ ] P0 Mod-Spielstände vom normalen Spielstand trennen, damit kein Save beschädigt wird
- [ ] P1 Installer oder einfache Installationsanleitung
- [ ] P1 Vor dem ersten fertigen Release die Rückmeldung von Pixelsplit berücksichtigen
- [ ] P2 Mehr als 2 Spieler prüfen (Coop-Logik ist fest auf P1 und P2 ausgelegt, daher aufwendig)

## Risiken

| Risiko | Auswirkung | Gegenmaßnahme |
|---|---|---|
| RPCs sind nur Altlast aus dem Vor-Coop-Stand (Coop-Klassen haben keine RPCs) | Weg A fällt aus, deutlich mehr Eigenbau | Spike A und B zuerst; Weg B als Plan B |
| Logik liegt in Blueprints ohne Netzwerkcode | Viele Stellen brauchen eigene Hooks | Blueprint-Anteil früh abschätzen |
| Coop setzt zwei lokale Spieler voraus | Remote-P2 passt nicht in die Coop-Logik | Coop-Ablauf in Phase 1 analysieren |
| Viele Gegner überlasten das Netzwerk | Laggy oder unspielbar | Spike D; Gegner gebündelt synchronisieren |
| Karte entsteht lokal, Objekte darauf replizieren | Doppelte Objekte oder abweichende Karte beim Client | Spike C; Seed übertragen, Generator oder Replikation gezielt einschränken |
| Spiel-Updates (seit 1.0 am 29.10.2026 aktiv gepatcht) | Hooks und SDK veralten | Versionsprüfung, Update-Regel, Adapter-Schicht |
| Offizieller Online-Coop erscheint | Mod wird überflüssig | Bei Pixelsplit nachfragen, bevor viel Aufwand fließt |
