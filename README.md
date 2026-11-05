# Deadly Days: Roadtrip – Online Multiplayer Mod

Fan-Mod, die echtes Online-Coop für **Deadly Days: Roadtrip** ermöglichen soll. Das Spiel selbst bietet nur lokales
Couch-Coop, online also nur über Steam Remote Play, bei dem der Host das Bild streamt. Mit der Mod läuft das Spiel bei
jedem Spieler selbst.

**Status:** Die Mod-Basis (Phase 2) ist abgeschlossen; weitere Analyse- und Integrationsaufgaben sind offen.
Der Spike erreicht den akzeptierten Endlos-Stand bis Tag 13. Gate G1 ist entschieden: native UE-Replikation mit gezielten Sync-Ergänzungen, siehe [TODO.md](TODO.md).

```mermaid
flowchart TD
    subgraph A["Klären, ob es geht"]
        direction LR
        P0["Setup"]:::done --> P1["Analyse und Spikes"]:::done --> G1{{"Gate G1"}}:::done
    end

    subgraph B["Spielbar machen"]
        direction LR
        P2["Fundament"]:::done --> P3["Verbindung"]:::open --> P4["Vertical Slice"]:::open --> G2{{"Gate G2"}}:::gate
    end

    subgraph C["Fertigstellen"]
        direction LR
        P5["Vollständiger Run"]:::open --> P6["Release"]:::open --> Z(["Mod nutzbar"]):::goal
    end

    A --> B --> C

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#ffffff
    classDef wip fill:#f9a825,stroke:#b27a00,color:#1a1a1a
    classDef open fill:#eceff1,stroke:#607d8b,color:#1a1a1a
    classDef gate fill:#6a1b9a,stroke:#3e1163,color:#ffffff
    classDef goal fill:#1565c0,stroke:#0d3c78,color:#ffffff
```

Was im Einzelnen noch offen ist, zeigt die [Roadmap als Graph](docs/ROADMAP.md).

| | |
|---|---|
| Spiel | Deadly Days: Roadtrip (Steam-AppID `3026450`) |
| Engine | Unreal Engine 5.8, interner Projektname `DDSurvivors` |
| Zielbild | 2 Spieler online, Host-autoritativ, jeder mit eigener Spielkopie |
| Technik | Native C++-DLL (Dumper-7-SDK + Hooks), siehe [Architektur](docs/ARCHITECTURE.md) |

## Fortschritt

Stand: 04.11.2026.

Gezählt sind die Aufgaben je Phase aus [TODO.md](TODO.md), ohne Wartung und optionale Optimierungen.

Erledigt ist, was in `TODO.md` unter „Erledigt“ steht. **Vollständig abgeschlossene Phasen sind grün hinterlegt.** `py tools/task_status.py update` aktualisiert Grafik und Texttabelle; `py tools/task_status.py check` und die CI verhindern abweichende Angaben. Reservierungen und PRs ändern den Abschlussstatus nicht.

![Projektfortschritt je Phase; vollständig abgeschlossene Phasen sind grün hinterlegt.](docs/assets/progress.svg)

<details>
<summary>Fortschritt als Texttabelle mit Links zu den Phasen</summary>


| Phase | Ziel | Erledigt | Fortschritt |
|---|---|---|---|
| [0](TODO.md#phase-0--projekt-setup) | Gleiche Toolchain für alle | 11 von 11 | ███████████ |
| [1](TODO.md#phase-1--analyse-und-spikes) | Fakten für Gate G1 | 39 von 39 | ███████████ |
| [2](TODO.md#phase-2--fundament) | Stabile Basis der Mod | 5 von 5 | ███████████ |
| [3](TODO.md#phase-3--session-und-verbindung) | Verbinden über Steam | 7 von 9 | █████████░░ |
| [4](TODO.md#phase-4--vertical-slice) | Spielbarer Ausschnitt | 12 von 16 | ████████░░░ |
| [5](TODO.md#phase-5--vollständiger-run) | Kompletter Run zu zweit | 3 von 20 | ██░░░░░░░░░ |
| [6](TODO.md#phase-6--release) | Veröffentlichung | 4 von 9 | █████░░░░░░ |
| **Gesamt** | | **81 von 109** | ████████░░░ |

</details>

Unter `src/` stehen Loader, Core, ein im Spiel geprüftes [Mod-Overlay](src/UI/README.md) und die [Game-Adapter-Schicht](src/Game/README.md) mit Game-Thread-Takt, die bisher nur liest. Phase 2 ist damit abgeschlossen; die Steam-Anbindung der Lobby-Knöpfe folgt in Phase 3. Die Punkte sind unterschiedlich groß, die Zahlen sagen also wenig über die verbleibende Zeit. Stand, Engpässe und nächste Schritte stehen in der [Roadmap](docs/ROADMAP.md#stand-und-nächste-schritte).

## Wo finde ich was?

| Dokument | Inhalt |
|---|---|
| [TODO.md](TODO.md) | Roadmap, Phasen, Gates und offene Aufgaben |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Graph: Was bis zur fertigen Mod noch offen ist |
| [docs/GAME_ANALYSIS.md](docs/GAME_ANALYSIS.md) | Was wir über das Spiel wissen, mit Belegen und Vertrauensgrad |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Technische Optionen, Entscheidungen, Modulgrenzen |
| [docs/OFFSETS.md](docs/OFFSETS.md) | Offsets und Funktionsadressen des unterstützten Builds |
| [docs/SETUP.md](docs/SETUP.md) | Entwicklungsumgebung einrichten |
| [docs/TESTING.md](docs/TESTING.md) | Zwei Instanzen starten, Abstürze auswerten, Ergebnisse festhalten |
| [research/spikes/a-connect/](research/spikes/a-connect/README.md) | Stand des Spikes, Patches, gescheiterte Ansätze |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Zusammenarbeit: Branches, PRs, Regeln für das Repo |
| [CLAUDE.md](CLAUDE.md) | Arbeitsregeln für KI-Agenten, auch über [AGENTS.md](AGENTS.md) erreichbar |

## Projektstruktur

```text
.
├─ README.md               Einstieg
├─ TODO.md                 Roadmap und Aufgaben
├─ CONTRIBUTING.md         Regeln für die Zusammenarbeit
├─ .github/                PR- und Issue-Vorlagen
├─ config/                 Vorlage für lokale Pfade (Local.props bleibt lokal)
├─ docs/                   Analyse, Architektur, Setup
├─ research/               Eigene RE-Notizen, Hook-Listen, Spike-Ergebnisse
├─ src/
│  ├─ Loader/              Proxy-DLL, lädt die Mod ins Spiel
│  ├─ Core/                Logging, Config, Hooks, Game-Thread-Dispatcher
│  ├─ Net/                 Transport, Steam-Lobby, Session, Handshake
│  ├─ Game/                Adapter auf Spielklassen (einziger Ort mit SDK-Zugriff)
│  ├─ Sync/                Spiellogik-Synchronisierung
│  └─ UI/                  Lobby-Overlay, Statusanzeige
├─ tests/                  Tests für Code ohne Spielabhängigkeit
├─ third_party/            Abhängigkeiten (Hinweise, siehe README dort)
└─ tools/                  Skripte (Spike bauen und testen, später Deploy)
```

Jedes Modul unter `src/` hat eine eigene `README.md` mit Aufgabe und Grenzen. So kann das Team parallel arbeiten, ohne
sich gegenseitig in dieselben Dateien zu schreiben (siehe [CONTRIBUTING.md](CONTRIBUTING.md)).

## Für neue Mitwirkende

Die bisherige Analyse muss niemand wiederholen. Ergebnisse, Offsets, Patches und Testskripte liegen im Repo.

1. Spiel in Steam installieren und auf die [unterstützte Version](docs/GAME_ANALYSIS.md#unterstützte-spielversion) bringen.
2. [docs/SETUP.md](docs/SETUP.md) durcharbeiten: einrichten, mit `tools/Test-Setup.ps1` prüfen, Solution bauen, Tests ausführen.
3. Eine Testsitzung nach [docs/TESTING.md](docs/TESTING.md) starten, dann läuft der aktuelle Stand bei dir.
4. [CONTRIBUTING.md](CONTRIBUTING.md) lesen und eine Aufgabe aus [Wo kann ich anfangen?](TODO.md#wo-kann-ich-anfangen) nehmen.

## Rechtliches

Dies ist ein inoffizielles Fan-Projekt ohne Verbindung zu Pixelsplit oder dem Publisher. Jeder Spieler braucht eine
eigene Kopie des Spiels. Der Code dieses Projekts steht unter der [MIT-Lizenz](LICENSE), das gilt nicht für das Spiel selbst.
Spieldateien, entpackte Assets und generierte SDKs gehören **nicht** ins Repo
(siehe [CONTRIBUTING.md](CONTRIBUTING.md#was-nie-ins-repo-darf)).
