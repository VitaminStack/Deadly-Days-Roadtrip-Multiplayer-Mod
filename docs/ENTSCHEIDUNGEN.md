# Entscheidungen

Verbindliche Vorgaben des Menschen, je eine Kennung. Andere Dokumente verweisen mit `E-<nn>` hierher, statt die Entscheidung zu wiederholen.
Eine neue Entscheidung bekommt die nächste freie Nummer; ersetzt sie eine ältere, steht dort „ersetzt durch“.

## E-01: Lokal zuerst, minimal synchronisieren

- **Datum:** 05.11.2026, projektweit.
- **Entscheidung:** Möglichst viel vorhandene Logik und Darstellung lokal beim Client ausführen und nur das für Multiplayer notwendige Minimum zwischen Host und Clients teilen. Pro gemeinsamem Zustand genau eine Autorität.
- **Folge:** Regeln in [ARCHITECTURE.md](ARCHITECTURE.md#projektweite-nutzervorgabe-vom-05112026), Fundstellen im [Client-lokal-Audit](CLIENT_LOCAL_AUDIT.md).

## E-02: Meta-Fortschritt des Hosts gilt

- **Datum:** 06.11.2026.
- **Entscheidung:** Für den gemeinsamen Run gilt der Meta-Stand des Hosts, nicht der des Clients.
- **Folge:** [Sync-Bedarf](SYNC_REQUIREMENTS.md).

## E-03: Itemsack zählt wie Airdrop für alle

- **Datum:** 06.11.2026.
- **Entscheidung:** Ein Itemsack gibt jedem Spieler Items wie ein Airdrop.
- **Folge:** Itemsack-Vergabe beim Host für alle Spieler (`src/Game/PatchPolicy.h`, `src/Game/InventoryPatches.cpp`).

## E-04: Wiederverbinden und Weekly-Sperre sind optional

- **Datum:** 09.11.2026.
- **Entscheidung:** `reconnect` und `weekly-sperre` sind keine Voraussetzung für den Run oder den Release.
- **Folge:** Beide stehen unter den Optionen in [TODO.md](../TODO.md).

## E-05: Einzelne Optionen vorgezogen

- **Datum:** 11.11.2026.
- **Entscheidung:** Mehrere `opt-*`-Aufgaben warten nicht auf `integration-run` und werden auf einem PC getestet.
- **Folge:** Ersetzt durch E-09: Optionen warten, bis Phase 0 bis 5 erledigt sind.

## E-06: Abschluss mit Ein-PC-Beleg

- **Datum:** 13.11.2026.
- **Entscheidung:** Eine Aufgabe darf mit Beleg auf einem PC (zwei Instanzen) abgeschlossen werden; die Prüfung zu zweit steht dann unter „Live-Bestätigung ausstehend“.
- **Folge:** [Live-Bestätigung ausstehend](PROZESS.md#live-bestätigung-ausstehend).

## E-07: Nur der neueste Spiel-Build

- **Datum:** 21.11.2026.
- **Entscheidung:** Die Mod unterstützt nur den neuesten Spiel-Build, derzeit `26786272`. Ältere Builds werden entfernt.
- **Folge:** Adressen in `src/Game/AddressMap.h`, alte Werte im [Archiv](archive/OFFSETS-ALTE-BUILDS.md).

## E-08: Weniger neue Aufgaben

- **Datum:** 21.11.2026.
- **Entscheidung:** Neue Kennungen nur für Funde, die den Run zu zweit verhindern (P0/P1), oder auf Anweisung. Funde mit derselben Ursache im selben PR beheben.
- **Folge:** Regel „Fokus“ in [CLAUDE.md](../CLAUDE.md).

## E-09: Erst spielbar

- **Datum:** nicht festgehalten, gültig seit spätestens 21.11.2026.
- **Entscheidung:** Vorrang hat, dass zwei Spieler einen Run durchspielen können. Optionale Aufgaben warten, bis Phase 0 bis 5 erledigt sind.
- **Folge:** `tasks.py` zeigt Optionen als `zurückgestellt`, `claim` lehnt sie ab.

## E-10: Release 1.0

- **Datum:** 24.11.2026, ersetzt die Festlegung „Mod-Version 1.0“ vom 17.11.2026.
- **Entscheidung:** Der Stand gilt als spielbar und erscheint als Release 1.0; `integration-run` ist erledigt.
- **Folge:** [Roadmap](ROADMAP.md#stand).

## E-11: Projekt aufräumen

- **Datum:** 24.11.2026.
- **Entscheidung:** Doku auf den Ist-Zustand reduzieren, Geschichte ins Archiv, Aufgabenprozess verschlanken (`projekt-aufraeumen`).
- **Folge:** [Prozess](PROZESS.md#regeln-für-doku-und-abschluss), Archiv unter `docs/archive/`.

## E-12: Bestätigungstests sind nur Info

- **Datum:** 24.11.2026.
- **Entscheidung:** Ausstehende Bestätigungstests halten nur fest, was noch zu prüfen ist. Sie verhindern keine andere Arbeit und haben keine Obergrenze.
- **Folge:** Die Aufgabe gilt als erledigt; der offene Test steht unter [Bestätigungstests ausstehend](ERLEDIGT.md#bestätigungstests-ausstehend). Ersetzt die Obergrenze aus `projekt-aufraeumen`.
