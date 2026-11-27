# Arbeitsprozess

Ablauf einer Aufgabe von der Wahl bis zum Merge. Befehle und Status der Werkzeuge stehen in [tools/README.md](../tools/README.md#taskspy).

## Fokus

Abschließen statt erweitern: Die Liste soll kleiner werden.

- `TODO.md` ist flach: **Probleme** vor **Verbesserungen**, ohne Phasen ([E-13](ENTSCHEIDUNGEN.md#e-13-phasen-abgeschlossen-flache-aufgabenliste)).
- Die Reihenfolge zeigen [Wo kann ich anfangen?](../TODO.md#wo-kann-ich-anfangen) und `py tools/tasks.py list --short`. Die oberste freie Zeile ist die nächste sinnvolle Aufgabe.
- Bearbeitung: **parallel** neben anderen, **zentral** (gemeinsame Schnittstellen) mit einer Person, **zu zweit** auf zwei PCs mit je einer Rolle.
- Aufgaben hängen nicht voneinander ab: Was eine Aufgabe braucht, baut sie selbst. Kein „nach X“ oder „wartet auf“ im Aufgabentext.
- Erledigtes ist nur Info und blockiert nichts ([E-12](ENTSCHEIDUNGEN.md#e-12-bestätigungstests-sind-nur-info)).
- Neue Kennungen nur für P0/P1 oder auf Wort des Nutzers ([E-08](ENTSCHEIDUNGEN.md#e-08-weniger-neue-aufgaben)). Andere Funde gehören in die PR-Beschreibung.
- Überschneidet sich ein Fund mit einer offenen Zeile, diese erweitern. Gleiche Ursache oder Dateien wie die eigene Aufgabe: im selben PR beheben.

## Nicht erneut machen

- Iris oder klassisch: klassisch. Verbindung, Spawn, Bewegung und Schießen laufen mit den [Patches](../research/spikes/a-connect/README.md#patches).
- Coop-Ablauf, Kartenerzeugung, Gegner-Pooling, Kamera und Pausen: [Spielanalyse](themen/README.md), [Sync-Bedarf](SYNC_REQUIREMENTS.md).
- Einzelprüfungen in den Inventar-Schleifen, höhere `NetPriority` des `GameState`, `OnRep_Slots` zurückhalten.
- Gegen fehlende PlayerStates Klassen pinnen oder Zeitstempel zurücksetzen, siehe [guidcache](archive/GAME_ANALYSIS.md#abschluss-und-akzeptierter-testumfang-guidcache).
- Bei seltsamem Laden zuerst `Run-Route.ps1` und `check_round.py` prüfen, siehe [Pausierte Welt und Busplatz](archive/GAME_ANALYSIS.md#pausierte-welt-und-busplatz-testumgebung).

## Vor dem Start

1. `py tools/tasks.py show <kennung>` statt der ganzen `TODO.md` lesen. Nur `frei` nehmen.
2. `py tools/tasks.py claim <kennung> <name>` legt Worktree `.worktrees/<kennung>`, Branch `task/<kennung>` und Draft-PR an. Lehnt es ab, eine andere Aufgabe wählen.
3. Bei „zu zweit“ reserviert jede Person ihre Rolle `host` oder `client`, siehe [Test auf zwei PCs](TESTING.md#test-auf-zwei-pcs).

Nie von Hand reservieren, nur im eigenen Worktree arbeiten. Aufgeben: `py tools/tasks.py release <kennung>`.

## Beim Arbeiten

- Es gelten die harten Regeln aus [CLAUDE.md](../CLAUDE.md#harte-regeln), für Spieltests zusätzlich [Spielzeit sparen](TESTING.md#spielzeit-sparen).
- Im Spike nur die Datei des eigenen Themas ändern, siehe [Spike-README](../research/spikes/a-connect/README.md#dateien).
- `Spike.h`, `Main.cpp`, die öffentlichen Schnittstellen in `src/Core` und `src/Net` und das Protokoll ändern sich nur in kurzen eigenen PRs.
- Ist `ModVersion` auf `main` seit dem Abzweig gestiegen, vor dem Merge darüber erhöhen.
- Breite Suchen übernimmt ein Explore-Subagent.
- Wiederkehrende Abläufe liegen als Skills unter `.claude/skills/`: `fehler-untersuchen`, `live-test-auswerten`, `patch-anlegen`.

## Abschluss

1. Ergebnis und Testgrenzen festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](TESTING.md#ergebnisse-festhalten).
2. `py tools/task_status.py check`, `py tools/doc_check.py check` und `py -B -m unittest discover -s tests` müssen bestehen.
3. Nur bei erfüllter Abnahme („Fertig, wenn“): `py tools/tasks.py done <kennung> "<Ergebnis in einem Satz mit Link>"`. Ein Teil-PR reicht nicht.
4. Bleibt ein Rest, der den Run zu zweit stört, den Text auf den Rest kürzen und die Zeile offen lassen. Sonst abschließen und den Rest im Ergebnis nennen.
5. Vor „Ready for review“ `main` einmergen und `py tools/task_status.py update` ausführen. Den Draft-PR fertigstellen, keinen zweiten öffnen: Titel, Beschreibung mit Testweg, dann `gh pr ready`.
6. Gemergt wird nur auf ausdrückliches Wort des Menschen. Die Reihenfolge schlägt `py tools/tasks.py merge-plan` vor. Danach räumt `py tools/tasks.py cleanup` auf.

In `TODO.md` nur die eigenen Zeilen ändern. Das Ergebnis steht nur in [docs/ERLEDIGT.md](ERLEDIGT.md#erledigt).

## Bestätigungstests

Ist die Abnahme nur auf einem PC belegt, gilt aber zu zweit live, schließt `tests pending` die Aufgabe ab, die Prüfung steht dann unter [Bestätigungstests ausstehend](ERLEDIGT.md#bestätigungstests-ausstehend).
Wer live spielt, bündelt sie mit `tests plan` und trägt sie mit `tests pass|reopen` nach, siehe [Bestätigungstests](../tools/README.md#bestätigungstests).

## Fallakten

Ein offener Fehler hat eine Fallakte `docs/faelle/<kennung>.md`, höchstens 15 000 Bytes. Mit der Abnahme wird sie gelöscht.

```markdown
# <kennung>: <Symptom in einem Satz>

## Reproduktion
## Belege            (nur [Belegt] mit Log-Auszug oder Messung, Build 26786272)
## Hypothesen        (je Zeile [Vermutung] oder [Widerlegt], mit Prüfidee)
## Nächster Schritt
## Abnahme
```

Eine Aufgabenzeile in `TODO.md` hat höchstens 800 Zeichen: `<Ziel>. **Fertig, wenn** <prüfbarer Satz>. **Einstieg** [Fallakte](docs/faelle/<kennung>.md)`.

## Regeln für Doku und Abschluss

- Doku beschreibt den Ist-Zustand auf dem unterstützten Build: ersetzen statt anhängen, keine datierten Befundabschnitte.
- Befunde und Messungen gehören in die PR-Beschreibung und die Fallakte, nicht in Themen-Doku oder das Archiv.
- Jeder behobene Absturz bekommt einen Unit-Test der zuständigen Policy oder ein Szenario für `Run-Route.ps1`.
- Ein Satz pro Zeile, keine Zeile über 400 Zeichen.
- Bei jedem Release wandert `docs/ERLEDIGT.md` als `docs/archive/ERLEDIGT-<version>.md` ins Archiv; die neue Datei behält nur die Bestätigungstests.
