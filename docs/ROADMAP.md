# Roadmap als Graph

Diese Seite zeigt, was bis zur fertigen und nutzbaren Mod noch offen ist. Führend bleibt [TODO.md](../TODO.md), hier steht nur die Übersicht.

**Fertig und nutzbar** heißt: Zwei Spieler finden sich über Steam, spielen einen kompletten Run online, und die Mod lässt sich installieren, ohne Spielstände oder Bestenlisten zu berühren. Dafür müssen alle Punkte mit P0 und P1 erledigt sein. P2 und [Optionale Optimierungen](../TODO.md#optionale-optimierungen) kommen danach.

Den Fortschritt je Phase zeigt das [README](../README.md#fortschritt).

## Stand und nächste Schritte

Projektanalyse vom 02.11.2026 auf Commit `6b9e651`.

**Was läuft.** Zwei Instanzen spielen im Spike dieselbe Mission, mit Bewegung und Schüssen beider Seiten. Der Client erreicht Tag 2. Die Analysen zu Coop, Karte, Gegnern, Kamera und Sync-Bedarf sind abgeschlossen.

**Was fehlt.** Es gibt keinen kompletten Run, keine Mod unter `src/` und keine CI. Der Sync-Entwurf ist weder umgesetzt noch im Netz getestet. Getestet wurde bisher nur über Loopback auf einem PC, nicht über echtes Internet.

**Geprüft bei dieser Analyse.** Installiert ist weiterhin Build `26187885`. Seit dem letzten erfolgreichen Spike-Build änderte sich nur Dokumentation. Die Unit-Tests und alle relativen Links der Doku sind in Ordnung. Ein Spieltest gehörte nicht dazu.

**Neue Befunde.**

- `rundenende` zeigt: Das Spiel überträgt die Endphase nicht, und nachspielen lässt sie sich beim Client nicht. Nötig ist eine eigene Host-Nachricht, also ein Kontrollkanal, den es auch im Spike noch nicht gibt.
- Die Ergebnisse von `rundenende` sind eingereicht, die Aufgabe bleibt offen: Die Phasenübertragung selbst fehlt, siehe [Untersuchung](GAME_ANALYSIS.md#rundenende-befund-und-offene-phasenübertragung-rundenende).
- In `TODO.md` fehlten fünf Aufgaben, die jetzt eingetragen sind: Kontrollkanal, Übernahme der Spike-Patches, Umsetzung der Kamera, Pausen über den Kontrollkanal und der einmalige Absturz im Missions-Tick.

**Reihenfolge.**

1. `tag3` lösen. Ohne diesen Schritt ist kein Run über Tag 2 hinaus möglich.
2. `spawnedplayers`, danach `remote-p2`, und `karte-client`. Diese Aufgaben brauchen Spielzeit, und pro PC läuft nur eine Sitzung.
3. Parallel ohne Spielzeit: `grundgeruest`, `tunnel` und `crash-reporter`.
4. `last` messen und an Gate G1 entscheiden.

**Wartet auf das Team.**

- Kamera entscheiden
- Aufteilung auf die Arbeitsbereiche festlegen
- Update-Regel festlegen und klären, ob alle auf derselben Build-ID bleiben können. Das Spiel wird aktiv gepatcht, und jedes Update macht die Offsets ungültig
- Pixelsplit über das Projekt informieren
- Dumper-7-Fix dem Dumper-7-Projekt vorschlagen

## Gesamtweg

```mermaid
flowchart TD
    subgraph A["Klären, ob es geht"]
        direction LR
        P0["Phase 0<br/>Setup"]:::wip --> P1["Phase 1<br/>Analyse und Spikes"]:::wip --> G1{{"Gate G1<br/>Technikentscheidung"}}:::gate
    end

    subgraph B["Spielbar machen"]
        direction LR
        P2["Phase 2<br/>Fundament"]:::open --> P3["Phase 3<br/>Session und Verbindung"]:::open --> P4["Phase 4<br/>Vertical Slice"]:::open --> G2{{"Gate G2<br/>10 Minuten über Internet"}}:::gate
    end

    subgraph C["Fertigstellen"]
        direction LR
        P5["Phase 5<br/>Vollständiger Run"]:::open --> P6["Phase 6<br/>Release"]:::open --> Z(["Mod fertig und nutzbar"]):::goal
    end

    A --> B --> C

    classDef wip fill:#f9a825,stroke:#b27a00,color:#1a1a1a
    classDef open fill:#eceff1,stroke:#607d8b,color:#1a1a1a
    classDef gate fill:#6a1b9a,stroke:#3e1163,color:#ffffff
    classDef goal fill:#1565c0,stroke:#0d3c78,color:#ffffff
```

## Weg bis Gate G1

Hier liegt die aktuelle Arbeit. Die Kennungen in Klammern stehen in [Wo kann ich anfangen?](../TODO.md#wo-kann-ich-anfangen), reserviert wird wie dort beschrieben.

```mermaid
flowchart LR
    subgraph BASIS["Erledigt"]
        A1["Verbindung, Spawn,<br/>Bewegung, Schießen"]:::done
        A2["Weg vom Menü<br/>in die Mission"]:::done
        A3["Analysen: Coop, Karte,<br/>Gegner, Kamera, Sync-Bedarf"]:::done
    end

    subgraph RUN["Run-Ablauf"]
        T3["Tag 3: Host-Absturz,<br/>Inventar abschließen<br/>(tag3)"]:::blocker
        RE["Rundenende, Lager,<br/>Routenkarte<br/>(rundenende)"]:::wip
        NI["Echte Niederlage,<br/>Rückkehr in die MetaBase<br/>(niederlage)"]:::open
        TR["Client-Absturz<br/>beim Trennen<br/>(trennen)"]:::open
        MT["Einmaliger Absturz<br/>im Missions-Tick"]:::open
        RE --> NI
    end

    subgraph SPIELER["Remote-Spieler"]
        SP["Remote-Controller in<br/>SpawnedPlayers<br/>(spawnedplayers)"]:::open
        RP["Remote-Controller als P2<br/>(remote-p2)"]:::open
        TF["Treffer des Clients<br/>direkt belegen<br/>(treffer)"]:::open
        SP --> RP
    end

    subgraph WELT["Karte und Kamera"]
        KC["Seed und Profil beim Client,<br/>Duplikate unterdrücken<br/>(karte-client)"]:::open
        SC["Spike C: gleiche<br/>Mission auf beiden Seiten"]:::open
        KE["Kamera: Entscheidung<br/>des Teams"]:::decision
        KC --> SC
    end

    subgraph LOGIK["Spiellogik"]
        PA["Pausen bei Level-up<br/>und Inventar<br/>(pausen)"]:::wip
        BP["Blueprint-Logik:<br/>Kontrollfluss und Events<br/>(blueprint-logik)"]:::wip
    end

    subgraph TECHNIK["Technik"]
        LA["Spike D: Last<br/>(last)"]:::open
        TU["Spike E: Steam-Tunnel<br/>(tunnel)"]:::open
        AP["Andockpunkte:<br/>Test im Spiel<br/>(andockpunkte)"]:::wip
        CR["Crash-Reporter<br/>zuverlässig aus<br/>(crash-reporter)"]:::open
    end

    BASIS --> RUN
    BASIS --> SPIELER
    BASIS --> WELT
    BASIS --> LOGIK
    BASIS --> TECHNIK

    RUN --> G1
    SPIELER --> G1
    WELT --> G1
    LOGIK --> G1
    TECHNIK --> G1

    G1{{"Gate G1:<br/>Weg A, Weg B oder Stopp"}}:::gate

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#ffffff
    classDef wip fill:#f9a825,stroke:#b27a00,color:#1a1a1a
    classDef open fill:#eceff1,stroke:#607d8b,color:#1a1a1a
    classDef blocker fill:#c62828,stroke:#7f1d1d,color:#ffffff
    classDef decision fill:#ef6c00,stroke:#a34800,color:#ffffff
    classDef gate fill:#6a1b9a,stroke:#3e1163,color:#ffffff
```

## Von Gate G1 bis zur fertigen Mod

```mermaid
flowchart TD
    G1{{"Gate G1"}}:::gate

    subgraph PH2["Phase 2: Fundament"]
        L["Loader: Proxy-DLL"]:::open
        C["Core: Logging, Hooks,<br/>Game Thread, Versionsprüfung"]:::open
        GA["Game: Adapter-Schicht"]:::open
        U["UI: Overlay"]:::open
        W["Deploy-Skript, Tests, CI"]:::open
        L --> C --> GA
        C --> U
    end

    subgraph PH3["Phase 3: Session und Verbindung"]
        LO["Steam-Lobby"]:::open
        TN["Transport-Tunnel"]:::open
        HS["Handshake: Build und Mod-Version"]:::open
        FE["Timeout, Abbruch,<br/>Host verlässt das Spiel"]:::open
        KK["Kontrollkanal"]:::open
        LO --> TN --> HS --> KK --> FE
    end

    subgraph PH4["Phase 4: Vertical Slice"]
        SH["Spike-Patches als Hooks<br/>übernehmen"]:::open
        R2["Remote-Spieler als P2"]:::open
        MS["Gemeinsamer Missionsstart,<br/>gleiche Karte"]:::open
        KM["Entschiedene Kamera"]:::open
        KA["Bewegung, Waffen, Treffer, Tod"]:::open
        GT["Ein Gegnertyp"]:::open
        EI["Eingaben von Remote<br/>trennen"]:::open
        SH --> R2 --> MS --> KM --> KA --> GT
        R2 --> EI
    end

    G2{{"Gate G2: 10 Minuten stabil<br/>zwischen zwei Haushalten"}}:::gate

    subgraph PH5["Phase 5: Vollständiger Run"]
        GE["Alle Gegner und Bosse"]:::open
        XP["Loot, XP, Level-ups,<br/>Fähigkeiten"]:::open
        IV["Inventar, Reward-Queue,<br/>Inventar-UI"]:::open
        PU["Pausen und blockierende UI"]:::open
        TG["Tageszyklus, Treibstoff,<br/>Etappen"]:::open
        ME["Run-Ende, Meta-Basis,<br/>Regel für Meta-Fortschritt"]:::open
    end

    subgraph PH6["Phase 6: Release"]
        SB["Bestenlisten und<br/>Erfolge sperren"]:::open
        TE["Telemetrie und<br/>Crash-Reports aus"]:::open
        SV["Eigene Mod-Spielstände"]:::open
        PS["Rückmeldung von Pixelsplit"]:::open
        IN["Installer oder Anleitung"]:::open
    end

    Z(["Mod fertig und nutzbar"]):::goal

    G1 --> PH2 --> PH3 --> PH4 --> G2 --> PH5 --> PH6 --> Z

    classDef open fill:#eceff1,stroke:#607d8b,color:#1a1a1a
    classDef gate fill:#6a1b9a,stroke:#3e1163,color:#ffffff
    classDef goal fill:#1565c0,stroke:#0d3c78,color:#ffffff
```

## Zentral und parallel

Türkis ist zentrale Arbeit, die nacheinander läuft. Grau sind Pakete, die daneben parallel laufen. Die Kennungen stehen in [Spätere Arbeitspakete](../TODO.md#spätere-arbeitspakete) und [Zentrale Arbeit](../TODO.md#zentrale-arbeit).

```mermaid
flowchart TD
    GR["grundgeruest"]:::central --> W1["Welle 1<br/>overlay, deploy, ci"]:::open
    GR --> GA["game-adapter"]:::central
    G1{{"Gate G1"}}:::gate --> W2["Welle 2<br/>steam-lobby, transport,<br/>net-fehler"]:::open
    GR --> W2
    G1 --> GA
    GA --> PT["patches"]:::central --> RS["remote-spieler"]:::central
    GA --> W5["Welle 5<br/>online-sperren, telemetrie-aus,<br/>mod-saves, installer"]:::open
    RS --> W3["Welle 3<br/>missionsstart, kamera-code, bewegung,<br/>eingaben, kampf, gegnertyp"]:::open
    W1 --> IG["integration-g2"]:::central
    W2 --> PR["protokoll"]:::central
    W2 --> IG
    W3 --> IG
    IG --> G2{{"Gate G2"}}:::gate
    PR --> W4["Welle 4<br/>gegner-alle, fortschritt, inventar-sync,<br/>inventar-ui, pausen-sync, tagesablauf,<br/>run-ende, vfx-sound, reconnect"]:::open
    G2 --> W4
    W4 --> IR["integration-run"]:::central
    W5 --> IR
    IR --> RL["release"]:::central
    RL --> Z(["Mod fertig und nutzbar"]):::goal

    classDef central fill:#00796b,stroke:#004d40,color:#ffffff
    classDef open fill:#eceff1,stroke:#607d8b,color:#1a1a1a
    classDef gate fill:#6a1b9a,stroke:#3e1163,color:#ffffff
    classDef goal fill:#1565c0,stroke:#0d3c78,color:#ffffff
```

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

Unabhängig von den Gates kann das [Grundgerüst](../TODO.md#phase-2--fundament) (`grundgeruest`) schon parallel entstehen. An G1 kann der Weg noch kippen: Bei Weg B wächst Phase 5 deutlich, bei einem Stopp entfallen die Phasen danach.

## Pflege

Wer eine Aufgabe abschließt, ändert hier die Farbe des Knotens und im [README](../README.md#fortschritt) die Zählung der Phase. Offene Teilpunkte aus `TODO.md`, die keinen eigenen Knoten haben, gehören zum thematisch passenden Knoten.
