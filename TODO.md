# TODO

Diese Datei ist die einzige Aufgabenliste des Projekts. **Jede Aufgabe steht genau einmal:** offen in der Tabelle ihrer Phase, erledigt in der Tabelle [Erledigt](#erledigt). Einen zweiten Status gibt es nicht. `py tools/task_status.py check` prüft das Format, die Abhängigkeiten und den Fortschritt im README, die CI ebenso.

Den Überblick als Graph zeigt [docs/ROADMAP.md](docs/ROADMAP.md), den Fortschritt je Phase das [README](README.md#fortschritt).

**Fokus: erst spielbar.** Vorrang hat, dass zwei Spieler einen Run durchspielen können. Kleine Fehler, die das nicht verhindern, kommen unter [Optionale Optimierungen](#optionale-optimierungen) und warten, bis der Run zu zweit läuft.

## So liest man die Tabellen

| Spalte | Bedeutung |
|---|---|
| **Kennung** | Eindeutiger Name, zugleich Branch `task/<kennung>` und Worktree `.worktrees/<kennung>` |
| **Prio** | **P0** blockiert andere Aufgaben oder den Run, **P1** nötig für das nächste Gate oder den Release, **P2** wichtig, aber später |
| **Aufgabe** | Was zu tun ist, **Fertig, wenn** nennt die Abnahme, **Einstieg** die Unterlagen |
| **Braucht** | Kennungen, die vorher erledigt sein müssen. `–` heißt keine Voraussetzung |
| **Bearbeitung** | **parallel**: läuft neben anderen Aufgaben. **zentral**: legt gemeinsame Schnittstellen fest oder lässt sich nur als Ganzes testen, immer nur eine Person, nie aufteilen. **zu zweit**: Test auf zwei PCs, zwei Personen dürfen je eine Rolle `host` oder `client` reservieren, siehe [Test auf zwei PCs](docs/TESTING.md#test-auf-zwei-pcs). **Team**: Entscheidung oder Abnahme durch die Menschen im Team, keine Reservierung |
| **Spielzeit** | Wie sehr die Aufgabe Testsitzungen braucht: keine, wenig, mittel, viel. Pro PC läuft nur eine Sitzung |

Daraus ergibt sich der Status, den `py tools/tasks.py list` anzeigt:

| Status | Bedeutung |
|---|---|
| frei | Alle Voraussetzungen erledigt, niemand hat reserviert. Darf genommen werden |
| reserviert, in Arbeit | Branch oder Draft-PR existiert. Nicht nehmen |
| blockiert | Mindestens eine Kennung aus „Braucht“ ist offen. `tasks.py claim` lehnt ab |
| Team | Entscheidet das Team, `tasks.py claim` lehnt ab |
| erledigt | Steht unter [Erledigt](#erledigt). Nie erneut reservieren, auch wenn der Branch gelöscht ist oder die GitHub CLI fehlt |

## Wo kann ich anfangen?

Diese Liste erzeugt `py tools/task_status.py update` aus den Tabellen, oben steht das Wichtigste. Ob jemand eine Aufgabe schon reserviert hat, zeigt nur `py tools/tasks.py list`.

<!-- frei:start -->
| Kennung | Prio | Phase | Bearbeitung | Spielzeit |
|---|---|---|---|---|
| `run-ende` | P1 | [5](#phase-5--vollständiger-run) | parallel | viel |
| `vfx-sound` | P2 | [5](#phase-5--vollständiger-run) | parallel | wenig |
| `opt-charakter-takt` | P2 | [Optional](#optionale-optimierungen) | parallel | wenig |
| `weekly-sperre` | P2 | [Optional](#optionale-optimierungen) | parallel | wenig |

**Wartet auf das Team:** nichts.

**Bestätigungstest ausstehend (nicht in Arbeit):** `fortschritt`, `lager-handel-client`, `lager-bestaetigen-absturz`.
<!-- frei:end -->

**Nicht erneut machen.** Das ist erledigt und dokumentiert:

- Iris oder klassisch: klassische Replikation, siehe [Spike](research/spikes/a-connect/README.md#ergebnis)
- Verbindung, Spawn, Bewegung und Schießen laufen mit den Patches, siehe [Spike](research/spikes/a-connect/README.md#patches)
- Weg vom Menü in die Mission und über die Route bis Tag 4, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf) und [Testen](docs/TESTING.md#fester-routenlauf)
- Coop-Ablauf, Karten-Erzeugung, Gegner-Pooling, Kamera, Pausen und Sync-Bedarf sind analysiert, siehe [Spielanalyse](docs/GAME_ANALYSIS.md) und [Sync-Bedarf](docs/SYNC_REQUIREMENTS.md)
- Weitere Einzelprüfungen in den Inventar-Schleifen: Nach jeder stürzt der Client an der nächsten Stelle ab
- Eine höhere `NetPriority` des `GameState` hilft nicht gegen den HUD-Hänger, `OnRep_Slots` zurückzuhalten hilft nicht beim Inventar
- Gegen fehlende PlayerStates keine Klassen im RootSet pinnen oder Read-only-Zeitstempel blind zurücksetzen. Der abgeschlossene `guidcache`-Fix erhält gezielt nachladbare Pfadidentitäten und prüft den Cache-eigenen Driver auch bei nullem `GWorld`. Eine `servertravel`-Schleife ersetzt den echten Routenlauf nicht; [Nachweise und akzeptierter Abschluss](docs/GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache). Run/Endlos bis Tag 13 mit zwei Biomwechseln ist vorerst ausreichend abgesichert; nur neue konkrete Fehler bekommen ein neues TODO.
- Pause ab Tag 2 und fehlende Client-Figur am Bus sind im Spike behoben. Bei seltsamem Laden zuerst `Run-Route.ps1` und `check_round.py` prüfen, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung)

**Gemeinsame Dateien.** Diese Dateien nutzen alle. Sie bleiben klein und ändern sich nur in kurzen eigenen PRs:

- `research/spikes/a-connect/Spike.h` und `Main.cpp`
- Öffentliche Schnittstellen in `src/Core` und `src/Net`
- Protokollversion und Nachrichtenformat, nur über `protokoll`

## Live-Sitzungen zu zweit

Manche Abnahmen brauchen zwei Menschen gleichzeitig: zwei Steam-Konten, zwei PCs, echtes Internet. Solche Termine sind knapp.
Es bietet sich an, diese Prüfungen zu sammeln und **in einer Sitzung nacheinander abzuarbeiten**, statt für jede Aufgabe einen
eigenen Termin anzusetzen. Die Liste unten ist eine Planungshilfe, keine zusätzliche Pflicht oder neue Freigaberegel.

**Bestätigungstest ausstehend** zählt nicht als „in Arbeit“ und nicht als erledigt. Solche
Aufgaben stehen separat in `tasks.py list`, bleiben aber bis zur echten Abnahme offen.
Mit `py tools/tasks.py tests plan` die verfügbaren Tests samt Abnahmen für einen Termin bündeln;
aktive fremde Aufgaben werden ausgeschlossen. `tests update <kennungen> --note "Stand"`
aktualisiert mehrere Teststände, `tests pass <kennungen> --note "Ergebnis mit Beleg"` schließt
sie erst nach bestandenem Test ab. Ohne Kennung geht `update --all` für alle verfügbaren
Bestätigungstests. Keine neue Reservierung je Test, Änderungen nur im eigenen Wartungs-Worktree
über einen PR. [Befehle und Grenzen](tools/README.md#bestätigungstests).

- **Offene Tests sammeln.** Bleibt nur noch ein Test zu zweit, kann er nach [Aufgabe abschließen](#aufgabe-abschließen), Punkt 3,
  als eigene Kennung weitergeführt und hier ergänzt werden. Bestehende Abnahmen und Abhängigkeiten bleiben dabei erhalten.
- **Gemeinsam vorbereiten.** Ein gemeinsamer Stand auf `main` und derselbe Build erleichtern den Termin. Ablauf und Aufbau stehen
  in [Testen](docs/TESTING.md#test-auf-zwei-pcs). Nach dem Verbinden lassen sich passende Punkte oft ohne Neustart kombinieren.
- **Hinweis zur Reservierung.** `host` und `client` sind nur für Aufgaben mit Bearbeitung „zu zweit“ vorgesehen. Zentrale Aufgaben
  behalten einen Verantwortlichen ohne Rolle; die zweite Person hilft beim Test. Für eine Gegenprobe erledigter Arbeit ist keine
  neue Reservierung nötig.
- **Ergebnisse zuordnen.** Ein kurzer Vermerk je Kennung hilft beim Überblick; offene Prüfungen können in einen späteren Termin.

Mögliche Punkte und Reihenfolge, je nach Bereitschaft:

1. ~~`lobby-live-rest` und `lobby-auth-live`~~: erledigt am 07.11.2026, siehe [Lobby-Abnahme](docs/GAME_ANALYSIS.md#lobby-abnahme-auf-zwei-pcs-am-07112026-lobby-auth-live-lobby-live-rest).
   Für weitere Termine verbindet `Start-Session.ps1 -Role Client -Mod -Tunnel Lobby -NoSpike` die beiden gleich.
2. `net-fehler`, nur zur Gegenprobe über das Internet: Host verlässt das Spiel, Leitung bricht ab. Umsetzung und Abnahme gehen allein.
3. `integration-g2`: zehn Minuten Vertical Slice zwischen zwei Haushalten, erst wenn alle Voraussetzungen erledigt sind.
4. `integration-run`: kompletter Run über das Internet, Phase 5.
5. `release`: Abschlusstest vor der Veröffentlichung.
6. Alle Bestätigungstests aus `py tools/tasks.py tests plan` in einem Lauf bis Tag 2. Gemeinsamer Ablauf für `fortschritt`,
   `update-welt` und `lager-handel-client`: [Plan](research/spikes/fortschritt/README.md#live-test-zu-zweit-etwa-30-minuten).
   Neue Aufgaben mit Live-Rest dort ergänzen statt einen eigenen Termin anzusetzen.

Die Entscheidung `meta-fortschritt` ist getroffen: Im gemeinsamen Run gilt der Host-Stand,
ohne dauerhafte Meta-Gutschrift beim Client, siehe [Regel](docs/SYNC_REQUIREMENTS.md#nutzervorgabe-meta-fortschritt-des-hosts-06112026).

**Ohne zweite Person.** Alles andere geht allein auf einem PC: zwei Instanzen unter einem Konto über 127.0.0.1, siehe
[Sitzung starten](docs/TESTING.md#sitzung-starten). Dazu gehören `protokoll`, `net-fehler`, `remote-spieler`, `missionsstart`,
`kamera-code`, `bewegung`, `kampf`, `gegnertyp`, `eingaben`, `restrictor`, `ohne-spike`, `latenz-test`, `desync-melder`,
`dauerlauf`, `weekly-sperre`, `mod-saves`, `installer` und die Sync-Aufgaben aus Phase 5. Dafür ist normalerweise kein gemeinsamer Termin nötig; die jeweiligen Abnahmen gelten weiterhin.

## Aufgabe abschließen

1. Ergebnis und Testgrenzen in der Doku festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](docs/TESTING.md#ergebnisse-festhalten).
2. Nur bei erfüllter Abnahme („Fertig, wenn“): die Zeile aus der Phasentabelle löschen und unter [Erledigt](#erledigt) alphabetisch einfügen, als `` | `kennung` | Phase | Ergebnis in einem Satz mit Link | ``. Die Phase ist eine Ziffer von 0 bis 6, `Wartung`, `Optional` oder `Coop` (für [Real Coop logic](#real-coop-logic)).
3. Teilergebnis: Die Zeile bleibt offen. Den Text auf den Rest kürzen oder den Rest als neue Kennung in dieselbe Phase eintragen und die alte Zeile nach „Erledigt“ verschieben. Wer eine neue Kennung anlegt, trägt sie bei allen Aufgaben, die darauf warten, unter „Braucht“ ein.
4. `py tools/task_status.py update` ausführen. Das erneuert „Wo kann ich anfangen?“ und den Fortschritt im README. Danach muss `py tools/task_status.py check` bestehen.
5. In [docs/ROADMAP.md](docs/ROADMAP.md) den Knoten umfärben, wenn es einen gibt.

Nur die eigenen Zeilen ändern, das hält Konflikte klein. Ein gemergter Teil-PR schließt keine Aufgabe ab.

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
| `fortschritt` | P0 | **Bestätigungstest ausstehend:** Bestand geprüft: Loot (itemsack-absturz, airdrop-*), XP/Level/XP-Bälle (client-visuell, xp-baelle-live), Level-up-Airdrop (bestaetigen-alle), Aufwertungen (werkbank-client) belegt. Offen nur Live-Abnahme: GAS-Status-Effekte beim Client (Giftwolke BP_Impact_Poison_Cloud_C, Item-Effekte über CharacterEffectManager) und Werteanzeige nach Upgrades; bekannte Lücke MovementSpeed in opt-tempo-upgrades. **Aufgabe und Abnahme:** Loot, XP, Level-ups, Fähigkeiten und Status-Effekte (GAS). Ordner `src/Sync/Combat` | `integration-g2`, `protokoll` | parallel | viel |
| `lager-handel-client` | P1 | **Bestätigungstest ausstehend:** Live-Test 10.11.2026 auf zwei PCs: 5 Verkäufe in einem Edit vom Host übernommen, Schrott danach gleich (80); offen Reparatur und Ausrüsten in einem übernommenen Edit; ein gemischter Edit wurde abgelehnt, siehe lager-edit-ablehnung **Aufgabe und Abnahme:** Gefunden im Live-Test am 07.11.2026 (zwei PCs, Build `26495302`): Im Lager kann der Client Items verschieben, aber nicht verkaufen und nicht reparieren, beim Host geht beides. Am Rundenende lassen sich beim Client nicht reparierte Items nicht ausrüsten. Erst klären, ob das Spiel kaputte Items auch beim Host sperrt. Verkauf und Reparatur buchen Währung und Itemzustand, darum entscheidet der Host wie beim Aufwerten (`werkbank-client`): Client handelt im nativen Panel, Host prüft und führt aus. **Fertig, wenn** der Client im Lager auf zwei PCs verkauft und repariert, Währung und Items bei beiden gleich sind und reparierte Items sich ausrüsten lassen. Ordner `src/Game`. **Einstieg:** [Live-Test 07.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-07112026-livetest-0711) | `rundenende-client` | parallel | mittel |
| `lager-edit-ablehnung` | P1 | Gefunden im Live-Test am 10.11.2026 (zwei PCs, Build `26495302`): Ein großer Lager-Edit des Clients (6 Verkäufe, 1 Reparatur, 2 Aufwertungen, 4 neue Felder) wurde vom Host abgelehnt; danach stellte der Client seinen Ausgangsstand nur unvollständig her (Revolver abweichend), angebaute Felder, Luftpumpe und die aufgewertete Schrotflinte gingen verloren, und weil das Lager ohne Bearbeitung offen blieb, liefen zwei Aufwertungen nur beim Client nativ und Verkäufe wurden verworfen. Die Rucksackgrafik stand danach neben den Feldern. Ablehnungsgrund steht nur im Host-Log. **Fertig, wenn** der Grund geklärt und behoben ist, eine Ablehnung den Ausgangsstand vollständig herstellt (Felder, Items, Grafik), nach einer Ablehnung eine neue Bearbeitung beginnt statt ungeschützter Aufwertungen, und ein gemischter Lager-Edit auf zwei PCs übernommen wird. Ordner `src/Game`, `src/Sync/Inventory`. **Einstieg:** [Live-Test 10.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-10112026-livetest-1011) | `lager-handel-client` | parallel | mittel |
| `run-ende` | P1 | Run-Ende und Rückkehr zur Meta-Basis. Ordner `src/Sync/World`, Dateien mit `tagesablauf` absprechen | `integration-g2`, `protokoll`, `niederlage`, `meta-fortschritt` | parallel | viel |
| `integration-run` | P0 | Kompletten Run zu zweit über Internet durchspielen und Fehler den Aufgaben zuordnen. **Fertig, wenn** ein Run von der `MetaBase` bis zum Endboss ohne Absturz und Desync läuft | `gegner-alle`, `fortschritt`, `werkbank-client`, `pausen-sync`, `tagesablauf`, `run-ende`, `dauerlauf` | zentral | viel |
| `vfx-sound` | P2 | Nutzerentscheidung vom 09.11.2026: erst live klären, dann gezielte Aufgaben. In einer Spielsitzung (ein PC reicht, der Mensch hört und schaut beim Client) gegenüber dem Host auflisten, welche Effekte und Sounds beim Client fehlen: Waffen, Treffer und Tod der Gegner, Boss-Angriffe, Kisten, Airdrop, Dash-Aufprall, Loot, Bus. Bekannt aus Code und Doku: Tod- und Treffereffekte kommen aus nicht replizierten Pools des Hosts ([Pooling](docs/GAME_ANALYSIS.md)), Boss-Angriffspartikel fehlen ([Gegner](src/Sync/Enemies/README.md)), es gibt keinen Sound- oder FMOD-Pfad in `src/`. **Fertig, wenn** jede belegte Lücke eine eigene Kennung mit Abnahme hat (Spielbarkeit gefährdet: Phase 5, sonst Optional, bestehende `opt-blut`, `opt-koerperteile`, `opt-dash-aufprall` ergänzen statt doppeln) | `integration-g2` | parallel | wenig |
| `lager-bestaetigen-absturz` | P1 | **Bestätigungstest ausstehend:** Fix im PR: Panels über Objekt-Array und Welt geprüft, Ein-PC-Test 5 Rundenenden ohne Absturz. Offen: Zwei-PC-Test mit drei Rundenenden, Client bestätigt im Lager **Aufgabe und Abnahme:** Gemeldet am 09.11.2026 aus einem Zwei-PC-Spiel: Der Client stürzte ab, als er in der Inventarverwaltung zum Rundenende „Bestätigen“ drückte. Im Ein-PC-Test am selben Tag nicht nachgestellt. Belege (Client-Mod-Log `DDCoop\logs\ddcoop-<PID>.log`, Absturzbericht oder Ereignis 1000) liegen auf dem anderen PC und fehlen noch. **Fertig, wenn** die Ursache aus Log oder Stapel belegt und behoben ist und ein Zwei-PC-Test drei Rundenenden mit Bestätigen beim Client ohne Absturz zeigt. Ordner `src/Game`, `src/Sync` | `integration-g2` | zu zweit | mittel |

## Phase 6 – Release

Ziel: Die Mod lässt sich installieren, ohne Spielstände oder Bestenlisten zu berühren.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `release` | P0 | Versionierung, Abschlusstest und Veröffentlichung. Ein Stand, eine Freigabe | `integration-run`, `tutorial-aus`, `online-sperren`, `telemetrie-aus`, `mod-saves`, `installer`, `virenscanner`, `lobby-live-rest` | zentral | wenig |

## Wartung

Zählt nicht zum Fortschritt.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|

## Optionale Optimierungen

Bekannte kleine Fehler, die die Spielbarkeit nicht verhindern. Sie warten auf `integration-run`. Rückt ein Punkt die Spielbarkeit doch in Gefahr, wandert er mit P0 oder P1 in die passende Phase. Zählt nicht zum Fortschritt.

Auch die zukünftige Update-Verwaltung bleibt vorerst optional: Wir erzwingen Build `26495302` und entwickeln auf diesem Stand. Die Aufgaben `update-startpruefung`, `update-releasefeed` und `update-autoupdater` sind keine Voraussetzung für den aktuellen Release und ändern weder die Build-Sperre noch die Teamentscheidung über neue Spielversionen. Erst lokale Prüfung, dann Update-Angebot, zuletzt optional automatische Installation.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `reconnect` | P2 | Nutzerentscheidung vom 09.11.2026: optional, keine Voraussetzung für den Run. Wiederverbinden während eines Runs, Hinweis: `OnCoopReconnectRequiredChanged`. Ordner `src/Net`, `src/Sync` | `integration-run`, `protokoll`, `net-fehler` | parallel | viel |
| `weekly-sperre` | P2 | Nutzerentscheidung vom 09.11.2026: optional, keine Release-Voraussetzung mehr. **Bestätigungstest ausstehend:** ddrweekly.deadlydays.com in Core::ReportHosts gesperrt, Unit-Tests bestehen. Offen: Solo-Spieltest mit Mod, Weekly-Eintrag im Hauptmenü anklicken, Menü bleibt bedienbar, Log zeigt keine Auflösung; ohne Mod unverändert. **Aufgabe und Abnahme:** Die Weekly-Challenge sperren, solange die Mod geladen ist, auch solo, wie Erfolge und Bestenlisten (Entscheidung `weeklymode`). `ddrweekly.deadlydays.com` in `Core::ReportHosts` aufnehmen und vorher prüfen, wie das Menü reagiert: Hängt oder stürzt der Weekly-Eintrag ohne Antwort, ihn sperren oder mit Hinweis versehen. **Fertig, wenn** mit Mod nichts an den Weekly-Server geht, das Menü normal bedienbar bleibt und ohne Mod alles unverändert ist. **Einstieg:** [Telemetrie und Berichte](docs/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus). Ordner `src/Core`, `src/Game` | `weeklymode`, `telemetrie-aus` | parallel | wenig |
| `opt-waffen-hover` | P2 | Beim Client hebt das Inventar beim Überfahren einer Waffe mit der Maus die Items nicht hervor, die auf die Waffe wirken; beim Host schon (Ein-PC-Test 09.11.2026, Bildvergleich). Lokal zuerst: die native Hervorhebung beim Client aus den replizierten Inventardaten anzeigen, keine neue Nachricht. **Fertig, wenn** der Client beim Überfahren dieselben Items hervorgehoben sieht wie der Host. Ordner `src/Game` | `integration-run` | parallel | wenig |
| `opt-client-takt` | P2 | Rest aus `host-lagspike` (09.11.2026): Beim Client halten die Widget-Suche der Fortschritts-Anzeige (`ShowSharedProgress`, rund 12 ms) und die Objektsuche der Boss-Leiste (`BossBarAdapter::ScanObjects`, rund 10 ms) einmal pro Sekunde den Frame auf, Log `Takt: …`. Im Spiel nicht mehr spürbar gemeldet. **Fertig, wenn** beim Client kein Sekunden-Abgleich mehr über 8 ms im Log steht | `integration-run` | parallel | wenig |
| `opt-desync-pivot` | P2 | Fehlalarm im Desync-Melder (Coop-Test 06.11.2026): Bei Items außerhalb des Rucksacks, etwa der Pistole, setzt die eigene Inventaransicht die Rasterposition lokal. Beim Host-Spieler repliziert sie zum Client, beim Client-Spieler bleibt sie beim Host auf `(0, 0)` (Client `(-19, -2)`). Spielerisch folgenlos, der Melder meldet aber dauerhaft „Abweichung Inventar“ und kann echte Inventarfehler überdecken. Position solcher Items im Inventar-Messfeld ignorieren | `integration-run` | parallel | wenig |
| `opt-host-bewegung` | P2 | Klären, warum der Host Remote-Figuren ohne clientautoritative Position fast nicht bewegt. Clientautoritative Position reicht für den Betrieb | `integration-run` | parallel | mittel |
| `opt-tempo-upgrades` | P2 | `MovementSpeed` und Beschleunigung: Upgrades kommen beim Client nicht an (Host 819 und 2866, Client 600 und 2048) | `integration-run` | parallel | mittel |
| `opt-gegnerzahl` | P2 | Gegner-Zähler weichen ab (30 beim Host, 48 beim Client), vermutlich bleiben tote Gegner als Objekte stehen | `integration-run` | parallel | mittel |
| `opt-gegner-spawnverzug` | P2 | Unter Wellenlast (`!wave`) erscheinen einzelne neue Gegner beim Client bis rund 8 s später als beim Host, am Bildrand. Ursache (Bandbreite, Priorität, Relevanz) nicht untersucht, [Befund](docs/GAME_ANALYSIS.md#ein-gegnertyp-synchron-gegnertyp) | `integration-run` | parallel | mittel |
| `opt-blut` | P2 | Blut- und Treffereffekte erscheinen nur beim Host | `integration-run` | parallel | wenig |
| `opt-schadenszahlen` | P2 | Schadenszahlen erscheinen nur beim Host (Live-Spiel 07.11.2026). `DamageNumberActor.ShowDamageText` ist lokal und läuft vermutlich nur dort, wo der Schaden entsteht. Lokal zuerst: beim Client aus der replizierten Gesundheit der Gegner anzeigen, keine neue Nachricht, Einstellung `EnableDamageNumbers` beachten. **Fertig, wenn** der Client bei eigenen Treffern und denen des Hosts Zahlen sieht, keine doppelt und keine ohne Treffer. Ordner `src/Game`. **Einstieg:** [Client-Darstellung 07.11.](docs/GAME_ANALYSIS.md#client-darstellung-im-live-spiel-am-07112026-todo-client-optik) | `integration-run` | parallel | wenig |
| `opt-koerperteile` | P2 | Körperteile sterbender Zombies erscheinen nur beim Host (Live-Spiel 07.11.2026), vermutlich wie `opt-blut` im Todesablauf beim Host. Zuerst den Auslöser finden, mit `opt-blut` zusammen lösen. **Fertig, wenn** der Client beim Tod eines Zombies dieselben Körperteile sieht wie der Host. Ordner `src/Game`. **Einstieg:** [Client-Darstellung 07.11.](docs/GAME_ANALYSIS.md#client-darstellung-im-live-spiel-am-07112026-todo-client-optik) | `integration-run` | parallel | wenig |
| `opt-lootbalken` | P2 | Der Fortschrittsbalken beim Durchsuchen von Kisten, Tonnen und Säcken fehlt beim Client (Live-Spiel 07.11.2026). `BP_EnvironmentObjectLootspot_C` hat `OnLootingUpdate` und `OnRep_IsLooting`; vermutlich treibt nur der Host den Balken. Lokal zuerst: beim Client aus `IsLooting` und der Lootdauer. **Fertig, wenn** der Client beim eigenen Durchsuchen den Balken wie der Host sieht und das Ergebnis unverändert vom Host kommt. Ordner `src/Game`. **Einstieg:** [Client-Darstellung 07.11.](docs/GAME_ANALYSIS.md#client-darstellung-im-live-spiel-am-07112026-todo-client-optik) | `integration-run` | parallel | wenig |
| `opt-dash-aufprall` | P2 | Prallt der Client mit dem Dash gegen ein Objekt, hört er kein Geräusch (Live-Spiel 07.11.2026). Seit `ausweichen-client` führt der Host den Dodge des Clients aus, vermutlich laufen Aufprall und Sound nur dort. Mit `vfx-sound` abstimmen. **Fertig, wenn** der Client beim Aufprall seines Dashes denselben Sound hört wie der Host bei seinem eigenen, ohne doppelten Sound. Ordner `src/Game`. **Einstieg:** [Client-Darstellung 07.11.](docs/GAME_ANALYSIS.md#client-darstellung-im-live-spiel-am-07112026-todo-client-optik) | `integration-run` | parallel | wenig |
| `opt-blickrichtung-client` | P2 | Die Figur des Clients schaut beim Client nur nach hinten (Live-Test 10.11.2026). Klären, ob Rotation oder Zielrichtung der eigenen Figur lokal verloren geht oder vom Host überschrieben wird (clientautoritative Position seit `bewegung`). **Fertig, wenn** die Figur des Clients bei beiden in Lauf- und Zielrichtung schaut. Ordner `src/Game`. **Einstieg:** [Live-Test 10.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-10112026-livetest-1011) | `integration-run` | parallel | wenig |
| `opt-laufanimation-client` | P2 | Die Laufanimation der Client-Figur stimmt nicht (Live-Test 10.11.2026), vermutlich zusammen mit `opt-blickrichtung-client`. **Fertig, wenn** die Client-Figur bei beiden wie die Host-Figur läuft. Ordner `src/Game`. **Einstieg:** [Live-Test 10.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-10112026-livetest-1011) | `integration-run` | parallel | wenig |
| `opt-gegner-indikatoren` | P2 | Beim Client fehlen die Warnanzeigen sprintender und spuckender Zombies (Live-Test 10.11.2026), der Client kann deren Angriffe kaum erkennen. Vermutlich laufen die Anzeigen nur dort, wo die Gegner-KI läuft. Rückt das die Spielbarkeit in Gefahr, nach Phase 5 mit P1. **Fertig, wenn** der Client dieselben Anzeigen sieht wie der Host. Ordner `src/Game`. **Einstieg:** [Live-Test 10.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-10112026-livetest-1011) | `integration-run` | parallel | wenig |
| `opt-levelup-sound` | P2 | Sound-Fehler der Level-up-Kiste im Endbildschirm beim Client (Live-Test 10.11.2026). Mit `vfx-sound` abstimmen. **Fertig, wenn** der Client den Kistensound wie der Host einmal und richtig hört. Ordner `src/Game`. **Einstieg:** [Live-Test 10.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-10112026-livetest-1011) | `integration-run` | parallel | wenig |
| `opt-schrott-anzeige` | P2 | Beim Aufsammeln zählt der Client Schrott kurz selbst zu hoch (etwa 9, 20, 24 statt 0; bis 185 statt 9), der Host korrigiert ihn jeweils nach etwa 0,3 s („Schrott: Stand des Hosts übernommen“), am Ende stimmt der Stand (Live-Test 10.11.2026). **Fertig, wenn** die Schrottanzeige des Clients beim Aufsammeln nicht mehr springt. Ordner `src/Game`. **Einstieg:** [Live-Test 10.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-10112026-livetest-1011) | `integration-run` | parallel | wenig |
| `opt-rundenstempel` | P2 | Die Rundenübersicht des Clients stempelt das geschaffte Level nicht ab (Live-Test 10.11.2026), vermutlich wie `opt-routenkarte` mangels Run-Info des Hosts. Mit `opt-routenkarte` zusammen lösen. **Fertig, wenn** der Client am Rundenende denselben Stempel sieht wie der Host. Ordner `src/Game`. **Einstieg:** [Live-Test 10.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-10112026-livetest-1011) | `integration-run` | parallel | wenig |
| `opt-client-visuell` | P2 | Feinschliff der Client-Darstellung aus `client-visuell`. **Bereits in `kampf` umgesetzt und auf einem PC geprüft:** Die Größe der getesteten `BP_Projectile_C` kommt trotz nicht repliziertem `BulletSize` über die Root-Skalierung (`RelativeScale3D`) an; neue Pool-Actors werden je Tick statt erst nach bis zu 1 s erfasst ([Nachweis](docs/GAME_ANALYSIS.md#kampf-beim-client-kampf), [PR #114](https://github.com/VitaminStack/Deadly-Days-Roadtrip-Multiplayer-Mod/pull/114)). **Offen:** Größen-Upgrades und andere Projektilklassen sind nicht geprüft; Treffer- und Partikeleffekte fehlen (siehe `opt-blut`), ebenso die XP-Ball-Randfälle Einsammeln durch den Client im Einzelfall, Kartenwechsel im Flug und verlorene Nachrichten. **Einstieg:** [Client-Darstellung](research/spikes/client-visuell/README.md) | `integration-run` | parallel | mittel |
| `opt-charakter-takt` | P2 | Rest aus `charakter-wahl` (10.11.2026): Der Sekunden-Abgleich der Charakterwahl (`TickCharacters`, volle Objektsuche nach dem `CharacterDefinitionApplier` der Remote-Figur) hielt einmal 10 bis 11 ms den Frame auf, Log `Takt: Charakterwahl …`. Außerdem ungeprüft: Reise und erneuter Beitritt mit gehaltener Wahl, Startausrüstung der gewählten Client-Figur im Run, Test auf zwei PCs. **Fertig, wenn** kein `Takt: Charakterwahl` über 8 ms im Log steht und die Wahl nach Reise und erneutem Beitritt bleibt. **Einstieg:** [Charakterwahl](docs/GAME_ANALYSIS.md#charakterwahl-in-der-metabase-charakter-wahl) | `charakter-wahl` | parallel | wenig |
| `opt-portraet` | P2 | Host zeigt an Tag 2 ein anderes Figurenporträt | `integration-run` | parallel | wenig |
| `opt-routenkarte` | P2 | Zurückgestellt aus `routenkarte-client` (09.11.2026): Der Client sieht dieselbe Routenkarte wie der Host (Knoten, Biome, Bus, Tag) und dieselbe Gefahrenstufe im HUD, wählen kann er weiter nicht. Belegt: Die Run-Info der GameInstance repliziert nicht, und `UBusPanel::OpenMapIfRequired` (`0x5AB7090`) befüllt die Karte nur bei `ROLE_Authority` des eigenen Controllers. Weg: Run-Info über den Kontrollkanal, Rollenprüfung beim Client umgehen oder `SetChoices` selbst aufrufen. **Fertig, wenn** der Client nach jedem Rundenende bis Tag 5 über den Biomwechsel nach Tag 4 dieselbe Karte und Gefahrenstufe wie der Host sieht und nicht reisen kann. **Einstieg:** [Routenkarte beim Client](docs/GAME_ANALYSIS.md#routenkarte-beim-client-routenkarte-client) | `integration-run` | parallel | mittel |
| `opt-kameras` | P2 | Getrennte Kameras je Spieler ermöglichen: Bewegungsbegrenzung anpassen, Sichtbereiche für das Gegner-Spawning synchronisieren oder die Spawn-Logik ersetzen, Gegnerverteilung prüfen. Keine Voraussetzung für die gemeinsame Kamera, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) | `integration-run` | parallel | viel |
| `opt-trennen-absturz` | P2 | Client stürzt beim `disconnect` aus der Mission manchmal mit „uobject leaks (world leaks)“ ab (`ReferenceChainSearch.cpp` Zeile 2061), einmal in zwei Versuchen nach „Verlassen“ des Clients. Der Client wollte ohnehin gehen, der Host spielt weiter. Ursache klären, etwa mit einem Build, der die Referenzkette loggt. **Einstieg:** [Im Spiel (05.11.2026)](src/Net/README.md#im-spiel-05112026) | `integration-run` | parallel | mittel |
| `opt-beitritt-absturz` | P2 | Host stürzt beim Lobby-Beitritt des Clients manchmal ab (`EXCEPTION_ACCESS_VIOLATION` an Adresse 0, Stack beginnt `5901ce8 2a8bfc4 2a8be06 2a7bb50`), einmal in zwei Starts mit `Start-Session.ps1 -Mod -Tunnel Lobby` am 05.11.2026. Ein zweiter Start lief durch. Ursache klären, etwa Stack gegen den Mod-Tunnel und das Overlay prüfen. **Einstieg:** `py tools/spike/crashes.py` | `integration-run` | parallel | mittel |
| `sync-aufraeumen` | P2 | Nach dem ersten kompletten Run alle Patches, Hooks und Kontrollkanal-Nachrichten durchgehen: Was deckt inzwischen native Replikation oder lokale Spiellogik ab, was synchronisiert doppelt, was ist Spike-Rest? Grundlage ist der [Client-lokal-Audit](docs/CLIENT_LOCAL_AUDIT.md). **Fertig, wenn** im Audit je Patch und Nachricht eine Entscheidung steht, die überflüssigen entfernt sind und ein Routenlauf bis Tag 4 mit Desync-Melder ohne Befund bleibt | `integration-run` | zentral | mittel |
| `opt-karte-details` | P2 | Mit übertragener Karte stimmen Kacheln, Straßen, Gebäude und PCG-Vegetation, im Spielbild wichen aber einzelne Details ab: ein Baum in anderer Art, ein anderes Dach, ein Pickup nur beim Client. Klären, ob zufällige Varianten einzelner Child Actors (`RandomChildActorSwapWrapper`) dahinterstecken, und sie dann aus der Position würfeln wie die PCG-Seeds. **Einstieg:** [Gleiche Karte in der Mod](docs/GAME_ANALYSIS.md#gleiche-karte-in-der-mod-missionsstart) | `integration-run` | parallel | mittel |
| `mehr-spieler` | P2 | Mehr als 2 Spieler prüfen. Die Coop-Logik ist fest auf P1 und P2 ausgelegt, daher aufwendig. Analyse, Reihenfolge und Vorbereitung: [Mehr als zwei Spieler](docs/MEHR_SPIELER.md) | `release` | parallel | viel |
| `coop-spielstand` | P2 | Der Host speichert einen Coop-Run und setzt ihn später mit demselben Client fort. **Fertig, wenn** nach Speichern, Beenden beider Spiele und erneutem Laden beim Host der Client beitritt und der Run mit gleichem Tag, gleicher Route und gleichen Inventaren weiterläuft | `integration-run`, `mod-saves` | parallel | viel |
| `update-startpruefung` | P2 | Mod-/Spiel-Kompatibilität vor dem Laden prüfen: gemeinsame Quelle für Mod-Version, Protokollversion, unterstützte Game-Builds, Kompatibilitätsprofil und Datei-Hashes; Starter prüft die tatsächlich gestartete Installation (auch eingefrorene Kopien, Exe und relevante Inhaltscontainer), nicht nur das Steam-Manifest. Bei direktem Steam-Start prüft der `version.dll`-Loader erneut vor dem Laden von `DDCoop.dll`; die interne Prüfung bleibt bestehen. **Fertig, wenn** unbekannte/veränderte Builds und widersprüchliche Paketangaben die Mod nicht laden, ein sichtbarer Hinweis Mod-Version, gefundenen/erwarteten Build und Grund nennt und Abbrechen oder ein nachweislich modfreier Start möglich sind; keine Meldung unter dem Loader-Lock. Eine ältere, weiterhin kompatible Mod bleibt erlaubt; Internet ist für die lokale Freigabe nicht nötig. Pflicht-Patch-/Adapterfehler verhindern die Coop-Freigabe, Teilinstallation darf nicht als erfolgreicher Start gelten. Positiv-, Negativ- und Direktstarttests dokumentieren. **Einstieg:** `src/Loader/Loader.cpp`, `src/Core/Version.cpp`, `src/Mod/Mod.cpp`, `src/Game/Patches.cpp`, `tools/game_build.py` | `integration-run`, `installer` | zentral | wenig |
| `update-releasefeed` | P2 | Vertrauenswürdig prüfbare Release-/Kompatibilitätsinformationen anbieten: signierte Metadaten mit Mod-Version, unterstützten Build-/Profilidentitäten, Protokollversion, Paket-Hashes und ausdrücklichen Rücknahmen kritischer Releases. Zunächst nur nach einem passenden Update suchen und es anbieten, nichts automatisch installieren. **Fertig, wenn** nur ausdrücklich freigegebene Pakete für den tatsächlich geprüften Game-Build angeboten werden, Signatur/Schema sowie veraltete oder zurückgespielte Metadaten geprüft werden und ungültige Antworten keine Freigabe bewirken; Offlinebetrieb nutzt die lokale Kompatibilitätsprüfung und kennzeichnet den Update-Status als unbekannt. Bekannte gültige Rücknahmen werden nicht durch Offlinebetrieb vergessen; eine bloß ältere kompatible Mod wird nicht gesperrt. Kein automatischer Support durch gefundene Signaturen. Tests für passende/unpassende Releases, Rücknahmen, ungültige Signaturen, veraltete Antworten und Netzausfall bestehen | `update-startpruefung` | zentral | keine |
| `update-autoupdater` | P2 | Später optionales automatisches Mod-Update auf dem Releasefeed aufbauen, nur nach ausdrücklicher Aktivierung. **Fertig, wenn** ein für den vorhandenen Game-Build freigegebenes Paket zunächst separat heruntergeladen, anhand signierter Metadaten und Hashes geprüft und ausschließlich bei beendetem Spiel transaktional installiert wird; Archive dürfen keine fremden Pfade überschreiben. Abbruch, beschädigter Download, Dateisperren und Installationsfehler lassen das alte vollständige Paket erhalten oder stellen es wieder her. Vorversion und Rückkehrmöglichkeit bleiben verfügbar, Konfiguration und Saves bleiben erhalten; Save-Kompatibilität vor Rückkehr prüfen. Keine Game-Updates, keine Änderung eingefrorener Spielkopien und kein Austausch geladener DLLs. Tests für Fehlerfälle, Wiederanlauf und Rückkehr dokumentieren | `update-releasefeed`, `installer`, `mod-saves` | zentral | wenig |

## Real Coop logic

Ausbau nach Release 1.0 hin zur echten Koop-Logik des Spiels, als zweiter wählbarer Multiplayer-Modus. Release 1.0 hat den Modus **Eigene Inventare**: Jeder Spieler hat ein vollständig eigenes Inventar ([Inventar lokal bearbeiten](src/Game/README.md#inventar-lokal-bearbeiten)). Dieser Modus bleibt erhalten. Neu kommt der Modus **Geteiltes Inventar** wie im nativen Koop: Die Spieler teilen sich einen Rucksack, Jeder behält seinen eigenen Bereich, Unterstützungs-Items wirken aber über die Bereichsgrenze und verstärken auch Waffen der Mitspieler. Das erlaubt gemeinsame Builds. Der Host wählt den Modus vor dem Run, je nachdem welche Inventarverwaltung die Gruppe bevorzugt. Ziel sind bis zu vier Spieler, darum keine neue Logik fest auf P1 und P2 auslegen. Es gilt das [Architekturprinzip](CLAUDE.md#verbindliches-architekturprinzip-lokal-zuerst-minimal-synchronisieren): Der Host bucht das gemeinsame Inventar, Anzeige und Wirkungsberechnung laufen lokal aus bestätigten Daten. Zählt nicht zum Fortschritt.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `coop-inventar-analyse` | P1 | Das native geteilte Koop-Inventar ohne Mod untersuchen, Referenz ist lokaler Koop mit zwei Controllern auf einem PC: Aufbau des gemeinsamen Rucksacks mit Bereichen je Spieler (`ECoopInventoryLocationType::P1Backpack`/`P2Backpack`, `CoopP1Field`/`CoopP2Field`, `dd.Coop.SplitBackpackView`), Regeln, nach denen Unterstützungs-Items über die Bereichsgrenze auf Waffen wirken, Zuordnung von Waffe zu Figur, Ablauf von Lager, Airdrop-Wahl (`ClaimReward`, `AwaitingClaims`) und Rundenende mit `bCoopEnabled`. Außerdem neu bewerten, warum `inventar-sync` den Weg über `CoopInventoryCoordinatorComponent` verworfen hat und warum das Lager mit Koop-Modus hing. **Fertig, wenn** ein Abschnitt in `docs/GAME_ANALYSIS.md` Datenmodell, Wirkregeln über Bereichsgrenzen, native Andockpunkte und die Grenzen der fest auf zwei Spieler ausgelegten Typen mit Belegt/Hypothese nennt, mit Bildvergleich. **Einstieg:** [Lokale Coop-Schnittstellen](docs/GAME_ANALYSIS.md#lokale-coop-schnittstellen), [Mehr als zwei Spieler](docs/MEHR_SPIELER.md) | `integration-run` | parallel | mittel |
| `coop-inventar-konzept` | P1 | Konzept für das geteilte Inventar online, ausgelegt auf zwei bis vier Spieler. Klären: Was bleibt lokal (Anzeige, Drag, Hervorhebung, Wirkungsberechnung), was geht über die Leitung, wer bucht; stabile Spieler-Slots statt fester P1/P2; Anordnung und Größe der Bereiche bei drei und vier Spielern; ob ein Spieler nur im eigenen Bereich ablegt; wie beide Modi nebeneinander bestehen (gemeinsame Teile, modusabhängige Pfade, keine doppelte Buchung) und wo der Modus gewählt und festgehalten wird; Beitritt, Ausstieg eines Spielers mitten im Run, Spielstand. Native Wege nur nutzen, wo die Analyse sie trägt. **Fertig, wenn** das Konzept in `docs/` steht, die Fragen aus dem Architekturprinzip beantwortet und das Team es freigegeben hat | `coop-inventar-analyse` | zentral | keine |
| `coop-inventar-zwei` | P1 | Geteiltes Inventar für zwei Spieler online nach dem Konzept: ein gemeinsamer Rucksack mit einem Bereich je Spieler, beide sehen dasselbe Raster, Unterstützungs-Items wirken über die Bereichsgrenze auf Waffen des Mitspielers, die Hervorhebung beim Überfahren zeigt auch diese Wirkungen (mit `opt-waffen-hover` abstimmen). Der Host bucht, der Client bearbeitet lokal und bekommt Bestätigung oder Rücknahme. **Fertig, wenn** auf zwei PCs ein Unterstützungs-Item im Bereich von Spieler A die Waffe von Spieler B nachweislich verstärkt, die Waffenwerte bei beiden gleich sind und der Desync-Melder über ein Rundenende ohne Befund bleibt. Ordner `src/Game`, `src/Sync` | `coop-inventar-konzept`, `release` | zentral | viel |
| `coop-inventar-ablauf` | P1 | Alle Inventarabläufe auf das geteilte Inventar umstellen: Airdrop-Wahl, Itemsäcke, Werkbank, Lager mit Verkauf und Reparatur, Rundenende mit Bestätigen. Der Modus Eigene Inventare bleibt unverändert nutzbar, beide Modi teilen sich vorhandene Pfade, wo das Konzept es vorsieht. **Fertig, wenn** ein Run auf zwei PCs von Tag 1 bis Tag 4 im Modus Geteiltes Inventar ohne Absturz und ohne Desync-Befund läuft und ein Run bis Tag 2 im Modus Eigene Inventare weiter wie in Release 1.0 läuft | `coop-inventar-zwei` | zentral | viel |
| `coop-modus-wahl` | P1 | Der Host wählt vor dem Run zwischen den Modi Eigene Inventare und Geteiltes Inventar, etwa in Lobby oder MetaBase. Der Modus geht beim Beitritt an alle Clients, gilt für den ganzen Run und lässt sich währenddessen nicht wechseln; Standard bleibt Eigene Inventare. Clients ohne passende Mod-Version lehnen ab statt im falschen Modus zu spielen. **Fertig, wenn** auf zwei PCs beide Modi nacheinander wählbar sind, jeder Run im gewählten Modus läuft und der Client den Modus des Hosts sieht. Mit `coop-spielstand` abstimmen: ein gespeicherter Run behält seinen Modus | `coop-inventar-ablauf` | parallel | mittel |
| `coop-inventar-vier` | P2 | Geteiltes Inventar auf drei und vier Spieler erweitern. Die nativen Typen kennen nur P1 und P2, darum eigene Bereichszuordnung je Spieler-Slot nach dem Konzept, Wirkungen über alle angrenzenden Bereiche, Anzeige aller Bereiche bei jedem Spieler. **Fertig, wenn** mit drei und mit vier Spielern ein Run bis Tag 2 mit geteiltem Inventar ohne Absturz und Desync läuft und Unterstützungs-Items über jede Bereichsgrenze wirken | `coop-inventar-ablauf`, `mehr-spieler` | zentral | viel |

## Erledigt

Alphabetisch sortiert. Details stehen in der verlinkten Doku.

| Kennung | Phase | Ergebnis |
|---|---|---|
| `airdrop-dodge-absturz` | 5 | Nach Entscheidung des Nutzers abgeschlossen: zwei PCs, Build 26495302, 5 statt 10 Airdrop-Dodges auf beiden Seiten ohne Absturz, Ausnahmefall SetupInventory nicht ausgelöst, keine Aufrufer-RVA, siehe [Live-Test 07.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-07112026-livetest-0711) |
| `airdrop-rucksack` | 5 | Wahl und Ablage des Clients kommen beim Host im Rucksack an, keine Auto-Bestätigung beim Drag, Client-Anzeige aus replizierten Pivot-Feldern; Live-Rest in `airdrop-rucksack-live`, siehe [Airdrop-Auswahl](src/Game/README.md#airdrop-auswahl) |
| `airdrop-rucksack-live` | 5 | Vom Nutzer abgenommen, kein Auto-Bestätigen und keine Geister-Items; Bestätigen-Logik lesend geprüft, siehe [Airdrop-Auswahl](src/Game/README.md#airdrop-auswahl) |
| `airdrop-warten` | 5 | Bestätigt der Host zuerst, steht das Spiel für beide bis zur Wahl des Clients (im Speicher gemessen); das nachgeholte Bestätigen wartet einen Drag ab und wird wiederholt, kein zweiter Klick, siehe [Airdrop-Auswahl](docs/GAME_ANALYSIS.md#airdrop-auswahl-für-beide-spieler-airdrop-auswahl) |
| `andockpunkte` | 1 | VERSION-Proxy, drei Live-Signaturen und ProcessEvent-Hook auf Host und Client erprobt, siehe [Andockpunkte](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885) |
| `ausruestung-felder` | 5 | Neue Ausrüstungsfelder des Clients erscheinen sofort: Der Client löst nach lokaler Feldanlage und Zurückrollen OnRep_Slots selbst aus, Host-Übernahme mit 3 neuen Feldern geprüft, Inventare gleich ([Befund](src/Game/README.md#inventar-lokal-bearbeiten)) |
| `ausweichen-client` | 5 | Der Client weicht so weit aus und bremst so ab wie der Host, Ausdauer wird verbraucht: Die Mod trägt beim Host die aus dem Ort geschätzte Laufrichtung direkt vor GA_Dash ein, der Host führt den Dodge nativ aus |
| `bestaetigen-alle` | 5 | Bestätigen beim Airdrop markiert bereit (Knopf gelb), weiter erst wenn Host und Client bereit sind, in beiden Reihenfolgen live geprüft; Rundenende als `bestaetigen-rundenende`, siehe [Alle bestätigen](src/Game/README.md#airdrop-auswahl) |
| `bestaetigen-rundenende` | 5 | Im Lager am Rundenende markiert Bestätigen nur bereit (Knopf gelb), der Host wählt den Tag erst, wenn auch der Client bestätigt hat, in beiden Reihenfolgen geprüft, siehe [Lager am Rundenende](src/Game/README.md#airdrop-auswahl) |
| `bestaetigungstests` | Wartung | Eigener Wartezustand und Batch-Befehle fuer Bestaetigungstests, aktive fremde Arbeit ausgeschlossen; siehe [Ablauf](tools/README.md#bestätigungstests) |
| `bewegung` | 4 | Dodge beim Client repariert: eigene Figur bekommt Reibung und Schwerkraft zurück, Host-Figur folgt bei unbekannter Unterlage dem replizierten Ort, siehe [Dodge beim Client](docs/GAME_ANALYSIS.md#dodge-beim-client-bewegung) |
| `blueprint-logik` | 1 | Repräsentative Spawn-, Effekt- und Eventpfade per Bytecode-Audit und Traces geklärt, siehe [Nachtrag](research/spikes/blueprint-logik/README.md#nachtrag-03112026-grenzpruefung-und-ereignistests) |
| `boss-dump` | 0 | Klassen aller sechs Bosse gezielt geladen und gedumpt, siehe [Boss-Dump](docs/GAME_ANALYSIS.md#boss-dump-boss-dump) |
| `branch-schutz` | 0 | Branch-Schutz für `main` aktiv |
| `build-id` | 0 | Geprüfte Kopie von Build `26187885` außerhalb von Steam, Start und Deploy prüfen alle Dateien, siehe [Einrichtung](docs/SETUP.md#gleiche-build-id-im-team) |
| `bus-ende` | 5 | Busabfahrt und `FinishMission` laufen nur noch beim Host, der Client folgt; mit Bus bis Tag 2 geprüft, siehe [Busabfahrt](docs/GAME_ANALYSIS.md#busabfahrt-nur-beim-host-bus-ende) |
| `charakter-wahl` | 5 | Jede Seite wählt ihre Figur nativ in der MetaBase, nur der Name der Definition geht über den Kontrollkanal und wird auf die Remote-Figur angewendet; Host bleibt beim POI-Betreten der Client-Figur beweglich, live auf einem PC bestätigt, Rest unter opt-charakter-takt ([Befund](docs/GAME_ANALYSIS.md#charakterwahl-in-der-metabase-charakter-wahl)) |
| `ci` | 2 | SDK-freie Tests und CI vorhanden |
| `ci-cache-fallback` | Wartung | Windows-CI baut bei fehlendem oder fehlerhaftem ccache automatisch mit direktem MSVC, die Cache-Parameterpfade haben PowerShell-Negativtests und Debug-/Release-CI bestanden (PR #182). |
| `ci-doku-pruefung` | Wartung | Doku-PRs lassen sich wieder mergen: Die CI läuft immer, `build-and-test` überspringt bei reinen Doku-Änderungen nur die Schritte und meldet die Pflichtprüfungen, scheitert die Erkennung, wird gebaut, siehe [CI](tests/README.md#ci) |
| `ci-tempo` | Wartung | CI-Lauf mit warmem Cache unter einer Minute je Job statt 3,5 bis 4,5 Minuten: ccache mit MultiToolTask, Skript-Tests einmal parallel, Doku-PRs ohne Build, überholte PR-Läufe brechen ab, Net-Tests 26 s → 9 s |
| `client-lokal-audit` | Wartung | Projektweite Regel lokal zuerst/minimal synchronisieren in CLAUDE.md, Architektur und Sync-Vertrag festgehalten; zehn Prüfbereiche mit Fundstellen, bestehenden Aufgaben und Abnahmen im [Audit](docs/CLIENT_LOCAL_AUDIT.md), keine Laufzeitänderung oder neue Spielabnahme |
| `client-visuell` | 4 | Client sieht XP-/Levelstand, XP-Bälle, Projektilbewegung und Airdrops wie der Host, ohne doppelte XP oder Rewards; Feinschliff in `opt-client-visuell`, siehe [Client-Darstellung](research/spikes/client-visuell/README.md) |
| `config-paks` | 1 | Config aus den Paks gelesen: `IpNetDriver`, `DefaultPlatformService=Steam`, Standard-Maps, siehe [Konfiguration](docs/GAME_ANALYSIS.md#konfiguration) |
| `controller-tausch` | 1 | Controller-Tausch nach dem Missionswechsel über `K2_OnSwapPlayerControllers` sofort in `SpawnedPlayers` abgeglichen, siehe [Spike](research/spikes/a-connect/README.md#controller-tausch-nach-dem-missionswechsel-controller-tausch) |
| `coop-ablauf` | 1 | P2 ist ein zweiter `ULocalPlayer` aus `SpawnLocalPlayer(1)`, 5 Stellen kommen ohne `LocalPlayer` nicht zurecht, siehe [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf) |
| `crash-reporter` | 1 | Mod und Spike lassen den Start von `CrashReportClient.exe` scheitern, `CrashGuard` bleibt erste Schicht, siehe [Crash-Reporter](research/spikes/a-connect/README.md#sperre-in-mod-und-spike-crash-reporter) |
| `dauerlauf` | 5 | 12 Tage mit Mod, Latenz Internet und Godmode ohne Absturz, Endboss und Endlosmodus; Speicher und NetGUID-Cache je Tag ohne stetiges Wachstum, siehe research/spikes/dauerlauf/README.md |
| `deploy` | 2 | Deploy-Skript `tools/Deploy-Mod.ps1` |
| `desync-melder` | 4 | Zustandssumme über den Kontrollkanal, Abweichungen in Mod-Log und F8, im Spiel rund 636 s ohne Fehlalarm, absichtliche Inventarabweichung auf beiden Seiten erkannt |
| `dodge-client-live` | 5 | Zwei PCs, Build 26495302: 40 Client-Dodges mit Richtung beim Host, kein Rutschen, Ausdauer wie beim Host, Figuren ohne Sprünge, vom Menschen abgenommen; Hinweis: Host setzte 8-mal hängende Dodge-Werte zurück, siehe [Live-Test 07.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-07112026-livetest-0711) |
| `doku-projektstatus` | Wartung | README (Deutsch und Englisch) und Roadmap nach dem belegten Gate G2 vom 07.11.2026 aktualisiert, historische Abschnitte gekennzeichnet und veraltete Diagrammstaende korrigiert; Doku-CI und GitHub-CI gruen (PR #181). |
| `doku-verweise` | Wartung | task_status.py check meldet Sätze, die eine erledigte Kennung noch ankündigen („kommt mit“, „folgt in“, „bis `x`“), läuft in der CI; bekannte Stellen korrigiert, Lobby-Weg auf zwei PCs ergänzt, siehe [task_status.py](tools/README.md#task_statuspy) |
| `dumper-fix` | 0 | Dumper-7-Fix liegt im eigenen Fork, eingebunden als Submodul `third_party/Dumper-7`, siehe [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) |
| `eingaben` | 4 | Lokale Geräte bleiben im Netzwerkspiel beim eigenen Spieler: keine Gerätezuordnung an P2, kein zweiter LocalPlayer; Bewegung, Dodge, Zielen und Menüs getrennt im Spiel belegt |
| `game-adapter` | 2 | Game-Thread-Takt am Spielfenster ruft `Drain` und die Adapter-Ticks; `Offsets.h` mit geschützten Lesezugriffen, beim Bauen gegen ein lokales Dumper-7-SDK geprüft; `WorldAdapter` als Muster liest Karte, Rolle und Spielerzahl. Mit Host und Client bis Tag 1 geprüft, [Muster](src/Game/README.md) |
| `gate-g1` | 1 | Am 04.11.2026 vom Nutzer verbindlich entschieden: Weg A (native UE-Replikation) als Grundlage, ergänzt durch eigenen Kontroll-/Sync-Kanal für XP, Währungen, Rewards, Pausen und UI; Weg B nur Rückfalloption. [Architekturentscheidung](docs/ARCHITECTURE.md#entscheidungen) |
| `gegner-alle` | 5 | Boss-Leiste beim Client in der Mod (BossBarAdapter), Boss-HP/Phase/Tod nativ repliziert und mit SpikeBoss und FrankensteinBoss belegt. Restliche Bosse (Ballerina, Lazy, Endboss), Join/Travel mit Boss und Boss-Effekte beim Client bei Bedarf als neue Aufgabe |
| `gegner-pooling` | 1 | Tote Gegner werden zerstört, nicht gepoolt, der `PoolManager` betrifft Projektile und Effekte, siehe [Gegner-Spawning](docs/GAME_ANALYSIS.md#gegner-spawning-und-pooling) |
| `gegnertyp` | 4 | Spawn und Tod von BP_StandardEnemy laufen nativ synchron (Tode beim Client im Median sofort, höchstens 2,9 s später), kein eigener Sync; der Desync-Melder vergleicht die Gegnerzahl jetzt standardmäßig mit 8 Bestätigungen, 7 min mit zwei Wellen ohne Alarm, [Befund](docs/GAME_ANALYSIS.md#ein-gegnertyp-synchron-gegnertyp) |
| `github-repo` | 0 | Öffentliches GitHub-Repo angelegt |
| `grundgeruest` | 2 | `Loader` als Proxy-DLL, `Core` mit Logging, Config, Hooks, Dispatcher und Versionsprüfung, mit Host und Client geprüft |
| `guidcache` | 1 | Blockaden-Autorität, NetGUID-Pfadlebensdauer und Sneak-Zielbereitschaft korrigiert; Test-Godmode für beide Spieler, Lauf bis Endlos-Tag 13 mit zwei Biomwechseln. Am 04.11.2026 vom Nutzer als abgeschlossen akzeptiert; vorerst keine weiteren Detailtests, neue konkrete Probleme erhalten ein neues TODO. [Nachweise und Testgrenzen](docs/GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache) |
| `host-lagspike` | 5 | Sekündlicher Ruckler beim Host behoben: Objektsuchen über `GObjects` gekürzt (`CachedRva`, `IsA`-Ergebnis je Klasse, `ULevel::Actors`), Zeitmessung im Takt-Log, beim Host im Ein-PC-Test kein Ruckler mehr. [Befund](docs/GAME_ANALYSIS.md#sekündlicher-ruckler-beim-host-host-lagspike) |
| `hp-leisten` | 1 | Spike-Fix: Die Lebensleisten über den Figuren blieben beim Client auf dem Startwert, weil das Spiel `OnHealthChanged` nur beim Host auslöst. Jetzt ruft der Spike die Empfänger nach `OnRep_Health` und `OnRep_MaxHealth` auf, siehe [Lebensleisten](docs/GAME_ANALYSIS.md#lebensleisten-beim-client-hp-leisten) |
| `installer` | 6 | Release-Paket mit Install.cmd/Uninstall.cmd und Anleitung docs/INSTALL.md; findet Steam-Bibliotheken, prüft Build und fremde version.dll, Deploy-Mod nutzt denselben Installer |
| `installer-builds` | 6 | Spieler-Installer nimmt alle Builds der Mod (`26187885`, `26495302`) statt nur `26187885`; ein Test hält die Liste gleich mit `Core/Version.cpp` und `Common.ps1`, siehe [Installation](docs/INSTALL.md#voraussetzungen) |
| `integration-g2` | 4 | Gate G2 bestanden: zwei PCs, Build 26495302, zehn Minuten Mission stabil, kein Absturz, nur kurze Desync-Meldungen, siehe [Bericht](docs/GAME_ANALYSIS.md#gate-g2-auf-zwei-pcs-am-07112026-integration-g2) |
| `inventar-client-abweichung` | 5 | Host-Absturz beim Zurückrollen und Ablehnung bei Reparatur+Aufwertung im Lager-Edit des Clients behoben |
| `inventar-lokal` | 5 | Client bearbeitet sein Inventar in der Itemauswahl lokal, der Host übernimmt beim Bestätigen einen Edit oder lehnt ab; zwei Airdrops, Umsortieren und Rundenende bis Tag 2 ohne Absturz, Inventare gleich ([Befund](src/Game/README.md#inventar-lokal-bearbeiten)) |
| `inventar-sync` | 5 | Durch andere Aufgaben abgedeckt, der Weg über den nativen `CoopInventoryCoordinator` ist verworfen: Rucksack über `inventar-lokal` und `ausruestung-felder`, Lager über `rundenende-client`, Reward-Queue über `airdrop-warten` und `bestaetigen-alle`, Itemsäcke über `inventar-transaktion`; die Werkbank ist `werkbank-client`, siehe [Inventar lokal bearbeiten](src/Game/README.md#inventar-lokal-bearbeiten) |
| `inventar-transaktion` | 5 | Ein Itemsack gibt jedem Spieler das Item, der Host vergibt Kopien; Client-lokales Inventar als `inventar-lokal`, siehe [Itemsack für alle](src/Game/README.md#itemsack-für-alle-spieler) |
| `inventar-ui` | 5 | Durch andere Aufgaben abgedeckt: Der Client bedient sein Inventar im nativen Panel, beim Airdrop über `inventar-lokal` und `bestaetigen-alle`, im Lager über `rundenende-client`; die Werkbank ist `werkbank-client`, siehe [Inventar lokal bearbeiten](src/Game/README.md#inventar-lokal-bearbeiten) |
| `iris` | 1 | Zur Laufzeit ist die klassische Replikation mit `ActorChannel` aktiv, nicht Iris |
| `itemsack-absturz` | 4 | Itemsack-Vergabe nur beim Host; drei natürliche Client-Aufsammlungen ohne Absturz mit exakt +1 Item beim Remote-Spieler und gleichen Inventaren geprüft. [Befund und Abnahme](research/spikes/itemsack-absturz/README.md) |
| `kamera-code` | 4 | Gemeinsame Kamera beim Client nachgewiesen: Kamera bei Host und Client auch mit Netzprofil Internet höchstens 28 Einheiten auseinander, gleicher Zoom, Bewegungsgrenze stoppt beide Figuren symmetrisch am Bildrand, siehe [Gemeinsame Kamera in der Mod](docs/GAME_ANALYSIS.md#gemeinsame-kamera-in-der-mod-kamera-code) |
| `kamera-entscheidung` | 1 | Gemeinsame Kamera (Option A) mit ursprünglichem Zoom und Bewegungsbegrenzung, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) |
| `kamera-viewtarget` | 1 | Spike-Fix: Der Client schaute nach Kartenwechseln aus der eigenen Figur, weil die Kamera am alten Controller landete. Jetzt nur der lokale Controller der aktuellen Welt, Prüfung jede Sekunde, siehe [Kamera](docs/GAME_ANALYSIS.md#viewtarget-beim-client-kamera-viewtarget) |
| `kampf` | 4 | Projektilgröße und -bewegung ab dem ersten Schuss über native Replikation, Treffer, Schaden und Tod beider Spieler mit 120 ms Ping geprüft, siehe [Kampf beim Client](docs/GAME_ANALYSIS.md#kampf-beim-client-kampf) |
| `karte-analyse` | 1 | Replizierte Kartenobjekte sind Child Actors der nicht replizierten Parzellen, siehe [Kartenobjekte](docs/GAME_ANALYSIS.md#karte-und-replizierte-kartenobjekte) |
| `karte-client` | 1 | Client erzeugt die Host-Karte ohne replizierte Duplikate (`MapGen.cpp`), siehe [Client erzeugt die Host-Karte](docs/GAME_ANALYSIS.md#client-erzeugt-die-host-karte-karte-client) |
| `karte-tunnel` | Wartung | Gleiche Karte auf zwei PCs: im Live-Test am 06.11.2026 standen beide Figuren auf dem Boden (gemeldet), die Karte kommt seit `missionsstart` über den Kontrollkanal der Mod, siehe [Host-Karte über den Tunnel](research/spikes/a-connect/README.md#host-karte-über-den-tunnel-karte-tunnel) |
| `karten-erzeugung` | 1 | Gleicher Run-Seed und gleiche Mission ergeben dieselbe Karte, siehe [Karte und Seed](docs/GAME_ANALYSIS.md#karte-und-seed) |
| `lager-absturz-client` | 5 | Zwei PCs, Build 26495302, Mod main 49e0194, nur Mod: Beitritt ohne Host-Absturz, drei Rundenenden ohne Absturz, Host wartet bei hart beendetem Client, Host beendet im Lager und Client landet im Hauptmenü, siehe [Live-Test 07.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-07112026-livetest-0711) |
| `last` | 1 | Spike D: bei rund 85 Gegnern 47 KB/s vom Host, beide Instanzen 60 fps, nur Loopback, siehe [Last](docs/GAME_ANALYSIS.md#last-spike-d-last) |
| `latenz-test` | 4 | Netzsimulation im Tunnel der Mod (`-ddcoop_netsim`, `-NetProfile Internet`), Routenlauf bis Tag 1 mit Profil sauber, [Latenz simulieren](docs/TESTING.md#latenz-simulieren) |
| `layerapps-sperre` | 6 | Die Mod sperrt auch `deadlydays.layerapps.com`, das das Spiel beim Run-Ende anfragt (`Core::ReportHosts`). Run-Ende und neuer Run laufen normal, siehe [Telemetrie und Berichte](docs/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus) |
| `livetest-befunde` | 5 | Live-Test auf zwei PCs bis Tag 2 ausgewertet, neue Aufgaben `lager-absturz-client`, `waffe-client-auswahl`, `inventar-client-abweichung`, `dodge-client-live`, siehe [Live-Test 06.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-06112026-livetest-befunde) |
| `lobby-auth` | 3 | Host nimmt P2P nur von Steam bestätigten Mitgliedern seiner Lobby an, vor AcceptConnection, ohne Platz zu belegen; 127.0.0.1 nur im Testmodus; 10 Net-Tests mit nachgebautem Steam, auf einem PC im Spiel geprüft, Test mit zwei Konten ist `lobby-auth-live`, siehe [Zugriffskontrolle](src/Net/README.md#zugriffskontrolle-lobby-auth) |
| `lobby-auth-live` | 3 | Mit zwei Konten über das Internet bestätigt: Mitglied verbindet, Konto außerhalb der Lobby bekommt 1005 ohne Platz, Host gibt den Platz nach dem Verlassen frei; neu `Start-Session.ps1 -SteamLaunch` und `-Tunnel Mod -NoSpike`, siehe [Lobby-Abnahme](docs/GAME_ANALYSIS.md#lobby-abnahme-auf-zwei-pcs-am-07112026-lobby-auth-live-lobby-live-rest) |
| `lobby-bereit` | 3 | Host schließt die Lobby im Hauptmenü oder ohne Listen-Server, Client erfährt es sofort, siehe [Lobby nur mit Server](src/Net/README.md#lobby-nur-mit-server-lobby-bereit) |
| `lobby-live` | 3 | Mit zwei Konten über das Steam-Relay geprüft: Einladung angenommen, P2P, Handshake des Kontrollkanals bestätigt, MetaBase erreicht, Host-Abgang sauber; Rest (`+connect_lobby`, Abgang des Clients) ist `lobby-live-rest`, siehe [Steam-Lobby über das Internet](docs/GAME_ANALYSIS.md#steam-lobby-über-das-internet-lobby-live) |
| `lobby-live-rest` | 3 | Mit zwei Konten auf zwei PCs bestätigt: Steam startet das geschlossene Spiel nach der Einladung mit `+connect_lobby`, der Client landet ohne Klick in der MetaBase des Hosts, nach „Verlassen“ gibt der Host den Platz frei, ohne Absturz und ohne Rest-Lobby, siehe [Lobby-Abnahme](docs/GAME_ANALYSIS.md#lobby-abnahme-auf-zwei-pcs-am-07112026-lobby-auth-live-lobby-live-rest) |
| `menue-weg` | 1 | Mission über den echten Weg durch das Spielmenü mit den Patches gestartet, siehe [Spike](research/spikes/a-connect/README.md) |
| `meta-fortschritt` | 5 | Nutzerentscheidung: Im gemeinsamen Run gilt der Meta-Fortschritt des Hosts, Client nur sitzungsweise ohne dauerhafte Meta-Gutschrift; Umsetzung bleibt offen, siehe [Regel](docs/SYNC_REQUIREMENTS.md#nutzervorgabe-meta-fortschritt-des-hosts-06112026) |
| `missionsstart` | 4 | Gleiche Karte bei Host und Client über den Kontrollkanal der Mod, die Erzeugung des Clients wartet auf die Werte des Hosts; auf einem PC mit Netzprofil Internet und ohne Spike geprüft, siehe [Gleiche Karte in der Mod](docs/GAME_ANALYSIS.md#gleiche-karte-in-der-mod-missionsstart) |
| `mitwirkende` | 0 | Freunde als Mitwirkende eingeladen |
| `mod-saves` | 6 | Mod-Spielstände in SaveGames_DDCoop getrennt (Datei-API-Umleitung, erste Kopie, ohne Umleitung kein Mod-Start); Rundenende beim Client, Wiederladen und Lauf ohne Mod belegt |
| `mod-steuerung` | Wartung | Testkanal der Mod: Named Pipe je Prozess mit -ddcoop_control, Zustand, Lobby, Konsole und Reise mit Antwort; Start-Session, Restart-Instance und Test-NetEnd ohne Spike-Befehle (27 s, 16 s, 50 s) |
| `modversion-overlay` | 6 | Fenstertitel und Statuszeile des F8-Overlays zeigen dauerhaft die geladene Mod-Version aus `Net::Protocol::ModVersion` wie im Handshake; UI-Tests prüfen Offline und Spielzustand, ein Sichttest bestätigt die Mission, siehe [PR #180](https://github.com/VitaminStack/Deadly-Days-Roadtrip-Multiplayer-Mod/pull/180) und [UI](src/UI/README.md#stand-overlay) |
| `net-fehler` | 3 | Jedes Sitzungsende (Verlassen beider Seiten, Abbruch, Steam- und Sitzungszeitlimits) erscheint mit eigenem Grund als lesbare Meldung in Overlay und Log, danach sind Erstellen und Beitreten wieder frei; im Spiel geprüft, siehe [Fehlerbehandlung](src/Net/README.md#fehlerbehandlung-net-fehler) |
| `niederlage` | 1 | Nach einer Niederlage kehren Host und Client über „Zurück zur Basis“ in die `MetaBase` zurück, und ein neuer Run startet; der Host-Absturz im Run-Ende-Handler ohne `LocalPlayer` ist im Spike behoben (`RunEnd.cpp`), siehe [Niederlage](docs/GAME_ANALYSIS.md#niederlage-niederlage) |
| `ohne-spike` | 4 | Mod läuft ohne Spike bis Mission Tag 1 und zurück zur Basis, Start-Session.ps1 -NoSpike, Liste der Spike-Eingriffe in src/Game/README.md |
| `online-sperren` | 6 | Die Mod sperrt die schreibenden Steam-Methoden für Erfolge, Statistiken und Bestenlisten (`Core::SteamStats`), lesen bleibt erlaubt, siehe [Erfolge und Bestenlisten](docs/GAME_ANALYSIS.md#erfolge-und-bestenlisten-online-sperren) |
| `opt-ausdauerbalken` | Optional | Der Client zeigt den Ausdauerbalken des Hosts wie der Host: Hooks auf OnRep_Stamina und OnRep_MaxStamina, live auf einem PC bestätigt ([Befund](docs/GAME_ANALYSIS.md#ausdauerbalken-beim-client-opt-ausdauerbalken)) |
| `opt-busbalken` | Optional | Der Client sieht den Ladebalken der Busabfahrt wie der Host: Start aus vollem Tank und allen Figuren im Busbereich abgeleitet, Balken lokal getrieben, live auf einem PC bestätigt ([Befund](docs/GAME_ANALYSIS.md#busbalken-beim-client-opt-busbalken)) |
| `opt-dev-tab` | Optional | Overlay-Tab Dev mit fünf Knöpfen (Spawn XP, Full Tank, Godmode, Spawn Zombies am Cursor, Spawn Random Item), solo lokal, im Koop über Host; Spieltest Host und Client 09.11.2026 |
| `opt-ladezeit-messung` | Wartung | check_round.py --wait trennt Prozessende von World 0 beim Laden und wartet auf alle Spieler im GameState; Run-Route.ps1 startet die Messung vor dem Klick. Routenlauf ohne Spike auf 26495302: load_seconds 1,0 s statt leer bzw. 0,0 |
| `overlay` | 2 | F8-Overlay mit Status und Debug-Anzeige, sichtbare Host-/Client-Abnahme bestanden, siehe [UI](src/UI/README.md#sichtbare-spielabnahme-03112026) |
| `overlay-fokus` | 3 | F8-Overlay bleibt beim Fokuswechsel sichtbar, Knopf „Lobby-ID kopieren“, siehe [UI](src/UI/README.md#stand-overlay) |
| `patch-regression` | Wartung | Statischer Check der Game-Patchlisten, Callbacks, Site-Arrays, Code-RVAs und Signaturtabelle mit gezielten Negativtests ergänzt; Python- und C++-CI ohne Spiel erfolgreich (PR #184). |
| `patch-signaturen` | Wartung | Byte-Signatur je Patch-Adresse, falsches Muster schaltet nur seinen Patch ab |
| `patches` | 4 | Die 25 Spielfehler-Fixes des Spikes stehen geprüft in `src/Game/*Patches.cpp`, alle oder keiner; mit der Mod lässt der Spike seine Fassungen aus. Route MetaBase bis Tag 5 und Niederlage mit Rückkehr in die MetaBase ohne Absturz, siehe [Patches](src/Game/README.md#patches) |
| `pausen` | 1 | Pausen-Logik gemessen, Host-Weltpause repliziert, Client-Wunsch nicht, siehe [Pausen-Spike](research/spikes/pausen/README.md#nachtrag-03112026-isolierte-ablaeufe) |
| `pausen-sync` | 5 | Pausemenü des Clients pausiert die Welt beim Host über 0x700, Host hält eine Zeitanfrage wie sein Pausemenü; Freigabe beim Schließen, nach 3 s ohne Meldung, bei Weltwechsel und Trennung; Airdrop, Lager, Reise unverändert, im Spiel in beide Richtungen geprüft, siehe [Pausen-Spike](research/spikes/pausen/README.md#umsetzung-pausen-sync-08112026) |
| `pcg` | 1 | Client erzeugt die PCG-Vegetation mit dem Graphen des Hosts (`Pcg.cpp`), siehe [PCG-Vegetation](docs/GAME_ANALYSIS.md#pcg-vegetation-beim-client-pcg) |
| `phasen-farbe` | Wartung | Vollständig abgeschlossene Phasen im README mit grünem Zeilenhintergrund in einer lokal erzeugten SVG; verlinkte Texttabelle bleibt aufklappbar. Generator, Konsistenzprüfung und Regressionstests halten Farben und Zahlen synchron. |
| `phasenadapter` | 1 | Client sieht Rundenübersicht, Lager und Routenkarte des Hosts und ist dort gesperrt, siehe [Rundenphasen](research/spikes/a-connect/RoundPhaseRuntime.md) |
| `powerup-duplikat` | 1 | Lokale Kopie von `BP_ChaosDestructable_PowerUp_C` beim Client unterdrückt, siehe [PowerUp-Objekte](docs/GAME_ANALYSIS.md#doppelte-powerup-objekte-powerup-duplikat) |
| `protokoll` | 3 | Handshake mit Spiel-Build, Mod-Version und Rolle sowie zuverlässiger Kontrollkanal im selben Steam-Socket wie die UE-Pakete, Nachrichten mit Run-ID, Epoche und Revision nach dem Vertrag; 24 Net-Tests, im Spiel bis Tag 1 mit XP-Anzeige darüber geprüft. Der Internet-Test mit zwei Konten gehört zu `lobby-live` bzw. `integration-g2`, siehe [Kontrollkanal](docs/CONTROL_PROTOCOL.md) |
| `reflexion-felder` | Wartung | Helfer Game::Reflection löst FProperty und UFunction zur Laufzeit auf; TutorialAdapter und TelemetryAdapter umgestellt, Laufzeitwerte gleich SDK, Spieltest mit und ohne Spike sauber |
| `release-paket-pruefung` | Wartung | Release-ZIP wird aus dem ohnehin vorhandenen Windows-Release-Build erstellt und gegen Dateiliste, sichere Pfade, x64-PE-Header und SHA256SUMS.txt geprüft; Python-Negativtests und CI bestanden (PR #183). |
| `remote-p2` | 1 | Phase-1-Frage geklärt: Host-P2 über schwache `ActivePlayers`, native Coop-Aktivierung, gezielten P2-Geräte-Guard und Join-Meldung ohne zweiten `LocalPlayer`; Laufzeit-/UI-Gegenproben zeigen, dass der lokale Lager-Handoff den geplanten Kontrollkanal braucht. Produktionsanbindung und UI-Sync bleiben eigene Aufgaben, [Nachweise und Grenzen](research/spikes/a-connect/RemoteP2.md) |
| `remote-spieler` | 4 | Host trägt den Remote-Spieler dauerhaft als P2 in die Coop-Logik ein, ohne zweiten LocalPlayer und vorerst ohne Koop-Modus, siehe [Remote-Spieler als P2](src/Game/README.md#remote-spieler-als-p2) |
| `replikation-blueprint` | 1 | 218 von 482 Blueprint-Actor-Klassen replizieren, vor allem über ihre native Basisklasse |
| `replikation-kern` | 1 | Replizierte Properties der Kernklassen erfasst, der Kern repliziert, siehe [Replikation](docs/GAME_ANALYSIS.md#replikation-zur-laufzeit) |
| `restrictor` | 4 | Null-Schutz an 0x5A17ACC und Nachtrag des GameState beim Client, im Spiel mit genulltem Feld ohne Absturz, siehe [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) |
| `rpc-vergleich` | 1 | RPC-Liste gegen den Juli-Dump: 3 RPCs entfernt, keiner hinzugekommen |
| `run-ablauf` | 1 | `TriggerStartRun`, `FinalizeRunStart`, Ladebildschirm und Etappenwechsel nachvollzogen, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf) |
| `rundenende-absturz` | 4 | Echtes Rundenende über Tank und Bus bis Tag 2 ohne Absturz geprüft, Tank zählt noch je Teilnehmer (`tank-gemeinsam`), siehe [Rundenende](docs/GAME_ANALYSIS.md#rundenende-über-tank-und-bus-rundenende-absturz) |
| `rundenende-client` | 5 | Client sieht nach dem Rundenende des Hosts Rundenübersicht und Lager selbst, ordnet dort sein Inventar, nur der Host reist, siehe [Rundenübersicht beim Client](docs/GAME_ANALYSIS.md#rundenübersicht-und-lager-beim-client-rundenende-client) |
| `sdk` | 0 | SDK für UE 5.8 erzeugt, siehe [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) |
| `sdk-mission` | 0 | SDK zusätzlich während einer Mission gedumpt, mit Blueprint-Klassen |
| `sdk-vergleich` | Wartung | tools/sdk_diff.py vergleicht zwei lokale Dumper-7-SDKs für alle von der Mod genutzten Felder, Funktionen und Klassen (SdkChecks.cpp, FindObject/Function/FieldSpec, Klasse.Feld in src und OFFSETS.md), Unit-Tests mit eigenen SDK-Ausschnitten, Aufruf unter Spiel-Updates in CONTRIBUTING.md |
| `skripte-rundenende` | Wartung | `Run-Route.ps1` und `Travel-Day.ps1` klicken am Rundenende Fortsetzen und Bestätigen bei jeder lokalen Instanz (`Complete-RoundEnd`) und warten auf die Routenkarte; Run-Route bis Tag 2 mit Exitcode 0, Travel-Day bis Tag 3 geprüft, siehe [Testen](docs/TESTING.md#mission-über-das-spielmenü) |
| `solution` | 0 | Visual-Studio-Solution mit einem Projekt je Modul, `config/Local.props` eingebunden |
| `spiel-update` | Wartung | Referenz-Build 26495302, Unterstützung für 26187885 entfernt, Coop-Route 2 Tage mit 44 Patches |
| `spike-a` | 1 | Spike A: Beide Instanzen kommen in dieselbe Welt, auch auf einem PC, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| `spike-b` | 1 | Spike B: Client stabil in der Mission, Bewegung beidseitig, Schüsse wirken beim Host, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| `spike-build-26495302` | Wartung | Spike läuft auf Build 26495302 mit Adressumrechnung je Build, siehe [Spike](docs/OFFSETS.md#spike-spike-build-26495302) |
| `spike-c` | 1 | Spike C: Mit `karte-client` und `pcg` ist die Mission beim Client gleich, `check_round.py` zählt an Tag 1 bis 4 gleich viele `MapTile`s, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung) |
| `steam-lobby` | 3 | Steam-Lobby nur für Freunde direkt über Steamworks (`Net::Session`, `src/Mod/Lobby.cpp`, Overlay-Knöpfe); auf einem PC erstellt, per Lobby-ID beigetreten und bis Tag 1 gespielt, Test mit zwei Konten ist `lobby-live`, siehe [Steam-Lobby](src/Net/README.md#steam-lobby-steam-lobby) |
| `sync-bedarf` | 1 | Sync-Weg für XP, Währungen, Rewards, Pausen und UI als Entwurf festgelegt, siehe [Sync-Bedarf](docs/SYNC_REQUIREMENTS.md) |
| `tagesablauf` | 5 | Tag, Uhr, Tank, Nacht und Missionsziele replizieren nativ, kein eigener Sync nötig; Testkanal-Anfrage day und day_state.py/Run-Route.ps1 -DayState vergleichen beide Seiten; Tag 1 über 300 s belegt, Mehrtageslauf nach Tag 1 beendet und vom Nutzer als erfolgreich abgenommen, siehe docs/GAME_ANALYSIS.md#tagesablauf-tag-uhr-tank-nacht-und-ziele-tagesablauf |
| `tank-gemeinsam` | 5 | Tank am GameState ist nativ ein Gruppenwert: Kanister von Host und Client zählen einmal und gleich, voller Tank gibt die Busflucht frei, keine Code-Änderung nötig |
| `team-aufteilung` | 0 | Keine festen Bereiche je Person und kein `CODEOWNERS`, die Arbeit teilt sich über `tools/tasks.py` |
| `telemetrie-aus` | 6 | Die Mod schaltet Balance-Telemetrie und automatische Spielstand-Berichte ab und sperrt die Namensauflösung für `codecks.io` und `pixelsplit.games` (`Game::TelemetryAdapter`, `Core::ReportHosts`), siehe [Telemetrie und Berichte](docs/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus) |
| `test-tempo` | Wartung | Sitzungsstart wartet auf Zustände statt fest zu schlafen (Lobby 28 s statt 220 s, direkt 42 s statt 100 s), `-Wait`, Einzelneustart des Clients, Netzwerkzeuge mit `Test-NetEnd.ps1` (Fälle a bis e in 58 s) und `Guided-Test.ps1` für Tests mit Menschen, siehe [Spielzeit sparen](docs/TESTING.md#spielzeit-sparen) |
| `testaufbau` | 1 | Zwei Instanzen auf einem PC unter demselben Steam-Konto, je mit eigenem `-UserDir=` |
| `testumgebung` | 1 | Tag 1 bis 4 laden sauber mit Spawn am Bus und je 2 PlayerStates, Prüfskript und fester Routenlauf, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung) |
| `tick-absturz` | 1 | Restrictor-Absturz `0x5A17ACC` nachgestellt, Laufzeitfix ist `restrictor`, siehe [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) |
| `tod-client` | 5 | Toter Client kriecht wie beim Host statt weiterzulaufen und dodgt nicht mehr; Client übernimmt das replizierte Tempo-Attribut, im Spiel mit Wiederbelebung, Niederlage und Rundenende belegt, siehe [Tod des Clients](docs/GAME_ANALYSIS.md#tod-des-clients-tod-client) |
| `token-sparen` | Wartung | `tasks.py show`/`done` und `-Brief` für die Testskripte, im Spiel geprüft, [Spielzeit sparen](docs/TESTING.md#spielzeit-sparen) |
| `transport` | 3 | Tunnel aus Spike E als Modul `src/Net/Transport` mit Tests ohne Spiel; im Spiel über den Tunnel der Mod bis Tag 1, Inventare gleich, siehe [Net](src/Net/README.md#transport-transport) |
| `treffer` | 1 | Projektile des Clients ziehen beim Host Leben ab und töten (`Hits.cpp`), siehe [Treffer](docs/GAME_ANALYSIS.md#treffer-des-clients-treffer) |
| `trennen` | 1 | Client-Absturz beim Trennen war eine Folge fehlender Inventar-Items, mit Teilfix kein Absturz, siehe [Trennen](docs/GAME_ANALYSIS.md#trennen-ins-hauptmenü-trennen) |
| `tunnel` | 1 | Der Spielverkehr läuft über `ISteamNetworkingSockets` aus der Steam-Laufzeit des Spiels, zwei Instanzen spielten so bis Tag 2 (`Tunnel.cpp`); P2P mit zwei Konten ist `tunnel-p2p`, siehe [Steam-Tunnel](research/spikes/a-connect/README.md#spike-e-steam-tunnel-tunnel) |
| `tunnel-p2p` | 1 | Zwei PCs mit zwei Steam-Konten spielten über den P2P-Tunnel bis in die Mission an Tag 1, 120 s ohne Absturz, Ping 41 ms im Median über ein Steam-Relay, nichts verworfen, siehe [Steam-Tunnel](research/spikes/a-connect/README.md#p2p-auf-zwei-pcs-tunnel-p2p) |
| `tutorial-aus` | 6 | Die Mod schaltet die Tutorial-Hinweise als Host und Client automatisch ab und prüft das jede Sekunde (`src/Game/TutorialAdapter`), siehe [Tutorial-Pause](docs/GAME_ANALYSIS.md#tutorial-pause-hält-den-run-an-testumgebung) |
| `update-basis` | Wartung | Builds 26187885 und 26495302 parallel, Adressen je Build über AddressMap und tools/rva_map.py, Teilinstallation mit -PartialPatches, siehe [Offsets](docs/OFFSETS.md#build-26495302-update-basis) |
| `update-client` | Wartung | Zwei PCs, Build 26495302, Mod main 49e0194, nur Mod: Beitritt ohne Host-Absturz, drei Rundenenden ohne Absturz, Host wartet bei hart beendetem Client, Host beendet im Lager und Client landet im Hauptmenü, siehe [Live-Test 07.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-07112026-livetest-0711) |
| `update-inventar` | Wartung | Zwei PCs, Build 26495302: alle Listen gesetzt, Airdrop, Belohnungswahl und Lager (Ordnen) beim Client ohne Absturz; Verkaufen und Reparieren beim Client fehlen, neue Aufgabe lager-handel-client, siehe [Live-Test 07.11.](docs/GAME_ANALYSIS.md#live-test-auf-zwei-pcs-am-07112026-livetest-0711) |
| `update-regel` | 0 | Spiel-Updates: einfrieren und nur bei Bedarf nachziehen, siehe [Update-Regel](CONTRIBUTING.md#spiel-updates) |
| `update-welt` | Wartung | Live-Test 09.11.2026, ein PC, Build 26495302, -Mod -Tunnel Lobby -NoSpike, 44 Patches: XP-Baelle beim Client sichtbar (Log 'XP-Baelle: 1 Host-Ausstoss(e) angezeigt', 5x, leicht verzoegert), Client-Dash am Kamerarand bleibt in der gemeinsamen Kamera, kein Absturz. |
| `versions-meldung` | Wartung | Abgelehnte erste Nachricht nennt den Grund mit beiden Werten (alte Mod, Protokoll, Build, Mod-Version, Rolle), siehe [Handshake](docs/CONTROL_PROTOCOL.md#handshake) |
| `virenscanner` | 6 | Defender 1.463.207.0 meldet das Paket nicht; Umgang mit Fehlalarmen und Proton (nicht unterstützt) in INSTALL.md und LIESMICH.txt |
| `waffe-client-auswahl` | 5 | Client-Waffen wirken sofort: Entfernstil wie das Panel, Host frischt Wirkungen nach dem Edit auf, Client zeigt Waffenslots lokal vor |
| `weeklymode` | 5 | Team-Entscheidung vom 05.11.2026: Die Weekly-Challenge wird nicht unterstützt und ist gesperrt, solange die Mod geladen ist, auch solo, wie Erfolge und Bestenlisten. Umsetzung ist `weekly-sperre` |
| `werkbank-client` | 5 | Client wertet im nativen Panel auf (Ablegen auf gleiches Item, Werkbank), der Edit meldet die Aufwertungen, der Host prüft (Ablegen oder Rezept) und ruft UpgradeItems selbst auf. Seltenheiten repliziert die Mod für das Clientinventar (0x308), Mod-Version 7. Im Lager Tag 1 und 2 geprüft, nach der Reise Inventare gleich. [Befund](src/Game/README.md#werkbank-und-aufwerten-beim-client) |
| `xp-baelle` | 5 | Client-Sichtgrenzen der XP-Bälle aus der lokalen Kamera statt aus dem fehlenden GameMode, Live-Gegenprobe in `xp-baelle-live`, siehe [XP-Bälle](docs/GAME_ANALYSIS.md#xp-bälle-beim-client-xp-baelle) |
| `xp-baelle-live` | 5 | Im Spiel gegengeprüft, ohne Code-Änderung: Sichtgrenzen beim Client wie beim Host, Bälle am Bildrand auf beiden Seiten gleich, beim Aufsammeln auf beiden weg, XP und Level gleich, nichts doppelt. [Befund](docs/GAME_ANALYSIS.md#xp-bälle-beim-client-xp-baelle) |
| `zwei-pc-test` | Wartung | Tests auf zwei PCs: `Test-TwoPc.ps1` fährt je PC eine Rolle automatisch bis in die Mission und schreibt einen Tunnel-Bericht, `Start-Session.ps1 -Role`, Aufgaben „zu zweit“ mit Rollen-Reservierung in `tasks.py`, siehe [Test auf zwei PCs](docs/TESTING.md#test-auf-zwei-pcs) |

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
