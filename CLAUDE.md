# Arbeitsregeln für KI-Agenten

Online-Coop-Mod für Deadly Days: Roadtrip, unterstützter Build `26187885`. Mehrere Agenten arbeiten parallel. Dokumentation auf Deutsch, Code und Bezeichner auf Englisch.

## Fokus

Erst spielbar: Vorrang hat, dass zwei Spieler einen Run durchspielen können. Kleine Fehler, die das nicht verhindern, kommen nur unter
[Optionale Optimierungen](TODO.md#optionale-optimierungen) und werden nicht behoben.

## Vor dem Start

1. [Wo kann ich anfangen?](TODO.md#wo-kann-ich-anfangen) lesen, auch die Liste „Nicht erneut machen“. Jede Aufgabe nennt unter „Braucht“ ihre Voraussetzungen und unter „Bearbeitung“, ob sie parallel läuft, zentral ist (nie aufteilen) oder das Team entscheidet.
2. Status aller Aufgaben ansehen. Nur Aufgaben mit Status `frei` nehmen, `blockiert` und `Team` lehnt das Skript ab:

   ```bash
   py tools/tasks.py list
   ```

3. Aufgabe reservieren. Das legt den Worktree `.worktrees/<kennung>` und den Branch `task/<kennung>` an, pusht ihn und öffnet einen Draft-PR `[WIP] <kennung>`, an dem alle auf GitHub sehen, wer woran arbeitet. Lehnt das Skript ab, hat jemand anderes die Aufgabe gerade genommen, dann eine andere wählen:

   ```bash
   py tools/tasks.py claim <kennung> <eigener-name>
   ```

Nie von Hand reservieren und nie eine Aufgabe übernehmen, die nicht `frei` ist. Steht dort „erst nachfragen“, ist die Reservierung seit mehreren Tagen ohne Commit. Dann den Menschen fragen, der sie im Team klärt. Neue Arbeit bekommt vorher eine eigene Zeile mit Kennung in der passenden Phase.

Worktrees liegen immer unter `.worktrees/` im Repo-Ordner, keine neuen Ordner daneben anlegen. Im eigenen Worktree arbeiten, nie im Hauptordner oder im Worktree eines anderen Agenten. Wer die Reservierung aufgibt, ruft `py tools/tasks.py release <kennung>` auf. Das schließt den Draft-PR und löscht den Branch.

## Beim Arbeiten

- Bei Spieltests gilt: **eine Sitzung pro PC.** `Start-Session.ps1` bricht ab, wenn eine andere läuft. Dann warten oder ohne Spiel weiterarbeiten, nie fremde Instanzen beenden.
- Die Exe nie von Hand mit dem Spike starten und nie `-unattended` verwenden, sonst können Absturzberichte bei Pixelsplit landen, siehe [Testen](docs/TESTING.md).
- Im Spike nur die Datei des eigenen Themas ändern, die Aufteilung steht in der [Spike-README](research/spikes/a-connect/README.md#dateien).
- Offsets gelten nur für den unterstützten Build. Neue Werte in [docs/OFFSETS.md](docs/OFFSETS.md) eintragen.
- Spieldateien, SDKs, decompilierter Code, persönliche Pfade und IDs gehören nie ins Repo, siehe [CONTRIBUTING.md](CONTRIBUTING.md#was-nie-ins-repo-darf). Das Repo ist öffentlich.

## Abschluss

1. Ergebnisse festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](docs/TESTING.md#ergebnisse-festhalten).
2. Abschließen wie in [Aufgabe abschließen](TODO.md#aufgabe-abschließen): nur bei erfüllter Abnahme die eigene Zeile nach „Erledigt“ verschieben, alphabetisch. Ein Teil-PR reicht nicht. In `TODO.md` nur die eigenen Zeilen ändern, das hält Konflikte klein. Danach `py tools/task_status.py update`; `py tools/task_status.py check` muss vor dem PR bestehen. [docs/ROADMAP.md](docs/ROADMAP.md) anpassen. Branch-Löschung oder fehlende GitHub CLI dürfen nie als „Aufgabe frei“ interpretiert werden.
3. Den Draft-PR fertigstellen, keinen zweiten öffnen: Titel und Beschreibung ersetzen, dabei sagen, wie getestet wurde, dann auf „Ready for review“ stellen. Nur auf ausdrückliche Anweisung des Menschen mergen.

   ```bash
   gh pr edit task/<kennung> --title "<kennung>: <Ergebnis>" --body-file <datei>
   gh pr ready task/<kennung>
   ```
