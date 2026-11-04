# TODO

Diese Datei ist die einzige Aufgabenliste des Projekts. **Jede Aufgabe steht genau einmal:** offen in der Tabelle ihrer Phase, erledigt in der Tabelle [Erledigt](#erledigt). Einen zweiten Status gibt es nicht. `py tools/task_status.py check` prüft das Format, die Abhängigkeiten und den Fortschritt im README, die CI ebenso.

Den Überblick als Graph zeigt [docs/ROADMAP.md](docs/ROADMAP.md), den Fortschritt je Phase das [README](README.md#fortschritt).

**Fokus: erst spielbar.** Vorrang hat, dass zwei Spieler einen Run durchspielen können. Kleine Fehler, die das nicht verhindern, kommen unter [Optionale Optimierungen](#optionale-optimierungen) und warten, bis der Run zu zweit läuft.

## So liest man die Tabellen

| Spalte | Bedeutung |
|---|---|
| **Kennung** | Eindeutiger Name, zugleich Branch `task/<kennung>` und Worktree `.worktrees/<kennung>` |
| **Prio** | **P0** blockiert andere Aufgaben oder den Run, **P1** nötig für das nächste Gate oder den Release, **P2** wichtig, aber später |
| **Aufgabe** | Was zu tun ist, **Fertig, wenn** nennt die Abnahme, **Einstieg** die Unterlagen |
| **Braucht** | Kennungen, die vorher erledigt sein müssen. `–` heißt keine Voraussetzung |
| **Bearbeitung** | **parallel**: läuft neben anderen Aufgaben. **zentral**: legt gemeinsame Schnittstellen fest oder lässt sich nur als Ganzes testen, immer nur eine Person, nie aufteilen. **Team**: Entscheidung oder Abnahme durch die Menschen im Team, keine Reservierung |
| **Spielzeit** | Wie sehr die Aufgabe Testsitzungen braucht: keine, wenig, mittel, viel. Pro PC läuft nur eine Sitzung |

Daraus ergibt sich der Status, den `py tools/tasks.py list` anzeigt:

| Status | Bedeutung |
|---|---|
| frei | Alle Voraussetzungen erledigt, niemand hat reserviert. Darf genommen werden |
| reserviert, in Arbeit | Branch oder Draft-PR existiert. Nicht nehmen |
| blockiert | Mindestens eine Kennung aus „Braucht“ ist offen. `tasks.py claim` lehnt ab |
| Team | Entscheidet das Team, `tasks.py claim` lehnt ab |
| erledigt | Steht unter [Erledigt](#erledigt). Nie erneut reservieren, auch wenn der Branch gelöscht ist oder die GitHub CLI fehlt |

## Wo kann ich anfangen?

Diese Liste erzeugt `py tools/task_status.py update` aus den Tabellen, oben steht das Wichtigste. Ob jemand eine Aufgabe schon reserviert hat, zeigt nur `py tools/tasks.py list`.

<!-- frei:start -->
| Kennung | Prio | Phase | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `phasenadapter` | P0 | [1](#phase-1--analyse-und-spikes) | parallel | viel |
| `remote-p2` | P0 | [1](#phase-1--analyse-und-spikes) | parallel | mittel |
| `steam-lobby` | P0 | [3](#phase-3--session-und-verbindung) | parallel | wenig |
| `patches` | P0 | [4](#phase-4--vertical-slice) | zentral | viel |
| `mod-saves` | P0 | [6](#phase-6--release) | parallel | mittel |
| `online-sperren` | P0 | [6](#phase-6--release) | parallel | wenig |
| `telemetrie-aus` | P0 | [6](#phase-6--release) | parallel | wenig |
| `tutorial-aus` | P0 | [6](#phase-6--release) | parallel | wenig |
| `tunnel` | P1 | [1](#phase-1--analyse-und-spikes) | parallel | wenig |
| `installer` | P1 | [6](#phase-6--release) | parallel | keine |

**Wartet auf das Team:** `meta-fortschritt`, `weeklymode`, `spiel-update`.
<!-- frei:end -->

**Nicht erneut machen.** Das ist erledigt und dokumentiert:

- Iris oder klassisch: klassische Replikation, siehe [Spike](research/spikes/a-connect/README.md#ergebnis)
- Verbindung, Spawn, Bewegung und Schießen laufen mit den Patches, siehe [Spike](research/spikes/a-connect/README.md#patches)
- Weg vom Menü in die Mission und über die Route bis Tag 4, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf) und [Testen](docs/TESTING.md#fester-routenlauf)
- Coop-Ablauf, Karten-Erzeugung, Gegner-Pooling, Kamera, Pausen und Sync-Bedarf sind analysiert, siehe [Spielanalyse](docs/GAME_ANALYSIS.md) und [Sync-Bedarf](docs/SYNC_REQUIREMENTS.md)
- Weitere Einzelprüfungen in den Inventar-Schleifen: Nach jeder stürzt der Client an der nächsten Stelle ab
- Eine höhere `NetPriority` des `GameState` hilft nicht gegen den HUD-Hänger, `OnRep_Slots` zurückzuhalten hilft nicht beim Inventar
- Gegen fehlende PlayerStates keine Klassen im RootSet pinnen oder Read-only-Zeitstempel blind zurücksetzen. Der abgeschlossene `guidcache`-Fix erhält gezielt nachladbare Pfadidentitäten und prüft den Cache-eigenen Driver auch bei nullem `GWorld`. Eine `servertravel`-Schleife ersetzt den echten Routenlauf nicht; [Nachweise und akzeptierter Abschluss](docs/GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache). Run/Endlos bis Tag 13 mit zwei Biomwechseln ist vorerst ausreichend abgesichert; nur neue konkrete Fehler bekommen ein neues TODO.
- Pause ab Tag 2 und fehlende Client-Figur am Bus sind im Spike behoben. Bei seltsamem Laden zuerst `Run-Route.ps1` und `check_round.py` prüfen, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung)

**Gemeinsame Dateien.** Diese Dateien nutzen alle. Sie bleiben klein und ändern sich nur in kurzen eigenen PRs:

- `research/spikes/a-connect/Spike.h` und `Main.cpp`
- Öffentliche Schnittstellen in `src/Core` und `src/Net`
- Protokollversion und Nachrichtenformat, nur über `protokoll`

## Aufgabe abschließen

1. Ergebnis und Testgrenzen in der Doku festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](docs/TESTING.md#ergebnisse-festhalten).
2. Nur bei erfüllter Abnahme („Fertig, wenn“): die Zeile aus der Phasentabelle löschen und unter [Erledigt](#erledigt) alphabetisch einfügen, als `` | `kennung` | Phase | Ergebnis in einem Satz mit Link | ``. Die Phase ist eine Ziffer von 0 bis 6, `Wartung` oder `Optional`.
3. Teilergebnis: Die Zeile bleibt offen. Den Text auf den Rest kürzen oder den Rest als neue Kennung in dieselbe Phase eintragen und die alte Zeile nach „Erledigt“ verschieben. Wer eine neue Kennung anlegt, trägt sie bei allen Aufgaben, die darauf warten, unter „Braucht“ ein.
4. `py tools/task_status.py update` ausführen. Das erneuert „Wo kann ich anfangen?“ und den Fortschritt im README. Danach muss `py tools/task_status.py check` bestehen.
5. In [docs/ROADMAP.md](docs/ROADMAP.md) den Knoten umfärben, wenn es einen gibt.

Nur die eigenen Zeilen ändern, das hält Konflikte klein. Ein gemergter Teil-PR schließt keine Aufgabe ab.

## Phase 0 – Projekt-Setup

Ziel: Alle arbeiten auf demselben Spielstand und mit derselben Toolchain. Alle Aufgaben sind erledigt.

## Phase 1 – Analyse und Spikes

Ziel: Fakten für Gate G1. Spike-Code ist Wegwerfcode und liegt unter `research/spikes/`, nicht in `src/`.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `phasenadapter` | P0 | Rundenende, Lager und Routenkarte beim Client abbilden. Codec, Empfänger und lokaler Laufzeitadapter in `Mission.cpp` sind ohne Spiel getestet. Es fehlen automatische Hostquellen für Rundenübersicht, Lager und Route, ein sichtbarer Lauf und die vollständige Eingabesperre. **Fertig, wenn** der Client bei jedem Rundenende dieselbe Phase sieht wie der Host und dort nichts auslösen kann. **Einstieg:** `research/spikes/a-connect/RoundPhaseRuntime.md`, [Rundenende](docs/GAME_ANALYSIS.md#rundenende-befund-und-offene-phasenübertragung-rundenende) | – | parallel | viel |
| `remote-p2` | P0 | Remote-Controller als P2 testen: `SetCoopEnabled(true)` beim Host, Eintrag in `ActivePlayers` ohne `CreateLocalPlayer`, Wirkung auf Coop-UI und Eingabe-Router prüfen. **Fertig, wenn** feststeht, ob und mit welchen Patches der Remote-Spieler als P2 läuft. **Einstieg:** [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf) | – | parallel | mittel |
| `tunnel` | P1 | Spike E: Spielverkehr über Steam P2P tunneln. C++ ohne Reverse Engineering, Steamworks SDK nötig. **Fertig, wenn** zwei Instanzen über den Tunnel verbunden spielen. **Einstieg:** [Transport](docs/ARCHITECTURE.md#transport) | – | parallel | wenig |
| `niederlage` | P1 | Echte Niederlage nachvollziehen: alle Spieler tot, Rundenende, Rückkehr in die `MetaBase`, dabei den Client mitnehmen. **Fertig, wenn** beide nach einer Niederlage in der `MetaBase` stehen und ein neuer Run startet. **Einstieg:** [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf) | `phasenadapter` | parallel | viel |

## Phase 2 – Fundament

Ziel: Stabile Basis der Mod, auf der alle parallel arbeiten können.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|

## Phase 3 – Session und Verbindung

Ziel: Zwei Spieler finden sich über Steam und verbinden sich ohne Portfreigabe.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `steam-lobby` | P0 | Steam-Lobby erstellen, Freunde einladen, beitreten, über `OnlineSubsystemSteam` oder direkt Steamworks. Ordner `src/Net` | `gate-g1` | parallel | wenig |
| `transport` | P0 | Transport-Tunnel aus Spike E als richtiges Modul. Ordner `src/Net` | `gate-g1`, `tunnel` | parallel | wenig |
| `protokoll` | P0 | Handshake (Spiel-Build und Mod-Version gleich) und verlässlicher Kontrollkanal mit Nachrichtenformat nach dem [Vertrag](docs/SYNC_REQUIREMENTS.md#kontrollkanal-und-datenmodell). Rundenende, Pausen, XP und Rewards brauchen ihn | `transport` | zentral | wenig |
| `net-fehler` | P1 | Saubere Fehlerbehandlung: Timeout, Abbruch, Host verlässt das Spiel. Ordner `src/Net`, `src/UI` | `transport` | parallel | wenig |

## Phase 4 – Vertical Slice

Ziel: Zwei Spieler starten eine Mission, kämpfen gegen einen Gegnertyp und sehen dasselbe. Abschluss ist Gate G2.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `patches` | P0 | [Spike-Patches](research/spikes/a-connect/README.md#patches) als geprüfte Hooks nach `src/Game` übernehmen. Die Patches greifen ineinander, ohne sie läuft keine Mission zu zweit | `game-adapter`, `gate-g1` | zentral | viel |
| `remote-spieler` | P0 | Remote-Spieler als P2 in die Coop-Logik einbinden, obwohl er kein lokaler Spieler ist. Grundlage für jeden Spieler-Sync | `patches`, `remote-p2` | zentral | mittel |
| `missionsstart` | P0 | Gemeinsamer Missionsstart, gleiche Karte bei beiden. Ordner `src/Sync/World` | `remote-spieler` | parallel | viel |
| `kamera-code` | P0 | Gemeinsame Kamera (Option A) auch beim Client umsetzen, ursprünglichen Zoom und Bewegungsbegrenzung erhalten. Ordner `src/Sync/World`, `src/Game`. **Einstieg:** [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) | `remote-spieler` | parallel | mittel |
| `bewegung` | P0 | Bewegung, Dash und Blickrichtung. Ordner `src/Sync/Players`, Dateien mit `eingaben` absprechen | `remote-spieler` | parallel | mittel |
| `kampf` | P0 | Waffen, Projektile, Treffer, Schaden, Tod. Ordner `src/Sync/Combat` | `remote-spieler` | parallel | mittel |
| `gegnertyp` | P0 | Ein Gegnertyp inklusive Spawn und Tod. Ordner `src/Sync/Enemies` | `remote-spieler` | parallel | mittel |
| `eingaben` | P1 | Eingaben aus dem lokalen `CoopInputRouter` sauber vom Remote-Spieler trennen. Ordner `src/Sync/Players`, Dateien mit `bewegung` absprechen | `remote-spieler` | parallel | mittel |
| `restrictor` | P1 | Beim Client `MissionGameState` des `InViewportMovementRestrictor` nachtragen, wenn der GameState erst nach dessen `BeginPlay` ankommt, sonst Absturz bei `0x5A17ACC`. **Einstieg:** [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) | `patches` | parallel | wenig |
| `integration-g2` | P0 | Gate G2: Vertical Slice zusammenführen und zehn Minuten über echtes Internet zwischen zwei Haushalten spielen. Zwei Menschen nötig. **Fertig, wenn** die Runde stabil, ohne Desync und spielbar ist. Sonst Ursachen beheben, bevor weitere Inhalte synchronisiert werden | `missionsstart`, `kamera-code`, `bewegung`, `kampf`, `gegnertyp`, `eingaben`, `restrictor`, `steam-lobby`, `transport`, `net-fehler` | zentral | viel |

## Phase 5 – Vollständiger Run

Ziel: Ein kompletter Run ist zu zweit online spielbar.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `gegner-alle` | P0 | Alle Gegner, Bosse und Boss-Phasen. Ordner `src/Sync/Enemies` | `integration-g2` | parallel | viel |
| `fortschritt` | P0 | Loot, XP, Level-ups, Fähigkeiten und Status-Effekte (GAS). Ordner `src/Sync/Combat` | `integration-g2`, `protokoll` | parallel | viel |
| `inventar-sync` | P0 | Rucksack, Stash, Werkbank, Reward-Queue (`CoopInventoryCoordinator`). Ordner `src/Sync/Inventory` | `integration-g2`, `protokoll`, `guidcache` | parallel | viel |
| `inventar-ui` | P0 | Inventar-UI beim Remote-Spieler. Ordner `src/Sync/Inventory`, `src/UI` | `inventar-sync` | parallel | mittel |
| `pausen-sync` | P0 | Pausen und blockierende UI beider Spieler über den Kontrollkanal abstimmen. Ordner `src/Sync/Players`. **Einstieg:** [Pausen-Spike](research/spikes/pausen/README.md) | `integration-g2`, `protokoll` | parallel | mittel |
| `tagesablauf` | P0 | Tageszyklus, Treibstoff, Missionsziele, Fahrt zwischen den Etappen. Ordner `src/Sync/World`, Dateien mit `run-ende` absprechen | `integration-g2`, `protokoll`, `guidcache`, `phasenadapter` | parallel | viel |
| `meta-fortschritt` | P1 | Entscheiden, wessen Meta-Fortschritt zählt und was der Client bekommt | – | Team | keine |
| `run-ende` | P1 | Run-Ende und Rückkehr zur Meta-Basis. Ordner `src/Sync/World`, Dateien mit `tagesablauf` absprechen | `integration-g2`, `protokoll`, `niederlage`, `meta-fortschritt` | parallel | viel |
| `integration-run` | P0 | Kompletten Run zu zweit über Internet durchspielen und Fehler den Aufgaben zuordnen. **Fertig, wenn** ein Run von der `MetaBase` bis zum Endboss ohne Absturz und Desync läuft | `gegner-alle`, `fortschritt`, `inventar-sync`, `inventar-ui`, `pausen-sync`, `tagesablauf`, `run-ende` | zentral | viel |
| `vfx-sound` | P2 | VFX und Sound (FMOD) beim Client. Ordner `src/Sync/Combat` | `integration-g2` | parallel | mittel |
| `reconnect` | P2 | Wiederverbinden während eines Runs, Hinweis: `OnCoopReconnectRequiredChanged`. Ordner `src/Net`, `src/Sync` | `integration-g2`, `protokoll`, `net-fehler` | parallel | viel |
| `weeklymode` | P2 | Entscheiden, ob die wöchentliche Herausforderung (`WeeklyMode`) unterstützt oder online gesperrt wird. Die Umsetzung bekommt danach eine eigene Kennung | – | Team | keine |

## Phase 6 – Release

Ziel: Die Mod lässt sich installieren, ohne Spielstände oder Bestenlisten zu berühren.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `tutorial-aus` | P0 | Tutorial-Hinweise im Coop-Betrieb auf beiden Seiten dauerhaft abschalten wie der Spike: `UTutorialStateSubsystem::SetForceSkipTutorials(true)`, sobald das Subsystem existiert, danach jede Sekunde prüfen. Vorrang, weil ein offener Hinweis die Welt für beide pausiert. Ordner `src/Game`. **Einstieg:** [Tutorial-Pause](docs/GAME_ANALYSIS.md#tutorial-pause-hält-den-run-an-testumgebung) | `game-adapter` | parallel | wenig |
| `online-sperren` | P0 | Bestenlisten und Erfolge online sperren, damit Mod-Runs keine Ranglisten verfälschen. Ordner `src/Game` | `game-adapter` | parallel | wenig |
| `telemetrie-aus` | P0 | Telemetrie, Bug-Reports und Crash-Reports im Mod-Betrieb abschalten, damit Pixelsplit keine Mod-Fehler als Spielfehler bekommt. Ordner `src/Game` | `game-adapter` | parallel | wenig |
| `mod-saves` | P0 | Mod-Spielstände vom normalen Spielstand trennen, damit kein Save beschädigt wird. Ordner `src/Game` | `game-adapter` | parallel | mittel |
| `installer` | P1 | Installer oder einfache Installationsanleitung. Ordner `tools`, `docs` | `game-adapter` | parallel | keine |
| `release` | P0 | Versionierung, Abschlusstest und Veröffentlichung. Ein Stand, eine Freigabe | `integration-run`, `tutorial-aus`, `online-sperren`, `telemetrie-aus`, `mod-saves`, `installer` | zentral | wenig |

## Wartung

Zählt nicht zum Fortschritt.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `spiel-update` | P2 | Nur wenn das Team einen neuen Spiel-Build beschließt: Offsets, Signaturen und Patches anpassen. Betrifft alles auf einmal, alle Spieltests warten darauf. **Einstieg:** [Update-Regel](CONTRIBUTING.md#spiel-updates) | – | Team | viel |

## Optionale Optimierungen

Bekannte kleine Fehler, die die Spielbarkeit nicht verhindern. Sie warten auf `integration-run`. Rückt ein Punkt die Spielbarkeit doch in Gefahr, wandert er mit P0 oder P1 in die passende Phase. Zählt nicht zum Fortschritt.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `opt-host-bewegung` | P2 | Klären, warum der Host Remote-Figuren ohne clientautoritative Position fast nicht bewegt. Clientautoritative Position reicht für den Betrieb | `integration-run` | parallel | mittel |
| `opt-tempo-upgrades` | P2 | `MovementSpeed` und Beschleunigung: Upgrades kommen beim Client nicht an (Host 819 und 2866, Client 600 und 2048) | `integration-run` | parallel | mittel |
| `opt-gegnerzahl` | P2 | Gegner-Zähler weichen ab (30 beim Host, 48 beim Client), vermutlich bleiben tote Gegner als Objekte stehen | `integration-run` | parallel | mittel |
| `opt-blut` | P2 | Blut- und Treffereffekte erscheinen nur beim Host | `integration-run` | parallel | wenig |
| `opt-portraet` | P2 | Host zeigt an Tag 2 ein anderes Figurenporträt | `integration-run` | parallel | wenig |
| `opt-kameras` | P2 | Getrennte Kameras je Spieler ermöglichen: Bewegungsbegrenzung anpassen, Sichtbereiche für das Gegner-Spawning synchronisieren oder die Spawn-Logik ersetzen, Gegnerverteilung prüfen. Keine Voraussetzung für die gemeinsame Kamera, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) | `integration-run` | parallel | viel |
| `mehr-spieler` | P2 | Mehr als 2 Spieler prüfen. Die Coop-Logik ist fest auf P1 und P2 ausgelegt, daher aufwendig | `release` | parallel | viel |

## Erledigt

Alphabetisch sortiert. Details stehen in der verlinkten Doku.

| Kennung | Phase | Ergebnis |
|---|---|---|
| `andockpunkte` | 1 | VERSION-Proxy, drei Live-Signaturen und ProcessEvent-Hook auf Host und Client erprobt, siehe [Andockpunkte](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885) |
| `blueprint-logik` | 1 | Repräsentative Spawn-, Effekt- und Eventpfade per Bytecode-Audit und Traces geklärt, siehe [Nachtrag](research/spikes/blueprint-logik/README.md#nachtrag-03112026-grenzpruefung-und-ereignistests) |
| `boss-dump` | 0 | Klassen aller sechs Bosse gezielt geladen und gedumpt, siehe [Boss-Dump](docs/GAME_ANALYSIS.md#boss-dump-boss-dump) |
| `branch-schutz` | 0 | Branch-Schutz für `main` aktiv |
| `build-id` | 0 | Geprüfte Kopie von Build `26187885` außerhalb von Steam, Start und Deploy prüfen alle Dateien, siehe [Einrichtung](docs/SETUP.md#gleiche-build-id-im-team) |
| `ci` | 2 | SDK-freie Tests und CI vorhanden |
| `config-paks` | 1 | Config aus den Paks gelesen: `IpNetDriver`, `DefaultPlatformService=Steam`, Standard-Maps, siehe [Konfiguration](docs/GAME_ANALYSIS.md#konfiguration) |
| `controller-tausch` | 1 | Controller-Tausch nach dem Missionswechsel über `K2_OnSwapPlayerControllers` sofort in `SpawnedPlayers` abgeglichen, siehe [Spike](research/spikes/a-connect/README.md#controller-tausch-nach-dem-missionswechsel-controller-tausch) |
| `coop-ablauf` | 1 | P2 ist ein zweiter `ULocalPlayer` aus `SpawnLocalPlayer(1)`, 5 Stellen kommen ohne `LocalPlayer` nicht zurecht, siehe [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf) |
| `crash-reporter` | 1 | Mod und Spike lassen den Start von `CrashReportClient.exe` scheitern, `CrashGuard` bleibt erste Schicht, siehe [Crash-Reporter](research/spikes/a-connect/README.md#sperre-in-mod-und-spike-crash-reporter) |
| `deploy` | 2 | Deploy-Skript `tools/Deploy-Mod.ps1` |
| `dumper-fix` | 0 | Dumper-7-Fix liegt im eigenen Fork, eingebunden als Submodul `third_party/Dumper-7`, siehe [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) |
| `game-adapter` | 2 | Game-Thread-Takt am Spielfenster ruft `Drain` und die Adapter-Ticks; `Offsets.h` mit geschützten Lesezugriffen, beim Bauen gegen ein lokales Dumper-7-SDK geprüft; `WorldAdapter` als Muster liest Karte, Rolle und Spielerzahl. Mit Host und Client bis Tag 1 geprüft, [Muster](src/Game/README.md) |
| `gate-g1` | 1 | Am 04.11.2026 vom Nutzer verbindlich entschieden: Weg A (native UE-Replikation) als Grundlage, ergänzt durch eigenen Kontroll-/Sync-Kanal für XP, Währungen, Rewards, Pausen und UI; Weg B nur Rückfalloption. [Architekturentscheidung](docs/ARCHITECTURE.md#entscheidungen) |
| `gegner-pooling` | 1 | Tote Gegner werden zerstört, nicht gepoolt, der `PoolManager` betrifft Projektile und Effekte, siehe [Gegner-Spawning](docs/GAME_ANALYSIS.md#gegner-spawning-und-pooling) |
| `github-repo` | 0 | Öffentliches GitHub-Repo angelegt |
| `grundgeruest` | 2 | `Loader` als Proxy-DLL, `Core` mit Logging, Config, Hooks, Dispatcher und Versionsprüfung, mit Host und Client geprüft |
| `guidcache` | 1 | Blockaden-Autorität, NetGUID-Pfadlebensdauer und Sneak-Zielbereitschaft korrigiert; Test-Godmode für beide Spieler, Lauf bis Endlos-Tag 13 mit zwei Biomwechseln. Am 04.11.2026 vom Nutzer als abgeschlossen akzeptiert; vorerst keine weiteren Detailtests, neue konkrete Probleme erhalten ein neues TODO. [Nachweise und Testgrenzen](docs/GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache) |
| `iris` | 1 | Zur Laufzeit ist die klassische Replikation mit `ActorChannel` aktiv, nicht Iris |
| `kamera-entscheidung` | 1 | Gemeinsame Kamera (Option A) mit ursprünglichem Zoom und Bewegungsbegrenzung, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) |
| `kamera-viewtarget` | 1 | Spike-Fix: Der Client schaute nach Kartenwechseln aus der eigenen Figur, weil die Kamera am alten Controller landete. Jetzt nur der lokale Controller der aktuellen Welt, Prüfung jede Sekunde, siehe [Kamera](docs/GAME_ANALYSIS.md#viewtarget-beim-client-kamera-viewtarget) |
| `karte-analyse` | 1 | Replizierte Kartenobjekte sind Child Actors der nicht replizierten Parzellen, siehe [Kartenobjekte](docs/GAME_ANALYSIS.md#karte-und-replizierte-kartenobjekte) |
| `karte-client` | 1 | Client erzeugt die Host-Karte ohne replizierte Duplikate (`MapGen.cpp`), siehe [Client erzeugt die Host-Karte](docs/GAME_ANALYSIS.md#client-erzeugt-die-host-karte-karte-client) |
| `karten-erzeugung` | 1 | Gleicher Run-Seed und gleiche Mission ergeben dieselbe Karte, siehe [Karte und Seed](docs/GAME_ANALYSIS.md#karte-und-seed) |
| `last` | 1 | Spike D: bei rund 85 Gegnern 47 KB/s vom Host, beide Instanzen 60 fps, nur Loopback, siehe [Last](docs/GAME_ANALYSIS.md#last-spike-d-last) |
| `menue-weg` | 1 | Mission über den echten Weg durch das Spielmenü mit den Patches gestartet, siehe [Spike](research/spikes/a-connect/README.md) |
| `mitwirkende` | 0 | Freunde als Mitwirkende eingeladen |
| `overlay` | 2 | F8-Overlay mit Status und Debug-Anzeige, sichtbare Host-/Client-Abnahme bestanden, siehe [UI](src/UI/README.md#sichtbare-spielabnahme-03112026) |
| `pausen` | 1 | Pausen-Logik gemessen, Host-Weltpause repliziert, Client-Wunsch nicht, siehe [Pausen-Spike](research/spikes/pausen/README.md#nachtrag-03112026-isolierte-ablaeufe) |
| `pcg` | 1 | Client erzeugt die PCG-Vegetation mit dem Graphen des Hosts (`Pcg.cpp`), siehe [PCG-Vegetation](docs/GAME_ANALYSIS.md#pcg-vegetation-beim-client-pcg) |
| `powerup-duplikat` | 1 | Lokale Kopie von `BP_ChaosDestructable_PowerUp_C` beim Client unterdrückt, siehe [PowerUp-Objekte](docs/GAME_ANALYSIS.md#doppelte-powerup-objekte-powerup-duplikat) |
| `replikation-blueprint` | 1 | 218 von 482 Blueprint-Actor-Klassen replizieren, vor allem über ihre native Basisklasse |
| `replikation-kern` | 1 | Replizierte Properties der Kernklassen erfasst, der Kern repliziert, siehe [Replikation](docs/GAME_ANALYSIS.md#replikation-zur-laufzeit) |
| `rpc-vergleich` | 1 | RPC-Liste gegen den Juli-Dump: 3 RPCs entfernt, keiner hinzugekommen |
| `run-ablauf` | 1 | `TriggerStartRun`, `FinalizeRunStart`, Ladebildschirm und Etappenwechsel nachvollzogen, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf) |
| `sdk` | 0 | SDK für UE 5.8 erzeugt, siehe [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) |
| `sdk-mission` | 0 | SDK zusätzlich während einer Mission gedumpt, mit Blueprint-Klassen |
| `solution` | 0 | Visual-Studio-Solution mit einem Projekt je Modul, `config/Local.props` eingebunden |
| `spike-a` | 1 | Spike A: Beide Instanzen kommen in dieselbe Welt, auch auf einem PC, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| `spike-b` | 1 | Spike B: Client stabil in der Mission, Bewegung beidseitig, Schüsse wirken beim Host, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| `spike-c` | 1 | Spike C: Mit `karte-client` und `pcg` ist die Mission beim Client gleich, `check_round.py` zählt an Tag 1 bis 4 gleich viele `MapTile`s, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung) |
| `sync-bedarf` | 1 | Sync-Weg für XP, Währungen, Rewards, Pausen und UI als Entwurf festgelegt, siehe [Sync-Bedarf](docs/SYNC_REQUIREMENTS.md) |
| `team-aufteilung` | 0 | Keine festen Bereiche je Person und kein `CODEOWNERS`, die Arbeit teilt sich über `tools/tasks.py` |
| `testaufbau` | 1 | Zwei Instanzen auf einem PC unter demselben Steam-Konto, je mit eigenem `-UserDir=` |
| `testumgebung` | 1 | Tag 1 bis 4 laden sauber mit Spawn am Bus und je 2 PlayerStates, Prüfskript und fester Routenlauf, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung) |
| `tick-absturz` | 1 | Restrictor-Absturz `0x5A17ACC` nachgestellt, Laufzeitfix ist `restrictor`, siehe [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) |
| `treffer` | 1 | Projektile des Clients ziehen beim Host Leben ab und töten (`Hits.cpp`), siehe [Treffer](docs/GAME_ANALYSIS.md#treffer-des-clients-treffer) |
| `trennen` | 1 | Client-Absturz beim Trennen war eine Folge fehlender Inventar-Items, mit Teilfix kein Absturz, siehe [Trennen](docs/GAME_ANALYSIS.md#trennen-ins-hauptmenü-trennen) |
| `update-regel` | 0 | Spiel-Updates: einfrieren und nur bei Bedarf nachziehen, siehe [Update-Regel](CONTRIBUTING.md#spiel-updates) |

## Risiken

| Risiko | Auswirkung | Gegenmaßnahme |
|---|---|---|
| RPCs sind nur Altlast aus dem Vor-Coop-Stand (Coop-Klassen haben keine RPCs) | Weg A fällt aus, deutlich mehr Eigenbau | Spike A und B zuerst; Weg B als Plan B |
| Logik liegt in Blueprints ohne Netzwerkcode | Viele Stellen brauchen eigene Hooks | Blueprint-Anteil früh abschätzen |
| Coop setzt zwei lokale Spieler voraus | Remote-P2 passt nicht in die Coop-Logik | `remote-p2` vor `remote-spieler` |
| Viele Gegner überlasten das Netzwerk | Laggy oder unspielbar | Spike D; Gegner gebündelt synchronisieren |
| Karte entsteht lokal, Objekte darauf replizieren | Doppelte Objekte oder abweichende Karte beim Client | Seed übertragen, lokale Kopien replizierter Klassen unterdrücken (`karte-client`) |
| Spiel-Updates (seit 1.0 am 29.10.2026 aktiv gepatcht) | Hooks und SDK veralten | Versionsprüfung, Update-Regel, Adapter-Schicht |
