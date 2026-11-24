# TODO

Diese Datei ist die einzige Aufgabenliste des Projekts. **Jede Aufgabe steht genau einmal:** offen in der Tabelle ihrer Phase, erledigt in der Tabelle [Erledigt](docs/ERLEDIGT.md#erledigt). Einen zweiten Status gibt es nicht. `py tools/task_status.py check` prüft das Format, die Abhängigkeiten und den Fortschritt im README, die CI ebenso.

Wie man die Tabellen liest, Fallakten anlegt und Aufgaben abschließt, was nicht erneut gemacht werden soll und welche Dateien alle teilen, steht in [docs/PROZESS.md](docs/PROZESS.md). Risiken stehen in der [Roadmap](docs/ROADMAP.md#risiken). Den Überblick als Graph zeigt [docs/ROADMAP.md](docs/ROADMAP.md), den Fortschritt je Phase das [README](README.md#progress).

**Fokus: erst spielbar.** Vorrang hat, dass zwei Spieler einen Run durchspielen können. Aufgaben unter [Optionale Optimierungen](#optionale-optimierungen) warten, bis Phase 0 bis 5 erledigt sind; `tasks.py` zeigt sie bis dahin als `zurückgestellt`. Neue Kennungen nur für Funde oder Reste, die den Run zu zweit verhindern, siehe [Fokus in CLAUDE.md](docs/PROZESS.md#fokus).

## Wo kann ich anfangen?

Diese Liste erzeugt `py tools/task_status.py update` aus den Tabellen, oben steht das Wichtigste. Ob jemand eine Aufgabe schon reserviert hat, zeigt nur `py tools/tasks.py list`.

<!-- frei:start -->
| Kennung | Prio | Phase | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `release` | P0 | [6](#phase-6--release) | zentral | wenig |
| `airdrop-auswahl-absturz` | P1 | [5](#phase-5--vollständiger-run) | parallel | mittel |
| `lobby-rauswurf` | P1 | [5](#phase-5--vollständiger-run) | parallel | mittel |
| `wiedereintritt-karte` | P1 | [5](#phase-5--vollständiger-run) | parallel | wenig |
| `projekt-aufraeumen` | P1 | [Wartung](#wartung) | zentral | wenig |
| `coop-inventar-analyse` | P1 | [Coop](#real-coop-logic) | parallel | mittel |

**Wartet auf das Team:** `spawn-nach-kartenaufbau`, `wechsel-26786272`, `dumper7-image-ende`.

**Zurückgestellt, bis Phase 0 bis 5 erledigt sind:** 29 Aufgaben unter [Optionale Optimierungen](#optionale-optimierungen).

**Bestätigungstests ausstehend (nur Info, blockiert nichts):** 32, siehe [docs/ERLEDIGT.md](docs/ERLEDIGT.md#bestätigungstests-ausstehend).
<!-- frei:end -->

## Phase 0 – Projekt-Setup

Ziel: Alle arbeiten auf demselben Spielstand und mit derselben Toolchain. Alle Aufgaben sind erledigt.

## Phase 1 – Analyse und Spikes

Ziel: Fakten für Gate G1. Spike-Code ist Wegwerfcode und liegt unter `research/spikes/`, nicht in `src/`. Alle Aufgaben sind erledigt.

## Phase 2 – Fundament

Ziel: Stabile Basis der Mod, auf der alle parallel arbeiten können.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|

## Phase 3 – Session und Verbindung

Ziel: Zwei Spieler finden sich über Steam und verbinden sich ohne Portfreigabe.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|

## Phase 4 – Vertical Slice

Ziel: Zwei Spieler starten eine Mission, kämpfen gegen einen Gegnertyp und sehen dasselbe. Abschluss ist Gate G2.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|

## Phase 5 – Vollständiger Run

Ziel: Ein kompletter Run ist zu zweit online spielbar.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `airdrop-auswahl-absturz` | P1 | Client stürzt ab, wenn sich eine Airdrop-Auswahl ohne Inventar öffnet. **Fertig, wenn** das nicht mehr abstürzt, die Auswahl danach mit Inventar aufgebaut wird und geklärt ist, warum das Panel ohne Inventar kam (zwei Instanzen auf einem PC). Ordner `src/Game`. **Einstieg** [Fallakte](docs/faelle/airdrop-auswahl-absturz.md) | – | parallel | mittel |
| `lobby-rauswurf` | P1 | Host trennt einen gesunden Client mit Grund 1005 („nicht mehr in der Lobby“). **Fertig, wenn** die Ursache belegt ist und ein verbundener Client nicht mehr wegen einer kurz leeren Mitgliederliste getrennt wird, ohne die Sperre gegen Fremde aufzugeben. Ordner `src/Net`. **Einstieg** [Fallakte](docs/faelle/lobby-rauswurf.md) | – | parallel | mittel |
| `wiedereintritt-karte` | P1 | Client baut beim Wiederbeitritt in die laufende Mission keine Karte und fällt durch. **Fertig, wenn** er ohne Neustart derselben Mission wieder beitreten kann und die Karte des Hosts aufbaut, mit Unit-Test, belegt auf einem PC mit zwei Instanzen. Ordner `src/Game`. **Einstieg** [Fallakte](docs/faelle/wiedereintritt-karte.md) | – | parallel | wenig |
| `spawn-nach-kartenaufbau` | P1 | **Nur wenn der Live-Test die Theorie bestätigt:** Host setzt die Figuren an den Bus, bevor der Client seine Karte gebaut hat. **Fertig, wenn** der Client nach jedem Zonenstart auf dem Boden steht, auch bei langsamem Kartenaufbau, und Join, Reconnect und Travel unverändert laufen. Ordner `src/Game`. **Einstieg** [Fallakte](docs/faelle/spawn-nach-kartenaufbau.md) | – | Team | mittel |

## Phase 6 – Release

Ziel: Die Mod lässt sich installieren, ohne Spielstände oder Bestenlisten zu berühren.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `release` | P0 | Versionierung, Abschlusstest und Veröffentlichung. Ein Stand, eine Freigabe | `integration-run`, `tutorial-aus`, `online-sperren`, `telemetrie-aus`, `mod-saves`, `installer`, `virenscanner`, `lobby-live-rest` | zentral | wenig |

## Wartung

Zählt nicht zum Fortschritt.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `dumper7-image-ende` | P2 | Rest, braucht einen Menschen mit Schreibrecht im Fork `VitaminStack/Dumper-7`: Der Fix gegen das Lesen hinter das Image-Ende (`FindStringInRange`, Absturz auf `26669225`) ist belegt und wird von `Build-Dumper7.ps1` als `tools/dumper7/image-ende.patch` auf die Baukopie angewendet; Dump auf `26786272` ohne lokale Änderung bestanden (21.11.2026). Offen: denselben Commit in den Fork übernehmen, Submodul `third_party/Dumper-7` umstellen, Patch und Skriptschritt entfernen, einmal bauen. **Fertig, wenn** das Submodul den Fix enthält und `Build-Dumper7.ps1` ohne den Patch ein SDK erzeugt. **Einstieg:** [Offsets](docs/OFFSETS.md#dumper-7-liest-hinter-das-image-ende-dumper7-image-ende) | – | Team | wenig |
| `projekt-aufraeumen` | P1 | Doku, Aufgabenprozess und Spike nach [Plan](docs/superpowers/plans/2026-11-24-projekt-aufraeumen.md) aufräumen, damit Agenten mit wenig Kontext und ohne veraltete Befunde arbeiten. **Fertig, wenn** alle Aufgaben des Plans erledigt sind und der Plan archiviert ist. | – | zentral | wenig |
| `wechsel-26786272` | P1 | Folge von `spiel-update-26786272` ([E-07](docs/ENTSCHEIDUNGEN.md#e-07-nur-der-neueste-spiel-build): nur noch der neueste Spiel-Build): Sobald der Umstieg auf `main` liegt, stellt jede Person ihre Entwicklungskopie mit `Freeze-GameBuild.ps1 -Build 26786272 -Activate` um; offene Branches von vor dem Umstieg werden vorher auf `main` gezogen oder nutzen bis dahin die alte Kopie. **Fertig, wenn** alle Entwicklungskopien auf `26786272` stehen und `Start-Session.ps1` auf `main` bei allen startet. **Einstieg:** [Referenz-Build 26786272](docs/OFFSETS.md#referenz-build-26786272-spiel-update-26786272) | – | Team | keine |

## Optionale Optimierungen

Je Aufgabe steht hier nur eine Kurzfassung, die vollständige Beschreibung unter ihrer Kennung in [docs/OPTIONEN.md](docs/OPTIONEN.md).

Bekannte kleine Fehler, die die Spielbarkeit nicht verhindern. Sie warten, bis Phase 0 bis 5 erledigt sind; bis dahin zeigt `tasks.py` sie als `zurückgestellt` und `claim` lehnt sie ab. Rückt ein Punkt die Spielbarkeit doch in Gefahr, wandert er mit P0 oder P1 in die passende Phase; zieht der Mensch einen Punkt vor, wandert er nach [Wartung](#wartung). Neue Zeilen kommen hier nur auf Anweisung des Menschen dazu. Eine optionale Aufgabe, die `tests reopen` wieder öffnet, ist danach ebenfalls zurückgestellt. Zählt nicht zum Fortschritt.

Auch die zukünftige Update-Verwaltung bleibt vorerst optional: Unterstützt wird nur der neueste Spiel-Build (`26786272`). Die Aufgaben `update-startpruefung`, `update-releasefeed` und `update-autoupdater` sind keine Voraussetzung für den aktuellen Release und ändern weder die Build-Sperre noch die Teamentscheidung über neue Spielversionen. Erst lokale Prüfung, dann Update-Angebot, zuletzt optional automatische Installation.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `reconnect` | P2 | Optional, keine Voraussetzung für den Run. Wiederverbinden während eines Runs, Hinweis: `OnCoopReconnectRequiredChanged`. | `integration-run`, `protokoll`, `net-fehler` | parallel | viel |
| `opt-waffen-hover` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Beim Client hebt das Inventar beim Überfahren einer Waffe mit der Maus die Items … | – | parallel | wenig |
| `opt-haende-vorschau-items` | P2 | Hantel (`ItemData_Dumbell`), Hand des Blutes (`ItemData_HandOfBlood`) und Plasmahand (`ItemData_PlasmaHand`) senken über Standardwirkungen die Hände … | – | parallel | wenig |
| `opt-client-takt` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Rest aus `host-lagspike` (09.11.2026): Beim Client halten die Widget-Suche der … | – | parallel | wenig |
| `opt-host-bewegung` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Klären, warum der Host Remote-Figuren ohne clientautoritative Position fast nicht … | – | parallel | mittel |
| `opt-giftwolke-live` | P2 | Optional, zum Erproben, keine Voraussetzung für Phase 5 oder den Run. | – | zu zweit | mittel |
| `opt-tempo-upgrades` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. `MovementSpeed` und Beschleunigung: Upgrades kommen beim Client nicht an … | – | parallel | mittel |
| `opt-gegnerzahl` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Gegner-Zähler weichen ab (30 beim Host, 48 beim Client), vermutlich bleiben tote … | – | parallel | mittel |
| `opt-gegnerzahl-labyrinth` | P2 | Im Labyrinth-Event zählte der Host 101 sichtbare Gegner, der Client 2; während der ganzen Karte stand das Feld auf „offen Gegner“. | – | parallel | mittel |
| `opt-gegner-spawnverzug` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Unter Wellenlast (`!wave`) erscheinen einzelne neue Gegner beim Client bis rund 8 … | – | parallel | mittel |
| `opt-dash-aufprall` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Prallt der Client mit dem Dash gegen ein Objekt, hört er kein Geräusch … | – | parallel | wenig |
| `opt-ergebnis-werte` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Der Ergebnisbildschirm des Clients am Run-Ende zeigt nur Nullen, die Run-Statistik … | – | parallel | mittel |
| `opt-rundenstempel` | P2 | Vorgezogen, wartet nicht auf `integration-run`, Test auf einem PC. Die Rundenübersicht des Clients stempelt das geschaffte Level nicht ab … | – | parallel | wenig |
| `opt-client-visuell` | P2 | Feinschliff der Client-Darstellung aus `client-visuell`. **Bereits in `kampf` umgesetzt und auf einem PC geprüft:** Die Größe der getesteten … | `integration-run` | parallel | mittel |
| `opt-highway-ziel` | P2 | Im Event „Räume den Highway!“ erschien kein Benzin, das Ziel verlangte einen Abschluss (2/10). Erst ohne Mod klären, wie das Event endet. | – | parallel | wenig |
| `opt-schuss-animation` | P2 | In höheren Leveln ist die Schuss-Animation der Client-Figur beim Client kaputt (Waffen übereinander), und die Figur gleitet über den Boden. | – | parallel | mittel |
| `opt-kopierladen` | P2 | Im Event `MGP_CopyShopEvent` kopiert der Client ein Item nur lokal. | – | parallel | mittel |
| `opt-zonenziel-anzeige` | P2 | In der Zone „Töte die Titanen“ sieht der Client die Fortschrittsanzeige des Zonenziels nicht. | – | parallel | wenig |
| `opt-itemsack-flug` | P2 | Itemsäcke aus Containern werden beim Client erst direkt an der Figur sichtbar, der Ausstoß fehlt. | – | parallel | wenig |
| `opt-metabase-absturz` | P2 | Der Client stürzte rund 4 Minuten nach dem Verbinden in der MetaBase ab … | `integration-run` | parallel | mittel |
| `opt-sensenmann-ansage-client` | P2 | Seit `karte-nach-labyrinth-absturz` erzeugt nur der Host den Sensenmann, der Spawner des Clients kehrt vor `OnReaperSpawned` zurück. | – | parallel | mittel |
| `opt-routenkarte` | P2 | Der Client sieht dieselbe Routenkarte wie der Host (Knoten, Biome, Bus, Tag) und dieselbe Gefahrenstufe im HUD, wählen kann er weiter nicht. | `integration-run` | parallel | mittel |
| `opt-kameras` | P2 | Getrennte Kameras je Spieler ermöglichen: Bewegungsbegrenzung anpassen, Sichtbereiche für das Gegner-Spawning synchronisieren oder die Spawn-Logik … | `integration-run` | parallel | viel |
| `opt-trennen-absturz` | P2 | Client stürzt beim `disconnect` aus der Mission manchmal mit „uobject leaks (world leaks)“ ab (`ReferenceChainSearch.cpp` Zeile 2061), einmal in zwei … | `integration-run` | parallel | mittel |
| `opt-beitritt-absturz` | P2 | Host stürzt beim Lobby-Beitritt des Clients manchmal ab (`EXCEPTION_ACCESS_VIOLATION` an Adresse 0, Stack beginnt `5901ce8 2a8bfc4 2a8be06 2a7bb50`) … | `integration-run` | parallel | mittel |
| `sync-aufraeumen` | P2 | Nach dem ersten kompletten Run alle Patches, Hooks und Kontrollkanal-Nachrichten durchgehen: Was deckt inzwischen native Replikation oder lokale … | `integration-run` | zentral | mittel |
| `opt-karte-details` | P2 | Mit übertragener Karte stimmen Kacheln, Straßen, Gebäude und PCG-Vegetation, im Spielbild wichen aber einzelne Details ab: ein Baum in anderer Art … | `integration-run` | parallel | mittel |
| `mehr-spieler` | P2 | Mehr als 2 Spieler prüfen. Die Coop-Logik ist fest auf P1 und P2 ausgelegt, daher aufwendig. | `release` | parallel | viel |
| `coop-spielstand` | P2 | Der Host speichert einen Coop-Run und setzt ihn später mit demselben Client fort. | `integration-run`, `mod-saves` | parallel | viel |
| `update-startpruefung` | P2 | Mod-/Spiel-Kompatibilität vor dem Laden prüfen: gemeinsame Quelle für Mod-Version, Protokollversion, unterstützte Game-Builds, Kompatibilitätsprofil … | `integration-run`, `installer` | zentral | wenig |
| `update-releasefeed` | P2 | Vertrauenswürdig prüfbare Release-/Kompatibilitätsinformationen anbieten: signierte Metadaten mit Mod-Version, unterstützten … | `update-startpruefung` | zentral | keine |
| `update-autoupdater` | P2 | Später optionales automatisches Mod-Update auf dem Releasefeed aufbauen, nur nach ausdrücklicher Aktivierung. | `update-releasefeed`, `installer`, `mod-saves` | zentral | wenig |

## Real Coop logic

Je Aufgabe steht hier nur eine Kurzfassung, die vollständige Beschreibung unter ihrer Kennung in [docs/OPTIONEN.md](docs/OPTIONEN.md).

Ausbau nach Release 1.0 hin zur echten Koop-Logik des Spiels, als zweiter wählbarer Multiplayer-Modus. Release 1.0 hat den Modus **Eigene Inventare**: Jeder Spieler hat ein vollständig eigenes Inventar ([Inventar lokal bearbeiten](src/Game/README.md#inventar-lokal-bearbeiten)). Dieser Modus bleibt erhalten. Neu kommt der Modus **Geteiltes Inventar** wie im nativen Koop: Die Spieler teilen sich einen Rucksack, Jeder behält seinen eigenen Bereich, Unterstützungs-Items wirken aber über die Bereichsgrenze und verstärken auch Waffen der Mitspieler. Das erlaubt gemeinsame Builds. Der Host wählt den Modus vor dem Run, je nachdem welche Inventarverwaltung die Gruppe bevorzugt. Ziel sind bis zu vier Spieler, darum keine neue Logik fest auf P1 und P2 auslegen. Es gilt das [Architekturprinzip](docs/ARCHITECTURE.md#verbindliches-architekturprinzip-lokal-zuerst-minimal-synchronisieren): Der Host bucht das gemeinsame Inventar, Anzeige und Wirkungsberechnung laufen lokal aus bestätigten Daten. Zählt nicht zum Fortschritt.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `coop-inventar-analyse` | P1 | Das native geteilte Koop-Inventar ohne Mod untersuchen, Referenz ist lokaler Koop mit zwei Controllern auf einem PC: Aufbau des gemeinsamen Rucksacks … | `integration-run` | parallel | mittel |
| `coop-inventar-konzept` | P1 | Konzept für das geteilte Inventar online, ausgelegt auf zwei bis vier Spieler. | `coop-inventar-analyse` | zentral | keine |
| `coop-inventar-zwei` | P1 | Geteiltes Inventar für zwei Spieler online nach dem Konzept: ein gemeinsamer Rucksack mit einem Bereich je Spieler, beide sehen dasselbe Raster … | `coop-inventar-konzept`, `release` | zentral | viel |
| `coop-inventar-ablauf` | P1 | Alle Inventarabläufe auf das geteilte Inventar umstellen: Airdrop-Wahl, Itemsäcke, Werkbank, Lager mit Verkauf und Reparatur, Rundenende mit … | `coop-inventar-zwei` | zentral | viel |
| `coop-modus-wahl` | P1 | Der Host wählt vor dem Run zwischen den Modi Eigene Inventare und Geteiltes Inventar, etwa in Lobby oder MetaBase. | `coop-inventar-ablauf` | parallel | mittel |
| `coop-inventar-vier` | P2 | Geteiltes Inventar auf drei und vier Spieler erweitern. Die nativen Typen kennen nur P1 und P2, darum eigene Bereichszuordnung je Spieler-Slot nach … | `coop-inventar-ablauf`, `mehr-spieler` | zentral | viel |
