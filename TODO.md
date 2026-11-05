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
| `lobby-live` | P0 | [3](#phase-3--session-und-verbindung) | zu zweit | wenig |
| `itemsack-absturz` | P0 | [4](#phase-4--vertical-slice) | parallel | wenig |
| `ohne-spike` | P0 | [4](#phase-4--vertical-slice) | zentral | mittel |
| `remote-spieler` | P0 | [4](#phase-4--vertical-slice) | zentral | mittel |
| `airdrop-auswahl` | P0 | [5](#phase-5--vollständiger-run) | parallel | mittel |
| `mod-saves` | P0 | [6](#phase-6--release) | parallel | mittel |
| `desync-melder` | P1 | [4](#phase-4--vertical-slice) | parallel | mittel |
| `latenz-test` | P1 | [4](#phase-4--vertical-slice) | parallel | wenig |
| `restrictor` | P1 | [4](#phase-4--vertical-slice) | parallel | wenig |
| `xp-baelle` | P1 | [5](#phase-5--vollständiger-run) | parallel | mittel |
| `installer` | P1 | [6](#phase-6--release) | parallel | keine |
| `weekly-sperre` | P1 | [6](#phase-6--release) | parallel | wenig |
| `test-tempo` | P1 | [Wartung](#wartung) | parallel | wenig |
| `doku-verweise` | P2 | [Wartung](#wartung) | parallel | keine |

**Wartet auf das Team:** `meta-fortschritt`, `spiel-update`.
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

- **Offene Tests sammeln.** Bleibt nur noch ein Test zu zweit, kann er nach [Aufgabe abschließen](#aufgabe-abschließen), Punkt 3,
  als eigene Kennung weitergeführt und hier ergänzt werden. Bestehende Abnahmen und Abhängigkeiten bleiben dabei erhalten.
- **Gemeinsam vorbereiten.** Ein gemeinsamer Stand auf `main` und derselbe Build erleichtern den Termin. Ablauf und Aufbau stehen
  in [Testen](docs/TESTING.md#test-auf-zwei-pcs). Nach dem Verbinden lassen sich passende Punkte oft ohne Neustart kombinieren.
- **Hinweis zur Reservierung.** `host` und `client` sind nur für Aufgaben mit Bearbeitung „zu zweit“ vorgesehen. Zentrale Aufgaben
  behalten einen Verantwortlichen ohne Rolle; die zweite Person hilft beim Test. Für eine Gegenprobe erledigter Arbeit ist keine
  neue Reservierung nötig.
- **Ergebnisse zuordnen.** Ein kurzer Vermerk je Kennung hilft beim Überblick; offene Prüfungen können in einen späteren Termin.

Mögliche Punkte und Reihenfolge, je nach Bereitschaft:

1. `lobby-live`: Einladung über das Steam-Overlay, Beitritt per P2P, Start über `+connect_lobby`, Verlassen. Verbindet die
   beiden gleich für die folgenden Punkte. Möglich, sobald `steam-lobby` auf `main` ist.
2. `net-fehler`, nur zur Gegenprobe über das Internet: Host verlässt das Spiel, Leitung bricht ab. Umsetzung und Abnahme gehen allein.
3. `integration-g2`: zehn Minuten Vertical Slice zwischen zwei Haushalten, erst wenn alle Voraussetzungen erledigt sind.
4. `integration-run`: kompletter Run über das Internet, Phase 5.
5. `release`: Abschlusstest vor der Veröffentlichung.

Die Team-Entscheidung `meta-fortschritt` braucht kein Spiel, passt aber in denselben Termin.

**Ohne zweite Person.** Alles andere geht allein auf einem PC: zwei Instanzen unter einem Konto über 127.0.0.1, siehe
[Sitzung starten](docs/TESTING.md#sitzung-starten). Dazu gehören `protokoll`, `net-fehler`, `remote-spieler`, `missionsstart`,
`kamera-code`, `bewegung`, `kampf`, `gegnertyp`, `eingaben`, `restrictor`, `ohne-spike`, `latenz-test`, `desync-melder`,
`dauerlauf`, `weekly-sperre`, `mod-saves`, `installer` und die Sync-Aufgaben aus Phase 5. Dafür ist normalerweise kein gemeinsamer Termin nötig; die jeweiligen Abnahmen gelten weiterhin.

## Aufgabe abschließen

1. Ergebnis und Testgrenzen in der Doku festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](docs/TESTING.md#ergebnisse-festhalten).
2. Nur bei erfüllter Abnahme („Fertig, wenn“): die Zeile aus der Phasentabelle löschen und unter [Erledigt](#erledigt) alphabetisch einfügen, als `` | `kennung` | Phase | Ergebnis in einem Satz mit Link | ``. Die Phase ist eine Ziffer von 0 bis 6, `Wartung` oder `Optional`.
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
| `lobby-live` | P0 | Steam-Lobby der Mod mit zwei Konten auf zwei PCs prüfen: Einladung über das Steam-Overlay annehmen, Beitritt per P2P, Start über `+connect_lobby` bei geschlossenem Spiel, Verlassen beider Seiten. **Fertig, wenn** der Client nur über die Einladung bis in die MetaBase kommt und der Handshake des [Kontrollkanals](docs/CONTROL_PROTOCOL.md) dabei über das Internet bestätigt ist. **Einstieg:** [Steam-Lobby](src/Net/README.md#steam-lobby-steam-lobby), [Test auf zwei PCs](docs/TESTING.md#test-auf-zwei-pcs) | `steam-lobby` | zu zweit | wenig |

## Phase 4 – Vertical Slice

Ziel: Zwei Spieler starten eine Mission, kämpfen gegen einen Gegnertyp und sehen dasselbe. Abschluss ist Gate G2.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `remote-spieler` | P0 | Remote-Spieler als P2 in die Coop-Logik einbinden, obwohl er kein lokaler Spieler ist. Grundlage für jeden Spieler-Sync | `patches`, `remote-p2` | zentral | mittel |
| `missionsstart` | P0 | Gemeinsamer Missionsstart, gleiche Karte bei beiden. Ordner `src/Sync/World` | `remote-spieler` | parallel | viel |
| `kamera-code` | P0 | Gemeinsame Kamera (Option A) auch beim Client umsetzen, ursprünglichen Zoom und Bewegungsbegrenzung erhalten. Ordner `src/Sync/World`, `src/Game`. **Einstieg:** [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) | `remote-spieler` | parallel | mittel |
| `bewegung` | P0 | Bewegung, Dash und Blickrichtung. Ordner `src/Sync/Players`, Dateien mit `eingaben` absprechen | `remote-spieler` | parallel | mittel |
| `kampf` | P0 | Waffen, Projektile, Treffer, Schaden, Tod. Ordner `src/Sync/Combat` | `remote-spieler` | parallel | mittel |
| `gegnertyp` | P0 | Ein Gegnertyp inklusive Spawn und Tod. Ordner `src/Sync/Enemies` | `remote-spieler` | parallel | mittel |
| `eingaben` | P1 | Eingaben aus dem lokalen `CoopInputRouter` sauber vom Remote-Spieler trennen. Ordner `src/Sync/Players`, Dateien mit `bewegung` absprechen | `remote-spieler` | parallel | mittel |
| `restrictor` | P1 | Beim Client `MissionGameState` des `InViewportMovementRestrictor` nachtragen, wenn der GameState erst nach dessen `BeginPlay` ankommt, sonst Absturz bei `0x5A17ACC`. **Einstieg:** [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) | `patches` | parallel | wenig |
| `itemsack-absturz` | P0 | Client stürzt beim Aufsammeln eines Itemsacks ab, gefunden in `client-visuell`. Ursache finden und beheben. **Fertig, wenn** der Client im Online-Koop mehrere Itemsäcke ohne Absturz aufsammelt und das Item beim richtigen Spieler landet. **Einstieg:** [Client-Darstellung](research/spikes/client-visuell/README.md) | `patches` | parallel | wenig |
| `ohne-spike` | P0 | Die Mod ohne Spike lauffähig machen. Heute laufen fast alle Tests mit Spike und Mod zusammen, im Release gibt es keinen Spike. Ohne seine Patches stürzen etwa beim Lobby-Beitritt Host (`0x5901CE8`) und Client (`0x56B16D9`) ab, siehe [Steam-Lobby prüfen](docs/TESTING.md#steam-lobby-der-mod-prüfen). Alle Spike-Eingriffe auflisten, die für den Betrieb nötig sind (nicht Messung und Testhilfen), fehlende nach `src/Game` übernehmen und `Start-Session.ps1` einen Schalter für „Mod ohne Spike“ geben, Crash-Schutz bleibt. **Fertig, wenn** Lobby-Beitritt, MetaBase und Mission an Tag 1 mit der Mod ohne `spike.dll` auf einem PC ohne Absturz laufen und die Liste der nur im Spike verbliebenen Eingriffe in [Game](src/Game/README.md) steht. Ordner `src/Game`, `tools/spike` | `patches`, `steam-lobby` | zentral | mittel |
| `latenz-test` | P1 | Latenz und Paketverlust in Tests auf einem PC simulieren. Bisher lief fast alles über Loopback mit 0 ms, über das Internet sind 40 bis 150 ms und Verluste normal. Zuerst prüfen, ob Unreals `Net PktLag`/`PktLoss` im Shipping-Build verfügbar ist, sonst Verzögerung, Schwankung und Verlust im Transport der Mod einbauen (Startschalter, nur für Tests). `Start-Session.ps1` und `Run-Route.ps1` bekommen ein Profil, etwa 120 ms, ±30 ms, 2 % Verlust. **Fertig, wenn** ein Routenlauf bis Tag 1 mit Profil läuft, die gemessenen Werte im Mod-Log stehen und [Testen](docs/TESTING.md) beschreibt, wann Abnahmen das Profil nutzen. Ordner `src/Net`, `tools/spike` | `transport` | parallel | wenig |
| `desync-melder` | P1 | Abweichungen zwischen Host und Client früh sichtbar machen. Host und Client tauschen alle paar Sekunden über den Kontrollkanal eine kleine Zustandssumme aus (Tag, Phase, Spielerzahl, HP, Gegnerzahl, Inventar-Hash), Abweichungen kommen ins Mod-Log und ins F8-Overlay. Bekannte Abweichungen aus [Optionale Optimierungen](#optionale-optimierungen) lassen sich einzeln ausnehmen. **Fertig, wenn** ein absichtlich erzeugter Unterschied (etwa ein Item nur beim Host) gemeldet wird und gleiche Zustände zehn Minuten ohne Fehlalarm bleiben. Ordner `src/Sync`, `src/UI`, Nachricht über `protokoll` anmelden | `protokoll` | parallel | mittel |
| `integration-g2` | P0 | Gate G2: Vertical Slice zusammenführen und zehn Minuten über echtes Internet zwischen zwei Haushalten spielen. Zwei Menschen nötig. **Fertig, wenn** die Runde stabil, ohne Desync und spielbar ist. Sonst Ursachen beheben, bevor weitere Inhalte synchronisiert werden | `missionsstart`, `kamera-code`, `bewegung`, `kampf`, `gegnertyp`, `eingaben`, `restrictor`, `steam-lobby`, `lobby-live`, `transport`, `net-fehler`, `ohne-spike`, `latenz-test`, `desync-melder` | zentral | viel |

## Phase 5 – Vollständiger Run

Ziel: Ein kompletter Run ist zu zweit online spielbar.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `gegner-alle` | P0 | Alle Gegner, Bosse und Boss-Phasen. Ordner `src/Sync/Enemies` | `integration-g2` | parallel | viel |
| `fortschritt` | P0 | Loot, XP, Level-ups, Fähigkeiten und Status-Effekte (GAS). Ordner `src/Sync/Combat` | `integration-g2`, `protokoll` | parallel | viel |
| `xp-baelle` | P1 | Der Client sieht die XP-Bälle nicht. Sie sind lokale Niagara-Partikel des `AExpEmissionHandler`, nur der Host erzeugt sie. Host meldet `RequestExpSpawning` (Menge, Ort), Client zeigt sie nur an, `ReceiveExp` beim Client gesperrt. **Fertig, wenn** der Client dieselben Bälle sieht, sie beim Aufsammeln verschwinden und XP nicht doppelt zählen. Ordner `src/Sync/Combat`. **Einstieg:** [XP-Bälle](docs/GAME_ANALYSIS.md#xp-bälle-beim-client-xp-baelle) | `protokoll` | parallel | mittel |
| `airdrop-auswahl` | P0 | Airdrop-Belohnungen für beide Spieler mit möglichst wenig Aufwand, damit ein Run spielbar wird. Das native lokale Koop-System nutzen statt eines eigenen Inventar-Syncs: Host trägt den Remote-Client als P2 ein (Weg aus `remote-p2`, P2-Bindung mit `remote-spieler` absprechen), dann gibt es pro Kiste eine Koop-Sitzung mit einem Claim je Spieler und Pause bis zum letzten Claim. Host-Panel auf die eigene Hälfte begrenzen. Der Client bekommt aus dem Host-Sitzungsbild eine einfache Auswahl der Choices und meldet nur seine Wahl über den Kontrollkanal; der Host ruft `ClaimReward(1, …)` auf und legt das Item automatisch in den P2-Rucksack, ersatzweise ins Lager. Eigene Rasterplatzierung beim Client und Reroll erst später unter `inventar-ui`. **Fertig, wenn** bei einem von Host oder Client eingesammelten Airdrop beide ihre Auswahl sehen, beide bestätigen müssen, das Spiel bis dahin für beide steht, danach für beide weiterläuft und jeder genau seine Wahl erhält. Ordner `src/Sync/Inventory`. **Einstieg:** [Airdrop-Auswahl](docs/GAME_ANALYSIS.md#airdrop-auswahl-für-beide-spieler-airdrop-auswahl) | `protokoll` | parallel | mittel |
| `inventar-sync` | P0 | Rucksack, Stash, Werkbank, Reward-Queue (`CoopInventoryCoordinator`). Ordner `src/Sync/Inventory` | `integration-g2`, `protokoll`, `guidcache` | parallel | viel |
| `inventar-ui` | P0 | Inventar-UI beim Remote-Spieler. Ordner `src/Sync/Inventory`, `src/UI` | `inventar-sync` | parallel | mittel |
| `pausen-sync` | P0 | Pausen und blockierende UI beider Spieler über den Kontrollkanal abstimmen. Ordner `src/Sync/Players`. **Einstieg:** [Pausen-Spike](research/spikes/pausen/README.md) | `integration-g2`, `protokoll` | parallel | mittel |
| `tagesablauf` | P0 | Tageszyklus, Treibstoff, Missionsziele, Fahrt zwischen den Etappen. Ordner `src/Sync/World`, Dateien mit `run-ende` absprechen | `integration-g2`, `protokoll`, `guidcache`, `phasenadapter` | parallel | viel |
| `meta-fortschritt` | P1 | Entscheiden, wessen Meta-Fortschritt zählt und was der Client bekommt | – | Team | keine |
| `run-ende` | P1 | Run-Ende und Rückkehr zur Meta-Basis. Ordner `src/Sync/World`, Dateien mit `tagesablauf` absprechen | `integration-g2`, `protokoll`, `niederlage`, `meta-fortschritt` | parallel | viel |
| `dauerlauf` | P1 | Automatischer Dauerlauf mit der Mod auf einem PC: `Run-Route.ps1` über viele Tage mit Latenzprofil, je Tag Speicher, Objektzahl und Größe des NetGUID-Caches beider Instanzen protokollieren. Ersetzt nicht den Spike-Lauf aus `guidcache`, sondern prüft den Sync-Code der Mod über die Laufzeit. **Fertig, wenn** ein Lauf über mindestens 13 Tage ohne Absturz läuft oder jeder Absturz eine eigene Kennung hat und das Wachstum je Tag dokumentiert ist. Ordner `tools/spike` | `integration-g2`, `latenz-test` | parallel | viel |
| `integration-run` | P0 | Kompletten Run zu zweit über Internet durchspielen und Fehler den Aufgaben zuordnen. **Fertig, wenn** ein Run von der `MetaBase` bis zum Endboss ohne Absturz und Desync läuft | `gegner-alle`, `fortschritt`, `inventar-sync`, `inventar-ui`, `pausen-sync`, `tagesablauf`, `run-ende`, `dauerlauf` | zentral | viel |
| `vfx-sound` | P2 | VFX und Sound (FMOD) beim Client. Ordner `src/Sync/Combat` | `integration-g2` | parallel | mittel |
| `reconnect` | P2 | Wiederverbinden während eines Runs, Hinweis: `OnCoopReconnectRequiredChanged`. Ordner `src/Net`, `src/Sync` | `integration-g2`, `protokoll`, `net-fehler` | parallel | viel |

## Phase 6 – Release

Ziel: Die Mod lässt sich installieren, ohne Spielstände oder Bestenlisten zu berühren.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `mod-saves` | P0 | Mod-Spielstände vom normalen Spielstand trennen, damit kein Save beschädigt wird. Ordner `src/Game` | `game-adapter` | parallel | mittel |
| `installer` | P1 | Installer oder einfache Installationsanleitung. Ordner `tools`, `docs` | `game-adapter` | parallel | keine |
| `weekly-sperre` | P1 | Die Weekly-Challenge sperren, solange die Mod geladen ist, auch solo, wie Erfolge und Bestenlisten (Entscheidung `weeklymode`). `ddrweekly.deadlydays.com` in `Core::ReportHosts` aufnehmen und vorher prüfen, wie das Menü reagiert: Hängt oder stürzt der Weekly-Eintrag ohne Antwort, ihn sperren oder mit Hinweis versehen. **Fertig, wenn** mit Mod nichts an den Weekly-Server geht, das Menü normal bedienbar bleibt und ohne Mod alles unverändert ist. **Einstieg:** [Telemetrie und Berichte](docs/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus). Ordner `src/Core`, `src/Game` | `weeklymode`, `telemetrie-aus` | parallel | wenig |
| `virenscanner` | P1 | Das Release-Paket gegen Windows Defender und weitere Scanner prüfen: Proxy-DLL und Hooks sehen für Scanner verdächtig aus. Fehlalarme bei den Herstellern melden, Hinweise in die Installationsanleitung. Steam Deck und Proton prüfen (`WINEDLLOVERRIDES="version=n,b" %command%`) oder als nicht unterstützt kennzeichnen. Nur das eigene Paket hochladen, nie Spieldateien. **Fertig, wenn** Defender das Paket nicht meldet oder der Umgang mit Fehlalarmen dokumentiert ist und der Stand zu Proton in der Anleitung steht. Ordner `docs`, `tools` | `installer` | parallel | keine |
| `release` | P0 | Versionierung, Abschlusstest und Veröffentlichung. Ein Stand, eine Freigabe | `integration-run`, `tutorial-aus`, `online-sperren`, `telemetrie-aus`, `mod-saves`, `installer`, `weekly-sperre`, `virenscanner` | zentral | wenig |

## Wartung

Zählt nicht zum Fortschritt.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `doku-verweise` | P2 | Veraltete Querverweise verhindern: `task_status.py check` meldet Sätze wie „folgt mit“, „kommt mit“ oder „bis `<kennung>`“, deren Kennung schon erledigt ist. Dabei die bekannten Stellen korrigieren: Overlay-Absatz in [Testen](docs/TESTING.md#mod-overlay-prüfen) (Lobby-Knöpfe), Phase-3-Satz im [README](README.md), „Das kommt mit `steam-lobby`“ in [Net](src/Net/README.md), Welle 2 in der [Roadmap](docs/ROADMAP.md), und im Abschnitt [Test auf zwei PCs](docs/TESTING.md#test-auf-zwei-pcs) den Weg über `-Tunnel Lobby` ergänzen. **Fertig, wenn** die Prüfung in der CI läuft und auf `main` nichts meldet | – | parallel | keine |
| `test-tempo` | P1 | Spielzeit sparen: `Start-Session.ps1` wartet auf Zustände statt fest zu schlafen und überspringt unnötige Reisen, wartet mit `-Wait` auf eine fremde Sitzung statt abzubrechen, startet mit `Restart-Instance.ps1` nur eine Instanz neu. Netzwerkzeuge für Overlay und Sitzungsende (`Overlay.ps1`, `Watch-Net.ps1`, `Test-NetEnd.ps1`) und `Guided-Test.ps1`, mit dem ein Mensch Schritte im Spiel schnell abarbeitet, während das Skript Zeiten, Logs und Abstürze festhält. **Fertig, wenn** ein Sitzungsstart messbar kürzer ist, die Fälle aus [Im Spiel (05.11.2026)](src/Net/README.md#im-spiel-05112026) ohne Handarbeit durchlaufen und ein geführter Test einen Bericht schreibt. **Einstieg:** [tools/spike](tools/README.md), [Testen](docs/TESTING.md) | – | parallel | wenig |
| `spiel-update` | P2 | Nur wenn das Team einen neuen Spiel-Build beschließt: Offsets, Signaturen und Patches anpassen. Betrifft alles auf einmal, alle Spieltests warten darauf. **Einstieg:** [Update-Regel](CONTRIBUTING.md#spiel-updates) | – | Team | viel |

## Optionale Optimierungen

Bekannte kleine Fehler, die die Spielbarkeit nicht verhindern. Sie warten auf `integration-run`. Rückt ein Punkt die Spielbarkeit doch in Gefahr, wandert er mit P0 oder P1 in die passende Phase. Zählt nicht zum Fortschritt.

Auch die zukünftige Update-Verwaltung bleibt vorerst optional: Wir erzwingen Build `26187885` und entwickeln auf diesem Stand. Die Aufgaben `update-startpruefung`, `update-releasefeed` und `update-autoupdater` sind keine Voraussetzung für den aktuellen Release und ändern weder die Build-Sperre noch die Teamentscheidung über neue Spielversionen. Erst lokale Prüfung, dann Update-Angebot, zuletzt optional automatische Installation.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `opt-host-bewegung` | P2 | Klären, warum der Host Remote-Figuren ohne clientautoritative Position fast nicht bewegt. Clientautoritative Position reicht für den Betrieb | `integration-run` | parallel | mittel |
| `opt-tempo-upgrades` | P2 | `MovementSpeed` und Beschleunigung: Upgrades kommen beim Client nicht an (Host 819 und 2866, Client 600 und 2048) | `integration-run` | parallel | mittel |
| `opt-gegnerzahl` | P2 | Gegner-Zähler weichen ab (30 beim Host, 48 beim Client), vermutlich bleiben tote Gegner als Objekte stehen | `integration-run` | parallel | mittel |
| `opt-blut` | P2 | Blut- und Treffereffekte erscheinen nur beim Host | `integration-run` | parallel | wenig |
| `opt-client-visuell` | P2 | Feinschliff der Client-Darstellung aus `client-visuell`. Projektilgröße (`BulletSize`, nicht repliziert) fehlt. Das erste Geschoss eines neuen Pool-Actors kann bis zu 1 s ohne Bewegung erscheinen. Treffer- und Partikeleffekte fehlen, siehe auch `opt-blut`. Offen sind außerdem Randfälle der XP-Bälle: Einsammeln durch den Client im Einzelfall, Kartenwechsel im Flug und verlorene Nachrichten. **Einstieg:** [Client-Darstellung](research/spikes/client-visuell/README.md) | `integration-run` | parallel | mittel |
| `opt-portraet` | P2 | Host zeigt an Tag 2 ein anderes Figurenporträt | `integration-run` | parallel | wenig |
| `opt-kameras` | P2 | Getrennte Kameras je Spieler ermöglichen: Bewegungsbegrenzung anpassen, Sichtbereiche für das Gegner-Spawning synchronisieren oder die Spawn-Logik ersetzen, Gegnerverteilung prüfen. Keine Voraussetzung für die gemeinsame Kamera, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) | `integration-run` | parallel | viel |
| `opt-trennen-absturz` | P2 | Client stürzt beim `disconnect` aus der Mission manchmal mit „uobject leaks (world leaks)“ ab (`ReferenceChainSearch.cpp` Zeile 2061), einmal in zwei Versuchen nach „Verlassen“ des Clients. Der Client wollte ohnehin gehen, der Host spielt weiter. Ursache klären, etwa mit einem Build, der die Referenzkette loggt. **Einstieg:** [Im Spiel (05.11.2026)](src/Net/README.md#im-spiel-05112026) | `integration-run` | parallel | mittel |
| `mehr-spieler` | P2 | Mehr als 2 Spieler prüfen. Die Coop-Logik ist fest auf P1 und P2 ausgelegt, daher aufwendig | `release` | parallel | viel |
| `coop-spielstand` | P2 | Der Host speichert einen Coop-Run und setzt ihn später mit demselben Client fort. **Fertig, wenn** nach Speichern, Beenden beider Spiele und erneutem Laden beim Host der Client beitritt und der Run mit gleichem Tag, gleicher Route und gleichen Inventaren weiterläuft | `integration-run`, `mod-saves` | parallel | viel |
| `update-startpruefung` | P2 | Mod-/Spiel-Kompatibilität vor dem Laden prüfen: gemeinsame Quelle für Mod-Version, Protokollversion, unterstützte Game-Builds, Kompatibilitätsprofil und Datei-Hashes; Starter prüft die tatsächlich gestartete Installation (auch eingefrorene Kopien, Exe und relevante Inhaltscontainer), nicht nur das Steam-Manifest. Bei direktem Steam-Start prüft der `version.dll`-Loader erneut vor dem Laden von `DDCoop.dll`; die interne Prüfung bleibt bestehen. **Fertig, wenn** unbekannte/veränderte Builds und widersprüchliche Paketangaben die Mod nicht laden, ein sichtbarer Hinweis Mod-Version, gefundenen/erwarteten Build und Grund nennt und Abbrechen oder ein nachweislich modfreier Start möglich sind; keine Meldung unter dem Loader-Lock. Eine ältere, weiterhin kompatible Mod bleibt erlaubt; Internet ist für die lokale Freigabe nicht nötig. Pflicht-Patch-/Adapterfehler verhindern die Coop-Freigabe, Teilinstallation darf nicht als erfolgreicher Start gelten. Positiv-, Negativ- und Direktstarttests dokumentieren. **Einstieg:** `src/Loader/Loader.cpp`, `src/Core/Version.cpp`, `src/Mod/Mod.cpp`, `src/Game/Patches.cpp`, `tools/game_build.py` | `integration-run`, `installer` | zentral | wenig |
| `update-releasefeed` | P2 | Vertrauenswürdig prüfbare Release-/Kompatibilitätsinformationen anbieten: signierte Metadaten mit Mod-Version, unterstützten Build-/Profilidentitäten, Protokollversion, Paket-Hashes und ausdrücklichen Rücknahmen kritischer Releases. Zunächst nur nach einem passenden Update suchen und es anbieten, nichts automatisch installieren. **Fertig, wenn** nur ausdrücklich freigegebene Pakete für den tatsächlich geprüften Game-Build angeboten werden, Signatur/Schema sowie veraltete oder zurückgespielte Metadaten geprüft werden und ungültige Antworten keine Freigabe bewirken; Offlinebetrieb nutzt die lokale Kompatibilitätsprüfung und kennzeichnet den Update-Status als unbekannt. Bekannte gültige Rücknahmen werden nicht durch Offlinebetrieb vergessen; eine bloß ältere kompatible Mod wird nicht gesperrt. Kein automatischer Support durch gefundene Signaturen. Tests für passende/unpassende Releases, Rücknahmen, ungültige Signaturen, veraltete Antworten und Netzausfall bestehen | `update-startpruefung` | zentral | keine |
| `update-autoupdater` | P2 | Später optionales automatisches Mod-Update auf dem Releasefeed aufbauen, nur nach ausdrücklicher Aktivierung. **Fertig, wenn** ein für den vorhandenen Game-Build freigegebenes Paket zunächst separat heruntergeladen, anhand signierter Metadaten und Hashes geprüft und ausschließlich bei beendetem Spiel transaktional installiert wird; Archive dürfen keine fremden Pfade überschreiben. Abbruch, beschädigter Download, Dateisperren und Installationsfehler lassen das alte vollständige Paket erhalten oder stellen es wieder her. Vorversion und Rückkehrmöglichkeit bleiben verfügbar, Konfiguration und Saves bleiben erhalten; Save-Kompatibilität vor Rückkehr prüfen. Keine Game-Updates, keine Änderung eingefrorener Spielkopien und kein Austausch geladener DLLs. Tests für Fehlerfälle, Wiederanlauf und Rückkehr dokumentieren | `update-releasefeed`, `installer`, `mod-saves` | zentral | wenig |

## Erledigt

Alphabetisch sortiert. Details stehen in der verlinkten Doku.

| Kennung | Phase | Ergebnis |
|---|---|---|
| `andockpunkte` | 1 | VERSION-Proxy, drei Live-Signaturen und ProcessEvent-Hook auf Host und Client erprobt, siehe [Andockpunkte](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885) |
| `blueprint-logik` | 1 | Repräsentative Spawn-, Effekt- und Eventpfade per Bytecode-Audit und Traces geklärt, siehe [Nachtrag](research/spikes/blueprint-logik/README.md#nachtrag-03112026-grenzpruefung-und-ereignistests) |
| `boss-dump` | 0 | Klassen aller sechs Bosse gezielt geladen und gedumpt, siehe [Boss-Dump](docs/GAME_ANALYSIS.md#boss-dump-boss-dump) |
| `branch-schutz` | 0 | Branch-Schutz für `main` aktiv |
| `build-id` | 0 | Geprüfte Kopie von Build `26187885` außerhalb von Steam, Start und Deploy prüfen alle Dateien, siehe [Einrichtung](docs/SETUP.md#gleiche-build-id-im-team) |
| `ci` | 2 | SDK-freie Tests und CI vorhanden |
| `client-visuell` | 4 | Client sieht XP-/Levelstand, XP-Bälle, Projektilbewegung und Airdrops wie der Host, ohne doppelte XP oder Rewards; Feinschliff in `opt-client-visuell`, siehe [Client-Darstellung](research/spikes/client-visuell/README.md) |
| `config-paks` | 1 | Config aus den Paks gelesen: `IpNetDriver`, `DefaultPlatformService=Steam`, Standard-Maps, siehe [Konfiguration](docs/GAME_ANALYSIS.md#konfiguration) |
| `controller-tausch` | 1 | Controller-Tausch nach dem Missionswechsel über `K2_OnSwapPlayerControllers` sofort in `SpawnedPlayers` abgeglichen, siehe [Spike](research/spikes/a-connect/README.md#controller-tausch-nach-dem-missionswechsel-controller-tausch) |
| `coop-ablauf` | 1 | P2 ist ein zweiter `ULocalPlayer` aus `SpawnLocalPlayer(1)`, 5 Stellen kommen ohne `LocalPlayer` nicht zurecht, siehe [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf) |
| `crash-reporter` | 1 | Mod und Spike lassen den Start von `CrashReportClient.exe` scheitern, `CrashGuard` bleibt erste Schicht, siehe [Crash-Reporter](research/spikes/a-connect/README.md#sperre-in-mod-und-spike-crash-reporter) |
| `deploy` | 2 | Deploy-Skript `tools/Deploy-Mod.ps1` |
| `dumper-fix` | 0 | Dumper-7-Fix liegt im eigenen Fork, eingebunden als Submodul `third_party/Dumper-7`, siehe [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) |
| `game-adapter` | 2 | Game-Thread-Takt am Spielfenster ruft `Drain` und die Adapter-Ticks; `Offsets.h` mit geschützten Lesezugriffen, beim Bauen gegen ein lokales Dumper-7-SDK geprüft; `WorldAdapter` als Muster liest Karte, Rolle und Spielerzahl. Mit Host und Client bis Tag 1 geprüft, [Muster](src/Game/README.md) |
| `gate-g1` | 1 | Am 04.11.2026 vom Nutzer verbindlich entschieden: Weg A (native UE-Replikation) als Grundlage, ergänzt durch eigenen Kontroll-/Sync-Kanal für XP, Währungen, Rewards, Pausen und UI; Weg B nur Rückfalloption. [Architekturentscheidung](docs/ARCHITECTURE.md#entscheidungen) |
| `gegner-pooling` | 1 | Tote Gegner werden zerstört, nicht gepoolt, der `PoolManager` betrifft Projektile und Effekte, siehe [Gegner-Spawning](docs/GAME_ANALYSIS.md#gegner-spawning-und-pooling) |
| `github-repo` | 0 | Öffentliches GitHub-Repo angelegt |
| `grundgeruest` | 2 | `Loader` als Proxy-DLL, `Core` mit Logging, Config, Hooks, Dispatcher und Versionsprüfung, mit Host und Client geprüft |
| `guidcache` | 1 | Blockaden-Autorität, NetGUID-Pfadlebensdauer und Sneak-Zielbereitschaft korrigiert; Test-Godmode für beide Spieler, Lauf bis Endlos-Tag 13 mit zwei Biomwechseln. Am 04.11.2026 vom Nutzer als abgeschlossen akzeptiert; vorerst keine weiteren Detailtests, neue konkrete Probleme erhalten ein neues TODO. [Nachweise und Testgrenzen](docs/GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache) |
| `hp-leisten` | 1 | Spike-Fix: Die Lebensleisten über den Figuren blieben beim Client auf dem Startwert, weil das Spiel `OnHealthChanged` nur beim Host auslöst. Jetzt ruft der Spike die Empfänger nach `OnRep_Health` und `OnRep_MaxHealth` auf, siehe [Lebensleisten](docs/GAME_ANALYSIS.md#lebensleisten-beim-client-hp-leisten) |
| `iris` | 1 | Zur Laufzeit ist die klassische Replikation mit `ActorChannel` aktiv, nicht Iris |
| `kamera-entscheidung` | 1 | Gemeinsame Kamera (Option A) mit ursprünglichem Zoom und Bewegungsbegrenzung, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) |
| `kamera-viewtarget` | 1 | Spike-Fix: Der Client schaute nach Kartenwechseln aus der eigenen Figur, weil die Kamera am alten Controller landete. Jetzt nur der lokale Controller der aktuellen Welt, Prüfung jede Sekunde, siehe [Kamera](docs/GAME_ANALYSIS.md#viewtarget-beim-client-kamera-viewtarget) |
| `karte-analyse` | 1 | Replizierte Kartenobjekte sind Child Actors der nicht replizierten Parzellen, siehe [Kartenobjekte](docs/GAME_ANALYSIS.md#karte-und-replizierte-kartenobjekte) |
| `karte-client` | 1 | Client erzeugt die Host-Karte ohne replizierte Duplikate (`MapGen.cpp`), siehe [Client erzeugt die Host-Karte](docs/GAME_ANALYSIS.md#client-erzeugt-die-host-karte-karte-client) |
| `karten-erzeugung` | 1 | Gleicher Run-Seed und gleiche Mission ergeben dieselbe Karte, siehe [Karte und Seed](docs/GAME_ANALYSIS.md#karte-und-seed) |
| `last` | 1 | Spike D: bei rund 85 Gegnern 47 KB/s vom Host, beide Instanzen 60 fps, nur Loopback, siehe [Last](docs/GAME_ANALYSIS.md#last-spike-d-last) |
| `layerapps-sperre` | 6 | Die Mod sperrt auch `deadlydays.layerapps.com`, das das Spiel beim Run-Ende anfragt (`Core::ReportHosts`). Run-Ende und neuer Run laufen normal, siehe [Telemetrie und Berichte](docs/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus) |
| `menue-weg` | 1 | Mission über den echten Weg durch das Spielmenü mit den Patches gestartet, siehe [Spike](research/spikes/a-connect/README.md) |
| `mitwirkende` | 0 | Freunde als Mitwirkende eingeladen |
| `net-fehler` | 3 | Jedes Sitzungsende (Verlassen beider Seiten, Abbruch, Steam- und Sitzungszeitlimits) erscheint mit eigenem Grund als lesbare Meldung in Overlay und Log, danach sind Erstellen und Beitreten wieder frei; im Spiel geprüft, siehe [Fehlerbehandlung](src/Net/README.md#fehlerbehandlung-net-fehler) |
| `niederlage` | 1 | Nach einer Niederlage kehren Host und Client über „Zurück zur Basis“ in die `MetaBase` zurück, und ein neuer Run startet; der Host-Absturz im Run-Ende-Handler ohne `LocalPlayer` ist im Spike behoben (`RunEnd.cpp`), siehe [Niederlage](docs/GAME_ANALYSIS.md#niederlage-niederlage) |
| `online-sperren` | 6 | Die Mod sperrt die schreibenden Steam-Methoden für Erfolge, Statistiken und Bestenlisten (`Core::SteamStats`), lesen bleibt erlaubt, siehe [Erfolge und Bestenlisten](docs/GAME_ANALYSIS.md#erfolge-und-bestenlisten-online-sperren) |
| `overlay` | 2 | F8-Overlay mit Status und Debug-Anzeige, sichtbare Host-/Client-Abnahme bestanden, siehe [UI](src/UI/README.md#sichtbare-spielabnahme-03112026) |
| `patches` | 4 | Die 25 Spielfehler-Fixes des Spikes stehen geprüft in `src/Game/*Patches.cpp`, alle oder keiner; mit der Mod lässt der Spike seine Fassungen aus. Route MetaBase bis Tag 5 und Niederlage mit Rückkehr in die MetaBase ohne Absturz, siehe [Patches](src/Game/README.md#patches) |
| `pausen` | 1 | Pausen-Logik gemessen, Host-Weltpause repliziert, Client-Wunsch nicht, siehe [Pausen-Spike](research/spikes/pausen/README.md#nachtrag-03112026-isolierte-ablaeufe) |
| `pcg` | 1 | Client erzeugt die PCG-Vegetation mit dem Graphen des Hosts (`Pcg.cpp`), siehe [PCG-Vegetation](docs/GAME_ANALYSIS.md#pcg-vegetation-beim-client-pcg) |
| `phasen-farbe` | Wartung | Vollständig abgeschlossene Phasen im README mit grünem Zeilenhintergrund in einer lokal erzeugten SVG; verlinkte Texttabelle bleibt aufklappbar. Generator, Konsistenzprüfung und Regressionstests halten Farben und Zahlen synchron. |
| `phasenadapter` | 1 | Client sieht Rundenübersicht, Lager und Routenkarte des Hosts und ist dort gesperrt, siehe [Rundenphasen](research/spikes/a-connect/RoundPhaseRuntime.md) |
| `powerup-duplikat` | 1 | Lokale Kopie von `BP_ChaosDestructable_PowerUp_C` beim Client unterdrückt, siehe [PowerUp-Objekte](docs/GAME_ANALYSIS.md#doppelte-powerup-objekte-powerup-duplikat) |
| `protokoll` | 3 | Handshake mit Spiel-Build, Mod-Version und Rolle sowie zuverlässiger Kontrollkanal im selben Steam-Socket wie die UE-Pakete, Nachrichten mit Run-ID, Epoche und Revision nach dem Vertrag; 24 Net-Tests, im Spiel bis Tag 1 mit XP-Anzeige darüber geprüft. Der Internet-Test mit zwei Konten gehört zu `lobby-live` bzw. `integration-g2`, siehe [Kontrollkanal](docs/CONTROL_PROTOCOL.md) |
| `remote-p2` | 1 | Phase-1-Frage geklärt: Host-P2 über schwache `ActivePlayers`, native Coop-Aktivierung, gezielten P2-Geräte-Guard und Join-Meldung ohne zweiten `LocalPlayer`; Laufzeit-/UI-Gegenproben zeigen, dass der lokale Lager-Handoff den geplanten Kontrollkanal braucht. Produktionsanbindung und UI-Sync bleiben eigene Aufgaben, [Nachweise und Grenzen](research/spikes/a-connect/RemoteP2.md) |
| `replikation-blueprint` | 1 | 218 von 482 Blueprint-Actor-Klassen replizieren, vor allem über ihre native Basisklasse |
| `replikation-kern` | 1 | Replizierte Properties der Kernklassen erfasst, der Kern repliziert, siehe [Replikation](docs/GAME_ANALYSIS.md#replikation-zur-laufzeit) |
| `rpc-vergleich` | 1 | RPC-Liste gegen den Juli-Dump: 3 RPCs entfernt, keiner hinzugekommen |
| `run-ablauf` | 1 | `TriggerStartRun`, `FinalizeRunStart`, Ladebildschirm und Etappenwechsel nachvollzogen, siehe [Run-Ablauf](docs/GAME_ANALYSIS.md#run-ablauf) |
| `sdk` | 0 | SDK für UE 5.8 erzeugt, siehe [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) |
| `sdk-mission` | 0 | SDK zusätzlich während einer Mission gedumpt, mit Blueprint-Klassen |
| `solution` | 0 | Visual-Studio-Solution mit einem Projekt je Modul, `config/Local.props` eingebunden |
| `spike-a` | 1 | Spike A: Beide Instanzen kommen in dieselbe Welt, auch auf einem PC, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| `spike-b` | 1 | Spike B: Client stabil in der Mission, Bewegung beidseitig, Schüsse wirken beim Host, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| `spike-c` | 1 | Spike C: Mit `karte-client` und `pcg` ist die Mission beim Client gleich, `check_round.py` zählt an Tag 1 bis 4 gleich viele `MapTile`s, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung) |
| `steam-lobby` | 3 | Steam-Lobby nur für Freunde direkt über Steamworks (`Net::Session`, `src/Mod/Lobby.cpp`, Overlay-Knöpfe); auf einem PC erstellt, per Lobby-ID beigetreten und bis Tag 1 gespielt, Test mit zwei Konten ist `lobby-live`, siehe [Steam-Lobby](src/Net/README.md#steam-lobby-steam-lobby) |
| `sync-bedarf` | 1 | Sync-Weg für XP, Währungen, Rewards, Pausen und UI als Entwurf festgelegt, siehe [Sync-Bedarf](docs/SYNC_REQUIREMENTS.md) |
| `team-aufteilung` | 0 | Keine festen Bereiche je Person und kein `CODEOWNERS`, die Arbeit teilt sich über `tools/tasks.py` |
| `telemetrie-aus` | 6 | Die Mod schaltet Balance-Telemetrie und automatische Spielstand-Berichte ab und sperrt die Namensauflösung für `codecks.io` und `pixelsplit.games` (`Game::TelemetryAdapter`, `Core::ReportHosts`), siehe [Telemetrie und Berichte](docs/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus) |
| `testaufbau` | 1 | Zwei Instanzen auf einem PC unter demselben Steam-Konto, je mit eigenem `-UserDir=` |
| `testumgebung` | 1 | Tag 1 bis 4 laden sauber mit Spawn am Bus und je 2 PlayerStates, Prüfskript und fester Routenlauf, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung) |
| `tick-absturz` | 1 | Restrictor-Absturz `0x5A17ACC` nachgestellt, Laufzeitfix ist `restrictor`, siehe [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) |
| `transport` | 3 | Tunnel aus Spike E als Modul `src/Net/Transport` mit Tests ohne Spiel; im Spiel über den Tunnel der Mod bis Tag 1, Inventare gleich, siehe [Net](src/Net/README.md#transport-transport) |
| `treffer` | 1 | Projektile des Clients ziehen beim Host Leben ab und töten (`Hits.cpp`), siehe [Treffer](docs/GAME_ANALYSIS.md#treffer-des-clients-treffer) |
| `trennen` | 1 | Client-Absturz beim Trennen war eine Folge fehlender Inventar-Items, mit Teilfix kein Absturz, siehe [Trennen](docs/GAME_ANALYSIS.md#trennen-ins-hauptmenü-trennen) |
| `tunnel` | 1 | Der Spielverkehr läuft über `ISteamNetworkingSockets` aus der Steam-Laufzeit des Spiels, zwei Instanzen spielten so bis Tag 2 (`Tunnel.cpp`); P2P mit zwei Konten ist `tunnel-p2p`, siehe [Steam-Tunnel](research/spikes/a-connect/README.md#spike-e-steam-tunnel-tunnel) |
| `tunnel-p2p` | 1 | Zwei PCs mit zwei Steam-Konten spielten über den P2P-Tunnel bis in die Mission an Tag 1, 120 s ohne Absturz, Ping 41 ms im Median über ein Steam-Relay, nichts verworfen, siehe [Steam-Tunnel](research/spikes/a-connect/README.md#p2p-auf-zwei-pcs-tunnel-p2p) |
| `tutorial-aus` | 6 | Die Mod schaltet die Tutorial-Hinweise als Host und Client automatisch ab und prüft das jede Sekunde (`src/Game/TutorialAdapter`), siehe [Tutorial-Pause](docs/GAME_ANALYSIS.md#tutorial-pause-hält-den-run-an-testumgebung) |
| `update-regel` | 0 | Spiel-Updates: einfrieren und nur bei Bedarf nachziehen, siehe [Update-Regel](CONTRIBUTING.md#spiel-updates) |
| `weeklymode` | 5 | Team-Entscheidung vom 05.11.2026: Die Weekly-Challenge wird nicht unterstützt und ist gesperrt, solange die Mod geladen ist, auch solo, wie Erfolge und Bestenlisten. Umsetzung ist `weekly-sperre` |
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
