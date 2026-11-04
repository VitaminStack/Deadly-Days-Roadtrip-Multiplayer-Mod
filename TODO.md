# TODO und Roadmap

Diese Datei ist die führende Aufgabenliste. **Die Checkboxen sind die verbindliche Quelle für offen/erledigt**, nicht vorhandene oder gelöschte Branches. Wer etwas übernimmt, reserviert es über einen Branch `task/<kennung>`, siehe [CONTRIBUTING.md](CONTRIBUTING.md#aufgaben-reservieren). Die Übersicht als Graph steht in [docs/ROADMAP.md](docs/ROADMAP.md).

Der [Fortschritt im README](README.md#fortschritt) zählt ausschließlich die Checkboxen der Phasen 0–6, ohne optionale Optimierungen und übergreifende Meilensteine. Nach jeder Änderung mit `py tools/task_status.py update` aktualisieren und mit `py tools/task_status.py check` prüfen; die CI prüft dies ebenfalls. Die unsichtbaren `<!-- tasks: kennung -->`-Marker direkt vor den Checkboxen ordnen die Pakete eindeutig zu. Umfasst ein Paket mehrere Checkboxen, ist es erst erledigt, wenn alle abgehakt sind. Neue Katalogeinträge brauchen eine solche Zuordnung. Ein gemergter Teil-PR schließt keinen noch offenen Punkt automatisch ab.

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

**Gruppe** fasst zusammen, was thematisch zusammengehört. Verschiedene Gruppen lassen sich gleichzeitig bearbeiten. Innerhalb einer Gruppe gilt: `niederlage` kommt nach `phasenadapter`, `guidcache` nach `testumgebung`. `remote-p2` und `controller-tausch` nicht gleichzeitig, weil beide `Players.cpp` ändern.

Die Pakete für die Zeit nach Phase 1 stehen unter [Spätere Arbeitspakete](#spätere-arbeitspakete), alles, was nicht parallel geht, unter [Zentrale Arbeit](#zentrale-arbeit).

| Kennung | Gruppe | Aufgabe | Art | Spielzeit | Einstieg |
|---|---|---|---|---|---|
| `testumgebung` | Run | Saubere Testumgebung belegen, bevor die Tageswechsel-Suche weitergeht: Jede Runde lädt vollständig, beide Spieler erscheinen am Bus, auf beiden Seiten 2 PlayerStates. Ein Prüfskript für jeden Wechsel (Welt, `PlayerArray`, Figurenpositionen am Bus, `guidcache.py`, `NewActor failed` im Log) und einen festen Routenlauf mit Seed ins Repo bringen. Läufe mit seltsamem Laden festhalten und klären, ob sie mit den fehlenden PlayerStates zusammenhängen | Spike, Test | viel | Abschnitt „Tageswechsel“ in [GAME_ANALYSIS](docs/GAME_ANALYSIS.md#tageswechsel-client-verliert-die-playerstates-tageswechsel-übergabestand), `tools/spike/Travel-Day.ps1`, `objects.py`, `guidcache.py` |
| `guidcache` | Run | Klären, wo dem Client an Tag 4 die NetGUID-Einträge `0x125` und `0x127` der PlayerState-Klasse verloren gehen, und das für jede Klasse, jeden Tag und den Endlosmodus beheben. Danach alle 8 Tage bis zum Endboss und den Endlosmodus über mindestens zwei Gebietswechsel fahren, Inventar abschließen | Reverse Engineering, Spike | viel | Offene Spuren im Abschnitt „Tageswechsel“ in [GAME_ANALYSIS](docs/GAME_ANALYSIS.md#tageswechsel-client-verliert-die-playerstates-tageswechsel-übergabestand), Layout in [OFFSETS](docs/OFFSETS.md) |
| `phasenadapter` | Run | Phasenadapter für Rundenende, Lager und Routenkarte im Spiel belegen: automatische Hostquellen, sichtbare Anzeige, vollständige Eingabesperre | Reverse Engineering, Spike | viel | `research/spikes/a-connect/RoundPhaseRuntime.md`, `Mission.cpp` |
| `controller-tausch` | Spieler | Den Weg finden, auf dem der Host nach einem Missionswechsel den Controller des Clients ersetzt, und dort `SpawnedPlayers` abgleichen, damit der Sekunden-Abgleich nichts mehr nachträgt | Reverse Engineering, Spike | mittel | Abschnitt „Abnahme auf der echten Route“ in der [Spike-README](research/spikes/a-connect/README.md#abnahme-auf-der-echten-route-spawnedplayers-lauf), `tools/spike/players.py` |
| `remote-p2` | Spieler | Remote-Controller als P2 testen | Spike | mittel | [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf) |
| `tunnel` | Technik | Steam-Tunnel (Spike E) | C++, ohne Reverse Engineering, Steamworks SDK nötig | wenig | [Transport](docs/ARCHITECTURE.md#transport) |
| `niederlage` | Run | Echte Niederlage und Rückkehr in die `MetaBase`, erst nach `phasenadapter` | Spike | viel | [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf) |

**Nicht erneut machen.** Das ist erledigt und dokumentiert:

- Iris oder klassisch: klassische Replikation, siehe [Spike](research/spikes/a-connect/README.md#ergebnis)
- Offsets und Patches für Build `26187885`, siehe [Offsets](docs/OFFSETS.md)
- Verbindung, Spawn, Bewegung und Schießen laufen mit den Patches, siehe [Spike](research/spikes/a-connect/README.md#patches)
- Weg vom Menü in die Mission, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf)
- Coop-Ablauf, Karten-Erzeugung, Gegner-Pooling und Kamera-Optionen sind analysiert, siehe [Spielanalyse](docs/GAME_ANALYSIS.md)
- Weitere Einzelprüfungen in den Inventar-Schleifen: Nach jeder stürzt der Client an der nächsten Stelle ab
- Eine höhere `NetPriority` des `GameState` hilft nicht gegen den HUD-Hänger, `OnRep_Slots` zurückzuhalten hilft nicht beim Inventar
- Gegen die fehlenden PlayerStates ab Tag 4 helfen weder Klassen im RootSet noch ein Patch am 90-s-Ablauf in `CleanReferences`, und eine `servertravel`-Schleife mit GC in jedem Frame stellt den Fehler nicht nach, siehe [Tageswechsel](docs/GAME_ANALYSIS.md#tageswechsel-client-verliert-die-playerstates-tageswechsel-übergabestand)

## Spätere Arbeitspakete

Diese Pakete laufen unabhängig voneinander, sobald ihre Voraussetzung auf `main` ist. Vorher nicht reservieren. Jedes Paket arbeitet in seinem eigenen Ordner.
In `src/Game` bekommt jedes Paket eine eigene Adapter-Datei, siehe [Arbeitsbereiche](CONTRIBUTING.md#arbeitsbereiche). Die einzelnen Punkte stehen weiter in den Phasen unten.

**Welle 1 ist abgeschlossen:** `overlay`, `deploy` und `ci`, siehe [Abgeschlossene Pakete](#abgeschlossene-pakete). Nicht erneut reservieren.

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
| Gemeinsame Kamera entschieden (03.11.2026) | `kamera-code` |
| Meta-Fortschritt: Wessen Stand zählt? | `run-ende` |
| `WeeklyMode` unterstützen oder online sperren | – |

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
| 1 | **Netzwerkcode** | Zwei Instanzen sind in derselben Welt, Bewegung und Schießen funktionieren, Iris oder klassisch steht fest | Technisch geklärt: klassische Replikation, zwei Spieler in derselben Mission, Bewegung beidseitig, Schüsse des Clients wirken beim Host (direkt belegt). Nur mit Patches, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| 2 | **Run-Ablauf** | Der Weg vom Menü in die Mission und zurück ist beschrieben, inklusive dessen, was beim Client laufen darf | Weitgehend geklärt, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf). Hinweg läuft mit Patches, der Client erreicht Tag 2, die Reise auf Tag 3 endet noch mit einem Host-Absturz. Offen: echte Niederlage und Rückkehr in die `MetaBase` |
| 3 | **Karte** | Beide Seiten haben dieselbe Karte, ohne doppelte Objekte | Seed gefunden. Im echten Ablauf ist die Karte des Clients eine andere (17 statt 144 bis 167 `MapTile`). Bodenkacheln sind nicht repliziert und in der `MetaBase` je Seite verschieden, das bricht die Client-Bewegung ohne Fix |
| 4 | **Remote-Spieler** | Ein Netzwerkspieler funktioniert in der Coop-Logik als P2 | Coop-Ablauf analysiert, der Test als P2 ist offen, siehe [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf) |
| 5 | **Kamera und Sichtbereich** | Gemeinsame oder getrennte Kamera ist entschieden, und das Gegner-Spawning passt dazu | Gemeinsame Kamera für den ersten spielbaren Stand entschieden (03.11.2026). Ursprünglicher Zoom und Bewegungsbegrenzung bleiben erhalten; Umsetzung in der Mod noch offen (`kamera-code`), siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) |
| 6 | **Spiellogik ohne Replikation** | Für XP, Währungen, Rewards, Pausen und UI ist je ein Sync-Weg festgelegt | Als Entwurf festgelegt, siehe [Sync-Bedarf](docs/SYNC_REQUIREMENTS.md). Nichts davon ist umgesetzt oder im Netz getestet |
| 7 | **Technische Andockpunkte** | Proxy-DLL gewählt, Hook-Weg über `ProcessEvent` und Signaturen an einem Beispiel erprobt | Vollständiger VERSION-Proxy, drei Live-Signaturen und kleiner ProcessEvent-Hook auf Host und Client erprobt, siehe [Andockpunkte](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885) |

## Abgeschlossene Pakete

Nicht erneut reservieren. Die Nachweise und verbindlichen Checkboxen bleiben in den Phasen unten erhalten. Die Tabelle ist ein Katalog, kein zweiter Abschlussstatus. Beim Abschluss ein Paket aus der Einstiegstabelle hierher verschieben; `tasks.py` liest unabhängig davon immer die zugeordneten Checkboxen.

| Kennung | Aufgabe |
|---|---|
| `boss-dump` | Bossklassen gezielt geladen und gedumpt (Phase 0) |
| `karte-client` | Host-Karte beim Client ohne replizierte Duplikate erzeugt (Phase 1) |
| `pcg` | PCG-Vegetation beim Client erzeugt (Phase 1) |
| `treffer` | Treffer des Clients direkt belegt (Phase 1) |
| `last` | Loopback-Last gemessen (Phase 1), kein Internet-Nachweis |
| `crash-reporter` | Reporter in Mod und Spike gesperrt, CrashGuard bleibt vorgeschaltet (Phase 1) |
| `andockpunkte` | Proxy und Hook-Weg im Spiel erprobt (Phase 1) |
| `pausen` | Pausen-Logik analysiert (Phase 1), Umsetzung bleibt `pausen-sync` |
| `blueprint-logik` | Repräsentative Blueprint-Pfade analysiert (Phase 1) |
| `trennen` | Trennungsabsturz eingegrenzt (Phase 1) |
| `tick-absturz` | Restrictor-Absturz nachgestellt (Phase 1), Laufzeitfix bleibt Phase 4 |
| `grundgeruest` | Loader und Core einschließlich Dispatcher und Versionsprüfung (Phase 2) |
| `overlay` | Sichtbare Host-/Client-Abnahme bestanden (Phase 2) |
| `deploy` | Deploy-Skript vorhanden (Phase 2) |
| `ci` | SDK-freie Tests und CI vorhanden (Phase 2) |

## Übergreifende Meilensteine und Wartung

Diese Pakete haben keine eigene gezählte Phasen-Checkbox. Ihr Abschluss wird hier festgehalten; sie erhöhen die Fortschrittszahlen im README nicht zusätzlich.

<!-- tasks: integration-g2 -->
- [ ] `integration-g2`: Gate G2 mit zehn Minuten zwischen zwei Haushalten abnehmen.
<!-- tasks: integration-run -->
- [ ] `integration-run`: Vollständigen Run zu zweit über Internet abnehmen.
<!-- tasks: release -->
- [ ] `release`: Versionierung, Abschlusstest und Veröffentlichung freigeben.
<!-- tasks: spiel-update -->
- [ ] `spiel-update`: Wartung nur bei einem konkret beschlossenen neuen Spiel-Build. Keine laufende Update-Aufgabe und keine Voraussetzung für den eingefrorenen Build.

## Optionale Optimierungen

Bekannte kleine Fehler, die die Spielbarkeit nicht verhindern. Sie werden angegangen, sobald die Mod lauffähig ist.
Rückt ein Punkt die Spielbarkeit doch in Gefahr, wandert er in die passende Phase.

- [ ] Klären, warum der Host Remote-Figuren ohne clientautoritative Position fast nicht bewegt. Clientautoritative Position reicht für den Betrieb
- [ ] `MovementSpeed` und Beschleunigung: Upgrades kommen beim Client nicht an (Host 819 und 2866, Client 600 und 2048)
- [ ] Gegner-Zähler weichen ab (30 beim Host, 48 beim Client), vermutlich bleiben tote Gegner als Objekte stehen
- [ ] Blut- und Treffereffekte erscheinen nur beim Host
- [ ] Tutorial-Hinweise laufen auf beiden Seiten getrennt
- [ ] Host zeigt an Tag 2 ein anderes Figurenporträt
- [ ] Optional getrennte Kameras je Spieler ermöglichen, erst nach dem ersten spielbaren Stand: Bewegungsbegrenzung anpassen, Host-/Client-Sichtbereiche für das Gegner-Spawning synchronisieren oder die Spawn-Logik ersetzen und Gegnerverteilung/Schwierigkeit prüfen. Keine Voraussetzung für den gemeinsamen Kameramodus, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich)

## Phase 0 – Projekt-Setup

Ziel: Alle können auf demselben Spielstand und mit derselben Toolchain arbeiten.

- [x] P0 Öffentliches GitHub-Repo anlegen
- [x] P0 Freunde als Mitwirkende einladen
- [x] P0 Branch-Schutz für `main` aktivieren
- [x] P0 Team-Aufteilung auf die [Arbeitsbereiche](CONTRIBUTING.md#arbeitsbereiche) festlegen, danach `CODEOWNERS` anlegen. Entschieden: Keine festen Bereiche je Person und kein `CODEOWNERS`. Die Arbeit teilt sich über die [Reservierung](CONTRIBUTING.md#aufgaben-reservieren) per `tools/tasks.py`, wo das möglich ist
- [x] P0 SDK für UE 5.8 erstellen. Dumper-7 brauchte dafür einen Fix, siehe [Analyse](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58)
- [x] P0 SDK zusätzlich während einer laufenden Mission dumpen, damit die Blueprint-Klassen enthalten sind
<!-- tasks: boss-dump -->
- [x] P2 SDK auch in einem Bosskampf dumpen, weil diese Klassen im Mission-Dump weitgehend fehlen. Ein echter Bosskampf war wegen des Host-Absturzes auf Tag 3 nicht erreichbar, die Klassen aller sechs Bosse sind deshalb in der Bosskarte gezielt geladen und gedumpt, siehe [Boss-Dump](docs/GAME_ANALYSIS.md#boss-dump-boss-dump)
- [x] P1 Dumper-7-Fix für alle Helfer ohne eigenen Patch bereitstellen. Entschieden: kein Beitrag upstream, der Fix liegt dauerhaft auf `main` des eigenen Forks `VitaminStack/Dumper-7`, eingebunden als Submodul `third_party/Dumper-7`, siehe [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58)
- [x] P0 Visual-Studio-Solution mit einem Projekt je Modul anlegen, `config/Local.props` einbinden
- [x] P1 Festlegen, wie das Team mit Spiel-Updates umgeht: einfrieren und nur bei Bedarf nachziehen ([Update-Regel](CONTRIBUTING.md#spiel-updates))
- [x] P1 Gleiche Build-ID für die Entwicklung festhalten (`build-id`): geprüfte, echte Dateikopie außerhalb von Steam mit `Freeze-GameBuild.ps1 -Activate`; Start und Deploy nutzen ausschließlich `PinnedGameDir` und prüfen alle eingefrorenen Dateien per SHA-256. Host/Client samt Mod auf `26187885` aus der Kopie verbunden getestet; Steam-Updates bleiben auf die Steam-Installation beschränkt, neue Build-Unterstützung erfolgt separat über `spiel-update`. [Einrichtung und Grenzen](docs/SETUP.md#gleiche-build-id-im-team), kein Zwei-PC-/Depot-Redownload-Test

## Phase 1 – Analyse und Spikes

Ziel: Fakten für Gate G1 sammeln. Spike-Code ist Wegwerfcode und landet nicht in `src/`, sondern unter `research/spikes/`.

**Analyse**

- [x] P0 Config aus den Paks lesen: `IpNetDriver`, `DefaultPlatformService=Steam`, Standard-Maps, siehe [Analyse](docs/GAME_ANALYSIS.md#konfiguration)
- [x] P0 RPC-Liste aus dem neuen Dump gegen den Juli-Dump vergleichen: 3 RPCs entfernt, keiner hinzugekommen
- [x] P0 Replizierte Properties und `bReplicates` der Kernklassen erfassen: Der Kern repliziert, siehe [Analyse](docs/GAME_ANALYSIS.md#replikation-zur-laufzeit)
- [x] P0 `bReplicates` der Blueprint-Actors prüfen: 218 von 482 Blueprint-Actor-Klassen replizieren, vor allem über ihre native Basisklasse
- [x] P1 Klären, ob zur Laufzeit Iris oder die klassische Replikation aktiv ist: klassisch mit `ActorChannel`
- [x] P0 Run-Ablauf nachvollziehen: `TriggerStartRun`, `FinalizeRunStart`, Ladebildschirm und Wechsel zwischen den Etappen, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf)
<!-- tasks: guidcache -->
- [ ] P0 Inventar beim Client lösen, für jeden Tag und den Endlosmodus: Unterobjekt-Teilfix, dazu aus `tag3` Achievement-Timer, Kartengenerator-Lambda und Exp-Aggregation. `tageswechsel` deckt alle Aufrufer der AttributeSet-Suche am Einstieg ab. Inventare an Tag 1 bis 3 vollständig und gleich. Ab Tag 4 fehlen dem Client die PlayerStates, weil seinem NetGUID-Cache die Einträge der PlayerState-Klasse fehlen, an Tag 5 stürzt er ab. Fertig erst, wenn kein Fix an einen bestimmten Tag gebunden ist, ein Run alle 8 Tage samt Gebietswechsel und Endboss schafft und der Endlosmodus danach über mindestens zwei Gebietswechsel weiterläuft, an jedem Tag mit `complete` und `equal`. Folgeaufgaben `testumgebung`, dann `guidcache`, siehe [Tageswechsel](docs/GAME_ANALYSIS.md#tageswechsel-client-verliert-die-playerstates-tageswechsel-übergabestand)
<!-- tasks: testumgebung -->
- [ ] P0 Saubere Testumgebung für den Run belegen: Jede Runde lädt vollständig, beide Spieler erscheinen am Bus, auf beiden Seiten 2 PlayerStates, mit Prüfskript für jeden Wechsel. Aufgabe `testumgebung`
<!-- tasks: phasenadapter -->
- [ ] P0 Rundenende, Lager und Routenkarte beim Client abbilden: Snapshot-Codec, Empfänger und lokaler Laufzeitadapter in `Mission.cpp` (Heartbeat, Dateitransport, Timeout, Textanzeige, Win32-Nachrichtenfilter) ohne Spiel getestet, bis `!phase bind` inaktiv. Automatische Hostquellen für Rundenübersicht, Lager und Route, sichtbarer Lauf und vollständige Eingabesperre fehlen, Folgeaufgabe `phasenadapter`, siehe [Untersuchung](docs/GAME_ANALYSIS.md#rundenende-befund-und-offene-phasenübertragung-rundenende)
<!-- tasks: controller-tausch -->
- [ ] P0 Neue Remote-Controller nach jedem Kartenwechsel sauber in `SpawnedPlayers` eintragen: echte TSet-Einfügung und SparseArray-Entfernung statt Schlüsselersetzung, nach zwei Missionstravels konsistent, synthetische Mengentests bestanden. Der Registrierungshook ist auf der echten Route belegt, deckt aber den Austausch des Client-Controllers nach einem Missionswechsel nicht ab, dort trägt der Sekunden-Abgleich nach. Folgeaufgabe `controller-tausch`, siehe [Spike](research/spikes/a-connect/README.md#abnahme-auf-der-echten-route-spawnedplayers-lauf)
<!-- tasks: trennen -->
- [x] P1 Client-Absturz beim Trennen klären (`0x5975A6A`), wenn der Host ins Hauptmenü geht: Folge fehlender Inventar-Items in `UInventoryComponent::EndPlay`. Mit Inventar-Teilfix kein Absturz, ohne ihn reproduziert, siehe [Trennen](docs/GAME_ANALYSIS.md#trennen-ins-hauptmenü-trennen)
<!-- tasks: tick-absturz -->
- [x] P1 Einmaligen Client-Absturz im Missions-Tick klären (`0x5A17ACC`, Lesen von `0x2C8`), trat bisher nur einmal auf: `MissionGameState` des Bewegungs-Restrictors war null, Absturz erst bei Bewegungseingabe, nachgestellt. Ursache des Null-Werts vermutlich ein später GameState, siehe [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz)
<!-- tasks: niederlage -->
- [ ] P1 Echte Niederlage nachvollziehen: Alle Spieler tot, Rundenende, Rückkehr in die `MetaBase`, dabei den Client mitnehmen
- [x] P0 Coop-Ablauf nachvollziehen: Wie ruft das Spiel `SetCoopEnabled` und `SpawnLocalPlayer` auf, wie entsteht P2, und kommt der Code mit einem Controller ohne lokalen Spieler zurecht? P2 ist ein zweiter `ULocalPlayer` aus `SpawnLocalPlayer(1)`. Index-Abfragen fallen ohne `LocalPlayer` sauber zurück, 5 Stellen nicht, siehe [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf)
<!-- tasks: remote-p2 -->
- [ ] P0 Remote-Controller als P2 testen: `SetCoopEnabled(true)` beim Host, Eintrag in `ActivePlayers` ohne `CreateLocalPlayer`, Wirkung auf Coop-UI und Eingabe-Router prüfen, siehe [Folgerungen](docs/GAME_ANALYSIS.md#coop-ablauf)
- [x] P0 Kamera entscheiden (03.11.2026): Gemeinsame Kamera (Option A) für den ersten spielbaren Stand festgelegt, mit ursprünglichem Zoom und Bewegungsbegrenzung. Analyse von `AllPlayerFramingCamera` und Viewport-Rand-Spawning in [Kamera und Sichtbereich](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich). Umsetzung bleibt `kamera-code`; getrennte Kameras sind nur ein optionales Zukunfts-TODO
<!-- tasks: andockpunkte -->
- [x] P1 Technische Andockpunkte klären: `andockpunkte`: vollständiger VERSION-Exportproxy, drei eindeutige Live-Signaturen und kleiner ProcessEvent-Hook auf Host und Client bis in die Mission erprobt (ein Originalaufruf, unveränderte Rückgabe, Slot wiederhergestellt). Grenzen und Fehlversuche in [Architektur](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885); kein fertiges Loader-/Core-Framework.
- [x] P0 Karten-Erzeugung verstehen: Wer startet den `MapGenerator`, und ergibt derselbe Seed dieselbe Karte? Start über `MapGenProfileInjectorAndBuildTrigger` auf beiden Seiten. Gleicher Run-Seed und gleiche Mission ergeben dieselbe Karte. Der Client hat keine Missionsdaten, deshalb `MapSeed=1` und eine gescheiterte Karte, siehe [Karte und Seed](docs/GAME_ANALYSIS.md#karte-und-seed)
- [x] P0 Klären, wie Karte und replizierte Kartenobjekte beim Client ohne Duplikate entstehen. Replizierte Kartenobjekte sind Child Actors der nicht replizierten Parzellen. Empfehlung: Client erzeugt die Karte selbst und unterdrückt lokale Kopien replizierter Klassen, siehe [Karte und replizierte Kartenobjekte](docs/GAME_ANALYSIS.md#karte-und-replizierte-kartenobjekte)
<!-- tasks: karte-client -->
- [x] P0 Spike: Beim Client `MapSeed` und `MapGenProfile` des Hosts setzen, Duplikate zählen und lokale Kopien replizierter Klassen unterdrücken. `MapGen.cpp` überträgt Seed, Profil, Größe und Tags, die Karte ist auf beiden Seiten gleich, lokale Kopien replizierter Child Actors gibt es nicht mehr, siehe [Client erzeugt die Host-Karte](docs/GAME_ANALYSIS.md#client-erzeugt-die-host-karte-karte-client)
<!-- tasks: pcg -->
- [x] P1 Beim Client fehlen Bäume und Pflanzen aus PCG (`BP_PCGVolume_C`), obwohl die Karte gleich ist. Klären, warum PCG dort nicht erzeugt. Der Graph kommt aus dem Biom der Route in der GameInstance, die beim Client leer ist. `Pcg.cpp` überträgt den Graphen und setzt die Seeds der Instanz-Komponenten, Vegetation und Baumarten sind auf beiden Seiten gleich, siehe [PCG-Vegetation](docs/GAME_ANALYSIS.md#pcg-vegetation-beim-client-pcg)
- [x] P1 Doppelte `BP_ChaosDestructable_PowerUp_C` beim Client: Jede Kiste spawnt sie auch lokal. Spike-Patch in `MapObjects.cpp` unterdrückt die lokale Kopie, beim Client bleibt nur die replizierte, siehe [Doppelte PowerUp-Objekte](docs/GAME_ANALYSIS.md#doppelte-powerup-objekte-powerup-duplikat)
- [x] P1 Enemy-Spawning und `PoolManager` verstehen: Wie verhalten sich wiederverwendete Gegner bei Replikation? Tote Gegner werden zerstört, nicht gepoolt. `RecycleArray` versetzt lebende, entfernte Gegner. Der `PoolManager` betrifft Projektile und Effekte, siehe [Gegner-Spawning und Pooling](docs/GAME_ANALYSIS.md#gegner-spawning-und-pooling)
<!-- tasks: pausen -->
- [x] P1 Pausen-Logik klären: `pausen`: Inventar im Pausemenue auf Host und Client geoeffnet/geschlossen, verschachtelte Sitzungen und gleichzeitige Pausen gemessen; Host-Weltpause repliziert, Client-Wunsch nicht. Kontrollierte XP-Aufnahme bis Level 2 ohne anhaltende Pause; XP/Level bleiben beim Client unveraendert. Benoetigter Host-/Session-Sync und Testgrenzen im [Pausen-Spike](research/spikes/pausen/README.md#nachtrag-03112026-isolierte-ablaeufe). Analyse, kein implementierter Sync.
- [x] P1 Sync-Bedarf der nicht replizierten Komponenten festlegen: `LevelSystemComponent`, Wallets, `RerollCurrency`, `CoopInventoryCoordinatorComponent`. `sync-bedarf`: [Matrix und Kontrollvertrag](docs/SYNC_REQUIREMENTS.md) fuer alle Komponenten, XP, Waehrungen, Rewards, Pausen und UI festgelegt; Architekturentwurf, kein implementierter oder im Netz getesteter Sync. Offene Semantik-/Integrationsfragen sind dort benannt.
<!-- tasks: blueprint-logik -->
- [x] P1 Blueprint-Logik auslesen: `blueprint-logik`: repraesentative Spawn-/Effekt-/Eventpfade durch Bytecode-Grenzaudit (42 Funktionen, 25 Eventgraph-Einspruenge) und Host-/Client-Traces geklaert. Kisten-Multicast zugestellt, Airdrop ohne Client-Actor, Projektilgroesse nicht automatisch nachgeliefert, Itemeffekt-Guards und lokale HUD-Ereignisse belegt. Benoetigten Client-Zustand und verbleibende Integrationspruefungen im [Nachtrag](research/spikes/blueprint-logik/README.md#nachtrag-03112026-grenzpruefung-und-ereignistests) festgehalten; kein Audit aller Blueprints und kein implementierter Sync.

**Spikes**

- [x] P0 **Spike A – Verbindung:** Beide kommen in dieselbe Welt, auch auf einem PC, siehe [Ergebnis](research/spikes/a-connect/README.md)
- [x] P0 **Spike B – Spieler:** Client stabil in der Mission, Bewegung beidseitig, Client-Schüsse wirken beim Host, siehe [Ergebnis](research/spikes/a-connect/README.md)
<!-- tasks: treffer -->
- [x] P0 Treffer des Clients direkt belegen, etwa über die Lebenspunkte eines Gegners, bisher nur aus Sterbeort und Reichweite geschlossen. `Hits.cpp` liest beim Host jede Treffermeldung: Projektile des Clients ziehen Leben ab und töten, 50 Treffer und 25 Kills in rund 2 Minuten, siehe [Treffer des Clients](docs/GAME_ANALYSIS.md#treffer-des-clients-treffer)
- [x] P1 Mission über den echten Weg durch das Spielmenü mit den Patches starten, siehe [Ergebnis](research/spikes/a-connect/README.md)
<!-- tasks: crash-reporter -->
- [x] P0 Crash-Reporter im Mod- und Testbetrieb zuverlässig abschalten, bisher hilft nur ein Skript. Testabstürze dürfen nicht bei Pixelsplit landen. Mod und Spike lassen den Start von `CrashReportClient.exe` scheitern, mit echten Abstürzen geprüft; `CrashGuard` bleibt erste Schicht für die Zeit vor dem Laden, siehe [Crash-Reporter](research/spikes/a-connect/README.md#sperre-in-mod-und-spike-crash-reporter)
- [ ] P1 **Spike C – Mission:** Lädt eine prozedurale Mission beim Client identisch? Direkt geladen: 17 `MapTile` je Seite und gleiches Bild am Start, kein Vergleich der ganzen Karte
<!-- tasks: last -->
- [x] P1 **Spike D – Last:** Bandbreite und Framerate bei voller Gegnerzahl messen. Bei rund 85 Gegnern sendet der Host 47 KB/s an einen Client, knapp die Hälfte der ausgehandelten 100.000 B/s, beide Instanzen bei 60 fps. Nur ein PC über Loopback, siehe [Last](docs/GAME_ANALYSIS.md#last-spike-d-last)
<!-- tasks: tunnel -->
- [ ] P1 **Spike E – Tunnel:** Spielverkehr über Steam P2P tunneln (siehe [Architektur](docs/ARCHITECTURE.md#transport))
- [x] P1 Testaufbau klären: Zwei Instanzen laufen auf einem PC unter demselben Steam-Konto, je mit eigenem `-UserDir=`

### Gate G1 – Technikentscheidung

Entscheiden und in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#entscheidungen) festhalten:

- **Weg A (native Replikation)**, wenn Spike A und B funktionieren und Spike D tragbar ist.
- **Weg B (eigene Snapshot-Synchronisierung)**, wenn native Replikation unbrauchbar ist.
- **Projekt stoppen oder verkleinern**, wenn keiner der Wege tragbar ist, etwa bei Logik, die fast nur in Blueprints liegt und nicht netzwerkfähig ist.

## Phase 2 – Fundament

Ziel: Stabile Basis, auf der alle parallel arbeiten können. Ab hier sind die Arbeitsbereiche weitgehend unabhängig.

<!-- tasks: grundgeruest -->
- [x] P0 `Loader`: Proxy-DLL lädt die Mod beim Spielstart. Im Spiel mit Host und Client geprüft
<!-- tasks: grundgeruest -->
- [x] P0 `Core`: Logging in Datei, Config, Hook-Framework
<!-- tasks: grundgeruest -->
- [x] P0 `Core`: Dispatcher für Aufrufe auf dem Game Thread, `Drain` ruft erst `game-adapter` auf
<!-- tasks: grundgeruest -->
- [x] P0 `Core`: Versionsprüfung – bei unbekannter Build-ID Mod sauber deaktivieren statt abstürzen
<!-- tasks: game-adapter -->
- [ ] P0 `Game`: Adapter-Schicht für die ersten Klassen
<!-- tasks: overlay -->
- [x] P1 `UI`: Einfaches Overlay mit Status, Lobby-Knöpfen und Debug-Anzeige (`overlay`): F8-Toolfenster, kopierter Status und geprüfte Game-Thread-Aktionen implementiert; ohne Netzwerkbackend bleiben Lobby-Knöpfe deaktiviert. Modell-/Fenster-Lebenszyklustests sowie sichtbare Host-/Client-Abnahme bei 100 % und 150 % Skalierung bestanden: F8, Tab, Schließen, Fokuswechsel, Verschieben und Minimieren/Wiederherstellen. Steam-Anbindung bleibt Phase 3, siehe [UI](src/UI/README.md#sichtbare-spielabnahme-03112026)
<!-- tasks: deploy -->
- [x] P1 `tools`: Deploy-Skript, das die Mod ins Spiel kopiert, `tools/Deploy-Mod.ps1`
<!-- tasks: ci -->
- [x] P1 `tests`: Testprojekt und CI für Code ohne SDK-Abhängigkeit

## Phase 3 – Session und Verbindung

Ziel: Zwei Spieler finden sich über Steam und verbinden sich ohne Portfreigabe.

<!-- tasks: steam-lobby -->
- [ ] P0 Steam-Lobby erstellen, Freunde einladen, beitreten (`OnlineSubsystemSteam` oder direkt Steamworks)
<!-- tasks: transport -->
- [ ] P0 Transport-Tunnel aus Spike E als richtiges Modul umsetzen
<!-- tasks: protokoll -->
- [ ] P0 Handshake: Spiel-Build und Mod-Version müssen bei beiden gleich sein
<!-- tasks: protokoll -->
- [ ] P0 Verlässlichen Kontrollkanal nach dem [Vertrag](docs/SYNC_REQUIREMENTS.md#kontrollkanal-und-datenmodell) bauen. Rundenende, Pausen, XP und Rewards brauchen ihn
<!-- tasks: net-fehler -->
- [ ] P1 Saubere Fehlerbehandlung: Timeout, Abbruch, Host verlässt das Spiel
<!-- tasks: reconnect -->
- [ ] P2 Wiederverbinden während eines Runs (Hinweis: `OnCoopReconnectRequiredChanged`)

## Phase 4 – Vertical Slice

Ziel: Ein spielbarer Ausschnitt von vorne bis hinten. Zwei Spieler starten eine Mission, kämpfen gegen einen Gegnertyp und sehen dasselbe.

<!-- tasks: patches -->
- [ ] P0 [Spike-Patches](research/spikes/a-connect/README.md#patches) als geprüfte Hooks nach `src/Game` übernehmen, ohne sie läuft keine Mission zu zweit
<!-- tasks: remote-spieler -->
- [ ] P0 Remote-Spieler als P2 in die Coop-Logik einbinden, obwohl er kein lokaler Spieler ist
<!-- tasks: missionsstart -->
- [ ] P0 Gemeinsamer Missionsstart, gleiche Karte bei beiden
<!-- tasks: kamera-code -->
- [ ] P0 Gemeinsame Kamera (Option A) umsetzen, auch beim Client (`kamera-code`): ursprünglichen Zoom und Bewegungsbegrenzung erhalten
<!-- tasks: bewegung -->
- [ ] P0 Bewegung, Dash und Blickrichtung
<!-- tasks: kampf -->
- [ ] P0 Waffen, Projektile, Treffer, Schaden, Tod
<!-- tasks: gegnertyp -->
- [ ] P0 Ein Gegnertyp inklusive Spawn und Tod
<!-- tasks: eingaben -->
- [ ] P1 Eingaben aus dem lokalen `CoopInputRouter` sauber vom Remote-Spieler trennen
- [ ] P1 Beim Client `MissionGameState` des `InViewportMovementRestrictor` nachtragen, wenn der GameState erst nach dessen `BeginPlay` ankommt, sonst Absturz bei `0x5A17ACC`, siehe [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz)

### Gate G2 – Spielbarkeit

Eine zehnminütige Testrunde über echtes Internet zwischen zwei Haushalten: stabil, ohne Desync und spielbar.
Falls nicht, Ursachen beheben, bevor weitere Spielinhalte synchronisiert werden.

## Phase 5 – Vollständiger Run

Ziel: Ein kompletter Run ist zu zweit online spielbar.

<!-- tasks: gegner-alle -->
- [ ] P0 Alle Gegner, Bosse und Boss-Phasen
<!-- tasks: fortschritt -->
- [ ] P0 Loot, XP, Level-ups, Fähigkeiten und Status-Effekte (GAS)
<!-- tasks: inventar-sync -->
- [ ] P0 Inventar: Rucksack, Stash, Werkbank, Reward-Queue (`CoopInventoryCoordinator`)
<!-- tasks: inventar-ui -->
- [ ] P0 Inventar-UI beim Remote-Spieler
<!-- tasks: pausen-sync -->
- [ ] P0 Pausen und blockierende UI beider Spieler über den Kontrollkanal abstimmen
<!-- tasks: tagesablauf -->
- [ ] P0 Tageszyklus, Treibstoff, Missionsziele, Fahrt zwischen den Etappen
<!-- tasks: run-ende -->
- [ ] P1 Run-Ende und Rückkehr zur Meta-Basis
- [ ] P1 Vorgehen für Meta-Fortschritt: Wessen Stand zählt, was bekommt der Client?
<!-- tasks: vfx-sound -->
- [ ] P2 VFX und Sound beim Client (FMOD)
- [ ] P2 Wöchentliche Herausforderung (WeeklyMode) entweder unterstützen oder online sperren

## Phase 6 – Release

<!-- tasks: online-sperren -->
- [ ] P0 **Bestenlisten und Erfolge online sperren**, damit Mod-Runs keine Ranglisten verfälschen
<!-- tasks: telemetrie-aus -->
- [ ] P0 **Telemetrie, Bug-Reports und Crash-Reports im Mod-Betrieb abschalten**, damit Pixelsplit keine Mod-Fehler als Spielfehler bekommt
<!-- tasks: mod-saves -->
- [ ] P0 Mod-Spielstände vom normalen Spielstand trennen, damit kein Save beschädigt wird
<!-- tasks: installer -->
- [ ] P1 Installer oder einfache Installationsanleitung
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
