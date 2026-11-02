# Arbeitsregeln für KI-Agenten

Online-Coop-Mod für Deadly Days: Roadtrip, unterstützter Build `26187885`. Mehrere Agenten arbeiten parallel. Dokumentation auf Deutsch, Code und Bezeichner auf Englisch.

## Fokus

Erst spielbar: Vorrang hat, dass zwei Spieler einen Run durchspielen können. Kleine Fehler, die das nicht verhindern, kommen nur unter
[Optionale Optimierungen](TODO.md#optionale-optimierungen) und werden nicht behoben.

## Vor dem Start

1. [Wo kann ich anfangen?](TODO.md#wo-kann-ich-anfangen) lesen, auch die Liste „Nicht erneut machen“.
2. Offene Reservierungen ansehen. Eine Aufgabe, deren Branch schon existiert, nicht übernehmen:

   ```bash
   git ls-remote --heads origin "task/*"
   ```

3. Aufgabe reservieren. Lehnt `push` ab, hat jemand anderes sie gerade genommen, dann eine andere wählen:

   ```bash
   git fetch origin
   git worktree add -b task/<kennung> ../dd-<kennung> origin/main
   git -C ../dd-<kennung> commit --allow-empty -m "task: claim <kennung> by <eigener-name>-$RANDOM"
   git -C ../dd-<kennung> push origin task/<kennung>
   ```

Im eigenen Worktree arbeiten, nie im Hauptordner oder im Worktree eines anderen Agenten. Wer die Reservierung aufgibt, löscht den Branch auf `origin`.

## Beim Arbeiten

- Bei Spieltests gilt: **eine Sitzung pro PC.** `Start-Session.ps1` bricht ab, wenn eine andere läuft. Dann warten oder ohne Spiel weiterarbeiten, nie fremde Instanzen beenden.
- Die Exe nie von Hand mit dem Spike starten und nie `-unattended` verwenden, sonst können Absturzberichte bei Pixelsplit landen, siehe [Testen](docs/TESTING.md).
- Im Spike nur die Datei des eigenen Themas ändern, die Aufteilung steht in der [Spike-README](research/spikes/a-connect/README.md#dateien).
- Offsets gelten nur für den unterstützten Build. Neue Werte in [docs/OFFSETS.md](docs/OFFSETS.md) eintragen.
- Spieldateien, SDKs, decompilierter Code, persönliche Pfade und IDs gehören nie ins Repo, siehe [CONTRIBUTING.md](CONTRIBUTING.md#was-nie-ins-repo-darf). Das Repo ist öffentlich.

## Abschluss

1. Ergebnisse festhalten, auch gescheiterte Versuche, siehe [Ergebnisse festhalten](docs/TESTING.md#ergebnisse-festhalten).
2. In `TODO.md` nur die eigenen Zeilen ändern, das hält Konflikte klein. Ist die Aufgabe fertig, auch [docs/ROADMAP.md](docs/ROADMAP.md) anpassen.
3. Pull Request nach `main` öffnen und sagen, wie getestet wurde. Nur auf ausdrückliche Anweisung des Menschen mergen.
