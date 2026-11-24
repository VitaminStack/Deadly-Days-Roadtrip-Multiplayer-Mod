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

## Risiken

| Risiko | Auswirkung | Gegenmaßnahme |
|---|---|---|
| RPCs sind nur Altlast aus dem Vor-Coop-Stand (Coop-Klassen haben keine RPCs) | Weg A fällt aus, deutlich mehr Eigenbau | Spike A und B zuerst; Weg B als Plan B |
| Logik liegt in Blueprints ohne Netzwerkcode | Viele Stellen brauchen eigene Hooks | Blueprint-Anteil früh abschätzen |
| Coop setzt zwei lokale Spieler voraus | Remote-P2 passt nicht in die Coop-Logik | `remote-p2` vor `remote-spieler` |
| Viele Gegner überlasten das Netzwerk | Laggy oder unspielbar | Spike D; Gegner gebündelt synchronisieren |
| Karte entsteht lokal, Objekte darauf replizieren | Doppelte Objekte oder abweichende Karte beim Client | Seed übertragen, lokale Kopien replizierter Klassen unterdrücken (`karte-client`) |
| Spiel-Updates (seit 1.0 am 29.10.2026 aktiv gepatcht) | Hooks und SDK veralten | Versionsprüfung, Update-Regel, Adapter-Schicht |
| Tests laufen fast nur mit Spike und Mod zusammen | Im Release fehlen Eingriffe, die nur der Spike macht, das Spiel stürzt ab | `ohne-spike` vor Gate G2 |
| Tests laufen über Loopback mit 0 ms | Timing-Fehler zeigen sich erst über das Internet, vermischt in `integration-g2` | `latenz-test`, Abnahmen mit Latenzprofil |
| Rein lokale Zustände weichen still ab (wie die XP-Bälle) | Desyncs fallen spät und schwer zuordenbar auf | `desync-melder` vor Gate G2 |
| Fehler wachsen mit der Laufzeit (Timer, NetGUIDs, Speicher) | Abstürze erst spät im Run | `dauerlauf` vor `integration-run` |
| Mod-Runs landen beim Hersteller oder in Steam | Absturzberichte belasten Pixelsplit, Erfolge und Bestenlisten werden verfälscht | Sperren in `Core::Protection` (erledigt), dazu `weekly-sperre` |
| Virenscanner melden Proxy-DLL und Hooks | Spieler können die Mod nicht installieren | `virenscanner` vor `release` |
| Doku veraltet, weil viele parallel arbeiten | Agenten folgen falschen Anleitungen | `doku-verweise` |

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
