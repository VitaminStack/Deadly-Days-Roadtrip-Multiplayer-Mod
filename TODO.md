# TODO

Die einzige Liste offener Arbeit: **Probleme** (Fehler) und **Verbesserungen**. Alle Projektphasen sind abgeschlossen ([E-13](docs/ENTSCHEIDUNGEN.md#e-13-phasen-abgeschlossen-flache-aufgabenliste)); Aufgaben hängen nicht voneinander ab. Erledigtes steht in [docs/ERLEDIGT.md](docs/ERLEDIGT.md), dort auch die [ausstehenden Bestätigungstests](docs/ERLEDIGT.md#bestätigungstests-ausstehend) (nur Info). `py tools/task_status.py check` prüft das Format.

Wie man die Tabellen liest, Fallakten anlegt und Aufgaben abschließt, was nicht erneut gemacht werden soll und welche Dateien alle teilen, steht in [docs/PROZESS.md](docs/PROZESS.md). Risiken stehen in der [Roadmap](docs/ROADMAP.md#risiken).

**Fokus:** Probleme, die den Run zu zweit stören, vor Verbesserungen; innerhalb nach Prio.

## Wo kann ich anfangen?

Diese Liste erzeugt `py tools/task_status.py update` aus den Tabellen, oben steht das Wichtigste. Ob jemand eine Aufgabe schon reserviert hat, zeigt nur `py tools/tasks.py list`.

<!-- frei:start -->
| Kennung | Prio | Phase | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `client-haenger-nach-niederlage` | P1 | [Problem](#probleme) | parallel | mittel |
| `aufgaben-werkzeuge-entschlacken` | P1 | [Verbesserung](#verbesserungen) | zentral | keine |
| `coop-inventar-ablauf` | P1 | [Verbesserung](#verbesserungen) | zentral | viel |
| `coop-inventar-analyse` | P1 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `coop-inventar-konzept` | P1 | [Verbesserung](#verbesserungen) | zentral | keine |
| `coop-inventar-zwei` | P1 | [Verbesserung](#verbesserungen) | zentral | viel |
| `coop-modus-wahl` | P1 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `prozess-neu-schreiben` | P1 | [Verbesserung](#verbesserungen) | zentral | keine |
| `verbesserungen-sichten` | P1 | [Verbesserung](#verbesserungen) | zentral | keine |
| `opt-dash-aufprall` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-gegner-spawnverzug` | P2 | [Problem](#probleme) | parallel | mittel |
| `opt-haende-vorschau-items` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-highway-ziel` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-itemsack-flug` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-schuss-animation` | P2 | [Problem](#probleme) | parallel | mittel |
| `opt-waffen-hover` | P2 | [Problem](#probleme) | parallel | wenig |
| `coop-inventar-geteilt` | P2 | [Verbesserung](#verbesserungen) | zentral | viel |
| `coop-spielstand` | P2 | [Verbesserung](#verbesserungen) | parallel | viel |
| `doku-rest-ist-zustand` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `dumper7-image-ende` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `game-ordner` | P2 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `game-readme-kuerzen` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `offsets-alte-rvas` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-client-takt` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `ps-skripte-pruefen` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `research-archivieren` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `spike-abloesen` | P2 | [Verbesserung](#verbesserungen) | zentral | mittel |
| `sync-aufraeumen` | P2 | [Verbesserung](#verbesserungen) | zentral | mittel |
| `themen-fuellen` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `update-autoupdater` | P2 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `update-releasefeed` | P2 | [Verbesserung](#verbesserungen) | zentral | keine |
| `update-startpruefung` | P2 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `werkzeuge-windows-pruefen` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |

**Wartet auf das Team:** `spawn-nach-kartenaufbau`.

**Bestätigungstests ausstehend (nur Info, blockiert nichts):** 43, siehe [docs/ERLEDIGT.md](docs/ERLEDIGT.md#bestätigungstests-ausstehend).
<!-- frei:end -->

## Probleme

Fehler im Spiel oder in den Werkzeugen. Die Beschreibung steht in [docs/OPTIONEN.md](docs/OPTIONEN.md) oder einer Fallakte unter `docs/faelle/`; wer einen Fehler bearbeitet, legt die Fallakte an.

| Kennung | Prio | Aufgabe | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `client-haenger-nach-niederlage` | P1 | Nach der Niederlage hängt der Client beim Ankommen in der MetaBase (Live-Test 26.11., zwei PCs), der Spielthread steht. **Fertig, wenn** der Client nach Niederlage und Sieg in der MetaBase bedienbar ankommt und ein neuer Run startet. Ordner `src/Game`. **Einstieg** [Fallakte](docs/faelle/client-haenger-nach-niederlage.md) | parallel | mittel |
| `spawn-nach-kartenaufbau` | P1 | **Nur wenn der Live-Test die Theorie bestätigt:** Host setzt die Figuren an den Bus, bevor der Client seine Karte gebaut hat. **Fertig, wenn** der Client nach jedem Zonenstart auf dem Boden steht, auch bei langsamem Kartenaufbau, und Join, Reconnect und Travel unverändert laufen. Ordner `src/Game`. **Einstieg** [Fallakte](docs/faelle/spawn-nach-kartenaufbau.md) | Team | mittel |
| `opt-dash-aufprall` | P2 | Prallt der Client mit dem Dash gegen ein Objekt, hört er kein Geräusch … | parallel | wenig |
| `opt-gegner-spawnverzug` | P2 | Unter Wellenlast (`!wave`) erscheinen neue Gegner beim Client bis rund 8 s später als beim Host. **Fertig, wenn** neue Gegner beim Client höchstens 1 s nach dem Host erscheinen, auch unter Wellenlast. | parallel | mittel |
| `opt-haende-vorschau-items` | P2 | Hantel (`ItemData_Dumbell`), Hand des Blutes (`ItemData_HandOfBlood`) und Plasmahand (`ItemData_PlasmaHand`) senken über Standardwirkungen die Hände … | parallel | wenig |
| `opt-highway-ziel` | P2 | Im Event „Räume den Highway!“ erschien kein Benzin, das Ziel verlangte einen Abschluss (2/10). Erst ohne Mod klären, wie das Event endet. | parallel | wenig |
| `opt-itemsack-flug` | P2 | Itemsäcke aus Containern werden beim Client erst direkt an der Figur sichtbar, der Ausstoß fehlt. | parallel | wenig |
| `opt-schuss-animation` | P2 | In höheren Leveln ist die Schuss-Animation der Client-Figur beim Client kaputt (Waffen übereinander), und die Figur gleitet über den Boden. | parallel | mittel |
| `opt-waffen-hover` | P2 | Beim Client hebt das Inventar beim Überfahren einer Waffe mit der Maus die Items … | parallel | wenig |

## Verbesserungen

Neue Funktionen, Feinschliff und Release. Je Aufgabe steht hier nur eine Kurzfassung, die vollständige Beschreibung unter ihrer Kennung in [docs/OPTIONEN.md](docs/OPTIONEN.md).

| Kennung | Prio | Aufgabe | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `aufgaben-werkzeuge-entschlacken` | P1 | Nach dem Merge von #292 die tote Logik in `tools/tasks.py` und `tools/task_status.py` entfernen: Phasen, „Braucht“, zurückgestellt, Merge-Plan nach Abhängigkeiten, alte Bestätigungszeilen; prüfen, ob `claim` mit Worktree, Draft-PR und `gh` noch nötig ist. **Fertig, wenn** der Code nur noch die flache Liste kennt und alle Tests grün sind. | zentral | keine |
| `coop-inventar-ablauf` | P1 | Alle Inventarabläufe auf das geteilte Inventar umstellen: Airdrop-Wahl, Itemsäcke, Werkbank, Lager mit Verkauf und Reparatur, Rundenende mit … | zentral | viel |
| `coop-inventar-analyse` | P1 | Das native geteilte Koop-Inventar ohne Mod untersuchen, Referenz ist lokaler Koop mit zwei Controllern auf einem PC: Aufbau des gemeinsamen Rucksacks … | parallel | mittel |
| `coop-inventar-konzept` | P1 | Konzept für das geteilte Inventar online, ausgelegt auf zwei bis vier Spieler. | zentral | keine |
| `coop-inventar-zwei` | P1 | Geteiltes Inventar für zwei Spieler online nach dem Konzept: ein gemeinsamer Rucksack mit einem Bereich je Spieler, beide sehen dasselbe Raster … | zentral | viel |
| `coop-modus-wahl` | P1 | Der Host wählt vor dem Hosten der Lobby zwischen den Modi Eigene Inventare und Geteiltes Inventar (`coop-inventar-geteilt`). | parallel | mittel |
| `prozess-neu-schreiben` | P1 | `docs/PROZESS.md` neu schreiben: ein Ablauf ohne Doppelungen. **Fertig, wenn** die Datei unter 6 KB liegt und `doc_check.py` diese Grenze prüft. | zentral | keine |
| `verbesserungen-sichten` | P1 | Jede Verbesserung samt `docs/OPTIONEN.md` von veralteten Vorgaben, alten Builds und Erledigtem befreien. **Fertig, wenn** jede Zeile einen aktuellen Fehler oder Wunsch mit prüfbarem „Fertig, wenn“ beschreibt. | zentral | keine |
| `coop-inventar-geteilt` | P2 | „Seperate new shared Inventory Logic like real coop“: eine neue, getrennte Logik für ein geteiltes Inventar wie im lokalen Koop, für zwei bis vier Spieler, als eigener Pfad neben der bisherigen Logik (Eigene Inventare). Der Host wählt vor dem Hosten der Lobby, welche Logik gilt. | zentral | viel |
| `coop-spielstand` | P2 | Der Host speichert einen Coop-Run und setzt ihn später mit demselben Client fort. | parallel | viel |
| `doku-rest-ist-zustand` | P2 | Übrige Doku unter `docs/` und `src/Net/README.md` auf den Ist-Zustand bringen. **Fertig, wenn** dort keine datierten Befunde und alten Builds mehr stehen. | parallel | keine |
| `dumper7-image-ende` | P2 | Prüfen, ob `tools/dumper7/image-ende.patch` noch nötig ist (der Fehler trat nur auf `26669225` auf): SDK auf `26786272` ohne Patch erzeugen. **Fertig, wenn** der Patch entfällt oder der Fix im Fork `VitaminStack/Dumper-7` liegt und das Submodul umgestellt ist. | parallel | wenig |
| `game-ordner` | P2 | `src/Game` (109 Dateien flach) nach Fachgebiet in Unterordner teilen und `RewardPatches.cpp` (2400 Zeilen) aufteilen. **Fertig, wenn** Ordner den Themen entsprechen und die CI grün ist. | zentral | wenig |
| `game-readme-kuerzen` | P2 | `src/Game/README.md` aufteilen: Die Patch-Tabelle deckt nur 12 von rund 40 Dateien ab. **Fertig, wenn** die Datei unter 60 KB liegt und jede `*Patches.cpp` beschreibt. | parallel | keine |
| `offsets-alte-rvas` | P2 | In `docs/OFFSETS.md` die Tabellen mit RVAs alter Builds auf `26786272` umrechnen (`tools/rva_map.py`) oder ins Archiv verschieben. **Fertig, wenn** die Datei nur Werte für den unterstützten Build enthält. | parallel | wenig |
| `opt-client-takt` | P2 | Beim Client halten die Widget-Suche der … | parallel | wenig |
| `ps-skripte-pruefen` | P2 | PSScriptAnalyzer für alle `*.ps1` in die CI aufnehmen, Funde beheben. **Fertig, wenn** der CI-Schritt grün ist. | parallel | keine |
| `research-archivieren` | P2 | Erledigte Ordner unter `research/spikes/` (außer `a-connect`) archivieren. **Fertig, wenn** dort nur noch genutzte Werkzeuge liegen. | parallel | keine |
| `spike-abloesen` | P2 | Testbefehle des Spikes (`!finish`, Godmode, `!wave`, Seed) in den Testkanal der Mod übernehmen, danach `research/spikes/a-connect` löschen und `tools/spike/` in `tools/test/` umbenennen. **Fertig, wenn** alle Testskripte ohne Spike laufen. | zentral | mittel |
| `sync-aufraeumen` | P2 | Nach dem ersten kompletten Run alle Patches, Hooks und Kontrollkanal-Nachrichten durchgehen: Was deckt inzwischen native Replikation oder lokale … | zentral | mittel |
| `themen-fuellen` | P2 | Die Themen-Dateien unter `docs/themen/` auf einen vollständigen Ist-Zustand bringen. **Fertig, wenn** jede Themen-Datei ihr Gebiet ohne Archiv erklärt. | parallel | keine |
| `update-autoupdater` | P2 | Später optionales automatisches Mod-Update auf dem Releasefeed aufbauen, nur nach ausdrücklicher Aktivierung. | zentral | wenig |
| `update-releasefeed` | P2 | Vertrauenswürdig prüfbare Release-/Kompatibilitätsinformationen anbieten: signierte Metadaten mit Mod-Version, unterstützten … | zentral | keine |
| `update-startpruefung` | P2 | Mod-/Spiel-Kompatibilität vor dem Laden prüfen: gemeinsame Quelle für Mod-Version, Protokollversion, unterstützte Game-Builds, Kompatibilitätsprofil … | zentral | wenig |
| `werkzeuge-windows-pruefen` | P2 | `Collect-Logs.ps1` (Kurzfassung) und die Projekt-Skills auf Windows prüfen. **Fertig, wenn** `*.summary.txt` entstehen und die Skills laden. | parallel | wenig |
