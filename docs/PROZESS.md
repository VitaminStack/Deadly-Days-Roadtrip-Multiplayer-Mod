# Arbeitsprozess

Wie Aufgaben gelesen, getestet und abgeschlossen werden. Die Aufgaben selbst stehen in [TODO.md](../TODO.md).

## So liest man die Tabellen

`TODO.md` hat zwei Tabellen: **Probleme** (Fehler) und **Verbesserungen**. Phasen und Abhängigkeiten gibt es nicht mehr ([E-13](ENTSCHEIDUNGEN.md#e-13-phasen-abgeschlossen-flache-aufgabenliste)).

| Spalte | Bedeutung |
|---|---|
| **Kennung** | Eindeutiger Name, zugleich Branch `task/<kennung>` und Worktree `.worktrees/<kennung>` |
| **Prio** | **P0** stört den Run zu zweit oder den Release, **P1** wichtig, **P2** später |
| **Aufgabe** | Was zu tun ist, **Fertig, wenn** nennt die Abnahme, **Einstieg** die Unterlagen |
| **Bearbeitung** | **parallel**: läuft neben anderen Aufgaben. **zentral**: legt gemeinsame Schnittstellen fest, immer nur eine Person, nie aufteilen. **zu zweit**: Test auf zwei PCs, je eine Rolle. **Team**: Entscheidung des Teams |
| **Spielzeit** | Wie sehr die Aufgabe Testsitzungen braucht: keine, wenig, mittel, viel. Pro PC läuft nur eine Sitzung |

Daraus ergibt sich der Status, den `py tools/tasks.py list` anzeigt:

| Status | Bedeutung |
|---|---|
| frei | Niemand hat reserviert. Darf genommen werden |
| reserviert, in Arbeit | Branch oder Draft-PR existiert. Nicht nehmen |
| Team | Entscheidet das Team, `tasks.py claim` lehnt ab |
| erledigt | Steht unter [Erledigt](ERLEDIGT.md#erledigt). Nie erneut reservieren, auch wenn der Branch gelöscht ist oder die GitHub CLI fehlt |

## Nicht erneut machen

Das ist erledigt und dokumentiert:

- Iris oder klassisch: klassische Replikation, siehe [Spike](../research/spikes/a-connect/README.md#ergebnis)
- Verbindung, Spawn, Bewegung und Schießen laufen mit den Patches, siehe [Spike](../research/spikes/a-connect/README.md#patches)
- Weg vom Menü in die Mission und über die Route bis Tag 4, siehe [Run-Ablauf](themen/REISE_UND_RUNDE.md) und [Testen](TESTING.md#fester-routenlauf)
- Coop-Ablauf, Karten-Erzeugung, Gegner-Pooling, Kamera, Pausen und Sync-Bedarf sind analysiert, siehe [Spielanalyse](themen/README.md) und [Sync-Bedarf](SYNC_REQUIREMENTS.md)
- Weitere Einzelprüfungen in den Inventar-Schleifen: Nach jeder stürzt der Client an der nächsten Stelle ab
- Eine höhere `NetPriority` des `GameState` hilft nicht gegen den HUD-Hänger, `OnRep_Slots` zurückzuhalten hilft nicht beim Inventar
- Gegen fehlende PlayerStates keine Klassen im RootSet pinnen oder Read-only-Zeitstempel blind zurücksetzen. Der abgeschlossene `guidcache`-Fix erhält gezielt nachladbare Pfadidentitäten und prüft den Cache-eigenen Driver auch bei nullem `GWorld`. Eine `servertravel`-Schleife ersetzt den echten Routenlauf nicht; [Nachweise und akzeptierter Abschluss](archive/GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache). Run/Endlos bis Tag 13 mit zwei Biomwechseln ist vorerst ausreichend abgesichert; nur neue konkrete Fehler bekommen ein neues TODO.
- Pause ab Tag 2 und fehlende Client-Figur am Bus sind im Spike behoben. Bei seltsamem Laden zuerst `Run-Route.ps1` und `check_round.py` prüfen, siehe [Pausierte Welt und Busplatz](archive/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung)

## Gemeinsame Dateien

Diese Dateien nutzen alle. Sie bleiben klein und ändern sich nur in kurzen eigenen PRs:

- `research/spikes/a-connect/Spike.h` und `Main.cpp`
- Öffentliche Schnittstellen in `src/Core` und `src/Net`
- Protokollversion und Nachrichtenformat, nur über `protokoll`

## Live-Sitzungen zu zweit

Manche Abnahmen brauchen zwei Menschen gleichzeitig: zwei Steam-Konten, zwei PCs, echtes Internet. Solche Termine sind knapp.
Es bietet sich an, diese Prüfungen zu sammeln und **in einer Sitzung nacheinander abzuarbeiten**, statt für jede Aufgabe einen
eigenen Termin anzusetzen. Die Liste unten ist eine Planungshilfe, keine zusätzliche Pflicht oder neue Freigaberegel.

**Bestätigungstest ausstehend** ist nur Info ([E-12](ENTSCHEIDUNGEN.md#e-12-bestätigungstests-sind-nur-info)): Die Aufgabe ist erledigt, die offene Prüfung steht unter [Bestätigungstests ausstehend](ERLEDIGT.md#bestätigungstests-ausstehend) und blockiert nichts.
Mit `py tools/tasks.py tests plan` lassen sich die Prüfungen für einen Termin bündeln, `tests update|pass|reopen` pflegt sie.

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
3. Offene [Probleme](../TODO.md#probleme) mit Spielzeit nebenbei nachstellen und Logs mit `Collect-Logs.ps1` sichern.

Die Entscheidung `meta-fortschritt` ist getroffen: Im gemeinsamen Run gilt der Host-Stand,
ohne dauerhafte Meta-Gutschrift beim Client, siehe [Regel](SYNC_REQUIREMENTS.md#nutzervorgabe-meta-fortschritt-des-hosts-06112026).

**Ohne zweite Person.** Alles andere geht allein auf einem PC: zwei Instanzen unter einem Konto über 127.0.0.1, siehe
[Sitzung starten](TESTING.md#sitzung-starten). Das gilt für die meisten Aufgaben mit Spielzeit; zu zweit sind nur Aufgaben
mit Bearbeitung „zu zweit“ und Abnahmen, die ausdrücklich zwei PCs oder das Internet verlangen. Die jeweiligen Abnahmen gelten weiterhin.

## Live-Bestätigung ausstehend

Offene Prüfungen erledigter Aufgaben stehen als Info unter [Bestätigungstests ausstehend](ERLEDIGT.md#bestätigungstests-ausstehend). Sie blockieren nichts ([E-12](ENTSCHEIDUNGEN.md#e-12-bestätigungstests-sind-nur-info)). In der nächsten Live-Sitzung prüfen und mit `py tools/tasks.py tests pass|reopen` abschließen.

## Aufgabe abschließen

1. Ergebnis und Testgrenzen in der Doku festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](TESTING.md#ergebnisse-festhalten).
2. Nur bei erfüllter Abnahme („Fertig, wenn“): die Zeile aus ihrer Tabelle löschen und unter [Erledigt](ERLEDIGT.md#erledigt) alphabetisch einfügen, als `` | `kennung` | Phase | Ergebnis in einem Satz mit Link | ``. Die Phase ist eine Ziffer von 0 bis 6, `Wartung`, `Optional` oder `Coop` (für [Real Coop logic](../TODO.md#verbesserungen)).
3. Teilergebnis: Verhindert der Rest den Run zu zweit, bleibt die Zeile offen: den Text auf den Rest kürzen oder den Rest als neue Kennung unter Probleme oder Verbesserungen eintragen und die alte Zeile nach „Erledigt“ ([docs/ERLEDIGT.md](ERLEDIGT.md)) verschieben. Sonst die Zeile nach „Erledigt“ verschieben und den Rest als Grenze im Ergebnis nennen, ohne neue Kennung. Wer eine neue Kennung anlegt, trägt sie bei allen Aufgaben, die darauf warten, unter „Braucht“ ein.
   Ist die Abnahme nur auf einem PC belegt, obwohl sie zu zweit live gilt, zusätzlich eine Zeile unter [Live-Bestätigung ausstehend](#live-bestätigung-ausstehend) eintragen.
4. `py tools/task_status.py update` ausführen. Das erneuert „Wo kann ich anfangen?“ und den Fortschritt im README. Danach muss `py tools/task_status.py check` bestehen.
5. [docs/ROADMAP.md](ROADMAP.md) nur ändern, wenn eine ganze Phase erledigt ist (Knoten umfärben, Abschnitt „Stand“ ersetzen).

Nur die eigenen Zeilen ändern, das hält Konflikte klein. Ein gemergter Teil-PR schließt keine Aufgabe ab.

## Vorlagen

**Aufgabenzeile** in `TODO.md`, Spalte „Aufgabe“ höchstens 800 Zeichen und mit „Fertig, wenn“ (beides prüft `task_status.py`):
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

## Projekt-Skills

Wiederkehrende Abläufe liegen als Skills unter `.claude/skills/`. Claude Code lädt sie nur, wenn sie passen:

- `fehler-untersuchen`: Fallakte, Belege, Log-Kurzfassung, Regressionstest.
- `live-test-auswerten`: Logs sammeln, Bestätigungstests abschließen, Ist-Zustand anpassen.
- `patch-anlegen`: Prüfliste, Policy mit Test, Hook, Prüfbytes, Offsets, Kontrollnachricht.

## Regeln für Doku und Abschluss

- Doku beschreibt den Ist-Zustand auf dem unterstützten Build: ersetzen statt anhängen, keine datierten Befundabschnitte.
- Befunde und Messungen gehören in die PR-Beschreibung und die Fallakte, nicht in Themen-Doku.
- Jeder behobene Absturz bekommt einen Unit-Test der zuständigen Policy oder ein Szenario für `Run-Route.ps1`.
- Bestätigungstests sind nur Info und blockieren nichts.
- Eine Sitzung pro Aufgabe. Breite Suchen übernimmt ein Explore-Subagent, der nur das Ergebnis zurückgibt.
- Dateien über 60 KB, das Archiv und `docs/ERLEDIGT.md` nie ganz lesen, nur per `grep -n` und Abschnitt.
- Neue Doku-Zeilen: ein Satz pro Zeile, keine Zeile über 400 Zeichen.
- Bei jedem Release wandert `docs/ERLEDIGT.md` als `docs/archive/ERLEDIGT-<version>.md` ins Archiv; die neue Datei behält nur den Abschnitt „Bestätigungstests ausstehend“.

## Fokus

Abschließen statt erweitern: Die Liste soll kleiner werden ([E-13](ENTSCHEIDUNGEN.md#e-13-phasen-abgeschlossen-flache-aufgabenliste)).
[Wo kann ich anfangen?](../TODO.md#wo-kann-ich-anfangen) und `py tools/tasks.py list` zeigen die Reihenfolge: nach Prio, bei gleicher Prio Probleme vor Verbesserungen, dann wenig Spielzeit vor viel und parallel vor zentral.
Die oberste freie Zeile ist die nächste sinnvolle Aufgabe; Aufgaben mit „parallel“ laufen in mehreren Sitzungen gleichzeitig.

- Jedes gefundene Problem und jede sinnvolle Verbesserung bekommt eine Zeile in `TODO.md` unter „Probleme“ oder „Verbesserungen“, vorher mit `py tools/tasks.py list` prüfen, dass es sie noch nicht gibt.
- Überschneidet sich ein Fund mit einer offenen Zeile, deren Text erweitern statt eine zweite Zeile anzulegen.
- Einen Fund mit derselben Ursache oder in denselben Dateien wie die eigene Aufgabe im selben PR beheben, dann ohne eigene Zeile.
- Keine Abhängigkeiten zwischen Aufgaben: Wer etwas braucht, das noch fehlt, schreibt es in den Aufgabentext.

## Vor dem Start

1. [Wo kann ich anfangen?](../TODO.md#wo-kann-ich-anfangen) lesen, dazu [Nicht erneut machen](#nicht-erneut-machen). Jede Aufgabe nennt unter „Bearbeitung“, ob sie parallel läuft, zentral ist (nie aufteilen) oder das Team entscheidet.
2. Status aller Aufgaben ansehen, Einzelheiten einer Aufgabe mit `py tools/tasks.py show <kennung>` statt der ganzen `TODO.md`. Nur Aufgaben mit Status `frei` nehmen, `Team` lehnt das Skript ab:

   ```bash
   py tools/tasks.py list
   ```

3. Aufgabe reservieren. Das legt den Worktree `.worktrees/<kennung>` und den Branch `task/<kennung>` an, pusht ihn und öffnet einen Draft-PR `[WIP] <kennung>`, an dem alle auf GitHub sehen, wer woran arbeitet. Lehnt das Skript ab, hat jemand anderes die Aufgabe gerade genommen, dann eine andere wählen:

   ```bash
   py tools/tasks.py claim <kennung> <eigener-name>
   ```

Bei Bearbeitung „zu zweit“ (Test auf zwei PCs) reserviert jede Person nur ihre Rolle: `claim <kennung> <name> host` oder `client`, ein freier Rest steht dann als `client frei` oder `host frei` in der Liste, siehe [Test auf zwei PCs](TESTING.md#test-auf-zwei-pcs).

Nie von Hand reservieren und nie eine Entwicklungsaufgabe übernehmen, die nicht `frei` ist. **Bestätigungstest ausstehend** ist nur Info ([E-12](ENTSCHEIDUNGEN.md#e-12-bestätigungstests-sind-nur-info)): Die Aufgabe ist erledigt, die offene Prüfung steht unter [Bestätigungstests ausstehend](ERLEDIGT.md#bestätigungstests-ausstehend) und blockiert nichts. Siehe [Bestätigungstests](../tools/README.md#bestätigungstests). Steht dort „erst nachfragen“, den Menschen zur Reservierung oder dem gemergten Teilstand fragen. Neue Arbeit bekommt vorher eine eigene Zeile mit Kennung in der passenden Phase, nur in den Grenzen unter [Fokus](#fokus).

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
