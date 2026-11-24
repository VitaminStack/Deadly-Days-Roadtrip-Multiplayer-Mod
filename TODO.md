# TODO

Die einzige Liste offener Arbeit: **Probleme** (Fehler) und **Verbesserungen**. Alle Projektphasen sind abgeschlossen ([E-13](docs/ENTSCHEIDUNGEN.md#e-13-phasen-abgeschlossen-flache-aufgabenliste)); Aufgaben hängen nicht voneinander ab. Erledigtes steht in [docs/ERLEDIGT.md](docs/ERLEDIGT.md), dort auch die [ausstehenden Bestätigungstests](docs/ERLEDIGT.md#bestätigungstests-ausstehend) (nur Info). `py tools/task_status.py check` prüft das Format.

Wie man die Tabellen liest, Fallakten anlegt und Aufgaben abschließt, was nicht erneut gemacht werden soll und welche Dateien alle teilen, steht in [docs/PROZESS.md](docs/PROZESS.md). Risiken stehen in der [Roadmap](docs/ROADMAP.md#risiken).

**Fokus:** Probleme, die den Run zu zweit stören, vor Verbesserungen; innerhalb nach Prio.

## Wo kann ich anfangen?

Diese Liste erzeugt `py tools/task_status.py update` aus den Tabellen, oben steht das Wichtigste. Ob jemand eine Aufgabe schon reserviert hat, zeigt nur `py tools/tasks.py list`.

<!-- frei:start -->
| Kennung | Prio | Phase | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `release` | P0 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `airdrop-auswahl-absturz` | P1 | [Problem](#probleme) | parallel | mittel |
| `lobby-rauswurf` | P1 | [Problem](#probleme) | parallel | mittel |
| `wiedereintritt-karte` | P1 | [Problem](#probleme) | parallel | wenig |
| `aufgaben-werkzeuge-entschlacken` | P1 | [Verbesserung](#verbesserungen) | zentral | keine |
| `coop-inventar-ablauf` | P1 | [Verbesserung](#verbesserungen) | zentral | viel |
| `coop-inventar-analyse` | P1 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `coop-inventar-konzept` | P1 | [Verbesserung](#verbesserungen) | zentral | keine |
| `coop-inventar-zwei` | P1 | [Verbesserung](#verbesserungen) | zentral | viel |
| `coop-modus-wahl` | P1 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `prozess-neu-schreiben` | P1 | [Verbesserung](#verbesserungen) | zentral | keine |
| `opt-beitritt-absturz` | P2 | [Problem](#probleme) | parallel | mittel |
| `opt-metabase-absturz` | P2 | [Problem](#probleme) | parallel | mittel |
| `opt-trennen-absturz` | P2 | [Problem](#probleme) | parallel | mittel |
| `coop-inventar-vier` | P2 | [Verbesserung](#verbesserungen) | zentral | viel |
| `coop-spielstand` | P2 | [Verbesserung](#verbesserungen) | parallel | viel |
| `game-ordner` | P2 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `game-readme-kuerzen` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `mehr-spieler` | P2 | [Verbesserung](#verbesserungen) | parallel | viel |
| `offsets-alte-rvas` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-client-takt` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-client-visuell` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-dash-aufprall` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-ergebnis-werte` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-gegner-spawnverzug` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-gegnerzahl` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-gegnerzahl-labyrinth` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-giftwolke-live` | P2 | [Verbesserung](#verbesserungen) | zu zweit | mittel |
| `opt-haende-vorschau-items` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-highway-ziel` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-host-bewegung` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-itemsack-flug` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-kameras` | P2 | [Verbesserung](#verbesserungen) | parallel | viel |
| `opt-karte-details` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-kopierladen` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-routenkarte` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-rundenstempel` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-schuss-animation` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-sensenmann-ansage-client` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-tempo-upgrades` | P2 | [Verbesserung](#verbesserungen) | parallel | mittel |
| `opt-waffen-hover` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `opt-zonenziel-anzeige` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |
| `reconnect` | P2 | [Verbesserung](#verbesserungen) | parallel | viel |
| `spike-abloesen` | P2 | [Verbesserung](#verbesserungen) | zentral | mittel |
| `sync-aufraeumen` | P2 | [Verbesserung](#verbesserungen) | zentral | mittel |
| `themen-fuellen` | P2 | [Verbesserung](#verbesserungen) | parallel | keine |
| `update-autoupdater` | P2 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `update-releasefeed` | P2 | [Verbesserung](#verbesserungen) | zentral | keine |
| `update-startpruefung` | P2 | [Verbesserung](#verbesserungen) | zentral | wenig |
| `werkzeuge-windows-pruefen` | P2 | [Verbesserung](#verbesserungen) | parallel | wenig |

**Wartet auf das Team:** `spawn-nach-kartenaufbau`, `wechsel-26786272`, `dumper7-image-ende`.

**Bestätigungstests ausstehend (nur Info, blockiert nichts):** 32, siehe [docs/ERLEDIGT.md](docs/ERLEDIGT.md#bestätigungstests-ausstehend).
<!-- frei:end -->

## Probleme

Fehler im Spiel oder in den Werkzeugen. Ein offener Fehler hat eine Fallakte unter `docs/faelle/`.

| Kennung | Prio | Aufgabe | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `airdrop-auswahl-absturz` | P1 | Client stürzt ab, wenn sich eine Airdrop-Auswahl ohne Inventar öffnet. **Fertig, wenn** das nicht mehr abstürzt, die Auswahl danach mit Inventar aufgebaut wird und geklärt ist, warum das Panel ohne Inventar kam (zwei Instanzen auf einem PC). Ordner `src/Game`. **Einstieg** [Fallakte](docs/faelle/airdrop-auswahl-absturz.md) | parallel | mittel |
| `lobby-rauswurf` | P1 | Host trennt einen gesunden Client mit Grund 1005 („nicht mehr in der Lobby“). **Fertig, wenn** die Ursache belegt ist und ein verbundener Client nicht mehr wegen einer kurz leeren Mitgliederliste getrennt wird, ohne die Sperre gegen Fremde aufzugeben. Ordner `src/Net`. **Einstieg** [Fallakte](docs/faelle/lobby-rauswurf.md) | parallel | mittel |
| `spawn-nach-kartenaufbau` | P1 | **Nur wenn der Live-Test die Theorie bestätigt:** Host setzt die Figuren an den Bus, bevor der Client seine Karte gebaut hat. **Fertig, wenn** der Client nach jedem Zonenstart auf dem Boden steht, auch bei langsamem Kartenaufbau, und Join, Reconnect und Travel unverändert laufen. Ordner `src/Game`. **Einstieg** [Fallakte](docs/faelle/spawn-nach-kartenaufbau.md) | Team | mittel |
| `wiedereintritt-karte` | P1 | Client baut beim Wiederbeitritt in die laufende Mission keine Karte und fällt durch. **Fertig, wenn** er ohne Neustart derselben Mission wieder beitreten kann und die Karte des Hosts aufbaut, mit Unit-Test, belegt auf einem PC mit zwei Instanzen. Ordner `src/Game`. **Einstieg** [Fallakte](docs/faelle/wiedereintritt-karte.md) | parallel | wenig |
| `dumper7-image-ende` | P2 | Rest, braucht einen Menschen mit Schreibrecht im Fork `VitaminStack/Dumper-7`: Der Fix gegen das Lesen hinter das Image-Ende (`FindStringInRange`, Absturz auf `26669225`) ist belegt und wird von `Build-Dumper7.ps1` als `tools/dumper7/image-ende.patch` auf die Baukopie angewendet; Dump auf `26786272` ohne lokale Änderung bestanden (21.11.2026). Offen: denselben Commit in den Fork übernehmen, Submodul `third_party/Dumper-7` umstellen, Patch und Skriptschritt entfernen, einmal bauen. **Fertig, wenn** das Submodul den Fix enthält und `Build-Dumper7.ps1` ohne den Patch ein SDK erzeugt. **Einstieg:** [Offsets](docs/OFFSETS.md#dumper-7-liest-hinter-das-image-ende-dumper7-image-ende) | Team | wenig |
| `opt-beitritt-absturz` | P2 | Host stürzt beim Lobby-Beitritt des Clients manchmal ab (`EXCEPTION_ACCESS_VIOLATION` an Adresse 0, Stack beginnt `5901ce8 2a8bfc4 2a8be06 2a7bb50`) … | parallel | mittel |
| `opt-metabase-absturz` | P2 | Der Client stürzte rund 4 Minuten nach dem Verbinden in der MetaBase ab … | parallel | mittel |
| `opt-trennen-absturz` | P2 | Client stürzt beim `disconnect` aus der Mission manchmal mit „uobject leaks (world leaks)“ ab (`ReferenceChainSearch.cpp` Zeile 2061), einmal in zwei … | parallel | mittel |

## Verbesserungen

Neue Funktionen, Feinschliff und Release. Je Aufgabe steht hier nur eine Kurzfassung, die vollständige Beschreibung unter ihrer Kennung in [docs/OPTIONEN.md](docs/OPTIONEN.md).

| Kennung | Prio | Aufgabe | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `aufgaben-werkzeuge-entschlacken` | P1 | Nach dem Merge von #292 die tote Logik in `tools/tasks.py` und `tools/task_status.py` entfernen: Phasen, „Braucht“, zurückgestellt, Merge-Plan nach Abhängigkeiten, alte Bestätigungszeilen. **Fertig, wenn** der Code nur noch die flache Liste kennt und alle Tests grün sind. | zentral | keine |
| `prozess-neu-schreiben` | P1 | `docs/PROZESS.md` (heute verschobene Altabschnitte) neu schreiben: ein Ablauf, keine Doppelungen oder Widersprüche. **Fertig, wenn** die Datei unter 6 KB liegt und `doc_check.py` diese Grenze prüft. | zentral | keine |
| `themen-fuellen` | P2 | Die Themen-Dateien unter `docs/themen/` auf einen vollständigen Ist-Zustand bringen. **Fertig, wenn** jede Themen-Datei ihr Gebiet ohne Archiv erklärt. | parallel | keine |
| `offsets-alte-rvas` | P2 | In `docs/OFFSETS.md` die Tabellen mit RVAs alter Builds auf `26786272` umrechnen (`tools/rva_map.py`) oder ins Archiv verschieben. **Fertig, wenn** die Datei nur Werte für den unterstützten Build enthält. | parallel | wenig |
| `game-readme-kuerzen` | P2 | `src/Game/README.md` aufteilen: Die Patch-Tabelle deckt nur 12 von rund 40 Dateien ab. **Fertig, wenn** die Datei unter 60 KB liegt und jede `*Patches.cpp` beschreibt. | parallel | keine |
| `game-ordner` | P2 | `src/Game` (109 Dateien flach) nach Fachgebiet in Unterordner teilen und `RewardPatches.cpp` (2400 Zeilen) aufteilen. **Fertig, wenn** Ordner den Themen entsprechen und die CI grün ist. | zentral | wenig |
| `spike-abloesen` | P2 | Testbefehle des Spikes (`!finish`, Godmode, `!wave`, Seed) in den Testkanal der Mod übernehmen, danach `research/spikes/a-connect` löschen. **Fertig, wenn** alle Testskripte ohne Spike laufen. | zentral | mittel |
| `werkzeuge-windows-pruefen` | P2 | Die Kurzfassung in `tools/spike/Collect-Logs.ps1` und die Projekt-Skills unter `.claude/skills/` auf Windows prüfen, beides ist ungetestet. **Fertig, wenn** `*.summary.txt` entstehen und die Skills laden. | parallel | wenig |
| `release` | P0 | Versionierung, Abschlusstest und Veröffentlichung. Ein Stand, eine Freigabe | zentral | wenig |
| `coop-inventar-ablauf` | P1 | Alle Inventarabläufe auf das geteilte Inventar umstellen: Airdrop-Wahl, Itemsäcke, Werkbank, Lager mit Verkauf und Reparatur, Rundenende mit … | zentral | viel |
| `coop-inventar-analyse` | P1 | Das native geteilte Koop-Inventar ohne Mod untersuchen, Referenz ist lokaler Koop mit zwei Controllern auf einem PC: Aufbau des gemeinsamen Rucksacks … | parallel | mittel |
| `coop-inventar-konzept` | P1 | Konzept für das geteilte Inventar online, ausgelegt auf zwei bis vier Spieler. | zentral | keine |
| `coop-inventar-zwei` | P1 | Geteiltes Inventar für zwei Spieler online nach dem Konzept: ein gemeinsamer Rucksack mit einem Bereich je Spieler, beide sehen dasselbe Raster … | zentral | viel |
| `coop-modus-wahl` | P1 | Der Host wählt vor dem Run zwischen den Modi Eigene Inventare und Geteiltes Inventar, etwa in Lobby oder MetaBase. | parallel | mittel |
| `wechsel-26786272` | P1 | Folge von `spiel-update-26786272` ([E-07](docs/ENTSCHEIDUNGEN.md#e-07-nur-der-neueste-spiel-build): nur noch der neueste Spiel-Build): Sobald der Umstieg auf `main` liegt, stellt jede Person ihre Entwicklungskopie mit `Freeze-GameBuild.ps1 -Build 26786272 -Activate` um; offene Branches von vor dem Umstieg werden vorher auf `main` gezogen oder nutzen bis dahin die alte Kopie. **Fertig, wenn** alle Entwicklungskopien auf `26786272` stehen und `Start-Session.ps1` auf `main` bei allen startet. **Einstieg:** [Referenz-Build 26786272](docs/OFFSETS.md#referenz-build-26786272-spiel-update-26786272) | Team | keine |
| `coop-inventar-vier` | P2 | Geteiltes Inventar auf drei und vier Spieler erweitern. Die nativen Typen kennen nur P1 und P2, darum eigene Bereichszuordnung je Spieler-Slot nach … | zentral | viel |
| `coop-spielstand` | P2 | Der Host speichert einen Coop-Run und setzt ihn später mit demselben Client fort. | parallel | viel |
| `mehr-spieler` | P2 | Mehr als 2 Spieler prüfen. Die Coop-Logik ist fest auf P1 und P2 ausgelegt, daher aufwendig. | parallel | viel |
| `opt-client-takt` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Rest aus `host-lagspike` (09.11.2026): Beim Client halten die Widget-Suche der … | parallel | wenig |
| `opt-client-visuell` | P2 | Feinschliff der Client-Darstellung aus `client-visuell`. **Bereits in `kampf` umgesetzt und auf einem PC geprüft:** Die Größe der getesteten … | parallel | mittel |
| `opt-dash-aufprall` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Prallt der Client mit dem Dash gegen ein Objekt, hört er kein Geräusch … | parallel | wenig |
| `opt-ergebnis-werte` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Der Ergebnisbildschirm des Clients am Run-Ende zeigt nur Nullen, die Run-Statistik … | parallel | mittel |
| `opt-gegner-spawnverzug` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Unter Wellenlast (`!wave`) erscheinen einzelne neue Gegner beim Client bis rund 8 … | parallel | mittel |
| `opt-gegnerzahl` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Gegner-Zähler weichen ab (30 beim Host, 48 beim Client), vermutlich bleiben tote … | parallel | mittel |
| `opt-gegnerzahl-labyrinth` | P2 | Im Labyrinth-Event zählte der Host 101 sichtbare Gegner, der Client 2; während der ganzen Karte stand das Feld auf „offen Gegner“. | parallel | mittel |
| `opt-giftwolke-live` | P2 | Optional, zum Erproben, keine Voraussetzung für Phase 5 oder den Run. | zu zweit | mittel |
| `opt-haende-vorschau-items` | P2 | Hantel (`ItemData_Dumbell`), Hand des Blutes (`ItemData_HandOfBlood`) und Plasmahand (`ItemData_PlasmaHand`) senken über Standardwirkungen die Hände … | parallel | wenig |
| `opt-highway-ziel` | P2 | Im Event „Räume den Highway!“ erschien kein Benzin, das Ziel verlangte einen Abschluss (2/10). Erst ohne Mod klären, wie das Event endet. | parallel | wenig |
| `opt-host-bewegung` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Klären, warum der Host Remote-Figuren ohne clientautoritative Position fast nicht … | parallel | mittel |
| `opt-itemsack-flug` | P2 | Itemsäcke aus Containern werden beim Client erst direkt an der Figur sichtbar, der Ausstoß fehlt. | parallel | wenig |
| `opt-kameras` | P2 | Getrennte Kameras je Spieler ermöglichen: Bewegungsbegrenzung anpassen, Sichtbereiche für das Gegner-Spawning synchronisieren oder die Spawn-Logik … | parallel | viel |
| `opt-karte-details` | P2 | Mit übertragener Karte stimmen Kacheln, Straßen, Gebäude und PCG-Vegetation, im Spielbild wichen aber einzelne Details ab: ein Baum in anderer Art … | parallel | mittel |
| `opt-kopierladen` | P2 | Im Event `MGP_CopyShopEvent` kopiert der Client ein Item nur lokal. | parallel | mittel |
| `opt-routenkarte` | P2 | Der Client sieht dieselbe Routenkarte wie der Host (Knoten, Biome, Bus, Tag) und dieselbe Gefahrenstufe im HUD, wählen kann er weiter nicht. | parallel | mittel |
| `opt-rundenstempel` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Die Rundenübersicht des Clients stempelt das geschaffte Level nicht ab … | parallel | wenig |
| `opt-schuss-animation` | P2 | In höheren Leveln ist die Schuss-Animation der Client-Figur beim Client kaputt (Waffen übereinander), und die Figur gleitet über den Boden. | parallel | mittel |
| `opt-sensenmann-ansage-client` | P2 | Seit `karte-nach-labyrinth-absturz` erzeugt nur der Host den Sensenmann, der Spawner des Clients kehrt vor `OnReaperSpawned` zurück. | parallel | mittel |
| `opt-tempo-upgrades` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. `MovementSpeed` und Beschleunigung: Upgrades kommen beim Client nicht an … | parallel | mittel |
| `opt-waffen-hover` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Beim Client hebt das Inventar beim Überfahren einer Waffe mit der Maus die Items … | parallel | wenig |
| `opt-zonenziel-anzeige` | P2 | In der Zone „Töte die Titanen“ sieht der Client die Fortschrittsanzeige des Zonenziels nicht. | parallel | wenig |
| `reconnect` | P2 | Optional, keine Voraussetzung für den Run. Wiederverbinden während eines Runs, Hinweis: `OnCoopReconnectRequiredChanged`. | parallel | viel |
| `sync-aufraeumen` | P2 | Nach dem ersten kompletten Run alle Patches, Hooks und Kontrollkanal-Nachrichten durchgehen: Was deckt inzwischen native Replikation oder lokale … | zentral | mittel |
| `update-autoupdater` | P2 | Später optionales automatisches Mod-Update auf dem Releasefeed aufbauen, nur nach ausdrücklicher Aktivierung. | zentral | wenig |
| `update-releasefeed` | P2 | Vertrauenswürdig prüfbare Release-/Kompatibilitätsinformationen anbieten: signierte Metadaten mit Mod-Version, unterstützten … | zentral | keine |
| `update-startpruefung` | P2 | Mod-/Spiel-Kompatibilität vor dem Laden prüfen: gemeinsame Quelle für Mod-Version, Protokollversion, unterstützte Game-Builds, Kompatibilitätsprofil … | zentral | wenig |
