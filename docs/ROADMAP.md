# Roadmap

Übersicht bis zur fertigen und nutzbaren Mod, nur auf Ebene der Phasen. Führend ist [TODO.md](../TODO.md).
Die frühere, ausführliche Roadmap mit Befunden steht im [Archiv](archive/ROADMAP.md).

**Fertig und nutzbar** heißt: Zwei Spieler finden sich über Steam, spielen einen kompletten Run online, und die Mod lässt sich installieren, ohne Spielstände oder Bestenlisten zu berühren.
Dafür müssen alle Aufgaben mit P0 und P1 erledigt sein.

## Gesamtweg

```mermaid
flowchart TD
    subgraph A["Klären, ob es geht"]
        direction LR
        P0["Phase 0<br/>Setup"]:::done --> P1["Phase 1<br/>Analyse und Spikes"]:::done --> G1{{"Gate G1<br/>Weg A + gezielter Sync"}}:::done
    end

    subgraph B["Spielbar machen"]
        direction LR
        P2["Phase 2<br/>Fundament"]:::done --> P3["Phase 3<br/>Session und Verbindung"]:::done --> P4["Phase 4<br/>Vertical Slice"]:::done --> G2{{"Gate G2<br/>10 Minuten über Internet"}}:::done
    end

    subgraph C["Fertigstellen"]
        direction LR
        P5["Phase 5<br/>Vollständiger Run"]:::open --> P6["Phase 6<br/>Release"]:::open --> Z(["Mod fertig und nutzbar"]):::goal
    end

    A --> B --> C

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#ffffff
    classDef wip fill:#f9a825,stroke:#b27a00,color:#1a1a1a
    classDef open fill:#eceff1,stroke:#607d8b,color:#1a1a1a
    classDef gate fill:#6a1b9a,stroke:#3e1163,color:#ffffff
    classDef goal fill:#1565c0,stroke:#0d3c78,color:#ffffff
```

## Stand

- Der Run zu zweit über das Internet ist spielbar: `integration-run` ist erledigt, der Stand erscheint als Release 1.0.
- Offene Fehler aus dem Live-Test vom 24.11.2026 (P1, Phase 5): `airdrop-auswahl-absturz`, `lobby-rauswurf`, `wiedereintritt-karte`, `spawn-nach-kartenaufbau` (nur bei bestätigter Theorie). Je eine Fallakte unter [docs/faelle/](faelle/).
- Bestätigungstests in Phase 5: `lager-edit-ablehnung`, `opt-metabase-inventar`, `labyrinth-event-client`.
- Danach `release` (Phase 6). Optionale Aufgaben warten, bis Phase 0 bis 5 erledigt sind.
- Den Fortschritt je Phase zeigt das [README](../README.md#progress), die aktuelle Liste `py tools/tasks.py list --short`.

## Legende

| Farbe | Bedeutung |
|---|---|
| Grün | Erledigt |
| Gelb | In Arbeit oder teilweise erledigt |
| Grau | Offen |
| Rot | Größtes Hindernis |
| Orange | Wartet auf eine Entscheidung des Teams |
| Lila | Gate |
| Blau | Ziel |
| Türkis | Zentrale Arbeit |

Die Pfeile innerhalb einer Phase zeigen eine sinnvolle Reihenfolge. Sie sind eine Planung, keine belegten technischen Abhängigkeiten.

Gate G1 ist mit Weg A und gezielten Sync-Ergänzungen abgeschlossen. Ein Wechsel zu Weg B oder eine Verkleinerung wäre nur bei neuen konkreten Gegenbefunden erneut vom Team zu entscheiden; derzeit ist dafür keine zusätzliche Aufgabe geplant.

## Pflege

Die Phasenknoten werden umgefärbt, wenn eine Phase erledigt ist. Der Abschnitt „Stand“ wird ersetzt, nicht ergänzt.
Einzelne Aufgaben und Befunde stehen nicht hier, sondern in [TODO.md](../TODO.md), [docs/ERLEDIGT.md](ERLEDIGT.md) und den Fallakten.
