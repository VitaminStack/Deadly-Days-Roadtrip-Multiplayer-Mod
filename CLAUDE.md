# Arbeitsregeln für KI-Agenten

Online-Coop-Mod für Deadly Days: Roadtrip, unterstützter Build `26786272` (immer nur der neueste Spiel-Build). Mehrere Agenten arbeiten parallel. Dokumentation auf Deutsch, Code und Bezeichner auf Englisch.

## Fokus

Erst spielbar: Vorrang hat, dass zwei Spieler einen Run durchspielen können. Aufgaben unter
[Optionale Optimierungen](TODO.md#optionale-optimierungen) warten, bis Phase 0 bis 5 erledigt sind; bis dahin zeigt
`tasks.py` sie als `zurückgestellt` und `claim` lehnt sie ab. Vorziehen entscheidet der Mensch, indem die Zeile in eine
Phase oder nach Wartung wandert.

**Weniger neue Aufgaben** (Nutzervorgabe vom 21.11.2026, bisher entstand mehr Arbeit, als erledigt wurde):

- Einen Fund mit derselben Ursache oder in denselben Dateien wie die eigene Aufgabe im selben PR beheben, ohne neue
  Kennung. Vorher mit `py tools/tasks.py list` prüfen, dass keine Kennung, Reservierung oder offener PR ihn schon abdeckt.
- Eine neue Kennung nur für einen Fund oder Rest, der den Run zu zweit verhindert (P0 oder P1 in der passenden Phase),
  oder auf Anweisung des Menschen.
- Alles andere bekommt keine Zeile in `TODO.md`: Fund und Messung stehen im Befund der eigenen Doku und in der
  PR-Beschreibung, ein Rest der eigenen Aufgabe als Grenze im Ergebnis. Keine gemeinsame Sammelliste anlegen.

## Verbindliches Architekturprinzip: lokal zuerst, minimal synchronisieren

Nutzervorgabe vom 05.11.2026, projektweit: **Möglichst viel vorhandene Logik und Darstellung lokal
beim Client ausführen und nur das für Multiplayer notwendige Minimum zwischen Host und Clients teilen.**
Ziel sind weniger Sonderfälle, Fehler und Entwicklungsaufwand, nicht bloß möglichst wenige Bytes.

- Native lokale Abläufe wiederverwenden: UI, Drag/Rotation, Platzierungsvorschau, Layout, Kamera,
  Animation, VFX/Sound und aus bestätigten Daten ableitbare Zustände brauchen keinen Host-Fernsteuerpfad.
- Pro gemeinsamem Spielzustand genau eine verbindliche Zuständigkeit behalten. Vergabe/Verbrauch,
  gemeinsame XP/Währungen, Schaden, Gegner und Weltfortschritt nicht unabhängig auf beiden Seiten buchen.
  Lokale Vorhersage/Vorprüfung ist erlaubt, ein bestätigtes Ergebnis muss sie abgleichen oder zurücknehmen.
- Zuerst vorhandene native Replikation nutzen. Eigene Nachrichten nur für nachgewiesene Lücken;
  keine parallele zweite Autorität oder zweite Replikation desselben Zustands aufbauen.
- Zusammengehörige Änderungen als einen fachlichen Vorgang behandeln, etwa Itemwahl, Rucksackverbrauch
  und neue Felder. Stabile IDs, Sitzung/Revision und Bestätigung statt Objektzeigern oder Widgetzuständen.
- Unveränderte ableitbare Anzeige nicht ständig übertragen. Änderungen/Ereignisse und nötige
  Start-/Wiederherstellungsbilder reichen, sofern Verlust, Join, Travel und Reconnect abgesichert bleiben.
- Vor jedem neuen Sync-/Hook-Pfad festhalten: Was bleibt lokal? Was muss geteilt werden und warum?
  Wer entscheidet? Welcher bestehende Weg wird wiederverwendet/ersetzt? Wie werden Ablehnung,
  Wiederholung und Zustandsneuaufbau getestet? Kein breiter Umbau ohne belegten Nutzen.

Grenzen und priorisierte Fundstellen: [Client-lokal-Audit](docs/CLIENT_LOCAL_AUDIT.md).
Die native Replikationsgrundlage aus Gate G1 und die gemeinsame Kamera bleiben bestehen.

## Vor dem Start

1. [Wo kann ich anfangen?](TODO.md#wo-kann-ich-anfangen) lesen, auch die Liste „Nicht erneut machen“. Jede Aufgabe nennt unter „Braucht“ ihre Voraussetzungen und unter „Bearbeitung“, ob sie parallel läuft, zentral ist (nie aufteilen) oder das Team entscheidet.
2. Status aller Aufgaben ansehen, Einzelheiten einer Aufgabe mit `py tools/tasks.py show <kennung>` statt der ganzen `TODO.md`. Nur Aufgaben mit Status `frei` nehmen, `blockiert` und `Team` lehnt das Skript ab:

   ```bash
   py tools/tasks.py list
   ```

3. Aufgabe reservieren. Das legt den Worktree `.worktrees/<kennung>` und den Branch `task/<kennung>` an, pusht ihn und öffnet einen Draft-PR `[WIP] <kennung>`, an dem alle auf GitHub sehen, wer woran arbeitet. Lehnt das Skript ab, hat jemand anderes die Aufgabe gerade genommen, dann eine andere wählen:

   ```bash
   py tools/tasks.py claim <kennung> <eigener-name>
   ```

Bei Bearbeitung „zu zweit“ (Test auf zwei PCs) reserviert jede Person nur ihre Rolle: `claim <kennung> <name> host` oder `client`, ein freier Rest steht dann als `client frei` oder `host frei` in der Liste, siehe [Test auf zwei PCs](docs/TESTING.md#test-auf-zwei-pcs).

Nie von Hand reservieren und nie eine Entwicklungsaufgabe übernehmen, die nicht `frei` ist. **Bestätigungstest ausstehend** ist ein eigener Wartezustand, nicht „in Arbeit“ und nicht erledigt: solche Tests mit `py tools/tasks.py tests plan` bündeln, ohne die getesteten Aufgaben neu zu reservieren. Teststände und belegte Ergebnisse im eigenen Wartungs-Worktree über `tests update` bzw. `tests pass` aktualisieren; aktive fremde Aufgaben bleiben ausgeschlossen. Siehe [Bestätigungstests](tools/README.md#bestätigungstests). Steht dort „erst nachfragen“, den Menschen zur Reservierung oder dem gemergten Teilstand fragen. Neue Arbeit bekommt vorher eine eigene Zeile mit Kennung in der passenden Phase, nur in den Grenzen unter [Fokus](#fokus).

Worktrees liegen immer unter `.worktrees/` im Repo-Ordner, keine neuen Ordner daneben anlegen. Im eigenen Worktree arbeiten, nie im Hauptordner oder im Worktree eines anderen Agenten. Wer die Reservierung aufgibt, ruft `py tools/tasks.py release <kennung>` auf. Das schließt den Draft-PR und löscht den Branch.

## Beim Arbeiten

- Bei Spieltests gilt: **eine Sitzung pro PC.** `Start-Session.ps1` bricht ab, wenn eine andere läuft. Dann mit `-Wait <Sekunden>` einreihen oder ohne Spiel weiterarbeiten, nie fremde Instanzen beenden. Spieltests bündeln und Skripte mit `-Brief` aufrufen, Bilder nur bei Bedarf ansehen, siehe [Spielzeit sparen](docs/TESTING.md#spielzeit-sparen).
- Die Exe nie von Hand mit dem Spike starten und nie `-unattended` verwenden, sonst können Absturzberichte bei Pixelsplit landen, siehe [Testen](docs/TESTING.md).
- Im Spike nur die Datei des eigenen Themas ändern, die Aufteilung steht in der [Spike-README](research/spikes/a-connect/README.md#dateien).
- Tests, die zwei Menschen live brauchen, lassen sich in einem Termin bündeln. Hinweise und mögliche Punkte stehen unter [Live-Sitzungen zu zweit](docs/PROZESS.md#live-sitzungen-zu-zweit); daraus entsteht keine zusätzliche Pflicht.
- Fehlt nach einem integrierten Teilstand nur noch die Prüfung, den Stand mit `py tools/tasks.py tests pending <kennung> --note "Stand und offene Abnahme"` kennzeichnen. Diagnose-only darf so auf eine Bestätigung warten, gilt aber nicht als behobener Fehler. Merge und Branch-Löschung ersetzen keinen Test. `py tools/tasks.py list --active` lässt Bestätigungstests aus.
- Neue Kontrollnachricht oder anderes Format: Typ zuerst in [Belegte Nachrichtentypen](docs/CONTROL_PROTOCOL.md#belegte-nachrichtentypen) eintragen und `ModVersion` erhöhen. Ist `ModVersion` auf `main` seit dem Abzweig gestiegen, vor dem Merge darüber erhöhen.
- Offsets gelten nur für den unterstützten Build. Neue Werte in [docs/OFFSETS.md](docs/OFFSETS.md) eintragen.
- Spieldateien, SDKs, decompilierter Code, persönliche Pfade und IDs gehören nie ins Repo, siehe [CONTRIBUTING.md](CONTRIBUTING.md#was-nie-ins-repo-darf). Das Repo ist öffentlich.

## Abschluss

1. Ergebnisse festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](docs/TESTING.md#ergebnisse-festhalten).
2. Abschließen wie in [Aufgabe abschließen](docs/PROZESS.md#aufgabe-abschließen): nur bei erfüllter Abnahme die eigene Zeile nach „Erledigt“ in [docs/ERLEDIGT.md](docs/ERLEDIGT.md) verschieben, alphabetisch, am einfachsten mit `py tools/tasks.py done <kennung> "<Ergebnis>"`. Ein Teil-PR reicht nicht. In `TODO.md` nur die eigenen Zeilen ändern, das hält Konflikte klein. Danach `py tools/task_status.py update` (macht `done` selbst); `py tools/task_status.py check` muss vor dem PR bestehen. [docs/ROADMAP.md](docs/ROADMAP.md) nur ändern, wenn eine ganze Phase erledigt ist: Das Ergebnis steht unter „Erledigt“, eine zweite Kopie erzeugt nur Merge-Konflikte. Befunde gehören unter eine eigene Überschrift mit der Kennung im Thema der Doku, in fremde Abschnitte und Live-Test-Berichte höchstens eine Zeile mit Link, siehe [Ergebnisse festhalten](docs/TESTING.md#ergebnisse-festhalten). Vor „Ready for review“ `main` einmergen und `py tools/task_status.py update` erneut ausführen. Nach dem Merge räumt `py tools/tasks.py cleanup` verwaiste Worktrees auf. Branch-Löschung oder fehlende GitHub CLI dürfen nie als „Aufgabe frei“ interpretiert werden.
3. Den Draft-PR fertigstellen, keinen zweiten öffnen: Titel und Beschreibung ersetzen, dabei sagen, wie getestet wurde, dann auf „Ready for review“ stellen. Nur auf ausdrückliche Anweisung des Menschen mergen. Die Reihenfolge beim Mergen schlägt `py tools/tasks.py merge-plan` vor; steht der eigene PR dort unter „Erst nachziehen“, `main` einmergen und `py tools/task_status.py update` ausführen.

   ```bash
   gh pr edit task/<kennung> --title "<kennung>: <Ergebnis>" --body-file <datei>
   gh pr ready task/<kennung>
   ```
