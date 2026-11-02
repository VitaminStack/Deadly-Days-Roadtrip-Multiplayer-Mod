# Deadly Days: Roadtrip – Online Multiplayer Mod

Fan-Mod, die echtes Online-Coop für **Deadly Days: Roadtrip** ermöglichen soll. Das Spiel selbst bietet nur lokales
Couch-Coop, online also nur über Steam Remote Play, bei dem der Host das Bild streamt. Mit der Mod läuft das Spiel bei
jedem Spieler selbst.

**Status:** Phase 1 – Analyse. Es gibt noch keine Mod, aber einen funktionierenden Spike: Zwei Instanzen spielen dieselbe Mission,
der Client übersteht den Wechsel zu Tag 2 noch nicht. Ob das Vorhaben aufgeht, entscheidet sich an Gate G1 in [TODO.md](TODO.md).

| | |
|---|---|
| Spiel | Deadly Days: Roadtrip (Steam-AppID `3026450`) |
| Engine | Unreal Engine 5.8, interner Projektname `DDSurvivors` |
| Zielbild | 2 Spieler online, Host-autoritativ, jeder mit eigener Spielkopie |
| Technik | Native C++-DLL (Dumper-7-SDK + Hooks), siehe [Architektur](docs/ARCHITECTURE.md) |

## Wo finde ich was?

| Dokument | Inhalt |
|---|---|
| [TODO.md](TODO.md) | Roadmap, Phasen, Gates und offene Aufgaben |
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
2. [docs/SETUP.md](docs/SETUP.md) durcharbeiten, das dauert nur wenige Minuten.
3. Eine Testsitzung nach [docs/TESTING.md](docs/TESTING.md) starten, dann läuft der aktuelle Stand bei dir.
4. [CONTRIBUTING.md](CONTRIBUTING.md) lesen und eine Aufgabe aus [Wo kann ich anfangen?](TODO.md#wo-kann-ich-anfangen) nehmen.

## Rechtliches

Dies ist ein inoffizielles Fan-Projekt ohne Verbindung zu Pixelsplit oder dem Publisher. Jeder Spieler braucht eine
eigene Kopie des Spiels. Der Code dieses Projekts steht unter der [MIT-Lizenz](LICENSE), das gilt nicht für das Spiel selbst.
Spieldateien, entpackte Assets und generierte SDKs gehören **nicht** ins Repo
(siehe [CONTRIBUTING.md](CONTRIBUTING.md#was-nie-ins-repo-darf)).
