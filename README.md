# Deadly Days: Roadtrip – Online Multiplayer Mod

Fan-Mod, die echtes Online-Coop für **Deadly Days: Roadtrip** ermöglichen soll. Das Spiel selbst bietet nur lokales
Couch-Coop, online also nur über Steam Remote Play, bei dem der Host das Bild streamt. Mit der Mod läuft das Spiel bei
jedem Spieler selbst.

*English summary: [see below](#english).*

**Stand:** Experimentell, noch kein Download für Spieler. Zwei Spielinstanzen treten über eine Steam-Lobby bei und spielen
dieselbe Mission: Bewegung, Kampf beider Spieler, Items, Level-ups und das Rundenende bis zum nächsten Tag. Getestet wurde
bisher vor allem auf einem PC. Der Test über echtes Internet zwischen zwei Haushalten ([Gate G2](TODO.md#phase-4--vertical-slice)) steht noch aus.

**Mitmachen:** Jeder kann mitarbeiten, auch ohne Einladung, etwa bei C++, Reverse Engineering, Tests zu zweit oder der Doku.
Der Einstieg steht unter [Für neue Mitwirkende](#für-neue-mitwirkende), freie Aufgaben unter [Wo kann ich anfangen?](TODO.md#wo-kann-ich-anfangen).

Was im Einzelnen noch offen ist, zeigt die [Roadmap als Graph](docs/ROADMAP.md).

| | |
|---|---|
| Spiel | Deadly Days: Roadtrip (Steam-AppID `3026450`) |
| Engine | Unreal Engine 5.8, interner Projektname `DDSurvivors` |
| Zielbild | 2 Spieler online, Host-autoritativ, jeder mit eigener Spielkopie |
| Technik | Native C++-DLL (Dumper-7-SDK + Hooks), siehe [Architektur](docs/ARCHITECTURE.md) |

## English

Fan mod that adds real online co-op to **Deadly Days: Roadtrip**. The game itself only offers local couch co-op, so playing
online currently means Steam Remote Play, where the host streams the picture. With this mod every player runs the game on
their own PC.

**Status:** Experimental, no download for players yet. Two game instances join a Steam lobby and play the same mission:
movement, both players fighting, items, level-ups and the end of a round through to the next day. Testing has mostly
happened on one PC. The test over the real internet between two homes (Gate G2) is still open.

**Contributing:** Anyone can help, no invitation needed: C++, reverse engineering, testing with two players, or docs. Fork the
repo and open a pull request. For bigger changes please open an issue first. Docs and the task list are written in German,
but issues and pull requests in English are welcome. Start with [docs/SETUP.md](docs/SETUP.md), then pick a free task from
[TODO.md](TODO.md#wo-kann-ich-anfangen) (`py tools/tasks.py list`).

**Legal:** Unofficial fan project, not affiliated with Pixelsplit or the publisher. Every player needs their own copy of the
game. The code is under the [MIT License](LICENSE). Game files are never part of this repo.

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
| [4](TODO.md#phase-4--vertical-slice) | Spielbarer Ausschnitt | 15 von 16 | ██████████░ |
| [5](TODO.md#phase-5--vollständiger-run) | Kompletter Run zu zweit | 14 von 28 | ██████░░░░░ |
| [6](TODO.md#phase-6--release) | Veröffentlichung | 4 von 9 | █████░░░░░░ |
| **Gesamt** | | **95 von 117** | █████████░░ |

</details>

Unter `src/` stehen Loader, Core, die Netzschicht über Steam, ein im Spiel geprüftes [Mod-Overlay](src/UI/README.md) und die
[Game-Adapter-Schicht](src/Game/README.md) mit den Eingriffen für den Coop. Die Punkte sind unterschiedlich groß, die Zahlen sagen also wenig über die verbleibende Zeit. Stand, Engpässe und nächste Schritte stehen in der [Roadmap](docs/ROADMAP.md#stand-und-nächste-schritte).

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
