# TODO

Die einzige Liste offener Arbeit: **Probleme** (Fehler) und **Verbesserungen**. Alle Projektphasen sind abgeschlossen ([E-13](docs/ENTSCHEIDUNGEN.md#e-13-phasen-abgeschlossen-flache-aufgabenliste)); Aufgaben hängen nicht voneinander ab. Erledigtes steht in [docs/ERLEDIGT.md](docs/ERLEDIGT.md), dort auch die [ausstehenden Bestätigungstests](docs/ERLEDIGT.md#bestätigungstests-ausstehend) (nur Info). `py tools/task_status.py check` prüft das Format.

Wie man die Tabellen liest, Fallakten anlegt und Aufgaben abschließt, was nicht erneut gemacht werden soll und welche Dateien alle teilen, steht in [docs/PROZESS.md](docs/PROZESS.md). Risiken stehen in der [Roadmap](docs/ROADMAP.md#risiken).

**Fokus:** Abschließen statt erweitern. Reihenfolge: nach Prio, bei gleicher Prio Probleme vor Verbesserungen, dann wenig Spielzeit vor viel und parallel vor zentral.

## Wo kann ich anfangen?

Diese Liste erzeugt `py tools/task_status.py update` aus den Tabellen. Die oberste freie Zeile ist die nächste sinnvolle Aufgabe; Zeilen mit „parallel“ können mehrere Sitzungen gleichzeitig nehmen. Ob jemand eine Aufgabe schon reserviert hat, zeigt nur `py tools/tasks.py list`; `list --short` hat dieselbe Reihenfolge, `list` wendet sie je Abschnitt an.

<!-- frei:start -->
| Kennung | Prio | Phase | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `aufgaben-werkzeuge-entschlacken` | P1 | [Verbesserung](#verbesserungen) | zentral | keine |
| `prozess-neu-schreiben` | P1 | [Verbesserung](#verbesserungen) | zentral | keine |
| `coop-inventar-konzept` | P1 | [Verbesserung](#verbesserungen) | zentral | mittel |
| `opt-dash-aufprall` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-haende-vorschau-items` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-highway-ziel` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-itemsack-flug` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-powerups-client` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-waffen-hover` | P2 | [Problem](#probleme) | parallel | wenig |
| `opt-gegner-spawnverzug` | P2 | [Problem](#probleme) | parallel | mittel |
| `opt-schuss-animation` | P2 | [Problem](#probleme) | parallel | mittel |
| `doku-rest-ist-zustand` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `game-readme-kuerzen` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `ps-skripte-pruefen` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `research-archivieren` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `themen-fuellen` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `update-releasefeed` | P2 | [Verbesserung](#verbesserungen) | zentral | keine |
| `dumper7-image-ende` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-client-takt` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `werkzeuge-windows-pruefen` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `game-ordner` | P2 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `update-autoupdater` | P2 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `update-startpruefung` | P2 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `spike-abloesen` | P2 | [Verbesserung](#verbesserungen) | zentral | mittel |
| `sync-aufraeumen` | P2 | [Verbesserung](#verbesserungen) | zentral | mittel |
| `coop-spielstand` | P2 | [Verbesserung](#verbesserungen) | parallel | viel |

**Wartet auf das Team:** `spawn-nach-kartenaufbau`, `coop-modus-wahl`, `coop-inventar-zwei`, `coop-inventar-geteilt`.

**Bestätigungstests ausstehend (nur Info, blockiert nichts):** 39, siehe [docs/ERLEDIGT.md](docs/ERLEDIGT.md#bestätigungstests-ausstehend).
<!-- frei:end -->

## Probleme

Fehler im Spiel oder in den Werkzeugen. Jede Zeile nennt ihre Abnahme („Fertig, wenn“); mehr steht in einer Fallakte unter `docs/faelle/` oder in [docs/OPTIONEN.md](docs/OPTIONEN.md). Wer einen Fehler bearbeitet, legt die Fallakte an.

| Kennung | Prio | Aufgabe | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `spawn-nach-kartenaufbau` | P1 | **Nur wenn der Live-Test die Theorie bestätigt:** Host setzt die Figuren an den Bus, bevor der Client seine Karte gebaut hat. **Fertig, wenn** der Client nach jedem Zonenstart auf dem Boden steht, auch bei langsamem Kartenaufbau, und Join, Reconnect und Travel unverändert laufen. Ordner `src/Game`. **Einstieg** [Fallakte](docs/faelle/spawn-nach-kartenaufbau.md) | Team | mittel |
| `opt-dash-aufprall` | P2 | Prallt der Client mit dem Dash gegen ein Objekt, hört er kein Geräusch. Vermutung: Der Host führt den Dodge des Clients aus, Aufprall und Sound laufen nur dort. **Fertig, wenn** der Client beim Aufprall seines Dashes denselben Sound hört wie der Host bei seinem eigenen, genau einmal, auf einem PC mit zwei Instanzen belegt. **Einstieg** [Client-Darstellung](docs/archive/GAME_ANALYSIS.md#client-darstellung-im-live-spiel-am-07112026-todo-client-optik) | parallel | wenig |
| `opt-haende-vorschau-items` | P2 | Hantel (`ItemData_Dumbell`), Hand des Blutes (`ItemData_HandOfBlood`) und Plasmahand (`ItemData_PlasmaHand`) senken die Hände, die eine Waffe braucht. Der Client legt Wirkungen in einer lokalen Bearbeitung nicht an; Vermutung: Sein Panel lässt darum eine Waffe nicht zu, die erst mit einem dieser Items passt. **Fertig, wenn** gemessen ist, ob `GetHandsRequiredForItem` beim Client in der offenen Bearbeitung dem Wert des Hosts nach dem Übernehmen folgt, und eine Abweichung behoben ist. **Einstieg** [Roboterarm beim Client](docs/archive/GAME_ANALYSIS.md#roboterarm-beim-client-client-roboterarm), Testweg `!shop place` | parallel | wenig |
| `opt-highway-ziel` | P2 | Im Event „Räume den Highway!“ erschien kein Benzin, das Ziel verlangte einen Abschluss (2/10). **Fertig, wenn** im Vergleich ohne Mod belegt ist, ob das natives Verhalten ist, und mit Mod das Ziel bei Host und Client gleich zählt und die Runde regulär endet. **Einstieg** [Live-Test 13.11.](docs/archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-livetest-1311) | parallel | wenig |
| `opt-itemsack-flug` | P2 | Itemsäcke aus Containern werden beim Client erst direkt an der Figur sichtbar, der Ausstoß fehlt. **Fertig, wenn** der Client die Säcke wie der Host aus dem Container fliegen sieht, auf einem PC mit zwei Instanzen im Bildvergleich belegt. **Einstieg** [Live-Test 13.11. abends](docs/archive/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-13112026-abends-livetest-1311-abend) | parallel | wenig |
| `opt-powerups-client` | P2 | Dosenessen und Staubsauger wirken beim Client nicht, beim Host schon (Live-Test 26.11.). **Fertig, wenn** Power-ups auf die Figur des Clients wie beim Host wirken. **Einstieg** [Fallakte](docs/faelle/opt-powerups-client.md) | parallel | wenig |
| `opt-waffen-hover` | P2 | Beim Client hebt das Inventar beim Überfahren einer Waffe mit der Maus die Items nicht hervor, die auf die Waffe wirken; beim Host schon. Lokal lösen: die native Hervorhebung aus den replizierten Inventardaten anzeigen, keine neue Nachricht. **Fertig, wenn** der Client beim Überfahren dieselben Items hervorgehoben sieht wie der Host, auf einem PC im Bildvergleich belegt. | parallel | wenig |
| `opt-gegner-spawnverzug` | P2 | Unter Wellenlast (`!wave`) erscheinen neue Gegner beim Client bis rund 8 s später als beim Host. **Fertig, wenn** ein Lesevergleich beider Instanzen (etwa `tools/spike/enemy_sync.py`) unter Wellenlast für neue Gegner höchstens 1 s Verzug beim Client misst. | parallel | mittel |
| `opt-schuss-animation` | P2 | Die Client-Figur gleitet ohne Laufanimation: beim Host starr (Tester im Live-Test 26.11., Bestätigungstest von `opt-laufanimation-client` durchgefallen), beim Client zuletzt am 13.11. gemeldet. Gegen die Geisterwaffen („Waffen übereinander“) ist #224 gemergt, auf einem PC gemessen, live nicht bestätigt. **Fertig, wenn** die Ursache des Gleitens belegt ist (auf einem PC nachgestellt oder im Log eines Live-Tests gemessen) und die Client-Figur danach bei Host und Client durchgehend mit Laufanimation läuft und keine Waffen übereinander trägt. **Einstieg** [Details](docs/OPTIONEN.md#opt-schuss-animation) | parallel | mittel |

## Verbesserungen

Neue Funktionen, Feinschliff und Release. Jede Zeile nennt ihre Abnahme („Fertig, wenn“); reicht die Zeile nicht, steht der Langtext unter der Kennung in [docs/OPTIONEN.md](docs/OPTIONEN.md).

| Kennung | Prio | Aufgabe | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `aufgaben-werkzeuge-entschlacken` | P1 | Die tote Logik in `tools/tasks.py` und `tools/task_status.py` entfernen: Phasen, „Braucht“, zurückgestellt, Merge-Plan nach Abhängigkeiten, alte Bestätigungszeilen; entscheiden, ob `claim` mit Worktree, Draft-PR und `gh` bleibt. **Fertig, wenn** der Code nur noch die flache Liste kennt, `tools/README.md` dazu passt und alle Tests grün sind. | zentral | keine |
| `prozess-neu-schreiben` | P1 | `docs/PROZESS.md` (rund 17 KB) neu schreiben: ein Ablauf ohne Doppelungen und ohne Phasen, „Braucht“ und zurückgestellte Aufgaben. **Fertig, wenn** die Datei unter 6 KB liegt und `doc_check.py` diese Grenze prüft. | zentral | keine |
| `coop-inventar-konzept` | P1 | Das native geteilte Koop-Inventar ohne Mod untersuchen (lokaler Koop mit zwei Controllern auf einem PC) und daraus das Konzept für das geteilte Inventar online für zwei bis vier Spieler schreiben. **Fertig, wenn** `docs/themen/INVENTAR_UND_HANDEL.md` Datenmodell, Wirkregeln über Bereichsgrenzen und native Andockpunkte mit Belegt/Vermutung nennt, das Konzept in `docs/` die Fragen des Architekturprinzips beantwortet und das Team es freigegeben hat. **Einstieg** [Details](docs/OPTIONEN.md#coop-inventar-konzept) | zentral | mittel |
| `coop-modus-wahl` | P1 | Der Host wählt vor dem Hosten der Lobby zwischen Eigene Inventare (Standard) und Geteiltes Inventar; der Modus gilt für den ganzen Run. Wartet auf die Freigabe von `coop-inventar-konzept` durch das Team, danach die Bearbeitung auf parallel stellen. **Fertig, wenn** auf zwei PCs beide Modi nacheinander wählbar sind, jeder Run im gewählten Modus läuft, der Client den Modus des Hosts sieht und ein Client mit unpassender Mod-Version ablehnt. **Einstieg** [Details](docs/OPTIONEN.md#coop-modus-wahl) | Team | mittel |
| `coop-inventar-zwei` | P1 | Geteiltes Inventar für zwei Spieler online nach dem Konzept: ein gemeinsamer Rucksack mit einem Bereich je Spieler, samt Airdrop-Wahl, Itemsäcken, Werkbank, Lager und Rundenende. Wartet auf die Freigabe von `coop-inventar-konzept` durch das Team, danach die Bearbeitung auf zentral stellen. **Fertig, wenn** auf zwei PCs ein Run von Tag 1 bis Tag 4 im geteilten Inventar ohne Absturz und ohne Desync-Befund läuft, ein Unterstützungs-Item im Bereich von Spieler A die Waffe von Spieler B bei beiden mit gleichen Werten verstärkt und ein Run bis Tag 2 mit eigenen Inventaren wie in Release 1.0 läuft. **Einstieg** [Details](docs/OPTIONEN.md#coop-inventar-zwei) | Team | viel |
| `doku-rest-ist-zustand` | P2 | Doku unter `docs/` (ohne `archive/`, `faelle/`, `ERLEDIGT.md`, `ENTSCHEIDUNGEN.md`) und `src/Net/README.md` auf den Ist-Zustand bringen; in `docs/OFFSETS.md` die Tabellen mit RVAs alter Builds ins Archiv verschieben oder mit `tools/rva_map.py` auf `26786272` umrechnen. **Fertig, wenn** dort keine andere Build-ID als `26786272` und kein datierter Befund mehr steht (Datumsangaben nur an Nutzervorgaben) und die Grenze für `docs/OFFSETS.md` in `doc_check.py` gesunken ist. | parallel | keine |
| `game-readme-kuerzen` | P2 | `src/Game/README.md` (81 KB) aufteilen: 14 der 30 `*Patches.cpp` kommen darin nicht vor. **Fertig, wenn** die Datei unter 60 KB liegt, ihr Eintrag in `LIMITS` von `doc_check.py` entfällt und jede `*Patches.cpp` in einer README unter `src/Game` beschrieben ist. | parallel | keine |
| `ps-skripte-pruefen` | P2 | PSScriptAnalyzer für alle `*.ps1` in die CI aufnehmen, Funde beheben. **Fertig, wenn** der CI-Schritt grün ist. | parallel | keine |
| `research-archivieren` | P2 | Erledigte Ordner unter `research/spikes/` (außer `a-connect`) archivieren. **Fertig, wenn** dort nur noch Ordner liegen, auf die ein Skript unter `tools/`, die Solution oder Doku außerhalb des Archivs verweist. | parallel | keine |
| `themen-fuellen` | P2 | Die Themen-Dateien unter `docs/themen/` erklären ihr Gebiet selbst, statt auf das Archiv zu verweisen (heute rund 115 Links nach `docs/archive/`). **Fertig, wenn** keine Themen-Datei mehr auf `docs/archive/` verlinkt und `doc_check.py check` grün ist. | parallel | keine |
| `update-releasefeed` | P2 | Vertrauenswürdig prüfbare Release- und Kompatibilitätsinformationen anbieten: signierte Metadaten mit Mod-Version, unterstützten Builds, Protokollversion, Paket-Hashes und Rücknahmen; nur anbieten, nichts installieren. **Fertig, wenn** die Tests für passende und unpassende Releases, Rücknahmen, ungültige Signaturen, veraltete Antworten und Netzausfall bestehen und ungültige Antworten keine Freigabe bewirken. **Einstieg** [Details](docs/OPTIONEN.md#update-releasefeed) | zentral | keine |
| `dumper7-image-ende` | P2 | Den Fix aus `tools/dumper7/image-ende.patch` in den Fork `VitaminStack/Dumper-7` übernehmen und das Submodul darauf umstellen; der Push in den Fork braucht die Freigabe des Menschen. **Fertig, wenn** das Submodul den Fix enthält, `image-ende.patch` gelöscht ist und `Build-Dumper7.ps1` ohne Patch ein SDK für `26786272` erzeugt. **Einstieg** [Offsets](docs/OFFSETS.md#dumper-7-liest-hinter-das-image-ende-dumper7-image-ende) | parallel | wenig |
| `opt-client-takt` | P2 | Beim Client halten die Widget-Suche der Fortschritts-Anzeige (`ShowSharedProgress`, rund 12 ms) und die Objektsuche der Boss-Leiste (`BossBarAdapter::ScanObjects`, rund 10 ms) einmal pro Sekunde den Frame auf. **Fertig, wenn** im Client-Log eines ganzen Tages mit Boss keine Zeile `Takt: … hielt den Frame … auf` über 8 ms für diese beiden Abgleiche steht. | parallel | wenig |
| `werkzeuge-windows-pruefen` | P2 | `Collect-Logs.ps1` (Kurzfassung) und die Projekt-Skills auf Windows prüfen. **Fertig, wenn** `*.summary.txt` entstehen und die Skills laden. | parallel | wenig |
| `game-ordner` | P2 | `src/Game` (rund 120 Quelldateien flach) nach Fachgebiet in Unterordner teilen und `RewardPatches.cpp` (rund 3100 Zeilen) aufteilen. Kollidiert mit jedem offenen PR unter `src/Game`, darum erst starten, wenn dort keiner offen ist. **Fertig, wenn** jede Quelldatei in einem Themenordner liegt, `RewardPatches.cpp` in mehrere Dateien geteilt ist und die CI grün ist. | zentral | wenig |
| `update-autoupdater` | P2 | Optionales automatisches Mod-Update, nur nach ausdrücklicher Aktivierung; baut auf `update-releasefeed` auf und beginnt erst danach. **Fertig, wenn** ein freigegebenes Paket getrennt geladen, gegen signierte Metadaten und Hashes geprüft und nur bei beendetem Spiel transaktional installiert wird und die Tests für Abbruch, beschädigten Download, Dateisperren, Wiederanlauf und Rückkehr bestehen. **Einstieg** [Details](docs/OPTIONEN.md#update-autoupdater) | zentral | wenig |
| `update-startpruefung` | P2 | Mod- und Spiel-Kompatibilität vor dem Laden prüfen: eine gemeinsame Quelle für Mod-Version, Protokollversion, unterstützte Builds und Datei-Hashes; Starter und `version.dll`-Loader prüfen die tatsächlich gestartete Installation. **Fertig, wenn** unbekannte oder veränderte Builds die Mod nicht laden, ein sichtbarer Hinweis Mod-Version, gefundenen und erwarteten Build und Grund nennt und Positiv-, Negativ- und Direktstarttests dokumentiert sind. **Einstieg** [Details](docs/OPTIONEN.md#update-startpruefung) | zentral | wenig |
| `spike-abloesen` | P2 | Testbefehle des Spikes (`!finish`, Godmode, `!wave`, Seed) in den Testkanal der Mod übernehmen, danach `research/spikes/a-connect` löschen und `tools/spike/` in `tools/test/` umbenennen. **Fertig, wenn** alle Testskripte ohne Spike laufen. | zentral | mittel |
| `sync-aufraeumen` | P2 | Alle Patches, Hooks und Kontrollkanal-Nachrichten durchgehen: Was deckt inzwischen native Replikation oder lokale Spiellogik ab, was synchronisiert doppelt, was ist Spike-Rest? Grundlage ist der [Client-lokal-Audit](docs/CLIENT_LOCAL_AUDIT.md). **Fertig, wenn** im Audit je Patch und Nachricht eine Entscheidung steht, die überflüssigen entfernt sind und ein Routenlauf bis Tag 4 mit Desync-Melder ohne Befund bleibt. | zentral | mittel |
| `coop-spielstand` | P2 | Der Host speichert einen Coop-Run und setzt ihn später mit demselben Client fort. **Fertig, wenn** nach Speichern, Beenden beider Spiele und erneutem Laden beim Host der Client beitritt und der Run mit gleichem Tag, gleicher Route und gleichen Inventaren weiterläuft. | parallel | viel |
| `coop-inventar-geteilt` | P2 | „Seperate new shared Inventory Logic like real coop“, Ausbau auf drei und vier Spieler: eigene Bereichszuordnung je Spieler-Slot, weil die nativen Typen nur P1 und P2 kennen. Baut auf `coop-inventar-zwei` und `coop-modus-wahl` auf und wartet wie sie auf die Freigabe des Konzepts. **Fertig, wenn** mit drei und mit vier Spielern ein Run bis Tag 2 im geteilten Inventar ohne Absturz und ohne Desync-Befund läuft und Unterstützungs-Items über jede Bereichsgrenze wirken. **Einstieg** [Details](docs/OPTIONEN.md#coop-inventar-geteilt) | Team | viel |
