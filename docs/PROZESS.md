# Arbeitsprozess

Wie Aufgaben gelesen, getestet und abgeschlossen werden. Die Aufgaben selbst stehen in [TODO.md](../TODO.md).

## So liest man die Tabellen

| Spalte | Bedeutung |
|---|---|
| **Kennung** | Eindeutiger Name, zugleich Branch `task/<kennung>` und Worktree `.worktrees/<kennung>` |
| **Prio** | **P0** blockiert andere Aufgaben oder den Run, **P1** nötig für das nächste Gate oder den Release, **P2** wichtig, aber später |
| **Aufgabe** | Was zu tun ist, **Fertig, wenn** nennt die Abnahme, **Einstieg** die Unterlagen |
| **Braucht** | Kennungen, die vorher erledigt sein müssen. `–` heißt keine Voraussetzung |
| **Bearbeitung** | **parallel**: läuft neben anderen Aufgaben. **zentral**: legt gemeinsame Schnittstellen fest oder lässt sich nur als Ganzes testen, immer nur eine Person, nie aufteilen. **zu zweit**: Test auf zwei PCs, zwei Personen dürfen je eine Rolle `host` oder `client` reservieren, siehe [Test auf zwei PCs](TESTING.md#test-auf-zwei-pcs). **Team**: Entscheidung oder Abnahme durch die Menschen im Team, keine Reservierung |
| **Spielzeit** | Wie sehr die Aufgabe Testsitzungen braucht: keine, wenig, mittel, viel. Pro PC läuft nur eine Sitzung |

Daraus ergibt sich der Status, den `py tools/tasks.py list` anzeigt:

| Status | Bedeutung |
|---|---|
| frei | Alle Voraussetzungen erledigt, niemand hat reserviert. Darf genommen werden |
| reserviert, in Arbeit | Branch oder Draft-PR existiert. Nicht nehmen |
| blockiert | Mindestens eine Kennung aus „Braucht“ ist offen. `tasks.py claim` lehnt ab |
| zurückgestellt | Optionale Aufgabe, solange eine Aufgabe aus Phase 0 bis 5 offen ist. `tasks.py claim` lehnt ab |
| Team | Entscheidet das Team, `tasks.py claim` lehnt ab |
| erledigt | Steht unter [Erledigt](ERLEDIGT.md#erledigt). Nie erneut reservieren, auch wenn der Branch gelöscht ist oder die GitHub CLI fehlt |

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
über einen PR. [Befehle und Grenzen](../tools/README.md#bestätigungstests).

- **Offene Tests sammeln.** Bleibt nur noch ein Test zu zweit, kann er nach [Aufgabe abschließen](#aufgabe-abschließen), Punkt 3,
  als eigene Kennung weitergeführt und hier ergänzt werden. Bestehende Abnahmen und Abhängigkeiten bleiben dabei erhalten.
- **Gemeinsam vorbereiten.** Ein gemeinsamer Stand auf `main` und derselbe Build erleichtern den Termin. Ablauf und Aufbau stehen
  in [Testen](TESTING.md#test-auf-zwei-pcs). Nach dem Verbinden lassen sich passende Punkte oft ohne Neustart kombinieren.
- **Hinweis zur Reservierung.** `host` und `client` sind nur für Aufgaben mit Bearbeitung „zu zweit“ vorgesehen. Zentrale Aufgaben
  behalten einen Verantwortlichen ohne Rolle; die zweite Person hilft beim Test. Für eine Gegenprobe erledigter Arbeit ist keine
  neue Reservierung nötig.
- **Ergebnisse zuordnen.** Ein kurzer Vermerk je Kennung hilft beim Überblick; offene Prüfungen können in einen späteren Termin.

Mögliche Punkte und Reihenfolge, je nach Bereitschaft. `Start-Session.ps1 -Role Client -Mod -Tunnel Lobby -NoSpike` verbindet
die beiden wie in den bisherigen Terminen.

1. Alle Bestätigungstests aus `py tools/tasks.py tests plan` in einem Lauf. Neue Aufgaben mit Live-Rest dort ergänzen statt einen
   eigenen Termin anzusetzen.
2. Die Punkte aus [Live-Bestätigung ausstehend](#live-bestätigung-ausstehend) nebenbei im selben Lauf prüfen.
3. `rundenende-1-0-2`, Gegenprobe auf `26669225`: Host bestätigt jede Airdrop-Auswahl und das Lager am Rundenende mit einem Klick, auch nach einem Lager-Edit des Clients mit Aufwertung und Verkauf; Host-Log aufheben. Auf einem PC bestanden, siehe [Rundenende auf 26669225](archive/OFFSETS-ALTE-BUILDS.md#rundenende-auf-26669225-rundenende-1-0-2).
4. `integration-run`: kompletter Run über das Internet, Phase 5, sobald die Voraussetzungen erledigt sind.
5. `release`: Abschlusstest vor der Veröffentlichung.

Die Entscheidung `meta-fortschritt` ist getroffen: Im gemeinsamen Run gilt der Host-Stand,
ohne dauerhafte Meta-Gutschrift beim Client, siehe [Regel](SYNC_REQUIREMENTS.md#nutzervorgabe-meta-fortschritt-des-hosts-06112026).

**Ohne zweite Person.** Alles andere geht allein auf einem PC: zwei Instanzen unter einem Konto über 127.0.0.1, siehe
[Sitzung starten](TESTING.md#sitzung-starten). Das gilt für die meisten Aufgaben mit Spielzeit; zu zweit sind nur Aufgaben
mit Bearbeitung „zu zweit“ und Abnahmen, die ausdrücklich zwei PCs oder das Internet verlangen. Die jeweiligen Abnahmen gelten weiterhin.

## Live-Bestätigung ausstehend

Erledigte Aufgaben, deren Abnahme bisher nur auf einem PC (zwei Instanzen) belegt ist. Sie bleiben unter [Erledigt](ERLEDIGT.md#erledigt)
und zählen als abgeschlossen. In der nächsten Live-Sitzung zu zweit mit Analyse nebenbei prüfen, siehe
[Live-Sitzungen zu zweit](#live-sitzungen-zu-zweit). Bestätigt: Zeile löschen, Ergebnis im Bericht der Sitzung festhalten.
Fällt ein Punkt durch: neue Aufgabe in der passenden Phase anlegen und die Zeile löschen. Wer eine Aufgabe nur mit
Ein-PC-Beleg abschließt, trägt sie hier ein ([E-06](ENTSCHEIDUNGEN.md#e-06-abschluss-mit-ein-pc-beleg)).

| Aufgabe | Belegt | In der Live-Sitzung prüfen |
|---|---|---|
| `boss-karte-absturz` | 21.11., ein PC, `26786272`, Routenkarte mit Testschalter, auch mit simulierter Latenz | Client betritt die Boss-Karte am Ende der Route ohne Absturz; im Client-Log genau eine Zeile „Boss-Leiste: lokal angelegt“ je Eintritt, Leiste folgt den Boss-HP |
| `charakter-wahl` | 10.11., ein PC | Jeder wählt in der MetaBase eine andere Figur, beide sehen die Wahl des anderen; nach Reise und Startausrüstung im Run |
| `client-runstand-abbruch` | 18.11., ein PC, `26495302` und `26669225` | Nach einem gemeinsamen Run (Client verlässt ihn über das Pausemenü oder der Host beendet) hostet der frühere Client: neuer Run mit Level 1 und Startausrüstung; ein eigener offener Run des Clients lässt sich danach fortsetzen |
| `lager-handel-client` | Verkauf/Reparatur 13.11. zwei PCs, Ausrüsten 13.11. ein PC | Client repariert im Lager ein kaputtes Item, legt es in den Rucksack, bestätigt; am nächsten Tag ausgerüstet |
| `opt-ausdauerbalken` | ein PC | Client sieht den Ausdauerbalken des Hosts beim Sprinten wie der Host |
| `opt-blickrichtung-client` | ein PC | Beide Figuren schauen beim Client in Lauf- und Zielrichtung |
| `opt-busbalken` | ein PC; 13.11. abends zwei PCs einmal fehlend gemeldet, später im Log abgeleitet | Ladebalken der Busabfahrt beim Client wie beim Host, bei jedem vollen Tank |
| `opt-charakter-takt` | 11.11., ein PC | Keine Zeile „Takt: Charakterwahl“, Wahl bleibt nach Reise und erneutem Beitritt |
| `opt-item-sound` | ein PC | Client hört Aufsammelsounds (Benzin, Schrott, Items) je Objekt einmal |
| `opt-laufanimation-client` | ein PC | Laufanimation der Client-Figur beim Host passt zum Tempo |
| `opt-levelup-sound` | ein PC | Kein Schrott-Dauersound beim Client, Level-up-Kiste im Endbildschirm ohne Soundfehler |
| `opt-schadenszahlen` | ein PC | Client sieht Schadenszahlen eigener und fremder Treffer |
| `opt-schrott-anzeige` | ein PC, zwei Tage | Schrottanzeige des Clients springt beim Aufsammeln nicht |
| `opt-gegner-sound` | 21.11., ein PC | Client hört Treffer- und Angriffssounds der Gegner etwa so oft wie der Host, ohne Doppel |
| `opt-bedrohung-sound` | 21.11., ein PC | Client hört den Sound bei jeder steigenden Bedrohungsstufe einmal, Zahl unter der Stoppuhr wie beim Host |
| `opt-airdrop-sound` | 21.11., ein PC | Client hört bei jeder Airdrop-Landung den Landesound einmal, passend zum Aufschlag |
| `opt-waffen-sound` | ein PC | Client hört Schuss- und Nachladesounds beider Figuren |
| `run-ende` | ein PC; Sieg und Rückkehr 13.11. abends zwei PCs | Niederlage beim Client nativ, neuer Run in derselben Sitzung |

## Aufgabe abschließen

1. Ergebnis und Testgrenzen in der Doku festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](TESTING.md#ergebnisse-festhalten).
2. Nur bei erfüllter Abnahme („Fertig, wenn“): die Zeile aus der Phasentabelle löschen und unter [Erledigt](ERLEDIGT.md#erledigt) alphabetisch einfügen, als `` | `kennung` | Phase | Ergebnis in einem Satz mit Link | ``. Die Phase ist eine Ziffer von 0 bis 6, `Wartung`, `Optional` oder `Coop` (für [Real Coop logic](../TODO.md#real-coop-logic)).
3. Teilergebnis: Verhindert der Rest den Run zu zweit, bleibt die Zeile offen: den Text auf den Rest kürzen oder den Rest als neue Kennung in dieselbe Phase eintragen und die alte Zeile nach „Erledigt“ ([docs/ERLEDIGT.md](ERLEDIGT.md)) verschieben. Sonst die Zeile nach „Erledigt“ verschieben und den Rest als Grenze im Ergebnis nennen, ohne neue Kennung. Wer eine neue Kennung anlegt, trägt sie bei allen Aufgaben, die darauf warten, unter „Braucht“ ein.
   Ist die Abnahme nur auf einem PC belegt, obwohl sie zu zweit live gilt, zusätzlich eine Zeile unter [Live-Bestätigung ausstehend](#live-bestätigung-ausstehend) eintragen.
4. `py tools/task_status.py update` ausführen. Das erneuert „Wo kann ich anfangen?“ und den Fortschritt im README. Danach muss `py tools/task_status.py check` bestehen.
5. [docs/ROADMAP.md](ROADMAP.md) nur ändern, wenn eine ganze Phase erledigt ist (Knoten umfärben, Abschnitt „Stand“ ersetzen).

Nur die eigenen Zeilen ändern, das hält Konflikte klein. Ein gemergter Teil-PR schließt keine Aufgabe ab.

## Vorlagen

**Aufgabenzeile** in `TODO.md`, Spalte „Aufgabe“ höchstens 800 Zeichen (prüft `task_status.py`):
`<Ziel in einem Satz>. **Fertig, wenn** <ein prüfbarer Satz>. **Einstieg** [Fallakte](docs/faelle/<kennung>.md)`.

**Fallakte** `docs/faelle/<kennung>.md` für einen offenen Fehler, höchstens 15 000 Bytes.
Sie wird mit der Abnahme gelöscht, das Ergebnis kommt als ein Satz nach [docs/ERLEDIGT.md](ERLEDIGT.md).

```markdown
# <kennung>: <Symptom in einem Satz>

## Reproduktion
## Belege            (nur [Belegt] mit Log-Auszug oder Messung, Build 26786272)
## Hypothesen        (je Zeile [Vermutung] oder [Widerlegt], mit Prüfidee)
## Nächster Schritt
## Abnahme
```

## Regeln für Doku und Abschluss

- Doku beschreibt den Ist-Zustand auf dem unterstützten Build: ersetzen statt anhängen, keine datierten Befundabschnitte.
- Befunde und Messungen gehören in die PR-Beschreibung und die Fallakte, nicht in Themen-Doku.
- Jeder behobene Absturz bekommt einen Unit-Test der zuständigen Policy oder ein Szenario für `Run-Route.ps1`.
- Höchstens 3 Bestätigungstests stehen offen.
- Eine Sitzung pro Aufgabe. Breite Suchen übernimmt ein Explore-Subagent, der nur das Ergebnis zurückgibt.
- Dateien über 60 KB, das Archiv und `docs/ERLEDIGT.md` nie ganz lesen, nur per `grep -n` und Abschnitt.
- Neue Doku-Zeilen: ein Satz pro Zeile, keine Zeile über 400 Zeichen.

## Fokus

Erst spielbar: Vorrang hat, dass zwei Spieler einen Run durchspielen können. Aufgaben unter
[Optionale Optimierungen](../TODO.md#optionale-optimierungen) warten, bis Phase 0 bis 5 erledigt sind; bis dahin zeigt
`tasks.py` sie als `zurückgestellt` und `claim` lehnt sie ab. Vorziehen entscheidet der Mensch, indem die Zeile in eine
Phase oder nach Wartung wandert.

**Weniger neue Aufgaben** (Nutzervorgabe vom 21.11.2026, bisher entstand mehr Arbeit, als erledigt wurde):

- Einen Fund mit derselben Ursache oder in denselben Dateien wie die eigene Aufgabe im selben PR beheben, ohne neue
  Kennung. Vorher mit `py tools/tasks.py list` prüfen, dass keine Kennung, Reservierung oder offener PR ihn schon abdeckt.
- Eine neue Kennung nur für einen Fund oder Rest, der den Run zu zweit verhindert (P0 oder P1 in der passenden Phase),
  oder auf Anweisung des Menschen.
- Alles andere bekommt keine Zeile in `TODO.md`: Fund und Messung stehen im Befund der eigenen Doku und in der
  PR-Beschreibung, ein Rest der eigenen Aufgabe als Grenze im Ergebnis. Keine gemeinsame Sammelliste anlegen.

## Vor dem Start

1. [Wo kann ich anfangen?](../TODO.md#wo-kann-ich-anfangen) lesen, auch die Liste „Nicht erneut machen“. Jede Aufgabe nennt unter „Braucht“ ihre Voraussetzungen und unter „Bearbeitung“, ob sie parallel läuft, zentral ist (nie aufteilen) oder das Team entscheidet.
2. Status aller Aufgaben ansehen, Einzelheiten einer Aufgabe mit `py tools/tasks.py show <kennung>` statt der ganzen `TODO.md`. Nur Aufgaben mit Status `frei` nehmen, `blockiert` und `Team` lehnt das Skript ab:

   ```bash
   py tools/tasks.py list
   ```

3. Aufgabe reservieren. Das legt den Worktree `.worktrees/<kennung>` und den Branch `task/<kennung>` an, pusht ihn und öffnet einen Draft-PR `[WIP] <kennung>`, an dem alle auf GitHub sehen, wer woran arbeitet. Lehnt das Skript ab, hat jemand anderes die Aufgabe gerade genommen, dann eine andere wählen:

   ```bash
   py tools/tasks.py claim <kennung> <eigener-name>
   ```

Bei Bearbeitung „zu zweit“ (Test auf zwei PCs) reserviert jede Person nur ihre Rolle: `claim <kennung> <name> host` oder `client`, ein freier Rest steht dann als `client frei` oder `host frei` in der Liste, siehe [Test auf zwei PCs](TESTING.md#test-auf-zwei-pcs).

Nie von Hand reservieren und nie eine Entwicklungsaufgabe übernehmen, die nicht `frei` ist. **Bestätigungstest ausstehend** ist ein eigener Wartezustand, nicht „in Arbeit“ und nicht erledigt: solche Tests mit `py tools/tasks.py tests plan` bündeln, ohne die getesteten Aufgaben neu zu reservieren. Teststände und belegte Ergebnisse im eigenen Wartungs-Worktree über `tests update` bzw. `tests pass` aktualisieren; aktive fremde Aufgaben bleiben ausgeschlossen. Siehe [Bestätigungstests](../tools/README.md#bestätigungstests). Steht dort „erst nachfragen“, den Menschen zur Reservierung oder dem gemergten Teilstand fragen. Neue Arbeit bekommt vorher eine eigene Zeile mit Kennung in der passenden Phase, nur in den Grenzen unter [Fokus](#fokus).

Worktrees liegen immer unter `.worktrees/` im Repo-Ordner, keine neuen Ordner daneben anlegen. Im eigenen Worktree arbeiten, nie im Hauptordner oder im Worktree eines anderen Agenten. Wer die Reservierung aufgibt, ruft `py tools/tasks.py release <kennung>` auf. Das schließt den Draft-PR und löscht den Branch.

## Beim Arbeiten

- Bei Spieltests gilt: **eine Sitzung pro PC.** `Start-Session.ps1` bricht ab, wenn eine andere läuft. Dann mit `-Wait <Sekunden>` einreihen oder ohne Spiel weiterarbeiten, nie fremde Instanzen beenden. Spieltests bündeln und Skripte mit `-Brief` aufrufen, Bilder nur bei Bedarf ansehen, siehe [Spielzeit sparen](TESTING.md#spielzeit-sparen).
- Die Exe nie von Hand mit dem Spike starten und nie `-unattended` verwenden, sonst können Absturzberichte bei Pixelsplit landen, siehe [Testen](TESTING.md).
- Im Spike nur die Datei des eigenen Themas ändern, die Aufteilung steht in der [Spike-README](../research/spikes/a-connect/README.md#dateien).
- Tests, die zwei Menschen live brauchen, lassen sich in einem Termin bündeln. Hinweise und mögliche Punkte stehen unter [Live-Sitzungen zu zweit](PROZESS.md#live-sitzungen-zu-zweit); daraus entsteht keine zusätzliche Pflicht.
- Fehlt nach einem integrierten Teilstand nur noch die Prüfung, den Stand mit `py tools/tasks.py tests pending <kennung> --note "Stand und offene Abnahme"` kennzeichnen. Diagnose-only darf so auf eine Bestätigung warten, gilt aber nicht als behobener Fehler. Merge und Branch-Löschung ersetzen keinen Test. `py tools/tasks.py list --active` lässt Bestätigungstests aus.
- Neue Kontrollnachricht oder anderes Format: Typ zuerst in [Belegte Nachrichtentypen](CONTROL_PROTOCOL.md#belegte-nachrichtentypen) eintragen und `ModVersion` erhöhen. Ist `ModVersion` auf `main` seit dem Abzweig gestiegen, vor dem Merge darüber erhöhen.
- Offsets gelten nur für den unterstützten Build. Neue Werte in [docs/OFFSETS.md](OFFSETS.md) eintragen.
- Spieldateien, SDKs, decompilierter Code, persönliche Pfade und IDs gehören nie ins Repo, siehe [CONTRIBUTING.md](../CONTRIBUTING.md#was-nie-ins-repo-darf). Das Repo ist öffentlich.

## Abschluss

1. Ergebnisse festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](TESTING.md#ergebnisse-festhalten).
2. Abschließen wie in [Aufgabe abschließen](PROZESS.md#aufgabe-abschließen): nur bei erfüllter Abnahme die eigene Zeile nach „Erledigt“ in [docs/ERLEDIGT.md](ERLEDIGT.md) verschieben, alphabetisch, am einfachsten mit `py tools/tasks.py done <kennung> "<Ergebnis>"`. Ein Teil-PR reicht nicht. In `TODO.md` nur die eigenen Zeilen ändern, das hält Konflikte klein. Danach `py tools/task_status.py update` (macht `done` selbst); `py tools/task_status.py check` muss vor dem PR bestehen. [docs/ROADMAP.md](ROADMAP.md) nur ändern, wenn eine ganze Phase erledigt ist: Das Ergebnis steht unter „Erledigt“, eine zweite Kopie erzeugt nur Merge-Konflikte. Befunde gehören unter eine eigene Überschrift mit der Kennung im Thema der Doku, in fremde Abschnitte und Live-Test-Berichte höchstens eine Zeile mit Link, siehe [Ergebnisse festhalten](TESTING.md#ergebnisse-festhalten). Vor „Ready for review“ `main` einmergen und `py tools/task_status.py update` erneut ausführen. Nach dem Merge räumt `py tools/tasks.py cleanup` verwaiste Worktrees auf. Branch-Löschung oder fehlende GitHub CLI dürfen nie als „Aufgabe frei“ interpretiert werden.
3. Den Draft-PR fertigstellen, keinen zweiten öffnen: Titel und Beschreibung ersetzen, dabei sagen, wie getestet wurde, dann auf „Ready for review“ stellen. Nur auf ausdrückliche Anweisung des Menschen mergen. Die Reihenfolge beim Mergen schlägt `py tools/tasks.py merge-plan` vor; steht der eigene PR dort unter „Erst nachziehen“, `main` einmergen und `py tools/task_status.py update` ausführen.

   ```bash
   gh pr edit task/<kennung> --title "<kennung>: <Ergebnis>" --body-file <datei>
   gh pr ready task/<kennung>
   ```
