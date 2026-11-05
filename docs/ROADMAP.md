# Roadmap als Graph

Diese Seite zeigt, was bis zur fertigen und nutzbaren Mod noch offen ist. Führend bleibt [TODO.md](../TODO.md), hier steht nur die Übersicht.

**Fertig und nutzbar** heißt: Zwei Spieler finden sich über Steam, spielen einen kompletten Run online, und die Mod lässt sich installieren, ohne Spielstände oder Bestenlisten zu berühren. Dafür müssen alle Punkte mit P0 und P1 erledigt sein. P2 und [Optionale Optimierungen](../TODO.md#optionale-optimierungen) kommen danach.

Den Fortschritt je Phase zeigt das [README](../README.md#fortschritt).

## Stand und nächste Schritte

Projektanalyse vom 02.11.2026 auf Commit `6b9e651`.

**Was läuft.** Zwei Instanzen spielen im Spike dieselbe Mission, mit Bewegung und Schüssen beider Seiten. Der Client erreicht Tag 2. Die Analysen zu Coop, Karte, Gegnern, Kamera und Sync-Bedarf sind abgeschlossen. Unter `src/` steht das Grundgerüst aus Solution, `Loader`, `Core` und Modul-Einstiegen.

**Was fehlt.** Es gibt keinen kompletten Run. Mod-Code mit Spielbezug gibt es erst als Adapter-Schicht, die nur liest. Der Sync-Entwurf ist weder umgesetzt noch im Netz getestet. Getestet wurde bisher nur über Loopback auf einem PC, nicht über echtes Internet.

**Geprüft bei dieser Analyse.** Installiert ist weiterhin Build `26187885`. Seit dem letzten erfolgreichen Spike-Build änderte sich nur Dokumentation. Die Unit-Tests und alle relativen Links der Doku sind in Ordnung. Ein Spieltest gehörte nicht dazu.

**Neue Befunde.**

- `desync-melder`, 05.11.2026: Diagnose über Kontrollkanal mit sechs Messfeldern, feldweiser Stabilisierung und Host-bestätigten Abweichungen in Log/F8 implementiert. Gegnerzahl standardmäßig ausgenommen. Nach zunächst belegter Sitzung eigene Lobby-Sitzung über echte Route bis Tag 1: alle fünf aktiven Felder vergleichbar und gleich. **Noch offen:** Ergebnis des laufenden Zehn-Minuten-/Abweichungstests. [Messumfang und Abnahmegrenzen](DESYNC.md). Kein Aufgabenabschluss durch synthetische Modelltests.

- `itemsack-absturz`, 05.11.2026: **Abgeschlossen.** Der Client versuchte beim Aufsammeln aus nicht replizierter `ItemData` ein Inventaritem zu erzeugen und stürzte ab. `InventoryPatches.cpp` belässt den vollständigen Vergabepfad beim Host. In einer Lobby-Sitzung auf einem PC nach echter Route bis Tag 1 drei natürliche Remote-Aufsammlungen mit jeweils exakt einem Item nur beim Remote-Spieler, unverändertem Host-Inventar und gleichen replizierten Inventaren geprüft. Client-Negativaufruf ohne Gewinn/Absturz, isolierte Host-Vergabe ebenfalls geprüft; [Befund, Fehlversuche und Grenzen](../research/spikes/itemsack-absturz/README.md).

- `client-visuell`, XP-Bälle und Projektile 05.11.2026: Der Host zeichnet die nativen XP-Ausstöße auf und sendet sie über den Kontrollkanal, der Client spielt sie nur zur Anzeige nach (`ReceiveExp` gesperrt). Gepoolte Projektile replizieren beim Host jetzt mit Bewegung. Beides über die Lobby im Spiel geprüft, Projektile vom Nutzer live bestätigt. **Abgeschlossen** auf Nutzerentscheidung; Projektilgröße, Treffer-/Partikeleffekte und XP-Randfälle in `opt-client-visuell`. Neu gefunden: Client-Absturz beim Aufsammeln eines Itemsacks, jetzt `itemsack-absturz` (P0).
- `client-visuell`, Airdrop-Nachtrag 05.11.2026: Einfache eingebaute Actor-/Bewegungsreplikation plus NetGUID-gebundene Anzeigezustände umgesetzt. Normaler Level-up-Airdrop im Client sichtbar; kontrollierte native Host-Öffnung und anschließende Entfernung auf beiden Seiten bestätigt. Hide-Fallback gegen zurückgebliebene Kopien ergänzt, sein tatsächlicher Einsatz im Abschlusstest bleibt unbekannt. Client-Reward-Auswahl und restliche visuelle Abnahme bleiben offen; doppelte Client-Belohnung ist vorerst erlaubt, wurde aber nicht benötigt.

- `client-visuell`, 05.11.2026: Erste Produktionsanbindung statt nur Diagnose: gemeinsame XP-/Levelanzeige als absoluter Host-Snapshot, ohne lokale XP-/Reward-Gutschrift. Der neue Kontrollkanal aus `protokoll` ist auch am Lobbytransport verfügbar. Beide Host-PlayerStates teilen nachweislich dasselbe Levelsystem; die Client-HUD zeigte im Test ebenfalls Level 5. NetGUID-, Nonce- und Revisionsbindung gegen alte Welten/Nachrichten. Orb-/Projektil-/Partikeldarstellung und komplette Abnahme bleiben offen, [aktueller Stand](../research/spikes/client-visuell/README.md#umsetzung-vom-05112026-gemeinsamer-xp-anzeigestand).

- `client-visuell`, 04.11.2026, auf Nutzerwunsch vorgezogen und noch offen: Projektile, XP-Orbs, Airdrops und Partikeleffekte im Host-/Client-Vergleich. Erste Messung zeigt fehlende XP-Niagara-Darstellungsdaten beim Client; reines Einschalten der Projektil-Bewegungsreplikation ist kein abgenommener Fix. Nutzerergänzung: Airdrops fehlen beim Client, XP-Aufnahmen sollen einmal für beide Spieler zum gemeinsamen Run-Fortschritt zählen. Kein Produktionscode geändert, [Befunde, Fehlversuch und nächste Schritte](../research/spikes/client-visuell/README.md).
- `protokoll`, 05.11.2026: Handshake mit Spiel-Build, Mod-Version und Rolle sowie zuverlässiger Kontrollkanal im selben Steam-Socket stehen in `src/Net`, Format und API in [Kontrollkanal](CONTROL_PROTOCOL.md). 24 Net-Tests, im Spiel bis Tag 1 geprüft, die XP-Anzeige aus `client-visuell` läuft schon darüber. Der Internet-Test mit zwei Konten gehört zu `lobby-live` bzw. `integration-g2`. Damit sind die Pakete der Welle 4 nicht mehr durch `protokoll` blockiert.
- `net-fehler`, 05.11.2026: Jedes Sitzungsende kommt mit eigenem Grund (Host oder Client verlässt, abgelehnt, belegt, Steam-Zeitlimit) und Sitzungszeitlimits als lesbare Meldung in Overlay und Log. Im Spiel auf einem PC geprüft: Verlassen wirkt in unter 1 s, harter Prozessabbruch nach 10,6 s, Neubeitritt per Lobby-ID, kein Fehlalarm beim Kartenwechsel. Einmal stürzte der Client beim Trennen aus der Mission mit „world leaks“ ab, offen als `opt-trennen-absturz`, siehe [Fehlerbehandlung](../src/Net/README.md#im-spiel-05112026).
- `test-tempo`, 05.11.2026: Spielzeit ist der Engpass, nicht die Zahl der Agenten. Sitzungsstart wartet jetzt auf Zustände (Lobby 28 s statt 220 s, direkt 42 s statt 100 s), Clients lassen sich einzeln neu starten, das Sitzungsende prüft `Test-NetEnd.ps1` in einer Minute, und `Guided-Test.ps1` bindet Menschen für Schritte ein, die per Skript umständlich sind. Nächster Schritt `mod-steuerung`: Steuerkanal mit Antworten in der Mod statt Befehlsdatei des Spikes, siehe [Spielzeit sparen](TESTING.md#spielzeit-sparen).
- `token-sparen`, 05.11.2026: Weniger Kontext je Schritt. `tasks.py show` liest eine Aufgabe statt der ganzen `TODO.md`, `tasks.py done` schließt sie ab, und die Testskripte geben mit `-Brief` eine Zeile je Instanz aus. `Travel-Day.ps1` wartet auf die neue Karte statt fest 35 s, siehe [Spielzeit sparen](TESTING.md#spielzeit-sparen).
- `todo-risiken`, 05.11.2026: Vorbeugende Aufgaben gegen spätere Probleme eingeplant. Vor Gate G2: `ohne-spike` (Mod läuft ohne Spike), `latenz-test` (Latenz und Verlust auf einem PC simulieren), `desync-melder` (Zustandssumme über den Kontrollkanal). Vor `integration-run`: `dauerlauf`. Vor `release`: `weekly-sperre` und `virenscanner`. Dazu `doku-verweise` (Wartung) und `coop-spielstand` (optional). Team-Entscheidung `weeklymode`: Die Weekly-Challenge wird mit Mod immer gesperrt, auch solo. Absturzberichte, Telemetrie, Erfolge und Bestenlisten sperrt die Mod schon. Keine Spielabnahme.
- Update-Verwaltung, 04.11.2026: Auf Nutzerwunsch nur als optionale Zukunftsarbeit eingeplant: lokale Mod-/Spiel-Prüfung mit sichtbarer Ablehnung vor dem Laden, danach geprüfte Releaseinformationen und erst später ein optionaler Auto-Updater. Die Aufgaben und Abnahmen stehen ausschließlich in [TODO.md](../TODO.md#optionale-optimierungen). Build `26187885` bleibt erzwungen; keine zusätzliche Voraussetzung für den aktuellen Release, keine Umsetzung und keine neue Spielabnahme.

- `layerapps-sperre`, 04.11.2026: Das Spiel fragt beim Run-Ende `deadlydays.layerapps.com` an. Die Mod sperrt den Namen jetzt wie `pixelsplit.games`, ohne den Inhalt zu untersuchen. Mod-Runs senden keine unnötigen Daten. Run-Ende und neuer Run liefen mit der Sperre normal.
- `patches`, 04.11.2026: Die 25 Spielfehler-Fixes aus dem Spike stehen als geprüfte Patches in `src/Game/*Patches.cpp`. Die Mod prüft beim Start alle Stellen und setzt dann alle oder keinen. Mit der Mod setzt der Spike seine eigenen Fassungen nicht mehr und meldet seinen Phasenbeobachter bei der Mod an. Karte, PCG, Testhilfen und Messung bleiben im Spike. Mit `-Mod` lief die echte Route von der MetaBase bis Tag 5 ohne Absturz, die Inventare waren nach jedem Wechsel gleich. Danach kamen nach einer Niederlage beide in die MetaBase zurück. [Patches](../src/Game/README.md#patches).

- `remote-p2`, 04.11.2026: Die Phase-1-Frage ist geklärt: aktuelle schwache `ActivePlayers`-Bindungen und native Coop-Aktivierung machen den Remote-Controller beim Host zu P2, ohne zweiten lokalen Spieler. Ein gezielter Guard nimmt nur die lokale P2-Gerätepflicht aus; native Join-Meldung und Aufräumen veralteter Bindungen separat geprüft. Bewegung und Controller-Tausch funktionieren in der Probe, die lokale UI-Kontrollübergabe im Lager dagegen nicht über das Netz. `remote-spieler`, `eingaben` und Inventar-/UI-Sync bleiben Umsetzungsaufgaben. [Nachweise, Gegenprobe und Grenzen](../research/spikes/a-connect/RemoteP2.md).
- `phasenadapter`, 04.11.2026: Der Client sieht bei jedem Rundenende dieselbe Phase wie der Host (Rundenübersicht, Lager, Routenkarte) und ist dort gesperrt. Der Host erkennt die Phasen an den Panels über `ProcessEvent`, der Client sperrt mit `DisableInput` und zeigt ein Banner. Im Spiel an der ersten Route und an drei Rundenenden geprüft. Transport ist noch eine Datei auf einem PC, über das Netz trägt ihn `transport`/`protokoll`. [Details](../research/spikes/a-connect/RoundPhaseRuntime.md).
- `gate-g1`, 04.11.2026: Nutzer bestätigt verbindlich Weg A als Grundlage, ergänzt um gezielten eigenen Sync für XP, Währungen, Rewards, Pausen und UI. Architekturentscheidung und G1 abgeschlossen; Weg B bleibt nur Rückfalloption bei neuen Gegenbefunden. Transport/Protokoll und konkrete Umsetzung bleiben eigene Aufgaben, Gate G2 und Internet-Abnahme sind dadurch nicht erledigt. [Entscheidung](ARCHITECTURE.md#entscheidungen).
- `game-adapter`, 04.11.2026: Phase 2 ist abgeschlossen. `src/Game` hängt einen Takt an die Fensterprozedur des `UnrealWindow` und ruft darüber `Core::GameThread().Drain()` und die Adapter-Ticks auf dem Game Thread. Offsets stehen SDK-frei in `Offsets.h` und werden beim Bauen gegen ein lokales Dumper-7-SDK geprüft; die CI baut ohne. Der `WorldAdapter` ist das Muster für alle weiteren Adapter. Host und Client bis Tag 1 ohne Fehler geprüft.
- `guidcache`, 04.11.2026: Blockaden-Clientablauf, Ablauf nachladbarer NetGUID-Pfade während Travel-GC und unvollständige Sneak-Zielwiederverwendung behoben. Test-Godmode für beide Spieler samt Schadens-/Replikationsprobe. Übergangslauf bis Endlos-Tag 13 mit zwei weiteren Biomwechseln; Nutzer akzeptiert den Stand als Abschluss, vorerst keine weiteren Detailtests. Neue konkrete Probleme aus künftigem Testing erhalten ein neues TODO. Testgrenzen (Godmode/`!finish`, kein natürlicher Bosskampf) bleiben dokumentiert, siehe [Abschluss](GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache).
- `todo-struktur`, 04.11.2026: `TODO.md` neu aufgebaut. Jede Aufgabe steht genau einmal, offen in der Tabelle ihrer Phase mit Prio, Voraussetzungen („Braucht“), Bearbeitungsart und Spielzeit, erledigt alphabetisch unter „Erledigt“. Checkboxen und tasks-Marker entfallen. „Wo kann ich anfangen?“ wird erzeugt, `tasks.py claim` lehnt blockierte und Team-Aufgaben ab. Spike C ist mit `karte-client` und `pcg` erledigt. Gezählt sind jetzt 45 von 84 Aufgaben.
- `testumgebung`, 04.11.2026: Das „seltsame Laden“ ab Tag 2 war eine pausierte Welt: Die Spike-Umleitung übersprang das Abräumen der Zeitanfragen, die Pause aus `FinishMission` blieb stehen. Dazu zerstörte das Freiräumen am Busplatz die Figur des Clients. Beides ist im Spike behoben, Tag 1 bis 4 laden sauber mit je 2 PlayerStates am Bus. Der PlayerState-Verlust an Tag 4 tritt damit nicht mehr auf, der Client-Absturz an Tag 5 bleibt für `guidcache`. Prüfskript `check_round.py` und fester Routenlauf `Run-Route.ps1` liegen im Repo.
- `aufgabenstatus`, 04.11.2026: Abschlussstatus kommt verbindlich aus den Checkboxen in `TODO.md`; erledigte Pakete stehen getrennt vom offenen Einstiegskatalog. README-Zahlen und Balken werden mit `tools/task_status.py update` daraus aktualisiert und in CI geprüft. Reservierungsbranches und Teil-PRs ersetzen keine Abnahme; fehlendes `gh` macht erledigte Pakete nicht wieder frei. Keine neuen Spielabnahmen und keine Änderung an den gezählten Abschlüssen (45/84).

- `overlay`, 03.11.2026: Phase-2-UI als F8-Toolfenster mit Status-/Debug-Anzeige und vorbereiteten Lobby-Aktionen umgesetzt. Sichtbare Host-/Client-Abnahme auf Build `26187885` bei 100 % und 150 % Skalierung bestanden (F8, Tab, Schließen, Fokuswechsel, Verschieben, Minimieren/Wiederherstellen). Ohne Phase-3-Backend bleiben Lobby-Aktionen gesperrt. [Abnahme und Grenzen](../src/UI/README.md#sichtbare-spielabnahme-03112026).
- `kamera-entscheidung`, 03.11.2026: Gemeinsame Kamera (Option A) für den ersten spielbaren Stand festgelegt, mit ursprünglichem Zoom und Bewegungsbegrenzung. Umsetzung bleibt `kamera-code`; getrennte Kameras sind nur ein optionales Zukunfts-TODO.
- `tageswechsel`, 03.11.2026: Der dritte Client-Absturz an Tag 4 ist behoben, die AttributeSet-Suche prüft jetzt selbst. Ursache der Abstürze ist aber, dass dem Client ab Tag 4 die PlayerStates fehlen: In seinem NetGUID-Cache fehlen die Einträge der PlayerState-Klasse, an Tag 5 stürzt er ab. Wo sie verloren gehen, ist offen. Zuerst `testumgebung`, dann `guidcache`.
- `tag3`, 03.11.2026: Der Host-Absturz bei der Reise auf Tag 3 kam von einem Kartengenerator-Lambda mit rohem Zeiger auf die alte Karte. Mit diesem Fix, dem Timer-Cleanup des Achievement-Detektors und der Exp-Aggregation kam der Run zweimal über die echte Route auf Tag 3, mit gleichen Inventaren. Der Client stürzt an Tag 4 noch ab, Folgeaufgabe `tageswechsel`.
- `spawnedplayers`, 03.11.2026: Hash-/SparseArray-korrekte Bereinigung und native TSet-Einfügung umgesetzt, synthetische Tests und begrenzte Traveltests bestanden. Den Registrierungshook auf der echten Route abzunehmen ist Folgeaufgabe `spawnedplayers-lauf`.
- `spawnedplayers-lauf`, 03.11.2026: Der Hook greift bei Beitritt, erneutem Beitritt und Missionswechsel. Nach jedem Missionswechsel ersetzt der Host aber den Controller des Clients ohne Registrierung, bis zu einer Sekunde lang steht dann ein alter Controller in der Menge. Folgeaufgabe `controller-tausch`.
- `controller-tausch`, 04.11.2026: Der Host meldet den Tausch über das Blueprint-Ereignis `K2_OnSwapPlayerControllers` des GameModes. Der Spike tauscht dort den Eintrag sofort aus. Über zwei Missionswechsel musste der Sekunden-Abgleich nichts mehr nachtragen.
- `build-id`, 03.11.2026: Geprüfte Entwicklungskopie außerhalb von Steam, keine Links zur automatisch aktualisierten Installation. Host/Client samt Mod aus Build `26187885` verbunden getestet. [Einrichtung](SETUP.md#gleiche-build-id-im-team), neue Build-Unterstützung bleibt eine eigene Aufgabe.
- `rundenende` zeigt: Das Spiel überträgt die Endphase nicht, und nachspielen lässt sie sich beim Client nicht. Nötig ist eine eigene Host-Nachricht, also ein Kontrollkanal, den es auch im Spike noch nicht gibt.
- `rundenende`, 03.11.2026: Codec, Empfänger und lokaler Laufzeitadapter ohne Spiel getestet. Automatische Hostquellen, sichtbarer Lauf und vollständige Eingabesperre fehlen, Folgeaufgabe `phasenadapter`.
- In `TODO.md` fehlten fünf Aufgaben, die jetzt eingetragen sind: Kontrollkanal, Übernahme der Spike-Patches, Umsetzung der Kamera, Pausen über den Kontrollkanal und der einmalige Absturz im Missions-Tick.

**Reihenfolge.**

1. `guidcache` ist mit akzeptiertem Run-/Endlos-Stand abgeschlossen. Keine weiteren Detailtests einplanen; bei einem neuen konkreten Fehler ein neues TODO anlegen.
2. `remote-p2` ist als Analyse abgeschlossen, ebenso `karte-client`. Als Grundlage für `remote-spieler` die dokumentierten Patches und die Grenze der lokalen Geräte-/UI-Logik übernehmen; die Produktionsintegration nicht mit diesem Spike-Abschluss verwechseln.
3. Parallel ohne Spielzeit: `grundgeruest`, `tunnel` und `crash-reporter`.
4. Gate G1 ist entschieden: Weg A mit gezielten Sync-Ergänzungen umsetzen. `steam-lobby`, `transport` und `patches` sind durch G1 nicht mehr blockiert; ihre übrigen Voraussetzungen gelten weiter.

## Gesamtweg

```mermaid
flowchart TD
    subgraph A["Klären, ob es geht"]
        direction LR
        P0["Phase 0<br/>Setup"]:::done --> P1["Phase 1<br/>Analyse und Spikes"]:::done --> G1{{"Gate G1<br/>Weg A + gezielter Sync"}}:::done
    end

    subgraph B["Spielbar machen"]
        direction LR
        P2["Phase 2<br/>Fundament"]:::done --> P3["Phase 3<br/>Session und Verbindung"]:::open --> P4["Phase 4<br/>Vertical Slice"]:::open --> G2{{"Gate G2<br/>10 Minuten über Internet"}}:::gate
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

## Weg bis Gate G1

Hier liegt die aktuelle Arbeit. Die Kennungen in Klammern stehen in [TODO.md](../TODO.md), was gerade frei ist, zeigt [Wo kann ich anfangen?](../TODO.md#wo-kann-ich-anfangen).

```mermaid
flowchart LR
    subgraph BASIS["Erledigt"]
        A1["Verbindung, Spawn,<br/>Bewegung, Schießen"]:::done
        A2["Weg vom Menü<br/>in die Mission"]:::done
        A3["Analysen: Coop, Karte,<br/>Gegner, Kamera, Sync-Bedarf"]:::done
    end

    subgraph RUN["Run-Ablauf"]
        TE["Saubere Testumgebung,<br/>Laden und Spawn am Bus<br/>(testumgebung)"]:::done
        T3["Run bis Endlos-Tag 13,<br/>zwei Biomwechsel akzeptiert<br/>(guidcache)"]:::done
        TE --> T3
        RE["Rundenende, Lager,<br/>Routenkarte<br/>(rundenende, phasenadapter)"]:::done
        NI["Echte Niederlage,<br/>Rückkehr in die MetaBase<br/>(niederlage)"]:::done
        TR["Client-Absturz<br/>beim Trennen<br/>(trennen)"]:::done
        MT["Einmaliger Absturz<br/>im Missions-Tick"]:::done
        RE --> NI
    end

    subgraph SPIELER["Remote-Spieler"]
        SP["Remote-Controller in<br/>SpawnedPlayers<br/>(spawnedplayers)"]:::done
        RP["Remote-P2-Weg und<br/>UI-Grenzen geprüft<br/>(remote-p2)"]:::done
        TF["Treffer des Clients<br/>direkt belegen<br/>(treffer)"]:::done
        SP --> RP
    end

    subgraph WELT["Karte und Kamera"]
        KC["Seed und Profil beim Client,<br/>Duplikate unterdrücken<br/>(karte-client)"]:::done
        SC["Spike C: gleiche<br/>Mission auf beiden Seiten<br/>(spike-c)"]:::done
        KE["Gemeinsame Kamera<br/>entschieden"]:::done
        KC --> SC
    end

    subgraph LOGIK["Spiellogik"]
        PA["Pausen bei Level-up<br/>und Inventar analysiert<br/>(pausen)"]:::done
        BP["Blueprint-Logik:<br/>typische Pfade geprueft<br/>(blueprint-logik)"]:::done
    end

    subgraph TECHNIK["Technik"]
        LA["Spike D: Last<br/>(last)"]:::done
        TU["Spike E: Steam-Tunnel<br/>(tunnel)"]:::done
        TP["P2P auf zwei PCs<br/>(tunnel-p2p)"]:::done
        TU --> TP
        AP["Andockpunkte:<br/>Test im Spiel bestanden<br/>(andockpunkte)"]:::done
        CR["Crash-Reporter<br/>zuverlässig aus<br/>(crash-reporter)"]:::done
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

    G1{{"Gate G1:<br/>Weg A + gezielter Sync"}}:::done

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
    G1{{"Gate G1: Weg A festgelegt"}}:::done

    subgraph PH2["Phase 2: Fundament"]
        L["Loader: Proxy-DLL"]:::done
        C["Core: Logging, Hooks,<br/>Game Thread, Versionsprüfung"]:::done
        GA["Game: Adapter-Schicht"]:::done
        U["UI: Overlay"]:::done
        W["Deploy-Skript, Tests, CI"]:::done
        L --> C --> GA
        C --> U
    end

    subgraph PH3["Phase 3: Session und Verbindung"]
        LO["Steam-Lobby<br/>(steam-lobby)"]:::done
        LL["Lobby mit zwei Konten<br/>(lobby-live, zu zweit)"]:::open
        TN["Transport-Tunnel<br/>(transport)"]:::done
        HS["Handshake: Build und Mod-Version"]:::open
        FE["Timeout, Abbruch,<br/>Host verlässt das Spiel<br/>(net-fehler)"]:::done
        KK["Kontrollkanal"]:::open
        LO --> TN --> HS --> KK --> FE
        LO --> LL
    end

    subgraph PH4["Phase 4: Vertical Slice"]
        SH["Spike-Patches als Hooks<br/>übernehmen"]:::done
        R2["Remote-Spieler als P2"]:::open
        MS["Gemeinsamer Missionsstart,<br/>gleiche Karte"]:::open
        KM["Gemeinsame Kamera<br/>umsetzen"]:::open
        KA["Bewegung, Waffen, Treffer, Tod"]:::open
        GT["Ein Gegnertyp"]:::open
        EI["Eingaben von Remote<br/>trennen"]:::open
        OS["Mod ohne Spike<br/>(ohne-spike)"]:::open
        LT["Latenz simulieren<br/>(latenz-test)"]:::open
        DM["Desync-Melder<br/>(desync-melder)"]:::open
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
        DL["Dauerlauf über viele Tage<br/>(dauerlauf)"]:::open
    end

    subgraph PH6["Phase 6: Release"]
        SB["Bestenlisten und<br/>Erfolge sperren"]:::done
        TE["Telemetrie und<br/>Crash-Reports aus"]:::done
        SV["Eigene Mod-Spielstände"]:::open
        IN["Installer oder Anleitung"]:::open
        WS["Weekly-Challenge sperren<br/>(weekly-sperre)"]:::open
        VS["Virenscanner und Proton<br/>(virenscanner)"]:::open
    end

    Z(["Mod fertig und nutzbar"]):::goal

    G1 --> PH2 --> PH3 --> PH4 --> G2 --> PH5 --> PH6 --> Z

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#ffffff
    classDef wip fill:#f9a825,stroke:#b27a00,color:#1a1a1a
    classDef open fill:#eceff1,stroke:#607d8b,color:#1a1a1a
    classDef gate fill:#6a1b9a,stroke:#3e1163,color:#ffffff
    classDef goal fill:#1565c0,stroke:#0d3c78,color:#ffffff
```

## Zentral und parallel

Türkis ist zentrale Arbeit, die nacheinander läuft. Grau sind Pakete, die daneben parallel laufen. Die Kennungen und ihre genauen Abhängigkeiten (Spalte „Braucht“) stehen in [TODO.md](../TODO.md).

```mermaid
flowchart TD
    GR["grundgeruest"]:::central --> W1["Welle 1<br/>overlay, deploy, ci"]:::open
    GR --> GA["game-adapter"]:::central
    G1{{"Gate G1: Weg A festgelegt"}}:::done --> W2["Welle 2<br/>steam-lobby, transport,<br/>net-fehler"]:::done
    GR --> W2
    G1 --> GA
    GA --> PT["patches"]:::central --> RS["remote-spieler"]:::central
    GA --> W5["Welle 5<br/>tutorial-aus, online-sperren,<br/>telemetrie-aus, mod-saves, installer"]:::open
    RS --> W3["Welle 3<br/>missionsstart, kamera-code, bewegung,<br/>eingaben, kampf, gegnertyp"]:::open
    W1 --> IG["integration-g2"]:::central
    W2 --> PR["protokoll"]:::done
    W2 --> IG
    W3 --> IG
    IG --> G2{{"Gate G2"}}:::gate
    PR --> W4["Welle 4<br/>gegner-alle, fortschritt, inventar-sync,<br/>airdrop-auswahl, inventar-ui, pausen-sync, tagesablauf,<br/>run-ende, vfx-sound, reconnect"]:::open
    G2 --> W4
    W4 --> IR["integration-run"]:::central
    W5 --> IR
    RV["Vorbeugung<br/>ohne-spike, latenz-test"]:::open --> IG
    PR --> DS["desync-melder"]:::open --> IG
    IG --> DL["dauerlauf"]:::open --> IR
    W5 --> W6["weekly-sperre, virenscanner"]:::open --> RL
    IR --> RL["release"]:::central
    RL --> Z(["Mod fertig und nutzbar"]):::goal

    classDef done fill:#2e7d32,stroke:#1b5e20,color:#ffffff
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

Gate G1 ist mit Weg A und gezielten Sync-Ergänzungen abgeschlossen. Ein Wechsel zu Weg B oder eine Verkleinerung wäre nur bei neuen konkreten Gegenbefunden erneut vom Team zu entscheiden; derzeit ist dafür keine zusätzliche Aufgabe geplant.

## Pflege

Wer eine Aufgabe abschließt, ändert hier die Farbe des Knotens. Die Zählung im [README](../README.md#fortschritt) erzeugt `py tools/task_status.py update`. Aufgaben ohne eigenen Knoten gehören zum thematisch passenden Knoten. Die Wellen fassen Aufgaben mit gleichen Voraussetzungen zusammen, verbindlich ist die Spalte „Braucht“ in `TODO.md`.
