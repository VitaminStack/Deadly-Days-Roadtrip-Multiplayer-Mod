# TODO und Roadmap

Diese Datei ist die führende Aufgabenliste. Wer etwas übernimmt, reserviert es über einen Branch `task/<kennung>`, siehe [CONTRIBUTING.md](CONTRIBUTING.md#aufgaben-reservieren).

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

| Kennung | Aufgabe | Art | Spielzeit | Einstieg |
|---|---|---|---|---|
| `inventar` | Inventar beim Client lösen | Reverse Engineering, Spike | viel | [Offener Client-Absturz](research/spikes/a-connect/README.md#offener-client-absturz), [Abstürze auswerten](docs/TESTING.md#abstürze-auswerten), `Inventory.cpp` |
| `rundenende` | Rundenende, Lager und Routenkarte beim Client | Reverse Engineering, Spike | viel | [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf), `!finish`, `Mission.cpp` |
| `spawnedplayers` | Remote-Controller sauber in `SpawnedPlayers` eintragen | Spike | mittel | `tools/spike/players.py`, `Players.cpp` |
| `coop-ablauf` | Coop-Ablauf: `SetCoopEnabled`, `SpawnLocalPlayer`, Controller ohne lokalen Spieler | Reverse Engineering, SDK nötig | wenig | [Thema 4](#grundthemen-bis-gate-g1) |
| `karte` | Karten-Erzeugung: Wer startet den `MapGenerator`, ergibt derselbe Seed dieselbe Karte? | Reverse Engineering | wenig | `CurrentRunInfo` in [Offsets](docs/OFFSETS.md), `tools/spike/objects.py` zählt `MapTile` |
| `treffer` | Treffer des Clients direkt belegen | Spike | mittel | `AttributeSet` in [Offsets](docs/OFFSETS.md), Mission über das [Spielmenü](docs/TESTING.md#mission-über-das-spielmenü) |
| `last` | Last messen (Spike D) | Test | viel | Mission über das Menü, Bandbreite und Framerate bei voller Gegnerzahl |
| `tunnel` | Steam-Tunnel (Spike E) | C++, ohne Reverse Engineering, Steamworks SDK nötig | wenig | [Transport](docs/ARCHITECTURE.md#transport) |
| `grundgeruest` | Solution und Grundgerüst für `Loader` und `Core` | C++, ohne Spielwissen | keine | [Phase 0](#phase-0--projekt-setup), [Phase 2](#phase-2--fundament) |

**Nicht erneut machen.** Das ist erledigt und dokumentiert:

- Iris oder klassisch: klassische Replikation, siehe [Spike](research/spikes/a-connect/README.md#ergebnis)
- Offsets und Patches für Build `26187885`, siehe [Offsets](docs/OFFSETS.md)
- Verbindung, Spawn, Bewegung und Schießen laufen mit den Patches, siehe [Spike](research/spikes/a-connect/README.md#patches)
- Weg vom Menü in die Mission, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf)
- Weitere Einzelprüfungen in den Inventar-Schleifen: Nach jeder stürzt der Client an der nächsten Stelle ab
- Eine höhere `NetPriority` des `GameState` hilft nicht gegen den HUD-Hänger, `OnRep_Slots` zurückzuhalten hilft nicht beim Inventar

## Grundthemen bis Gate G1

Diese Themen müssen geklärt sein, bevor die eigentliche Umsetzung beginnt. Die einzelnen Aufgaben stehen in [Phase 1](#phase-1--analyse-und-spikes).
Die Reihenfolge ist die Arbeitsreihenfolge: Thema 1 entscheidet über den Weg, die übrigen legen fest, wie gebaut wird.

| # | Thema | Geklärt, wenn | Stand |
|---|---|---|---|
| 1 | **Netzwerkcode** | Zwei Instanzen sind in derselben Welt, Bewegung und Schießen funktionieren, Iris oder klassisch steht fest | Technisch geklärt: klassische Replikation, zwei Spieler in derselben Mission, Bewegung beidseitig, Schüsse des Clients wirken beim Host (indirekt belegt). Nur mit Patches, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| 2 | **Run-Ablauf** | Der Weg vom Menü in die Mission und zurück ist beschrieben, inklusive dessen, was beim Client laufen darf | Weitgehend geklärt, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf). Hinweg läuft mit Patches, der Client überlebt Tag 2 noch nicht. Offen: echte Niederlage und Rückkehr in die `MetaBase` |
| 3 | **Karte** | Beide Seiten haben dieselbe Karte, ohne doppelte Objekte | Seed gefunden. Im echten Ablauf ist die Karte des Clients eine andere (17 statt 144 bis 167 `MapTile`). Bodenkacheln sind nicht repliziert und in der `MetaBase` je Seite verschieden, das bricht die Client-Bewegung ohne Fix |
| 4 | **Remote-Spieler** | Ein Netzwerkspieler funktioniert in der Coop-Logik als P2 | Andockpunkte bekannt, Ablauf offen |
| 5 | **Kamera und Sichtbereich** | Gemeinsame oder getrennte Kamera ist entschieden, und das Gegner-Spawning passt dazu | Gemeinsame Kamera läuft beim Client mit Patch. Entscheidung offen |
| 6 | **Spiellogik ohne Replikation** | Für XP, Währungen, Rewards, Pausen und UI ist je ein Sync-Weg festgelegt | Lücken bekannt, Lösung offen |
| 7 | **Technische Andockpunkte** | Proxy-DLL gewählt, Hook-Weg über `ProcessEvent` und Signaturen an einem Beispiel erprobt | Offen |

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
- [ ] P0 Freunde als Mitwirkende einladen
- [x] P0 Branch-Schutz für `main` aktivieren
- [ ] P0 Team-Aufteilung auf die [Arbeitsbereiche](CONTRIBUTING.md#arbeitsbereiche) festlegen, danach `CODEOWNERS` anlegen
- [x] P0 SDK für UE 5.8 erstellen. Dumper-7 brauchte dafür einen Fix, siehe [Analyse](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58)
- [x] P0 SDK zusätzlich während einer laufenden Mission dumpen, damit die Blueprint-Klassen enthalten sind
- [ ] P2 SDK auch in einem Bosskampf dumpen, weil diese Klassen im Mission-Dump weitgehend fehlen
- [ ] P1 Dumper-7-Fix dem Dumper-7-Projekt vorschlagen, damit alle Helfer ihn ohne eigenen Patch bekommen
- [ ] P0 Visual-Studio-Solution mit einem Projekt je Modul anlegen, `config/Local.props` einbinden
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
- [ ] P0 Rundenende, Lager und Routenkarte beim Client abbilden, bisher spielt er nach dem Missionsende einfach weiter
- [ ] P0 Neue Remote-Controller nach jedem Kartenwechsel sauber in `SpawnedPlayers` eintragen, der Spike ersetzt den Eintrag nur direkt
- [ ] P1 Client-Absturz beim Trennen klären (`0x5975A6A`), wenn der Host ins Hauptmenü geht
- [ ] P1 Echte Niederlage nachvollziehen: Alle Spieler tot, Rundenende, Rückkehr in die `MetaBase`, dabei den Client mitnehmen
- [ ] P0 Coop-Ablauf nachvollziehen: Wie ruft das Spiel `SetCoopEnabled` und `SpawnLocalPlayer` auf, wie entsteht P2, und kommt der Code mit einem Controller ohne lokalen Spieler zurecht?
- [ ] P0 Kamera entscheiden: Was macht `AllPlayerFramingCamera`, und wie wirkt sich das Spawning an den Viewport-Rändern bei getrennten Kameras aus?
- [ ] P1 Technische Andockpunkte klären: geeignete Proxy-DLL, Hook-Weg über `ProcessEvent`, Signaturen für native Funktionen. `andockpunkte`: `version.dll` im isolierten Ladebeispiel und drei eindeutige Byte-Signaturen geprüft; Hook-Wege dokumentiert. Spiel-Laden und neuer Hook-Laufzeittest noch offen, siehe [Architektur](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885).
- [x] P0 Karten-Erzeugung verstehen: Wer startet den `MapGenerator`, und ergibt derselbe Seed dieselbe Karte? Start über `MapGenProfileInjectorAndBuildTrigger` auf beiden Seiten. Gleicher Run-Seed und gleiche Mission ergeben dieselbe Karte. Der Client hat keine Missionsdaten, deshalb `MapSeed=1` und eine gescheiterte Karte, siehe [Karte und Seed](docs/GAME_ANALYSIS.md#karte-und-seed)
- [ ] P0 Klären, wie Karte und replizierte Kartenobjekte beim Client ohne Duplikate entstehen
- [ ] P1 Enemy-Spawning und `PoolManager` verstehen: Wie verhalten sich wiederverwendete Gegner bei Replikation?
- [ ] P1 Pausen-Logik klären: Was passiert bei Level-up und Inventar, und was muss beim Client ankommen? (Hinweis: `UTimescaleManager`, `bBlocksGameplay`)
- [x] P1 Sync-Bedarf der nicht replizierten Komponenten festlegen: `LevelSystemComponent`, Wallets, `RerollCurrency`, `CoopInventoryCoordinatorComponent`. `sync-bedarf`: [Matrix und Kontrollvertrag](docs/SYNC_REQUIREMENTS.md) fuer alle Komponenten, XP, Waehrungen, Rewards, Pausen und UI festgelegt; Architekturentwurf, kein implementierter oder im Netz getesteter Sync. Offene Semantik-/Integrationsfragen sind dort benannt.
- [ ] P1 Blueprint-Logik auslesen: Was läuft dort bei Spawns, Effekten und Events, und was davon muss der Client sehen?
- [ ] P1 Pixelsplit über das öffentliche Projekt informieren: Ist offizieller Online-Coop geplant, und wie stehen sie zu der Mod?

**Spikes**

- [x] P0 **Spike A – Verbindung:** Beide kommen in dieselbe Welt, auch auf einem PC, siehe [Ergebnis](research/spikes/a-connect/README.md)
- [x] P0 **Spike B – Spieler:** Client stabil in der Mission, Bewegung beidseitig, Client-Schüsse wirken beim Host, siehe [Ergebnis](research/spikes/a-connect/README.md)
- [ ] P0 Treffer des Clients direkt belegen, etwa über die Lebenspunkte eines Gegners, bisher nur aus Sterbeort und Reichweite geschlossen
- [x] P1 Mission über den echten Weg durch das Spielmenü mit den Patches starten, siehe [Ergebnis](research/spikes/a-connect/README.md)
- [ ] P0 Crash-Reporter im Mod- und Testbetrieb zuverlässig abschalten, bisher hilft nur ein Skript. Testabstürze dürfen nicht bei Pixelsplit landen
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

- [ ] P0 `Loader`: Proxy-DLL lädt die Mod beim Spielstart
- [ ] P0 `Core`: Logging in Datei, Config, Hook-Framework
- [ ] P0 `Core`: Dispatcher für Aufrufe auf dem Game Thread
- [ ] P0 `Core`: Versionsprüfung – bei unbekannter Build-ID Mod sauber deaktivieren statt abstürzen
- [ ] P0 `Game`: Adapter-Schicht für die ersten Klassen
- [ ] P1 `UI`: Einfaches Overlay mit Status, Lobby-Knöpfen und Debug-Anzeige
- [ ] P1 `tools`: Deploy-Skript, das die Mod ins Spiel kopiert
- [ ] P1 `tests`: Testprojekt und CI für Code ohne SDK-Abhängigkeit

## Phase 3 – Session und Verbindung

Ziel: Zwei Spieler finden sich über Steam und verbinden sich ohne Portfreigabe.

- [ ] P0 Steam-Lobby erstellen, Freunde einladen, beitreten (`OnlineSubsystemSteam` oder direkt Steamworks)
- [ ] P0 Transport-Tunnel aus Spike E als richtiges Modul umsetzen
- [ ] P0 Handshake: Spiel-Build und Mod-Version müssen bei beiden gleich sein
- [ ] P1 Saubere Fehlerbehandlung: Timeout, Abbruch, Host verlässt das Spiel
- [ ] P2 Wiederverbinden während eines Runs (Hinweis: `OnCoopReconnectRequiredChanged`)

## Phase 4 – Vertical Slice

Ziel: Ein spielbarer Ausschnitt von vorne bis hinten. Zwei Spieler starten eine Mission, kämpfen gegen einen Gegnertyp und sehen dasselbe.

- [ ] P0 Remote-Spieler als P2 in die Coop-Logik einbinden, obwohl er kein lokaler Spieler ist
- [ ] P0 Gemeinsamer Missionsstart, gleiche Karte bei beiden
- [ ] P0 Bewegung, Dash und Blickrichtung
- [ ] P0 Waffen, Projektile, Treffer, Schaden, Tod
- [ ] P0 Ein Gegnertyp inklusive Spawn und Tod
- [ ] P1 Eingaben aus dem lokalen `CoopInputRouter` sauber vom Remote-Spieler trennen

### Gate G2 – Spielbarkeit

Eine zehnminütige Testrunde über echtes Internet zwischen zwei Haushalten: stabil, ohne Desync und spielbar.
Falls nicht, Ursachen beheben, bevor weitere Spielinhalte synchronisiert werden.

## Phase 5 – Vollständiger Run

Ziel: Ein kompletter Run ist zu zweit online spielbar.

- [ ] P0 Alle Gegner, Bosse und Boss-Phasen
- [ ] P0 Loot, XP, Level-ups, Fähigkeiten und Status-Effekte (GAS)
- [ ] P0 Inventar: Rucksack, Stash, Werkbank, Reward-Queue (`CoopInventoryCoordinator`)
- [ ] P0 Inventar-UI beim Remote-Spieler
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
