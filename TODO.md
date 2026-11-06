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
| `lobby-auth-live` | P0 | [3](#phase-3--session-und-verbindung) | zu zweit | wenig |
| `lobby-live` | P0 | [3](#phase-3--session-und-verbindung) | zu zweit | wenig |
| `gegnertyp` | P0 | [4](#phase-4--vertical-slice) | parallel | mittel |
| `bestaetigen-rundenende` | P0 | [5](#phase-5--vollständiger-run) | parallel | mittel |
| `mod-saves` | P0 | [6](#phase-6--release) | parallel | mittel |
| `eingaben` | P1 | [4](#phase-4--vertical-slice) | parallel | mittel |
| `airdrop-dodge-absturz` | P1 | [5](#phase-5--vollständiger-run) | parallel | mittel |
| `xp-baelle-live` | P1 | [5](#phase-5--vollständiger-run) | parallel | wenig |
| `installer` | P1 | [6](#phase-6--release) | parallel | keine |
| `weekly-sperre` | P1 | [6](#phase-6--release) | parallel | wenig |
| `karte-tunnel` | P1 | [Wartung](#wartung) | zu zweit | wenig |
| `mod-steuerung` | P1 | [Wartung](#wartung) | parallel | wenig |
| `reflexion-felder` | P1 | [Wartung](#wartung) | parallel | wenig |
| `doku-verweise` | P2 | [Wartung](#wartung) | parallel | keine |
| `opt-ladezeit-messung` | P2 | [Wartung](#wartung) | parallel | wenig |
| `patch-signaturen` | P2 | [Wartung](#wartung) | parallel | wenig |
| `sdk-vergleich` | P2 | [Wartung](#wartung) | parallel | keine |

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
| `lobby-auth-live` | P0 | Rest aus `lobby-auth`: Mit getrennten Steam-Konten prüfen, dass der eingeladene Freund verbindet, ein Konto außerhalb der Lobby mit SteamID des Hosts (`-Role Client -Tunnel Mod -Peer <SteamID64>` gegen einen Host mit `-Tunnel Lobby`) „nicht Mitglied seiner Lobby“ sieht und der Platz frei bleibt, und dass der Host nach Verlassen der Lobby trennt. **Fertig, wenn** alle drei Fälle über das Internet so ablaufen. Passt in einen Termin mit `lobby-live`. **Einstieg:** [Zugriffskontrolle](src/Net/README.md#zugriffskontrolle-lobby-auth) | `lobby-auth` | zu zweit | wenig |
| `lobby-live` | P0 | Steam-Lobby der Mod mit zwei Konten auf zwei PCs prüfen: Einladung über das Steam-Overlay annehmen, Beitritt per P2P, Start über `+connect_lobby` bei geschlossenem Spiel, Verlassen beider Seiten. **Fertig, wenn** der Client nur über die Einladung bis in die MetaBase kommt und der Handshake des [Kontrollkanals](docs/CONTROL_PROTOCOL.md) dabei über das Internet bestätigt ist. **Einstieg:** [Steam-Lobby](src/Net/README.md#steam-lobby-steam-lobby), [Test auf zwei PCs](docs/TESTING.md#test-auf-zwei-pcs) | `steam-lobby` | zu zweit | wenig |

## Phase 4 – Vertical Slice

Ziel: Zwei Spieler starten eine Mission, kämpfen gegen einen Gegnertyp und sehen dasselbe. Abschluss ist Gate G2.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `gegnertyp` | P0 | Ein Gegnertyp inklusive Spawn und Tod. Ordner `src/Sync/Enemies` | `remote-spieler` | parallel | mittel |
| `eingaben` | P1 | Eingaben aus dem lokalen `CoopInputRouter` sauber vom Remote-Spieler trennen. Ordner `src/Sync/Players`, Dateien mit `bewegung` absprechen | `remote-spieler` | parallel | mittel |
| `integration-g2` | P0 | Gate G2: Vertical Slice zusammenführen und zehn Minuten über echtes Internet zwischen zwei Haushalten spielen. Zwei Menschen nötig. **Fertig, wenn** die Runde stabil, ohne Desync und spielbar ist. Sonst Ursachen beheben, bevor weitere Inhalte synchronisiert werden | `missionsstart`, `kamera-code`, `bewegung`, `kampf`, `gegnertyp`, `eingaben`, `restrictor`, `steam-lobby`, `lobby-live`, `transport`, `net-fehler`, `ohne-spike`, `latenz-test`, `desync-melder` | zentral | viel |

## Phase 5 – Vollständiger Run

Ziel: Ein kompletter Run ist zu zweit online spielbar.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `gegner-alle` | P0 | Alle Gegner, Bosse und Boss-Phasen. Ordner `src/Sync/Enemies` | `integration-g2` | parallel | viel |
| `fortschritt` | P0 | Loot, XP, Level-ups, Fähigkeiten und Status-Effekte (GAS). Ordner `src/Sync/Combat` | `integration-g2`, `protokoll` | parallel | viel |
| `bestaetigen-rundenende` | P0 | Rest aus `bestaetigen-alle`: Am Rundenende mit Rundenübersicht und Lager soll wie beim Airdrop jeder Spieler Bestätigen drücken (Knopf gelb), erst wenn alle samt Host bestätigt haben, wählt der Host den nächsten Tag. Den Airdrop-Teil (`0x305`, `MainButton` gelb, Host holt sein Bestätigen nach) wiederverwenden, siehe [Alle bestätigen](src/Game/README.md#airdrop-auswahl). **Fertig, wenn** der Client nach der Busflucht im Lager bestätigt, sein Knopf gelb ist und der Host erst danach weiterkommt. Ordner `src/Game`, `src/Sync/Inventory` | `rundenende-client` | parallel | mittel |
| `xp-baelle-live` | P1 | Rest aus `xp-baelle`: Die Sichtgrenzen der XP-Bälle kommen beim Client jetzt aus der lokalen Kamera (`ExpPatches.cpp`), weil ihm der GameMode fehlt. Die sichtbare Gegenprobe im Spiel fehlt noch. **Fertig, wenn** der Client dieselben Bälle auch am Bildschirmrand sieht, sie beim Aufsammeln verschwinden und XP nicht doppelt zählen. Ordner `src/Game`. **Einstieg:** [XP-Bälle](docs/GAME_ANALYSIS.md#xp-bälle-beim-client-xp-baelle) | `protokoll` | parallel | wenig |
| `inventar-sync` | P0 | Rucksack, Stash, Werkbank, Reward-Queue (`CoopInventoryCoordinator`). Ordner `src/Sync/Inventory` | `integration-g2`, `protokoll`, `guidcache` | parallel | viel |
| `inventar-ui` | P0 | Inventar-UI beim Remote-Spieler. Ordner `src/Sync/Inventory`, `src/UI` | `inventar-sync` | parallel | mittel |
| `pausen-sync` | P0 | Pausen und blockierende UI beider Spieler über den Kontrollkanal abstimmen. Ordner `src/Sync/Players`. **Einstieg:** [Pausen-Spike](research/spikes/pausen/README.md) | `integration-g2`, `protokoll` | parallel | mittel |
| `tagesablauf` | P0 | Tageszyklus, Treibstoff, Missionsziele, Fahrt zwischen den Etappen. Ordner `src/Sync/World`, Dateien mit `run-ende` absprechen | `integration-g2`, `protokoll`, `guidcache`, `phasenadapter` | parallel | viel |
| `airdrop-dodge-absturz` | P1 | Gefunden in `ausweichen-client` am 06.11.2026: Dodgt der Client in einen Airdrop, stürzt er manchmal in `InventoryPanel.SetupInventory` ab (`InventoryComponent` an `+0x370` leer), nicht jedes Mal. **Fertig, wenn** der Ablauf erklärt ist und zehn Dodges des Clients in Airdrops ohne Absturz durchlaufen. Ordner `src/Game`. **Einstieg:** [Dodge des Clients](docs/GAME_ANALYSIS.md#dodge-des-clients-beim-host-ausweichen-client) | `remote-spieler` | parallel | mittel |
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
| `mod-steuerung` | P1 | Steuerkanal in der Mod für Testskripte: Befehle mit Antwort über eine lokale Named Pipe je Prozess, nur mit Testschalter aktiv. Mindestens Zustand als JSON (Welt, Rolle, Spieler, Sitzung, Overlay), Lobby erstellen, beitreten, verlassen, Konsolenbefehl und Reise. Ersetzt die Befehlsdatei des Spikes (`Send-SpikeCommand`, eine Sekunde Abfrage, keine Antwort) und das Auslesen per `netstate.py` und Overlay-Fenster. **Fertig, wenn** `Start-Session.ps1`, `Restart-Instance.ps1` und `Test-NetEnd.ps1` ohne Spike-Befehle auskommen und gleich schnell oder schneller sind. **Einstieg:** [Spielzeit sparen](docs/TESTING.md#spielzeit-sparen), `tools/spike/Common.ps1` | `test-tempo` | parallel | wenig |
| `karte-tunnel` | P1 | Beim Test auf zwei PCs fielen die Figuren durch den Boden: Der Client bekam die Host-Karte nicht, `mapgen.txt` gab es nur auf dem Host-PC. Der Steam-Tunnel trägt die Karte jetzt mit (`Tunnel.cpp`, in `main` seit #81, auf einem PC geprüft). Offen ist nur noch der Test zu zweit. **Fertig, wenn** der Client auf einem zweiten PC dieselbe Karte erzeugt und beide Figuren in der Mission auf dem Boden stehen. **Einstieg:** [Host-Karte über den Tunnel](research/spikes/a-connect/README.md#host-karte-über-den-tunnel-karte-tunnel) | `tunnel-p2p` | zu zweit | wenig |
| `reflexion-felder` | P1 | Feld- und Funktionszugriffe auf Spielklassen zur Laufzeit über die Engine-Reflexion auflösen statt über feste Offsets. Ein kleiner Helfer in `src/Game` sucht `FProperty` nach Klasse und Feldname über `ChildProperties` (Offset, Größe, Typ) und `UFunction` nach Name, einmal je Start gecacht. Die dafür nötigen Engine-Grundlayouts (`ChildProperties`, `FField`, `FProperty`) kommen nach `Offsets.h` und werden von `SdkChecks.cpp` geprüft. Fehlt ein Feld oder passt der Typ nicht, schaltet sich nur der betroffene Adapter oder Patch mit Logmeldung ab, statt falsch zu lesen. Früh eingeplant, damit der Sync-Code der Phasen 4 und 5 gleich darauf aufbaut und ein `spiel-update` weniger Werte von Hand braucht. **Fertig, wenn** der Helfer mit Unit-Tests auf `main` ist, mindestens zwei bestehende Adapter umgestellt sind, ihre Laufzeitwerte im Spiel mit den SDK-Werten übereinstimmen und die Regel „neue Feldzugriffe über den Helfer“ in [Game](src/Game/README.md#muster-für-neue-adapter) steht. Ordner `src/Game` | – | parallel | wenig |
| `patch-signaturen` | P2 | Die festen Code-Adressen der Patches (rund 90 RVAs in `src/Game/*Patches.cpp` und `CodeCave.cpp`) in eine zentrale Tabelle mit Byte-Signatur und erwarteter RVA überführen, wie `Core::Signature` es für `ProcessEvent` schon macht. Beim Start gilt: genau ein Treffer an der erwarteten Stelle, sonst bleibt nur dieser Patch aus und das Log nennt ihn. Dateiweise in kurzen PRs umstellen und Dateien laufender Aufgaben auslassen. Neue Patches kommen danach gleich mit Signatur in die Tabelle. **Fertig, wenn** jede Patch-Adresse in der Tabelle steht, ein Prüfskript alle Signaturen gegen die eingefrorene Exe als eindeutig meldet und ein absichtlich falsches Muster im Spiel nur seinen Patch abschaltet. **Einstieg:** [Andockpunkte](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885), `src/Core/Signature.h` | – | parallel | wenig |
| `sdk-vergleich` | P2 | Werkzeug für `spiel-update`: Vergleicht zwei lokale Dumper-7-SDK-Ausgaben (alter und neuer Build) und listet für alles, was die Mod nutzt (`Offsets.h`, [OFFSETS.md](docs/OFFSETS.md), Klassen- und Funktionsnamen aus `src/`), ob es unverändert, verschoben oder verschwunden ist. Liest nur lokale Pfade, schreibt nie SDK-Inhalte ins Repo. **Fertig, wenn** `tools/sdk_diff.py` auf zwei SDK-Ordnern diese Liste ausgibt, Unit-Tests mit kleinen selbst geschriebenen SDK-Ausschnitten bestehen und der Aufruf unter [Spiel-Updates](CONTRIBUTING.md#spiel-updates) steht | – | parallel | keine |
| `opt-ladezeit-messung` | P2 | `check_round.py --wait` meldet keine Ladezeit (`load_seconds` leer): Während des Ladens liefert `world()` 0, `alive()` hält das für eine beendete Instanz und `wait_new_world` gibt sofort auf. Die Prüfung danach stimmt trotzdem, weil `Run-Route.ps1` vorher Bilder macht und wartet. Seit mindestens 05.11.2026 in allen Routenläufen, auch ohne Netzprofil. Als Testwerkzeug unter Wartung statt optional. **Einstieg:** `tools/spike/check_round.py`, `wait_new_world` | – | parallel | wenig |
| `spiel-update` | P2 | Nur wenn das Team einen neuen Spiel-Build beschließt: Offsets, Signaturen und Patches anpassen. Betrifft alles auf einmal, alle Spieltests warten darauf. Leichter mit `reflexion-felder`, `patch-signaturen` und `sdk-vergleich`, aber nicht davon abhängig. **Einstieg:** [Update-Regel](CONTRIBUTING.md#spiel-updates) | – | Team | viel |

## Optionale Optimierungen

Bekannte kleine Fehler, die die Spielbarkeit nicht verhindern. Sie warten auf `integration-run`. Rückt ein Punkt die Spielbarkeit doch in Gefahr, wandert er mit P0 oder P1 in die passende Phase. Zählt nicht zum Fortschritt.

Auch die zukünftige Update-Verwaltung bleibt vorerst optional: Wir erzwingen Build `26187885` und entwickeln auf diesem Stand. Die Aufgaben `update-startpruefung`, `update-releasefeed` und `update-autoupdater` sind keine Voraussetzung für den aktuellen Release und ändern weder die Build-Sperre noch die Teamentscheidung über neue Spielversionen. Erst lokale Prüfung, dann Update-Angebot, zuletzt optional automatische Installation.

| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |
|---|---|---|---|---|---|
| `opt-desync-pivot` | P2 | Fehlalarm im Desync-Melder (Coop-Test 06.11.2026): Bei Items außerhalb des Rucksacks, etwa der Pistole, setzt die eigene Inventaransicht die Rasterposition lokal. Beim Host-Spieler repliziert sie zum Client, beim Client-Spieler bleibt sie beim Host auf `(0, 0)` (Client `(-19, -2)`). Spielerisch folgenlos, der Melder meldet aber dauerhaft „Abweichung Inventar“ und kann echte Inventarfehler überdecken. Position solcher Items im Inventar-Messfeld ignorieren | `integration-run` | parallel | wenig |
| `opt-host-bewegung` | P2 | Klären, warum der Host Remote-Figuren ohne clientautoritative Position fast nicht bewegt. Clientautoritative Position reicht für den Betrieb | `integration-run` | parallel | mittel |
| `opt-tempo-upgrades` | P2 | `MovementSpeed` und Beschleunigung: Upgrades kommen beim Client nicht an (Host 819 und 2866, Client 600 und 2048) | `integration-run` | parallel | mittel |
| `opt-gegnerzahl` | P2 | Gegner-Zähler weichen ab (30 beim Host, 48 beim Client), vermutlich bleiben tote Gegner als Objekte stehen | `integration-run` | parallel | mittel |
| `opt-blut` | P2 | Blut- und Treffereffekte erscheinen nur beim Host | `integration-run` | parallel | wenig |
| `opt-client-visuell` | P2 | Feinschliff der Client-Darstellung aus `client-visuell`. Projektilgröße (`BulletSize`, nicht repliziert) fehlt. Das erste Geschoss eines neuen Pool-Actors kann bis zu 1 s ohne Bewegung erscheinen. Treffer- und Partikeleffekte fehlen, siehe auch `opt-blut`. Offen sind außerdem Randfälle der XP-Bälle: Einsammeln durch den Client im Einzelfall, Kartenwechsel im Flug und verlorene Nachrichten. **Einstieg:** [Client-Darstellung](research/spikes/client-visuell/README.md) | `integration-run` | parallel | mittel |
| `opt-portraet` | P2 | Host zeigt an Tag 2 ein anderes Figurenporträt | `integration-run` | parallel | wenig |
| `opt-kameras` | P2 | Getrennte Kameras je Spieler ermöglichen: Bewegungsbegrenzung anpassen, Sichtbereiche für das Gegner-Spawning synchronisieren oder die Spawn-Logik ersetzen, Gegnerverteilung prüfen. Keine Voraussetzung für die gemeinsame Kamera, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) | `integration-run` | parallel | viel |
| `opt-trennen-absturz` | P2 | Client stürzt beim `disconnect` aus der Mission manchmal mit „uobject leaks (world leaks)“ ab (`ReferenceChainSearch.cpp` Zeile 2061), einmal in zwei Versuchen nach „Verlassen“ des Clients. Der Client wollte ohnehin gehen, der Host spielt weiter. Ursache klären, etwa mit einem Build, der die Referenzkette loggt. **Einstieg:** [Im Spiel (05.11.2026)](src/Net/README.md#im-spiel-05112026) | `integration-run` | parallel | mittel |
| `opt-beitritt-absturz` | P2 | Host stürzt beim Lobby-Beitritt des Clients manchmal ab (`EXCEPTION_ACCESS_VIOLATION` an Adresse 0, Stack beginnt `5901ce8 2a8bfc4 2a8be06 2a7bb50`), einmal in zwei Starts mit `Start-Session.ps1 -Mod -Tunnel Lobby` am 05.11.2026. Ein zweiter Start lief durch. Ursache klären, etwa Stack gegen den Mod-Tunnel und das Overlay prüfen. **Einstieg:** `py tools/spike/crashes.py` | `integration-run` | parallel | mittel |
| `sync-aufraeumen` | P2 | Nach dem ersten kompletten Run alle Patches, Hooks und Kontrollkanal-Nachrichten durchgehen: Was deckt inzwischen native Replikation oder lokale Spiellogik ab, was synchronisiert doppelt, was ist Spike-Rest? Grundlage ist der [Client-lokal-Audit](docs/CLIENT_LOCAL_AUDIT.md). **Fertig, wenn** im Audit je Patch und Nachricht eine Entscheidung steht, die überflüssigen entfernt sind und ein Routenlauf bis Tag 4 mit Desync-Melder ohne Befund bleibt | `integration-run` | zentral | mittel |
| `opt-karte-details` | P2 | Mit übertragener Karte stimmen Kacheln, Straßen, Gebäude und PCG-Vegetation, im Spielbild wichen aber einzelne Details ab: ein Baum in anderer Art, ein anderes Dach, ein Pickup nur beim Client. Klären, ob zufällige Varianten einzelner Child Actors (`RandomChildActorSwapWrapper`) dahinterstecken, und sie dann aus der Position würfeln wie die PCG-Seeds. **Einstieg:** [Gleiche Karte in der Mod](docs/GAME_ANALYSIS.md#gleiche-karte-in-der-mod-missionsstart) | `integration-run` | parallel | mittel |
| `mehr-spieler` | P2 | Mehr als 2 Spieler prüfen. Die Coop-Logik ist fest auf P1 und P2 ausgelegt, daher aufwendig | `release` | parallel | viel |
| `coop-spielstand` | P2 | Der Host speichert einen Coop-Run und setzt ihn später mit demselben Client fort. **Fertig, wenn** nach Speichern, Beenden beider Spiele und erneutem Laden beim Host der Client beitritt und der Run mit gleichem Tag, gleicher Route und gleichen Inventaren weiterläuft | `integration-run`, `mod-saves` | parallel | viel |
| `update-startpruefung` | P2 | Mod-/Spiel-Kompatibilität vor dem Laden prüfen: gemeinsame Quelle für Mod-Version, Protokollversion, unterstützte Game-Builds, Kompatibilitätsprofil und Datei-Hashes; Starter prüft die tatsächlich gestartete Installation (auch eingefrorene Kopien, Exe und relevante Inhaltscontainer), nicht nur das Steam-Manifest. Bei direktem Steam-Start prüft der `version.dll`-Loader erneut vor dem Laden von `DDCoop.dll`; die interne Prüfung bleibt bestehen. **Fertig, wenn** unbekannte/veränderte Builds und widersprüchliche Paketangaben die Mod nicht laden, ein sichtbarer Hinweis Mod-Version, gefundenen/erwarteten Build und Grund nennt und Abbrechen oder ein nachweislich modfreier Start möglich sind; keine Meldung unter dem Loader-Lock. Eine ältere, weiterhin kompatible Mod bleibt erlaubt; Internet ist für die lokale Freigabe nicht nötig. Pflicht-Patch-/Adapterfehler verhindern die Coop-Freigabe, Teilinstallation darf nicht als erfolgreicher Start gelten. Positiv-, Negativ- und Direktstarttests dokumentieren. **Einstieg:** `src/Loader/Loader.cpp`, `src/Core/Version.cpp`, `src/Mod/Mod.cpp`, `src/Game/Patches.cpp`, `tools/game_build.py` | `integration-run`, `installer` | zentral | wenig |
| `update-releasefeed` | P2 | Vertrauenswürdig prüfbare Release-/Kompatibilitätsinformationen anbieten: signierte Metadaten mit Mod-Version, unterstützten Build-/Profilidentitäten, Protokollversion, Paket-Hashes und ausdrücklichen Rücknahmen kritischer Releases. Zunächst nur nach einem passenden Update suchen und es anbieten, nichts automatisch installieren. **Fertig, wenn** nur ausdrücklich freigegebene Pakete für den tatsächlich geprüften Game-Build angeboten werden, Signatur/Schema sowie veraltete oder zurückgespielte Metadaten geprüft werden und ungültige Antworten keine Freigabe bewirken; Offlinebetrieb nutzt die lokale Kompatibilitätsprüfung und kennzeichnet den Update-Status als unbekannt. Bekannte gültige Rücknahmen werden nicht durch Offlinebetrieb vergessen; eine bloß ältere kompatible Mod wird nicht gesperrt. Kein automatischer Support durch gefundene Signaturen. Tests für passende/unpassende Releases, Rücknahmen, ungültige Signaturen, veraltete Antworten und Netzausfall bestehen | `update-startpruefung` | zentral | keine |
| `update-autoupdater` | P2 | Später optionales automatisches Mod-Update auf dem Releasefeed aufbauen, nur nach ausdrücklicher Aktivierung. **Fertig, wenn** ein für den vorhandenen Game-Build freigegebenes Paket zunächst separat heruntergeladen, anhand signierter Metadaten und Hashes geprüft und ausschließlich bei beendetem Spiel transaktional installiert wird; Archive dürfen keine fremden Pfade überschreiben. Abbruch, beschädigter Download, Dateisperren und Installationsfehler lassen das alte vollständige Paket erhalten oder stellen es wieder her. Vorversion und Rückkehrmöglichkeit bleiben verfügbar, Konfiguration und Saves bleiben erhalten; Save-Kompatibilität vor Rückkehr prüfen. Keine Game-Updates, keine Änderung eingefrorener Spielkopien und kein Austausch geladener DLLs. Tests für Fehlerfälle, Wiederanlauf und Rückkehr dokumentieren | `update-releasefeed`, `installer`, `mod-saves` | zentral | wenig |

## Erledigt

Alphabetisch sortiert. Details stehen in der verlinkten Doku.

| Kennung | Phase | Ergebnis |
|---|---|---|
| `airdrop-rucksack` | 5 | Wahl und Ablage des Clients kommen beim Host im Rucksack an, keine Auto-Bestätigung beim Drag, Client-Anzeige aus replizierten Pivot-Feldern; Live-Rest in `airdrop-rucksack-live`, siehe [Airdrop-Auswahl](src/Game/README.md#airdrop-auswahl) |
| `airdrop-rucksack-live` | 5 | Vom Nutzer abgenommen, kein Auto-Bestätigen und keine Geister-Items; Bestätigen-Logik lesend geprüft, siehe [Airdrop-Auswahl](src/Game/README.md#airdrop-auswahl) |
| `airdrop-warten` | 5 | Bestätigt der Host zuerst, steht das Spiel für beide bis zur Wahl des Clients (im Speicher gemessen); das nachgeholte Bestätigen wartet einen Drag ab und wird wiederholt, kein zweiter Klick, siehe [Airdrop-Auswahl](docs/GAME_ANALYSIS.md#airdrop-auswahl-für-beide-spieler-airdrop-auswahl) |
| `andockpunkte` | 1 | VERSION-Proxy, drei Live-Signaturen und ProcessEvent-Hook auf Host und Client erprobt, siehe [Andockpunkte](docs/ARCHITECTURE.md#technische-andockpunkte-build-26187885) |
| `ausruestung-felder` | 5 | Neue Ausrüstungsfelder des Clients erscheinen sofort: Der Client löst nach lokaler Feldanlage und Zurückrollen OnRep_Slots selbst aus, Host-Übernahme mit 3 neuen Feldern geprüft, Inventare gleich ([Befund](src/Game/README.md#inventar-lokal-bearbeiten)) |
| `ausweichen-client` | 5 | Der Client weicht so weit aus und bremst so ab wie der Host, Ausdauer wird verbraucht: Die Mod trägt beim Host die aus dem Ort geschätzte Laufrichtung direkt vor GA_Dash ein, der Host führt den Dodge nativ aus |
| `bestaetigen-alle` | 5 | Bestätigen beim Airdrop markiert bereit (Knopf gelb), weiter erst wenn Host und Client bereit sind, in beiden Reihenfolgen live geprüft; Rundenende als `bestaetigen-rundenende`, siehe [Alle bestätigen](src/Game/README.md#airdrop-auswahl) |
| `bewegung` | 4 | Dodge beim Client repariert: eigene Figur bekommt Reibung und Schwerkraft zurück, Host-Figur folgt bei unbekannter Unterlage dem replizierten Ort, siehe [Dodge beim Client](docs/GAME_ANALYSIS.md#dodge-beim-client-bewegung) |
| `blueprint-logik` | 1 | Repräsentative Spawn-, Effekt- und Eventpfade per Bytecode-Audit und Traces geklärt, siehe [Nachtrag](research/spikes/blueprint-logik/README.md#nachtrag-03112026-grenzpruefung-und-ereignistests) |
| `boss-dump` | 0 | Klassen aller sechs Bosse gezielt geladen und gedumpt, siehe [Boss-Dump](docs/GAME_ANALYSIS.md#boss-dump-boss-dump) |
| `branch-schutz` | 0 | Branch-Schutz für `main` aktiv |
| `build-id` | 0 | Geprüfte Kopie von Build `26187885` außerhalb von Steam, Start und Deploy prüfen alle Dateien, siehe [Einrichtung](docs/SETUP.md#gleiche-build-id-im-team) |
| `bus-ende` | 5 | Busabfahrt und `FinishMission` laufen nur noch beim Host, der Client folgt; mit Bus bis Tag 2 geprüft, siehe [Busabfahrt](docs/GAME_ANALYSIS.md#busabfahrt-nur-beim-host-bus-ende) |
| `ci` | 2 | SDK-freie Tests und CI vorhanden |
| `ci-doku-pruefung` | Wartung | Doku-PRs lassen sich wieder mergen: Die CI läuft immer, `build-and-test` überspringt bei reinen Doku-Änderungen nur die Schritte und meldet die Pflichtprüfungen, scheitert die Erkennung, wird gebaut, siehe [CI](tests/README.md#ci) |
| `ci-tempo` | Wartung | CI-Lauf mit warmem Cache unter einer Minute je Job statt 3,5 bis 4,5 Minuten: ccache mit MultiToolTask, Skript-Tests einmal parallel, Doku-PRs ohne Build, überholte PR-Läufe brechen ab, Net-Tests 26 s → 9 s |
| `client-lokal-audit` | Wartung | Projektweite Regel lokal zuerst/minimal synchronisieren in CLAUDE.md, Architektur und Sync-Vertrag festgehalten; zehn Prüfbereiche mit Fundstellen, bestehenden Aufgaben und Abnahmen im [Audit](docs/CLIENT_LOCAL_AUDIT.md), keine Laufzeitänderung oder neue Spielabnahme |
| `client-visuell` | 4 | Client sieht XP-/Levelstand, XP-Bälle, Projektilbewegung und Airdrops wie der Host, ohne doppelte XP oder Rewards; Feinschliff in `opt-client-visuell`, siehe [Client-Darstellung](research/spikes/client-visuell/README.md) |
| `config-paks` | 1 | Config aus den Paks gelesen: `IpNetDriver`, `DefaultPlatformService=Steam`, Standard-Maps, siehe [Konfiguration](docs/GAME_ANALYSIS.md#konfiguration) |
| `controller-tausch` | 1 | Controller-Tausch nach dem Missionswechsel über `K2_OnSwapPlayerControllers` sofort in `SpawnedPlayers` abgeglichen, siehe [Spike](research/spikes/a-connect/README.md#controller-tausch-nach-dem-missionswechsel-controller-tausch) |
| `coop-ablauf` | 1 | P2 ist ein zweiter `ULocalPlayer` aus `SpawnLocalPlayer(1)`, 5 Stellen kommen ohne `LocalPlayer` nicht zurecht, siehe [Coop-Ablauf](docs/GAME_ANALYSIS.md#coop-ablauf) |
| `crash-reporter` | 1 | Mod und Spike lassen den Start von `CrashReportClient.exe` scheitern, `CrashGuard` bleibt erste Schicht, siehe [Crash-Reporter](research/spikes/a-connect/README.md#sperre-in-mod-und-spike-crash-reporter) |
| `deploy` | 2 | Deploy-Skript `tools/Deploy-Mod.ps1` |
| `desync-melder` | 4 | Zustandssumme über den Kontrollkanal, Abweichungen in Mod-Log und F8, im Spiel rund 636 s ohne Fehlalarm, absichtliche Inventarabweichung auf beiden Seiten erkannt |
| `dumper-fix` | 0 | Dumper-7-Fix liegt im eigenen Fork, eingebunden als Submodul `third_party/Dumper-7`, siehe [SDK-Erzeugung](docs/GAME_ANALYSIS.md#sdk-erzeugung-unter-ue-58) |
| `game-adapter` | 2 | Game-Thread-Takt am Spielfenster ruft `Drain` und die Adapter-Ticks; `Offsets.h` mit geschützten Lesezugriffen, beim Bauen gegen ein lokales Dumper-7-SDK geprüft; `WorldAdapter` als Muster liest Karte, Rolle und Spielerzahl. Mit Host und Client bis Tag 1 geprüft, [Muster](src/Game/README.md) |
| `gate-g1` | 1 | Am 04.11.2026 vom Nutzer verbindlich entschieden: Weg A (native UE-Replikation) als Grundlage, ergänzt durch eigenen Kontroll-/Sync-Kanal für XP, Währungen, Rewards, Pausen und UI; Weg B nur Rückfalloption. [Architekturentscheidung](docs/ARCHITECTURE.md#entscheidungen) |
| `gegner-pooling` | 1 | Tote Gegner werden zerstört, nicht gepoolt, der `PoolManager` betrifft Projektile und Effekte, siehe [Gegner-Spawning](docs/GAME_ANALYSIS.md#gegner-spawning-und-pooling) |
| `github-repo` | 0 | Öffentliches GitHub-Repo angelegt |
| `grundgeruest` | 2 | `Loader` als Proxy-DLL, `Core` mit Logging, Config, Hooks, Dispatcher und Versionsprüfung, mit Host und Client geprüft |
| `guidcache` | 1 | Blockaden-Autorität, NetGUID-Pfadlebensdauer und Sneak-Zielbereitschaft korrigiert; Test-Godmode für beide Spieler, Lauf bis Endlos-Tag 13 mit zwei Biomwechseln. Am 04.11.2026 vom Nutzer als abgeschlossen akzeptiert; vorerst keine weiteren Detailtests, neue konkrete Probleme erhalten ein neues TODO. [Nachweise und Testgrenzen](docs/GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache) |
| `hp-leisten` | 1 | Spike-Fix: Die Lebensleisten über den Figuren blieben beim Client auf dem Startwert, weil das Spiel `OnHealthChanged` nur beim Host auslöst. Jetzt ruft der Spike die Empfänger nach `OnRep_Health` und `OnRep_MaxHealth` auf, siehe [Lebensleisten](docs/GAME_ANALYSIS.md#lebensleisten-beim-client-hp-leisten) |
| `inventar-lokal` | 5 | Client bearbeitet sein Inventar in der Itemauswahl lokal, der Host übernimmt beim Bestätigen einen Edit oder lehnt ab; zwei Airdrops, Umsortieren und Rundenende bis Tag 2 ohne Absturz, Inventare gleich ([Befund](src/Game/README.md#inventar-lokal-bearbeiten)) |
| `inventar-transaktion` | 5 | Ein Itemsack gibt jedem Spieler das Item, der Host vergibt Kopien; Client-lokales Inventar als `inventar-lokal`, siehe [Itemsack für alle](src/Game/README.md#itemsack-für-alle-spieler) |
| `iris` | 1 | Zur Laufzeit ist die klassische Replikation mit `ActorChannel` aktiv, nicht Iris |
| `itemsack-absturz` | 4 | Itemsack-Vergabe nur beim Host; drei natürliche Client-Aufsammlungen ohne Absturz mit exakt +1 Item beim Remote-Spieler und gleichen Inventaren geprüft. [Befund und Abnahme](research/spikes/itemsack-absturz/README.md) |
| `kamera-code` | 4 | Gemeinsame Kamera beim Client nachgewiesen: Kamera bei Host und Client auch mit Netzprofil Internet höchstens 28 Einheiten auseinander, gleicher Zoom, Bewegungsgrenze stoppt beide Figuren symmetrisch am Bildrand, siehe [Gemeinsame Kamera in der Mod](docs/GAME_ANALYSIS.md#gemeinsame-kamera-in-der-mod-kamera-code) |
| `kamera-entscheidung` | 1 | Gemeinsame Kamera (Option A) mit ursprünglichem Zoom und Bewegungsbegrenzung, siehe [Kamera](docs/GAME_ANALYSIS.md#kamera-und-sichtbereich) |
| `kamera-viewtarget` | 1 | Spike-Fix: Der Client schaute nach Kartenwechseln aus der eigenen Figur, weil die Kamera am alten Controller landete. Jetzt nur der lokale Controller der aktuellen Welt, Prüfung jede Sekunde, siehe [Kamera](docs/GAME_ANALYSIS.md#viewtarget-beim-client-kamera-viewtarget) |
| `kampf` | 4 | Projektilgröße und -bewegung ab dem ersten Schuss über native Replikation, Treffer, Schaden und Tod beider Spieler mit 120 ms Ping geprüft, siehe [Kampf beim Client](docs/GAME_ANALYSIS.md#kampf-beim-client-kampf) |
| `karte-analyse` | 1 | Replizierte Kartenobjekte sind Child Actors der nicht replizierten Parzellen, siehe [Kartenobjekte](docs/GAME_ANALYSIS.md#karte-und-replizierte-kartenobjekte) |
| `karte-client` | 1 | Client erzeugt die Host-Karte ohne replizierte Duplikate (`MapGen.cpp`), siehe [Client erzeugt die Host-Karte](docs/GAME_ANALYSIS.md#client-erzeugt-die-host-karte-karte-client) |
| `karten-erzeugung` | 1 | Gleicher Run-Seed und gleiche Mission ergeben dieselbe Karte, siehe [Karte und Seed](docs/GAME_ANALYSIS.md#karte-und-seed) |
| `last` | 1 | Spike D: bei rund 85 Gegnern 47 KB/s vom Host, beide Instanzen 60 fps, nur Loopback, siehe [Last](docs/GAME_ANALYSIS.md#last-spike-d-last) |
| `latenz-test` | 4 | Netzsimulation im Tunnel der Mod (`-ddcoop_netsim`, `-NetProfile Internet`), Routenlauf bis Tag 1 mit Profil sauber, [Latenz simulieren](docs/TESTING.md#latenz-simulieren) |
| `layerapps-sperre` | 6 | Die Mod sperrt auch `deadlydays.layerapps.com`, das das Spiel beim Run-Ende anfragt (`Core::ReportHosts`). Run-Ende und neuer Run laufen normal, siehe [Telemetrie und Berichte](docs/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus) |
| `lobby-auth` | 3 | Host nimmt P2P nur von Steam bestätigten Mitgliedern seiner Lobby an, vor AcceptConnection, ohne Platz zu belegen; 127.0.0.1 nur im Testmodus; 10 Net-Tests mit nachgebautem Steam, auf einem PC im Spiel geprüft, Test mit zwei Konten ist `lobby-auth-live`, siehe [Zugriffskontrolle](src/Net/README.md#zugriffskontrolle-lobby-auth) |
| `lobby-bereit` | 3 | Host schließt die Lobby im Hauptmenü oder ohne Listen-Server, Client erfährt es sofort, siehe [Lobby nur mit Server](src/Net/README.md#lobby-nur-mit-server-lobby-bereit) |
| `menue-weg` | 1 | Mission über den echten Weg durch das Spielmenü mit den Patches gestartet, siehe [Spike](research/spikes/a-connect/README.md) |
| `missionsstart` | 4 | Gleiche Karte bei Host und Client über den Kontrollkanal der Mod, die Erzeugung des Clients wartet auf die Werte des Hosts; auf einem PC mit Netzprofil Internet und ohne Spike geprüft, siehe [Gleiche Karte in der Mod](docs/GAME_ANALYSIS.md#gleiche-karte-in-der-mod-missionsstart) |
| `mitwirkende` | 0 | Freunde als Mitwirkende eingeladen |
| `net-fehler` | 3 | Jedes Sitzungsende (Verlassen beider Seiten, Abbruch, Steam- und Sitzungszeitlimits) erscheint mit eigenem Grund als lesbare Meldung in Overlay und Log, danach sind Erstellen und Beitreten wieder frei; im Spiel geprüft, siehe [Fehlerbehandlung](src/Net/README.md#fehlerbehandlung-net-fehler) |
| `niederlage` | 1 | Nach einer Niederlage kehren Host und Client über „Zurück zur Basis“ in die `MetaBase` zurück, und ein neuer Run startet; der Host-Absturz im Run-Ende-Handler ohne `LocalPlayer` ist im Spike behoben (`RunEnd.cpp`), siehe [Niederlage](docs/GAME_ANALYSIS.md#niederlage-niederlage) |
| `ohne-spike` | 4 | Mod läuft ohne Spike bis Mission Tag 1 und zurück zur Basis, Start-Session.ps1 -NoSpike, Liste der Spike-Eingriffe in src/Game/README.md |
| `online-sperren` | 6 | Die Mod sperrt die schreibenden Steam-Methoden für Erfolge, Statistiken und Bestenlisten (`Core::SteamStats`), lesen bleibt erlaubt, siehe [Erfolge und Bestenlisten](docs/GAME_ANALYSIS.md#erfolge-und-bestenlisten-online-sperren) |
| `overlay` | 2 | F8-Overlay mit Status und Debug-Anzeige, sichtbare Host-/Client-Abnahme bestanden, siehe [UI](src/UI/README.md#sichtbare-spielabnahme-03112026) |
| `overlay-fokus` | 3 | F8-Overlay bleibt beim Fokuswechsel sichtbar, Knopf „Lobby-ID kopieren“, siehe [UI](src/UI/README.md#stand-overlay) |
| `patches` | 4 | Die 25 Spielfehler-Fixes des Spikes stehen geprüft in `src/Game/*Patches.cpp`, alle oder keiner; mit der Mod lässt der Spike seine Fassungen aus. Route MetaBase bis Tag 5 und Niederlage mit Rückkehr in die MetaBase ohne Absturz, siehe [Patches](src/Game/README.md#patches) |
| `pausen` | 1 | Pausen-Logik gemessen, Host-Weltpause repliziert, Client-Wunsch nicht, siehe [Pausen-Spike](research/spikes/pausen/README.md#nachtrag-03112026-isolierte-ablaeufe) |
| `pcg` | 1 | Client erzeugt die PCG-Vegetation mit dem Graphen des Hosts (`Pcg.cpp`), siehe [PCG-Vegetation](docs/GAME_ANALYSIS.md#pcg-vegetation-beim-client-pcg) |
| `phasen-farbe` | Wartung | Vollständig abgeschlossene Phasen im README mit grünem Zeilenhintergrund in einer lokal erzeugten SVG; verlinkte Texttabelle bleibt aufklappbar. Generator, Konsistenzprüfung und Regressionstests halten Farben und Zahlen synchron. |
| `phasenadapter` | 1 | Client sieht Rundenübersicht, Lager und Routenkarte des Hosts und ist dort gesperrt, siehe [Rundenphasen](research/spikes/a-connect/RoundPhaseRuntime.md) |
| `powerup-duplikat` | 1 | Lokale Kopie von `BP_ChaosDestructable_PowerUp_C` beim Client unterdrückt, siehe [PowerUp-Objekte](docs/GAME_ANALYSIS.md#doppelte-powerup-objekte-powerup-duplikat) |
| `protokoll` | 3 | Handshake mit Spiel-Build, Mod-Version und Rolle sowie zuverlässiger Kontrollkanal im selben Steam-Socket wie die UE-Pakete, Nachrichten mit Run-ID, Epoche und Revision nach dem Vertrag; 24 Net-Tests, im Spiel bis Tag 1 mit XP-Anzeige darüber geprüft. Der Internet-Test mit zwei Konten gehört zu `lobby-live` bzw. `integration-g2`, siehe [Kontrollkanal](docs/CONTROL_PROTOCOL.md) |
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
| `solution` | 0 | Visual-Studio-Solution mit einem Projekt je Modul, `config/Local.props` eingebunden |
| `spike-a` | 1 | Spike A: Beide Instanzen kommen in dieselbe Welt, auch auf einem PC, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| `spike-b` | 1 | Spike B: Client stabil in der Mission, Bewegung beidseitig, Schüsse wirken beim Host, siehe [Ergebnis](research/spikes/a-connect/README.md) |
| `spike-c` | 1 | Spike C: Mit `karte-client` und `pcg` ist die Mission beim Client gleich, `check_round.py` zählt an Tag 1 bis 4 gleich viele `MapTile`s, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung) |
| `steam-lobby` | 3 | Steam-Lobby nur für Freunde direkt über Steamworks (`Net::Session`, `src/Mod/Lobby.cpp`, Overlay-Knöpfe); auf einem PC erstellt, per Lobby-ID beigetreten und bis Tag 1 gespielt, Test mit zwei Konten ist `lobby-live`, siehe [Steam-Lobby](src/Net/README.md#steam-lobby-steam-lobby) |
| `sync-bedarf` | 1 | Sync-Weg für XP, Währungen, Rewards, Pausen und UI als Entwurf festgelegt, siehe [Sync-Bedarf](docs/SYNC_REQUIREMENTS.md) |
| `tank-gemeinsam` | 5 | Tank am GameState ist nativ ein Gruppenwert: Kanister von Host und Client zählen einmal und gleich, voller Tank gibt die Busflucht frei, keine Code-Änderung nötig |
| `team-aufteilung` | 0 | Keine festen Bereiche je Person und kein `CODEOWNERS`, die Arbeit teilt sich über `tools/tasks.py` |
| `telemetrie-aus` | 6 | Die Mod schaltet Balance-Telemetrie und automatische Spielstand-Berichte ab und sperrt die Namensauflösung für `codecks.io` und `pixelsplit.games` (`Game::TelemetryAdapter`, `Core::ReportHosts`), siehe [Telemetrie und Berichte](docs/GAME_ANALYSIS.md#telemetrie-und-berichte-telemetrie-aus) |
| `test-tempo` | Wartung | Sitzungsstart wartet auf Zustände statt fest zu schlafen (Lobby 28 s statt 220 s, direkt 42 s statt 100 s), `-Wait`, Einzelneustart des Clients, Netzwerkzeuge mit `Test-NetEnd.ps1` (Fälle a bis e in 58 s) und `Guided-Test.ps1` für Tests mit Menschen, siehe [Spielzeit sparen](docs/TESTING.md#spielzeit-sparen) |
| `testaufbau` | 1 | Zwei Instanzen auf einem PC unter demselben Steam-Konto, je mit eigenem `-UserDir=` |
| `testumgebung` | 1 | Tag 1 bis 4 laden sauber mit Spawn am Bus und je 2 PlayerStates, Prüfskript und fester Routenlauf, siehe [Pausierte Welt und Busplatz](docs/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung) |
| `tick-absturz` | 1 | Restrictor-Absturz `0x5A17ACC` nachgestellt, Laufzeitfix ist `restrictor`, siehe [Restrictor](docs/GAME_ANALYSIS.md#client-absturz-im-restrictor-tick-absturz) |
| `token-sparen` | Wartung | `tasks.py show`/`done` und `-Brief` für die Testskripte, im Spiel geprüft, [Spielzeit sparen](docs/TESTING.md#spielzeit-sparen) |
| `transport` | 3 | Tunnel aus Spike E als Modul `src/Net/Transport` mit Tests ohne Spiel; im Spiel über den Tunnel der Mod bis Tag 1, Inventare gleich, siehe [Net](src/Net/README.md#transport-transport) |
| `treffer` | 1 | Projektile des Clients ziehen beim Host Leben ab und töten (`Hits.cpp`), siehe [Treffer](docs/GAME_ANALYSIS.md#treffer-des-clients-treffer) |
| `trennen` | 1 | Client-Absturz beim Trennen war eine Folge fehlender Inventar-Items, mit Teilfix kein Absturz, siehe [Trennen](docs/GAME_ANALYSIS.md#trennen-ins-hauptmenü-trennen) |
| `tunnel` | 1 | Der Spielverkehr läuft über `ISteamNetworkingSockets` aus der Steam-Laufzeit des Spiels, zwei Instanzen spielten so bis Tag 2 (`Tunnel.cpp`); P2P mit zwei Konten ist `tunnel-p2p`, siehe [Steam-Tunnel](research/spikes/a-connect/README.md#spike-e-steam-tunnel-tunnel) |
| `tunnel-p2p` | 1 | Zwei PCs mit zwei Steam-Konten spielten über den P2P-Tunnel bis in die Mission an Tag 1, 120 s ohne Absturz, Ping 41 ms im Median über ein Steam-Relay, nichts verworfen, siehe [Steam-Tunnel](research/spikes/a-connect/README.md#p2p-auf-zwei-pcs-tunnel-p2p) |
| `tutorial-aus` | 6 | Die Mod schaltet die Tutorial-Hinweise als Host und Client automatisch ab und prüft das jede Sekunde (`src/Game/TutorialAdapter`), siehe [Tutorial-Pause](docs/GAME_ANALYSIS.md#tutorial-pause-hält-den-run-an-testumgebung) |
| `update-regel` | 0 | Spiel-Updates: einfrieren und nur bei Bedarf nachziehen, siehe [Update-Regel](CONTRIBUTING.md#spiel-updates) |
| `versions-meldung` | Wartung | Abgelehnte erste Nachricht nennt den Grund mit beiden Werten (alte Mod, Protokoll, Build, Mod-Version, Rolle), siehe [Handshake](docs/CONTROL_PROTOCOL.md#handshake) |
| `weeklymode` | 5 | Team-Entscheidung vom 05.11.2026: Die Weekly-Challenge wird nicht unterstützt und ist gesperrt, solange die Mod geladen ist, auch solo, wie Erfolge und Bestenlisten. Umsetzung ist `weekly-sperre` |
| `xp-baelle` | 5 | Client-Sichtgrenzen der XP-Bälle aus der lokalen Kamera statt aus dem fehlenden GameMode, Live-Gegenprobe in `xp-baelle-live`, siehe [XP-Bälle](docs/GAME_ANALYSIS.md#xp-bälle-beim-client-xp-baelle) |
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
