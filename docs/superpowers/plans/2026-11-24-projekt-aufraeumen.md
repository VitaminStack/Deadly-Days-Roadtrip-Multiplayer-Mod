# Projekt aufräumen: Umsetzungsplan

> **Für ausführende Agenten:** Pflicht-Skill: superpowers:subagent-driven-development (empfohlen) oder
> superpowers:executing-plans, Aufgabe für Aufgabe. Schritte haben Checkboxen (`- [ ]`) zum Abhaken.
> Nach Abschluss aller Aufgaben wird diese Datei gelöscht; das Ergebnis steht dann in `docs/ERLEDIGT.md`.

**Ziel:** Kontext je Aufgabe verkleinern und die Doku auf den Ist-Zustand reduzieren, damit Agenten nicht
aus veralteten Befunden halluzinieren und die offenen Fehler der Phase 5 gezielt behoben werden.

**Architektur:** Zuerst Leitplanken in der CI (Größengrenzen, Linkprüfung, Archiv-Kennzeichnung), damit
nichts wieder wächst. Dann verschieben wir Geschichte wortgleich nach `docs/archive/` (Anker bleiben gültig,
Links werden per Werkzeug umgeschrieben) und schreiben kurze Themen-Dateien mit dem Ist-Zustand auf Build
`26786272`. Danach kürzen wir Einstiegsdokumente und Prozess und lösen den Spike aus `main`.

**Technik:** Python 3 nur mit Standardbibliothek und `unittest` (wie `tools/task_status.py`), GitHub Actions
(`.github/workflows/docs.yml`), Markdown, Claude-Code-Einstellungen (`.claude/settings.json`, verschachtelte `CLAUDE.md`).

**Spec:** Analyse aus der Sitzung vom 24.11.2026, zusammengefasst unter [Befunde](#befunde).

## Befunde

Gemessen auf `main` am 24.11.2026:

| Befund | Zahl |
|---|---|
| Markdown gesamt | 1,45 MB, etwa 400k Tokens, mehr als der C++-Code |
| `docs/GAME_ANALYSIS.md` | 544 KB, Aufgabenjournal: 104 Überschriften tragen eine Kennung, 12 Live-Test-Berichte |
| `TODO.md` | 119 KB, davon 54 KB „Erledigt“ (207 Zeilen), 30 KB Optionen, nur ~9 KB offene Phase 5/6 |
| `docs/OFFSETS.md` | 156 KB, davon ~36 KB für die alten Builds `26495302`/`26669225` |
| `docs/ROADMAP.md` | 99 KB, doppelt den Status aus `TODO.md` |
| `src/Game/README.md` | 81 KB Pflichtlektüre für Arbeit in `src/Game` |
| Verweise auf alte Builds | `26495302` 210×, `26669225` 140×, aktueller Build nur 145× |
| Datierte Einträge in Markdown | 603 |
| Links in `GAME_ANALYSIS.md#…` | 314 |
| Markdown-Zeilen über 400 Zeichen | 410 |
| `third_party/doctest/doctest.h` | 363 KB, für Agenten nutzlos |
| Spike `research/spikes/a-connect` | ~9000 Zeilen, `RemoteP2Policy.h` doppelt in Spike und `src/Game` mit 126 Zeilen Abweichung |
| Probe-Skripte `tools/spike/*.py` | 34 Einzelskripte ohne gemeinsame Übersicht |
| Bestätigungstests ausstehend | 13 |
| CI für Doku | `docs.yml` prüft nur `task_status.py`, kein Link- oder Größencheck |

## Globale Vorgaben

- Unterstützt wird nur Build `26786272`. Aussagen zu anderen Builds stehen nur im Archiv.
- Doku auf Deutsch, Code, Bezeichner und Dateinamen von Werkzeugen auf Englisch, Doku-Dateinamen wie bisher in Großbuchstaben.
- Nichts aus Spiel, SDK oder decompiliertem Code ins Repo, siehe `CONTRIBUTING.md#was-nie-ins-repo-darf`.
- **Verschieben ist wortgleich.** Wird ein Abschnitt archiviert, wandert sein Text unverändert. Umformuliert
  wird nur in den neuen Ist-Zustand-Dateien. So geht kein Beleg verloren und Anker bleiben stabil.
- Jede Datei unter `docs/archive/` beginnt mit genau dieser Zeile (wird geprüft):
  `> **Archiv.** Historischer Stand, nicht aktuell. Gilt nur, wenn eine aktuelle Doku darauf verweist.`
- Python-Werkzeuge: nur Standardbibliothek, Tests mit `unittest` in `tests/test_<werkzeug>.py`, Aufruf
  unter Linux `python3 -B …`, unter Windows `py -B …`.
- Nach jeder Aufgabe bestehen: `python3 -B tools/task_status.py check`, `python3 -B tools/doc_check.py check`
  (ab Aufgabe 1) und `python3 -B -m unittest discover -s tests -p 'test_*.py'`.
- Neue Doku-Zeilen: ein Satz pro Zeile, keine Zeile über 400 Zeichen.
- Commit-Nachrichten auf Englisch im Stil des Repos, eine Aufgabe = mindestens ein Commit.

## Prüfschwerpunkte

Was keine Aufgabe ausdrücklich verlangt, aber am ehesten schiefgeht. Die passenden Tests stehen in den genannten Aufgaben.

1. **Überschriften mit Umlauten, Backticks oder doppeltem Text.** GitHub erzeugt `#lager-und-händler`,
   `#niederlage-niederlage` und bei Dopplung `-1`, `-2`. Die Linkprüfung muss dieselben Anker bilden,
   sonst meldet sie Hunderte falsche Fehler (Aufgabe 1, `test_anchor_slugs_match_github`).
2. **CRLF-Dateien.** `TODO.md` kann CRLF haben, die bestehenden Tests prüfen das. Aufteilen und Zusammenführen
   von `TODO.md`/`docs/ERLEDIGT.md` muss das Zeilenende je Datei erhalten (Aufgabe 4, `test_split_keeps_line_endings_per_file`).
3. **Stand vor dem Merge.** `tasks.py` liest `origin/main`. Solange dort `docs/ERLEDIGT.md` fehlt, muss es als
   leer gelten, ohne Absturz (Aufgabe 4, `test_missing_done_file_reads_as_empty`).
4. **Links in Codeblöcken und auf Quelltextzeilen.** Beispiele in ```` ``` ````-Blöcken sind keine Links,
   `Datei.cpp#L12` ist gültig, solange die Datei existiert (Aufgabe 1, `test_code_fences_and_line_anchors`).
5. **Archiv wird als aktuell gelesen.** Fehlt das Archiv-Banner, liest ein Agent alte Befunde als gültig
   (Aufgabe 2, `test_archive_files_need_banner`).

---

## Stufe 0: Freigabe

### Aufgabe 0: Entscheidungen des Menschen einholen

**Dateien:** keine Änderungen, nur Entscheidungen. Sie werden in Aufgabe 10 in `docs/ENTSCHEIDUNGEN.md` übernommen.

- [ ] **Schritt 1: Kennung festlegen.** Der Mensch bestätigt die Kennung `projekt-aufraeumen` (Wartung, P1,
  zentral, Spielzeit keine). Die Zeile kommt mit Aufgabe 4 in `TODO.md`.
- [ ] **Schritt 2: Parallelarbeit stoppen.** Während der Stufen 1 bis 3 ändert kein anderer Agent Markdown.
  Den reservierten Branch `tests-aufraeumen` ohne Kennung klärt der Mensch (mergen, freigeben oder fortsetzen).
- [ ] **Schritt 3: Bestätigungstests entscheiden.** Für jede der 13 Kennungen aus `py tools/tasks.py tests list`
  entscheidet der Mensch: **bündeln** (kommt in die nächste Live-Sitzung) oder **abschließen** (mit
  `tests pass` und Beleg „in Live-Nutzung unauffällig“) oder **zum Bug machen** (`tests reopen`).
  Es dürfen höchstens 3 gebündelt bleiben (Grenze aus Aufgabe 12).
- [ ] **Schritt 4: GAME_ANALYSIS-Themen bestätigen.** Der Mensch bestätigt die Themenliste aus Aufgabe 8 oder passt sie an.

---

## Stufe 1: Leitplanken

### Aufgabe 1: Doku-Prüfung mit Größengrenzen und Links

**Dateien:**
- Neu: `tools/doc_check.py`
- Neu: `tests/test_doc_check.py`
- Ändern: `.github/workflows/docs.yml` (neuer Schritt nach „TODO-Aufgaben und README-Fortschritt pruefen“)
- Ändern: `tools/README.md` (Abschnitt `doc_check.py`, höchstens 10 Zeilen)

**Schnittstellen:**
- Nutzt: `task_status.slug(heading) -> str` und `task_status.markdown_files(root) -> list[str]` (beide vorhanden)
- Liefert:
  - `DEFAULT_LIMIT: int = 60_000`
  - `LIMITS: dict[str, int]`: Obergrenzen je Pfad in Bytes, die von Aufgabe zu Aufgabe sinken
  - `EXCLUDED: tuple[str, ...] = ('docs/archive/',)`: keine Größengrenze, Links werden trotzdem geprüft
  - `anchors(text: str) -> set[str]`: GitHub-Anker aller Überschriften, Dopplungen mit `-1`, `-2`, …
  - `check_sizes(root: Path, files: list[str]) -> list[str]`, Meldung `"<pfad>: <n> Bytes > Grenze <m>"`
  - `check_links(root: Path, files: list[str]) -> list[str]`, Meldung `"<pfad>:<zeile>: <ziel>: Datei fehlt"` bzw. `"…: Anker fehlt"`
  - CLI: `doc_check.py check` (Exit 1 bei Meldungen), `doc_check.py sizes` (größte 20 Dateien mit Bytes)

- [ ] **Schritt 1: Fehlschlagende Tests schreiben** in `tests/test_doc_check.py`, Klasse `DocCheckTests`, mit temporärem Ordner (`tempfile.TemporaryDirectory`):

```python
def test_anchor_slugs_match_github(self):
    text = '# Lager und Händler\n## Niederlage (`niederlage`)\n## Gleich\n## Gleich\n'
    self.assertEqual(anchors(text), {'lager-und-händler', 'niederlage-niederlage', 'gleich', 'gleich-1'})

def test_missing_file_and_anchor_reported(self):
    # a.md: "[x](b.md#da) [y](b.md#weg) [z](fehlt.md)", b.md: "## Da"
    self.assertEqual(check_links(root, ['a.md', 'b.md']),
                     ['a.md:1: b.md#weg: Anker fehlt', 'a.md:1: fehlt.md: Datei fehlt'])

def test_code_fences_and_line_anchors(self):
    # a.md: "```\n[x](nix.md)\n```\n[c](src/X.cpp#L12)", src/X.cpp existiert
    self.assertEqual(check_links(root, ['a.md']), [])

def test_external_and_same_file_links(self):
    # a.md: "## Hier\n[w](https://example.com) [h](#hier) [n](#nicht)"
    self.assertEqual(check_links(root, ['a.md']), ['a.md:2: #nicht: Anker fehlt'])

def test_size_limits_default_override_and_archive(self):
    # a.md 70_000 Bytes, TODO.md 16_000 Bytes, docs/archive/x.md 900_000 Bytes; LIMITS = {'TODO.md': 15_000}
    self.assertEqual(check_sizes(root, files),
                     ['TODO.md: 16000 Bytes > Grenze 15000', 'a.md: 70000 Bytes > Grenze 60000'])
```

- [ ] **Schritt 2: Tests ausführen, sie schlagen fehl.**
  Befehl: `python3 -B -m unittest tests.test_doc_check -v`. Erwartet: `ModuleNotFoundError: doc_check`.

- [ ] **Schritt 3: `tools/doc_check.py` umsetzen.** Links per Regex `\[[^\]]*\]\(([^)\s]+)\)` außerhalb von
  ```` ``` ````-Blöcken. Ziele mit `://` oder `mailto:` überspringen. Pfad relativ zur Quelldatei auflösen,
  URL-Dekodierung mit `urllib.parse.unquote`. Anker nur bei `.md`-Zielen prüfen. Ausgabe sortiert.
  Importiert `slug` und `markdown_files` aus `task_status` wie `tasks.py` (beide Importvarianten).

- [ ] **Schritt 4: Tests ausführen, sie bestehen.** Befehl wie Schritt 2. Erwartet: `OK`.

- [ ] **Schritt 5: Ist-Stand erfassen.** `python3 -B tools/doc_check.py sizes` ausführen. Jede Datei über
  60 000 Bytes bekommt in `LIMITS` ihre heutige Größe, auf volle 1000 aufgerundet. Erwartet sind
  `TODO.md`, `docs/GAME_ANALYSIS.md`, `docs/OFFSETS.md`, `docs/ROADMAP.md` und `src/Game/README.md`.
  Zusätzlich gilt `'CLAUDE.md': 10_000`.
  Diese Werte sinken in den späteren Aufgaben, sie dürfen nie steigen. Das steht als Kommentar über `LIMITS`.

- [ ] **Schritt 6: Kaputte Links beheben.** `python3 -B tools/doc_check.py check` ausführen. Jede gemeldete
  Stelle korrigieren (Ziel suchen mit `grep -rn "^#.*<Wort>" docs src`). Ist kein Ziel mehr auffindbar,
  wird der Link zu reinem Text. Erwartet am Ende: keine Ausgabe, Exit 0.

- [ ] **Schritt 7: CI einhängen.** In `docs.yml` nach dem vorhandenen Prüfschritt ergänzen:
  `- name: Doku-Größen und Links pruefen` mit `run: python3 -B tools/doc_check.py check`.
  Danach `- name: Tests der Doku-Pruefung` mit `run: python3 -B -m unittest tests.test_doc_check`.

- [ ] **Schritt 8: Commit.**
  ```bash
  git add tools/doc_check.py tests/test_doc_check.py .github/workflows/docs.yml tools/README.md
  git commit -m "Add doc size and link check to docs CI"
  ```
  Behobene Links kommen im selben Commit mit.

### Aufgabe 2: Links umschreiben und Archiv kennzeichnen

**Dateien:**
- Ändern: `tools/doc_check.py`, `tests/test_doc_check.py`

**Schnittstellen:**
- Nutzt: `check_links`, `anchors` aus Aufgabe 1
- Liefert:
  - `ARCHIVE_BANNER: str`: die Zeile aus den Globalen Vorgaben
  - `check_archive(root: Path, files: list[str]) -> list[str]`, Meldung `"<pfad>: Archiv-Banner fehlt"`, in `check` eingebunden
  - `rewrite_links(root: Path, files: list[str], old: str, new: str, only_anchors: set[str] | None = None) -> int`:
    Schreibt in allen `files` relative Links auf die Repo-Datei `old` auf `new` um, mit dem richtigen relativen
    Pfad je Quelldatei. Ist `only_anchors` gesetzt, betrifft das nur Links mit diesen Ankern. Liefert die Zahl
    der geänderten Links. Zeilenenden bleiben erhalten.
  - CLI: `doc_check.py move <alt> <neu> [--anchors a,b,c]` ruft `rewrite_links` für alle Markdown-Dateien auf
    und gibt die Zahl aus. Die Datei selbst verschiebt der Aufrufer mit `git mv`.

- [ ] **Schritt 1: Fehlschlagende Tests schreiben:**

```python
def test_archive_files_need_banner(self):
    # docs/archive/a.md ohne Banner, docs/archive/b.md mit Banner als erster Zeile
    self.assertEqual(check_archive(root, files), ['docs/archive/a.md: Archiv-Banner fehlt'])

def test_rewrite_links_relative_per_source(self):
    # docs/X.md: "[a](GAME_ANALYSIS.md#k)", src/Game/README.md: "[b](../../docs/GAME_ANALYSIS.md)"
    n = rewrite_links(root, files, 'docs/GAME_ANALYSIS.md', 'docs/archive/GAME_ANALYSIS.md')
    self.assertEqual(n, 2)
    # docs/X.md enthält danach "[a](archive/GAME_ANALYSIS.md#k)",
    # src/Game/README.md enthält "[b](../../docs/archive/GAME_ANALYSIS.md)"

def test_rewrite_only_selected_anchors_and_idempotent(self):
    # "[a](OFFSETS.md#alt) [b](OFFSETS.md#neu)", only_anchors={'alt'}
    # erster Lauf: 1, zweiter Lauf: 0; "#neu" bleibt unverändert

def test_rewrite_keeps_crlf(self):
    # Quelldatei mit \r\n bleibt \r\n
```

- [ ] **Schritt 2: Tests schlagen fehl.** `python3 -B -m unittest tests.test_doc_check -v`. Erwartet: `ImportError`/`AttributeError` für die neuen Namen.
- [ ] **Schritt 3: Umsetzen.** Relativer Pfad mit `os.path.relpath(new, start=dirname(quelle))`, als POSIX-Pfad.
- [ ] **Schritt 4: Tests bestehen.** Gleicher Befehl, `OK`.
- [ ] **Schritt 5: Commit.** `git commit -m "Add link rewriting and archive banner check"`

### Aufgabe 3: Kurze Aufgabenliste und gesperrte Fremddateien

**Dateien:**
- Ändern: `tools/tasks.py` (`cmd_list`, Argumente in `main`)
- Ändern: `tests/test_task_status.py`
- Neu: `.claude/settings.json`

**Schnittstellen:**
- Liefert: `cmd_list(active_only=False, short=False)`. `tasks.py list --short` gibt aus:
  - je offener Aufgabe aus Phase 0 bis 6 und Wartung eine Zeile `<kennung> <status>` ohne Beschreibung
  - für Optionen und „Real Coop logic“ je eine Sammelzeile: `Optional: <n> Aufgaben (tasks.py list)`
  - eine Zeile `Bestätigungstests: <n> (tasks.py tests list)`

- [ ] **Schritt 1: Fehlschlagender Test** in `tests/test_task_status.py`, aufgebaut wie `test_list_without_github_hides_done_and_shows_status`:

```python
def test_list_short_has_no_descriptions_and_collapses_optional(self):
    # gleiche Patches wie test_list_without_github_hides_done_and_shows_status, ROWS um eine Option ergänzt
    tasks.cmd_list(short=True)
    lines = out.getvalue().splitlines()
    self.assertTrue(any(line.split() == ['open', 'frei'] for line in lines))
    self.assertTrue(all(len(line) <= 80 for line in lines))
    self.assertTrue(any(line.startswith('Optional: ') for line in lines))
    self.assertTrue(any(line.startswith('Bestätigungstests: ') for line in lines))
```

- [ ] **Schritt 2: Test schlägt fehl.** `python3 -B -m unittest tests.test_task_status -v`, erwartet `TypeError: unexpected keyword argument 'short'`.
- [ ] **Schritt 3: Umsetzen.** `--short` als zusätzlichen Schalter für `list` in `main` parsen.
- [ ] **Schritt 4: Test besteht.**
- [ ] **Schritt 5: Fremddateien sperren.** `.claude/settings.json` anlegen:

```json
{
  "permissions": {
    "deny": ["Read(third_party/**)", "Read(research/local/**)"]
  }
}
```

  Die Datei prüfen, bevor sie committed wird: `.gitignore` darf `.claude/settings.json` nicht ausschließen,
  `.claude/settings.local.json` bleibt lokal.
- [ ] **Schritt 6: Commit.** `git commit -m "Add short task list and deny reading third-party sources"`

---

## Stufe 2: TODO.md entschlacken

### Aufgabe 4: Erledigte Aufgaben nach `docs/ERLEDIGT.md`

**Dateien:**
- Ändern: `tools/task_status.py` (`regenerate`, neue Funktionen), `tools/tasks.py` (`todo_text`, `cmd_done`, Hilfetext)
- Ändern: `tests/test_task_status.py`
- Neu: `docs/ERLEDIGT.md` (Abschnitt `## Erledigt` wortgleich aus `TODO.md`)
- Ändern: `TODO.md`, `CLAUDE.md`, alle Links auf `TODO.md#erledigt`

**Schnittstellen:**
- Liefert in `task_status.py`:
  - `DONE_FILE = 'docs/ERLEDIGT.md'`
  - `combine(todo: str, done: str) -> str`: hängt `done` an `todo` an. Ist `done` leer, bleibt `todo` unverändert.
  - `split(text: str) -> tuple[str, str]`: trennt an der Zeile `## Erledigt` bis zur nächsten `## `-Überschrift.
    Liefert `(todo ohne diesen Abschnitt, Dateiinhalt für DONE_FILE)`. Zeilenenden je Teil wie in der Herkunft.
  - `parse`, `mark_done`, `confirmation_update` bleiben unverändert und arbeiten auf dem kombinierten Text.
  - `regenerate(root, check=False)` liest beide Dateien, kombiniert sie, erzeugt neu, trennt wieder und
    vergleicht/schreibt beide Dateien.
- In `tasks.py`: `todo_text()` liefert `combine(<origin/main:TODO.md>, <origin/main:docs/ERLEDIGT.md oder ''>)`.
  `cmd_done` schreibt beide Dateien.
- Inhalt `docs/ERLEDIGT.md`: Titel `# Erledigte Aufgaben`, ein Satz („Abgeschlossene Kennungen mit Ergebnis,
  nie erneut reservieren.“), danach der Abschnitt `## Erledigt` mit Tabelle wortgleich.

- [ ] **Schritt 1: Fehlschlagende Tests:**

```python
def test_split_and_combine_roundtrip(self):
    todo, done = task_status.split(SAMPLE)
    self.assertNotIn('## Erledigt', todo)
    self.assertIn('## Erledigt', done)
    self.assertEqual(task_status.parse(task_status.combine(todo, done)), task_status.parse(SAMPLE))

def test_split_keeps_line_endings_per_file(self):
    todo, done = task_status.split(SAMPLE.replace('\n', '\r\n'))
    self.assertIn('\r\n', todo); self.assertIn('\r\n', done)

def test_missing_done_file_reads_as_empty(self):
    # todo_text mit einem git-Stub, der für docs/ERLEDIGT.md CalledProcessError wirft
    self.assertEqual(tasks.todo_text(), OLD_STYLE_TODO)

def test_mark_done_moves_row_into_done_file(self):
    todo, done = task_status.split(task_status.mark_done(task_status.combine(TODO, DONE), 'frei-a', 'Ergebnis.'))
    self.assertNotIn('`frei-a`', todo)
    self.assertIn('| `frei-a` | 5 | Ergebnis. |', done)
```

  `test_real_documents_are_consistent` muss danach mit beiden echten Dateien laufen.
- [ ] **Schritt 2: Tests schlagen fehl.** `python3 -B -m unittest tests.test_task_status -v`.
- [ ] **Schritt 3: Umsetzen.** `todo_text()` fängt `subprocess.CalledProcessError` nur für `docs/ERLEDIGT.md` ab.
- [ ] **Schritt 4: Tests bestehen.**
- [ ] **Schritt 5: Abschnitt verschieben.** Ein kurzes Python-Snippet, das `split` auf `TODO.md` anwendet und
  beide Dateien schreibt. Dann `python3 -B tools/doc_check.py move TODO.md docs/ERLEDIGT.md --anchors erledigt`.
  `CLAUDE.md` nennt danach `docs/ERLEDIGT.md`.
- [ ] **Schritt 6: Prüfen.** `python3 -B tools/task_status.py check` gibt „konsistent“ aus,
  `python3 -B tools/doc_check.py check` hat keine Ausgabe, `python3 -B tools/tasks.py list` zeigt weiter
  `Erledigt: 207 Aufgaben`, nun mit Verweis auf `docs/ERLEDIGT.md`.
- [ ] **Schritt 7: Kennung eintragen.** Die Zeile `projekt-aufraeumen` aus Aufgabe 0 kommt unter „Wartung“ in `TODO.md`.
- [ ] **Schritt 8: Commit.** `git commit -m "Move done tasks to docs/ERLEDIGT.md"`

### Aufgabe 5: Prozess, Fallakten und kurze Aufgabenzeilen

**Dateien:**
- Neu: `docs/PROZESS.md`, `docs/faelle/<kennung>.md` je offener Phase-5-Aufgabe, `docs/archive/OPTIONEN.md`
- Ändern: `TODO.md`, `tools/task_status.py` (`parse`), `tests/test_task_status.py`, `tools/doc_check.py` (`LIMITS`), `CLAUDE.md` (Links)

**Schnittstellen:**
- Liefert: `task_status.MAX_TASK_TEXT = 800`. `parse` wirft `ValueError(f'{where}: {name}: Aufgabentext über 800 Zeichen, Details in docs/faelle/{name}.md')`
  für offene Zeilen, deren Spalte „Aufgabe“ länger ist. Erledigt-Zeilen sind ausgenommen.
- Vorlage für eine Fallakte `docs/faelle/<kennung>.md`, höchstens 15 000 Bytes. Mit der Abnahme wird sie gelöscht,
  ihr Ergebnis kommt als ein Satz nach `docs/ERLEDIGT.md`:

```markdown
# <kennung>: <Symptom in einem Satz>

## Reproduktion
## Belege            (nur [Belegt] mit Log-Auszug oder Messung, Build 26786272)
## Hypothesen        (je Zeile [Vermutung] oder [Widerlegt], mit Prüfidee)
## Nächster Schritt
## Abnahme
```

- Vorlage für eine Aufgabenzeile in `TODO.md` (Spalte „Aufgabe“): `<Ziel in einem Satz>. **Fertig, wenn** <ein prüfbarer Satz>. **Einstieg** [Fallakte](docs/faelle/<kennung>.md)`.

- [ ] **Schritt 1: Fehlschlagender Test:**

```python
def test_long_open_task_text_rejected(self):
    with self.assertRaisesRegex(ValueError, 'über 800 Zeichen'):
        task_status.parse(todo_with_open_text('x' * 801))
```

- [ ] **Schritt 2: Test schlägt fehl. Schritt 3: in `parse` umsetzen. Schritt 4: Test besteht.**
  Erwartet ist, dass `test_real_documents_are_consistent` jetzt fehlschlägt, bis Schritt 6 erledigt ist.
- [ ] **Schritt 5: Prozessabschnitte verschieben.** „So liest man die Tabellen“, „Live-Sitzungen zu zweit“,
  „Live-Bestätigung ausstehend“ und „Aufgabe abschließen“ wandern wortgleich nach `docs/PROZESS.md`.
  Links mit `doc_check.py move TODO.md docs/PROZESS.md --anchors <die vier Anker>` umschreiben.
  In `TODO.md` bleiben „Wo kann ich anfangen?“ mit dem erzeugten Block `<!-- frei:start -->` …
  `<!-- frei:end -->`, die Phasen, Wartung, Optionen, „Real Coop logic“ und „Risiken“.
- [ ] **Schritt 6: Lange Aufgabenzeilen kürzen.** Für jede offene Zeile über 800 Zeichen:
  - Phase 5/6 und Wartung: Langtext als Fallakte nach Vorlage (Befunde sortieren, nichts weglassen), Zeile nach Vorlage.
  - Optionen und „Real Coop logic“: Langtext wortgleich nach `docs/archive/OPTIONEN.md` unter `## <kennung>`,
    Zeile = ein Satz und Link auf diesen Anker.
- [ ] **Schritt 7: Regeln ergänzen.** In `docs/PROZESS.md` einen Abschnitt `## Regeln für Doku und Abschluss` mit diesen Sätzen anlegen:
  - Doku beschreibt den Ist-Zustand auf dem unterstützten Build: ersetzen statt anhängen, keine datierten Befundabschnitte.
  - Befunde und Messungen gehören in die PR-Beschreibung und die Fallakte, nicht in Themen-Doku.
  - Jeder behobene Absturz bekommt einen Unit-Test der zuständigen Policy oder ein Szenario für `Run-Route.ps1`.
  - Höchstens 3 Bestätigungstests stehen offen.
  - Eine Sitzung pro Aufgabe. Breite Suchen übernimmt ein Explore-Subagent, der nur das Ergebnis zurückgibt.
  - Dateien über 60 KB, das Archiv und `docs/ERLEDIGT.md` nie ganz lesen, nur per `grep -n` und Abschnitt.
- [ ] **Schritt 8: Größe absenken.** `TODO.md` muss unter 15 000 Bytes liegen, `LIMITS['TODO.md'] = 15_000`.
  Ist das nicht erreicht, weiter kürzen. Das Kürzen der Texte bleibt aber auf die Vorlagen beschränkt.
- [ ] **Schritt 9: Prüfen und Commit.** Alle drei Prüfbefehle aus den Globalen Vorgaben bestehen.
  `git commit -m "Split process docs and case files out of TODO.md"`

---

## Stufe 3: Ist-Zustand statt Journal

Für jede Aufgabe dieser Stufe gilt derselbe Ablauf: (a) wortgleich archivieren mit `git mv` oder Ausschneiden,
dann Banner ergänzen; (b) `doc_check.py move …`; (c) Ist-Zustand neu schreiben; (d) `LIMITS` absenken;
(e) Prüfungen; (f) Commit.

### Aufgabe 6: Alte Builds aus `docs/OFFSETS.md`

**Dateien:** Neu `docs/archive/OFFSETS-ALTE-BUILDS.md`, ändern `docs/OFFSETS.md`, `tools/doc_check.py` (`LIMITS`)

- [ ] **Schritt 1:** Alle `##`-Abschnitte, deren Werte nur für `26187885`, `26495302` oder `26669225` gelten
  (mindestens „Build 26669225 (`update-basis-1-0-2`)“, „Build 26495302 (`update-basis`)“,
  „Rundenende auf 26669225 (`rundenende-1-0-2`)“), wortgleich ins Archiv verschieben.
  Gemischte Abschnitte: nur Tabellenzeilen und Sätze für alte Builds verschieben.
- [ ] **Schritt 2:** `doc_check.py move docs/OFFSETS.md docs/archive/OFFSETS-ALTE-BUILDS.md --anchors <verschobene Anker>`.
- [ ] **Schritt 3: Prüfen.** `grep -nE '26187885|26495302|26669225' docs/OFFSETS.md` zeigt nur noch einen
  Verweissatz auf das Archiv. Die Werte aus `src/` müssen weiter in `docs/OFFSETS.md` stehen:
  `python3 -B tools/check_patch_registry.py` und `python3 -B -m unittest tests.test_patch_registry tests.test_patch_signatures` bestehen.
- [ ] **Schritt 4:** `LIMITS['docs/OFFSETS.md']` auf die neue Größe absenken (aufgerundet auf 1000). Commit `"Archive offsets of old game builds"`.

### Aufgabe 7: Roadmap nur auf Phasenebene

**Dateien:** Neu `docs/archive/ROADMAP.md`, ändern `docs/ROADMAP.md`, `tools/tasks.py` (Text in `cmd_done`), `CLAUDE.md`, `docs/PROZESS.md`

- [ ] **Schritt 1:** `git mv docs/ROADMAP.md docs/archive/ROADMAP.md`, Banner ergänzen,
  `doc_check.py move docs/ROADMAP.md docs/archive/ROADMAP.md`.
- [ ] **Schritt 2:** Neue `docs/ROADMAP.md` höchstens 8 000 Bytes: Mermaid-Diagramm aus „Gesamtweg“ (nur Phasen und Gates),
  „Legende“, darunter höchstens 15 Zeilen „Stand“: was läuft, was fehlt bis Release, mit Link auf `TODO.md`.
  Keine Kennungen außer den offenen P0/P1.
- [ ] **Schritt 3:** Die Pflicht „Knoten umfärben“ entfällt. `cmd_done` gibt `Weiter: committen, PR fertigstellen.` aus.
  Den Satz in `CLAUDE.md`/`docs/PROZESS.md` entsprechend ändern.
- [ ] **Schritt 4:** `LIMITS['docs/ROADMAP.md']` entfernen (Standardgrenze reicht). Prüfungen, Commit `"Reduce roadmap to phase graph"`.

### Aufgabe 8: `GAME_ANALYSIS.md` in Themen-Dateien

**Dateien:**
- Verschieben: `docs/GAME_ANALYSIS.md` → `docs/archive/GAME_ANALYSIS.md` (wortgleich, Banner)
- Neu: `docs/themen/README.md` (Übersicht, eine Zeile je Datei) und je Thema eine Datei:

| Datei | Inhalt (Quellabschnitte im Archiv) |
|---|---|
| `SPIEL_UND_SDK.md` | Unterstützte Spielversion, SDK-Erzeugung, Technische Basis |
| `NETZWERK.md` | Netzwerk, Steam-Lobby, Trennen, Wegfall des Clients |
| `SPIELER_UND_KAMERA.md` | Lokales Coop, Spawn, Controller-Tausch, Kamera, Charakterwahl |
| `REISE_UND_RUNDE.md` | Wege zwischen den Karten, Kartenwechsel, Rundenende, Tank/Bus, Tagesablauf, Routenkarte, Niederlage, Labyrinth, Sensenmann |
| `INVENTAR_UND_HANDEL.md` | Inventar-Unterobjekte, Lager-Edit, Werkbank, Händler, Airdrop, Run-Stand des Clients |
| `GEGNER_UND_BOSSE.md` | Gegner-Sync, Endboss, Boss-Leiste, Bosstod, Bedrohungsstufe |
| `DARSTELLUNG.md` | Sound und Effekte, Körperteile, Client-Darstellung, Porträt, Schadenszahlen |

Live-Test-Berichte (`livetest-*`, `testlauf-1111`, `integration-g2`) bekommen keine Themen-Datei, sie bleiben nur im Archiv.

**Aufbau jeder Themen-Datei** (höchstens 40 000 Bytes, Ziel 15 000):

```markdown
# <Thema>

Gilt für Build 26786272. Historie: [Archiv](../archive/GAME_ANALYSIS.md#<anker>).

## Überblick          (5–10 Sätze: Wie läuft das im Spiel, was ändert die Mod)
## Ablauf             (nummeriert oder Tabelle: native Funktionen, wer entscheidet, welche Nachricht)
## Code               (Tabelle: Datei in src/ → Aufgabe der Datei → Test)
## Belegte Grenzen    (was nicht geht oder nicht geprüft ist, je Zeile [Belegt]/[Ungeprüft])
## Offene Fehler      (nur Links auf docs/faelle/*.md)
```

- [ ] **Schritt 1: Archivieren.** `git mv`, Banner, dann `python3 -B tools/doc_check.py move docs/GAME_ANALYSIS.md docs/archive/GAME_ANALYSIS.md`.
  Erwartet: etwa 314 umgeschriebene Links. `doc_check.py check` ist danach ohne Ausgabe.
  Commit `"Archive GAME_ANALYSIS.md verbatim"`.
- [ ] **Schritt 2–8: je Thema eine Datei, ein Commit** (`"Add current-state topic doc <Thema>"`). Für jede Datei gilt:
  - Quellabschnitte im Archiv lesen, nur abschnittsweise (`sed -n '<von>,<bis>p'`, Zeilennummern über `grep -n '^##'`).
  - Nur Aussagen übernehmen, die für `26786272` gelten. Eine Aussage gilt als aktuell, wenn sie
    auf `26786272` gemessen wurde oder der zugehörige Code in `src/` sie heute umsetzt (Datei und Funktion nennen).
    Alles andere bleibt im Archiv und steht nicht in der Themen-Datei.
  - Keine Datumsangaben, keine Kennungen in Überschriften, keine Spike-Fassung als Ist-Zustand.
  - Jede Aussage unter „Code“ mit `git grep` gegen `src/` prüfen, bevor sie geschrieben wird.
- [ ] **Schritt 9: Links auf den Ist-Zustand umleiten.** Links aus `CLAUDE.md`, `docs/*.md` (ohne Archiv)
  und `src/**/README.md`, die auf ein Thema zeigen, mit `doc_check.py move docs/archive/GAME_ANALYSIS.md docs/themen/<DATEI>.md --anchors …`
  auf die neue Datei umschreiben. Links aus Fallakten und `docs/ERLEDIGT.md` dürfen ins Archiv zeigen.
- [ ] **Schritt 10: Prüfen.** `python3 -B tools/doc_check.py check` ohne Ausgabe. `git grep -c 'archive/GAME_ANALYSIS' -- CLAUDE.md 'src/**/CLAUDE.md'` liefert keine Treffer.
  `LIMITS['docs/GAME_ANALYSIS.md']` entfernen.

### Aufgabe 9: Kurze `CLAUDE.md` je Modul

**Dateien:** Neu `src/Game/CLAUDE.md`, `src/Net/CLAUDE.md`, `src/Sync/CLAUDE.md`, `src/UI/CLAUDE.md`, `src/Core/CLAUDE.md`, `src/Mod/CLAUDE.md`, `tools/CLAUDE.md`.
Ändern `tools/doc_check.py` (`LIMITS`: je Datei `3_000`).

Claude Code lädt eine verschachtelte `CLAUDE.md` erst, wenn ein Agent in diesem Ordner liest. Jede Datei hat höchstens 3 000 Bytes und diesen Aufbau:

```markdown
# <Modul>

Zweck in 2 Sätzen. Darf einbinden: <aus tests/test_module_boundaries.py ALLOWED>.

## Muster        (z. B. Game: Logik in *Policy.h mit Test in tests/GameTests, Hook in *Patches.cpp nur dünn)
## Wo was liegt  (höchstens 10 Zeilen: Datei-Gruppe → Zweck)
## Tests         (Befehl und Testordner)
## Nicht tun     (höchstens 5 Zeilen)
## Mehr          (Link auf README.md des Moduls und die passende docs/themen-Datei)
```

- [ ] **Schritt 1:** Je Modul aus dessen `README.md` und dem Code ableiten. Jede genannte Datei mit `ls` prüfen.
- [ ] **Schritt 2:** `src/Game/README.md` bleibt Nachschlagewerk. Der Abschnitt „Ohne Spike“ wandert mit Aufgabe 17 ins Archiv.
- [ ] **Schritt 3:** Prüfungen, Commit `"Add short per-module CLAUDE.md files"`.

### Aufgabe 10: Entscheidungen und Glossar

**Dateien:** Neu `docs/ENTSCHEIDUNGEN.md`, `docs/GLOSSAR.md`. Ändern: Fundstellen von „Nutzerentscheidung vom“ / „Nutzervorgabe vom“ außerhalb des Archivs.

- [ ] **Schritt 1: Entscheidungen sammeln.** `git grep -n -E 'Nutzer(entscheidung|vorgabe) vom' -- ':!docs/archive'` liefert die Fundstellen (heute 33).
  Jede Entscheidung bekommt in `docs/ENTSCHEIDUNGEN.md` einen Eintrag `## E-<nn>: <Titel>` mit drei Zeilen: Datum, Entscheidung, Folge.
  Doppelte Entscheidungen fasst man zusammen. Spätere Entscheidungen, die frühere ersetzen, markieren diese mit „ersetzt durch E-<nn>“.
  Die Entscheidungen aus Aufgabe 0 kommen dazu.
- [ ] **Schritt 2: Fundstellen ersetzen.** Jede Fundstelle wird zu `[E-<nn>](docs/ENTSCHEIDUNGEN.md#e-nn-titel)`, Pfad relativ.
- [ ] **Schritt 3: Glossar.** `docs/GLOSSAR.md` als Tabelle `Begriff in der Doku | Bezeichner im Spiel/Code | Bedeutung`,
  mindestens für: Lager, Rucksack, Routenkarte, Rundenende, Rundenübersicht, Werkbank, Händler, Airdrop, Itemsack,
  MetaBase, Bus, Tank, Labyrinth, Sensenmann, Edit, Bestätigungstest, Spike, Kontrollkanal.
  Jeden Bezeichner mit `git grep -n <Bezeichner> src` belegen.
- [ ] **Schritt 4:** Prüfungen, Commit `"Collect user decisions and add glossary"`.

### Aufgabe 11: `CLAUDE.md` kürzen und Kontext-Landkarte

**Dateien:** Ändern `CLAUDE.md`, `AGENTS.md`, `tools/doc_check.py` (`LIMITS['CLAUDE.md'] = 4_000`). Neu `docs/KONTEXT.md`.

- [ ] **Schritt 1: `docs/KONTEXT.md`** als Tabelle `Thema | Ist-Zustand | Code | Tests | Offene Fälle` mit je einer Zeile
  pro Datei aus `docs/themen/`, dazu Zeilen für Protokoll (`docs/CONTROL_PROTOCOL.md`), Offsets, Testen, Prozess.
  Darunter die Leseregel: „Archiv, `docs/ERLEDIGT.md` und Dateien über 60 KB nie ganz lesen, nur per `grep -n` und Abschnitt.“
- [ ] **Schritt 2: `CLAUDE.md` höchstens 4 000 Bytes.** Inhalt:
  1. Projekt in 2 Sätzen, Build `26786272`.
  2. Fokus: Run zu zweit zuerst, neue Kennungen nur für P0/P1 oder auf Anweisung (Kern der heutigen Regel).
  3. Architekturprinzip in 5 Zeilen, Link auf `docs/CLIENT_LOCAL_AUDIT.md`.
  4. Harte Regeln: nichts aus dem Spiel ins Repo, eine Spielsitzung pro PC, Exe nie von Hand mit Spike, kein `-unattended`, `ModVersion` bei Protokolländerung.
  5. Ablauf in 4 Zeilen: `tasks.py list --short` → `claim` → Arbeit nach `docs/PROZESS.md` → `done`.
  6. Link auf `docs/KONTEXT.md` als Einstieg für jedes Thema.
  Alles Weitere aus der heutigen `CLAUDE.md` steht schon in `docs/PROZESS.md` oder wandert dorthin, wortgleich.
- [ ] **Schritt 3:** `AGENTS.md` bleibt ein Verweis auf `CLAUDE.md`.
- [ ] **Schritt 4:** Prüfungen, Commit `"Shrink CLAUDE.md and add context map"`.

---

## Stufe 4: Prozess

### Aufgabe 12: Obergrenze für Bestätigungstests

**Braucht:** Entscheidung aus Aufgabe 0, Schritt 3, ausgeführt mit `tasks.py tests pass|reopen`.

**Dateien:** Ändern `tools/task_status.py` (`parse` oder `validate_graph`), `tests/test_task_status.py`

**Schnittstellen:** `MAX_PENDING = 3`. `parse` wirft `ValueError('Mehr als 3 Bestätigungstests offen: <kennungen>')`.
`confirmation_update(..., 'pending', ...)` lehnt mit derselben Meldung ab, wenn danach mehr als 3 offen wären.

- [ ] **Schritt 1: Fehlschlagende Tests:** `test_more_than_three_pending_rejected`, `test_pending_action_rejected_when_limit_reached`.
- [ ] **Schritt 2–4:** fehlschlagen lassen, umsetzen, bestehen lassen.
- [ ] **Schritt 5:** `python3 -B tools/task_status.py check` besteht mit dem echten `TODO.md`.
- [ ] **Schritt 6:** Commit `"Limit open confirmation tests to three"`.

---

## Stufe 5: Werkzeuge für weniger Kontext

### Aufgabe 13: Log-Zusammenfassung

**Dateien:** Neu `tools/log_summary.py`, `tests/test_log_summary.py`. Ändern `tools/spike/Collect-Logs.ps1`, `docs/TESTING.md` (eine Zeile), `tools/README.md`.

**Schnittstellen:**
- Logformat der Mod (`src/Core/Log.cpp`): `YYYY-MM-DD HH:MM:SS.fff [<thread>] [<LEVEL>] <text>`
- `summarize(lines: Iterable[str], limit: int = 200) -> list[str]`. Behalten werden:
  - `[WARN]` und `[ERROR]`
  - Zeilen mit `EXCEPTION`, `Absturz` oder `Crash`
  - Zeilen mit `Liste ` und zugleich ` gesetzt` oder ` ausgelassen`
  - die erste und die letzte Zeile des Logs

  Folgen gleicher Nachrichten (Text nach `] [LEVEL] `) werden zu einer Zeile mit Zusatz ` (×<n>)`.
  Ab `limit` Zeilen ist Schluss, die letzte Zeile lautet dann `… <k> weitere Zeilen`.
- CLI: `log_summary.py <log> [--limit N]` gibt auf stdout aus.

- [ ] **Schritt 1: Fehlschlagende Tests:**

```python
L = '2026-11-24 15:41:00.123 [42] [{lvl}] {msg}'
def test_keeps_warnings_errors_and_patch_lists(self):
    out = summarize([L.format(lvl='INFO', msg='Start'), L.format(lvl='INFO', msg='Tick'),
                     L.format(lvl='INFO', msg='Liste Reward gesetzt'), L.format(lvl='WARN', msg='x'),
                     L.format(lvl='INFO', msg='Ende')])
    self.assertEqual(len(out), 4)  # Start, Liste, WARN, Ende; "Tick" fällt weg

def test_collapses_repeats(self):
    out = summarize([L.format(lvl='WARN', msg='gleich')] * 5)
    self.assertEqual(out, [L.format(lvl='WARN', msg='gleich') + ' (×5)'])

def test_limit_adds_rest_line(self):
    out = summarize([L.format(lvl='ERROR', msg=f'e{i}') for i in range(10)], limit=3)
    self.assertEqual(out[-1], '… 7 weitere Zeilen')
```

- [ ] **Schritt 2–4:** fehlschlagen lassen, umsetzen, bestehen lassen.
- [ ] **Schritt 5:** `Collect-Logs.ps1` schreibt für jedes gesammelte Mod-Log zusätzlich `<log>.summary.txt`
  mit `py -B tools/log_summary.py`. In `docs/TESTING.md` steht dazu ein Satz: „Zuerst `*.summary.txt` lesen, das volle Log nur abschnittsweise.“
- [ ] **Schritt 6:** Commit `"Add mod log summary for agents"`.

### Aufgabe 14: Ein Einstieg für die Probe-Skripte

**Dateien:** Neu `tools/spike/probe.py`, `tests/test_probe.py`. Ändern: Modul-Docstrings der Skripte in `tools/spike/*.py`, `tools/README.md`.

**Schnittstellen:**
- `scripts(folder: Path) -> dict[str, str]`: Name ohne `.py` → erste Zeile des Modul-Docstrings, für alle `*.py` außer `probe.py`.
- CLI: `probe.py list` (eine Zeile je Skript, alphabetisch), `probe.py <name> [args…]` führt das Skript mit
  `runpy.run_path(..., run_name='__main__')` aus, `sys.argv` gesetzt.

- [ ] **Schritt 1: Fehlschlagende Tests:** `test_every_probe_script_has_docstring` (über den echten Ordner `tools/spike`, Meldung nennt die Skripte ohne Docstring)
  und `test_list_is_sorted_one_line_each` (temporärer Ordner mit zwei Skripten).
- [ ] **Schritt 2–4:** fehlschlagen lassen, `probe.py` umsetzen, fehlende Docstrings ergänzen (je ein Satz: was das Skript misst), bestehen lassen.
- [ ] **Schritt 5:** Commit `"Add probe.py entry point for spike probe scripts"`.

### Aufgabe 15: Wiederkehrende Abläufe als Projekt-Skills

**Dateien:** Neu `.claude/skills/fehler-untersuchen/SKILL.md`, `.claude/skills/live-test-auswerten/SKILL.md`,
`.claude/skills/patch-anlegen/SKILL.md`. Ändern `docs/PROZESS.md` (ein Satz je Skill mit Link).

Skills lädt Claude Code erst, wenn die Beschreibung passt. So stehen Abläufe nicht dauerhaft im Kontext.
Jede Datei hat höchstens 4 000 Bytes, YAML-Kopf mit `name` und `description` (ein Satz: wann der Skill gilt),
danach nummerierte Schritte mit Befehlen. Die Schritte verlinken auf bestehende Doku statt sie zu kopieren.

| Skill | Gilt, wenn | Schritte stammen aus |
|---|---|---|
| `fehler-untersuchen` | ein Absturz oder Fehlverhalten einer Kennung untersucht wird | Fallakten-Vorlage (Aufgabe 5), `[Belegt]`/`[Vermutung]`, Log-Zusammenfassung (Aufgabe 13), Regressionstest-Regel |
| `live-test-auswerten` | Logs einer Sitzung auf einem oder zwei PCs ausgewertet werden | `docs/TESTING.md#ergebnisse-festhalten`, `Collect-Logs.ps1`, `tasks.py tests pass/update/reopen` |
| `patch-anlegen` | ein neuer Hook oder Byte-Patch in `src/Game` entsteht | Abschnitt „Muster für neue Adapter“ in `src/Game/README.md`, `docs/OFFSETS.md`, `tools/check_patch_registry.py`, `ModVersion`-Regel |

- [ ] **Schritt 1:** Die drei Dateien schreiben. Jeden genannten Befehl und Pfad mit `ls` oder `--help` prüfen.
- [ ] **Schritt 2:** Prüfen, dass `.gitignore` `.claude/skills/` nicht ausschließt (`git check-ignore -v .claude/skills/x/SKILL.md` liefert nichts).
- [ ] **Schritt 3:** Commit `"Add project skills for debugging, live test review and patches"`.

---

## Stufe 6: Spike auflösen

### Aufgabe 16: Spike-Bedarf auf `26786272` prüfen

**Braucht:** Windows-PC mit Spiel. Eine Sitzung pro PC, `Start-Session.ps1` mit `-Brief`, nie `-unattended`.

**Dateien:** Ändern `docs/faelle/projekt-aufraeumen-spike.md` (Fallakte nach Vorlage)

- [ ] **Schritt 1: Kopplung auflisten.** `git grep -n 'spikes/a-connect' -- src tools tests .github` ausführen und
  jede Fundstelle in der Fallakte einordnen: Kommentar, Test-Include (z. B. `tests/round_phase.cpp` bindet
  `research/spikes/a-connect/RoundPhase.h` ein), Build-Skript oder Laufzeitbedarf.
- [ ] **Schritt 2: Ohne Spike spielen.** Ablauf wie in `docs/TESTING.md#nur-mod-ohne-spike`, aber mit
  `Run-Route.ps1 -Start -NoSpike` über mindestens Tag 1 bis Rundenende inklusive Lager und Routenkarte beim Client.
  Ergebnis in die Fallakte, mit Log-Zusammenfassung aus Aufgabe 13.
- [ ] **Schritt 3: Doppelte Header vergleichen.** Für jeden Header, der in Spike und `src/` liegt (mindestens
  `RemoteP2Policy.h`), mit `diff` festhalten, ob `src/` alle Fälle der Spike-Fassung abdeckt.
- [ ] **Schritt 4:** Ergebnis: Liste „in die Mod übernehmen“ (mit Kennung, nur wenn der Run zu zweit sie braucht) oder „entfällt“.
  Commit `"Record spike dependencies on build 26786272"`.

### Aufgabe 17: Spike aus `main` entfernen

**Braucht:** Aufgabe 16 ohne offenen Laufzeitbedarf, oder dieser Bedarf ist als eigene Kennung erledigt.

**Dateien:** Löschen `research/spikes/a-connect/`. Ändern: die Fundstellen aus Aufgabe 16, `tools/spike/Start-Session.ps1`
(Spike-Schalter), `tests/Run-*.ps1` und `tests/*.cpp`, die Spike-Header einbinden, `src/Game/README.md` (Abschnitt
„Ohne Spike“ ins Archiv), `research/README.md`.

- [ ] **Schritt 1: Tag setzen.** `git tag spike-final` auf dem letzten Commit mit Spike, Push des Tags nur nach Zustimmung des Menschen.
- [ ] **Schritt 2: Header umziehen.** Header, die Tests unter `tests/` brauchen und die Mod nutzt, kommen nach
  `src/Game/`. Die Fassung in `src/` gilt. Tests, die nur Spike-Logik prüfen, werden mit dem Spike gelöscht.
- [ ] **Schritt 3: Skripte.** Den Schalter `-NoSpike` als Standard behandeln. Gibt es einen Schalter `-Spike`, entfernt
  ihn die Aufgabe samt `Build-Spike.ps1`. Werkzeuge in `tools/spike/` behalten ihren Ordnernamen
  (Umbenennen ist nicht Teil dieser Aufgabe).
- [ ] **Schritt 4: Prüfen.** `git grep -n 'spikes/a-connect'` liefert nur Treffer unter `docs/archive/`.
  Windows-CI (`ci.yml`) baut und alle Testprogramme laufen. Ein Spieltest wie Aufgabe 16, Schritt 2 läuft ohne Absturz.
- [ ] **Schritt 5:** Commit `"Remove spike from main (tag spike-final)"`.

---

## Folgeplan (nicht Teil dieses Plans)

Ordnerstruktur `src/Game/` nach Fachgebiet (`Player/`, `Inventory/`, `Reward/`, `Enemy/`, `Map/`, `Travel/`, `UI/`),
Aufteilung von `src/Game/RewardPatches.cpp` (2417 Zeilen) und Erzeugung der Patch- und Nachrichtentabellen in der Doku
aus dem Code. Das berührt `Game.vcxproj`, Includes und Tests. Es bekommt erst nach Stufe 1 bis 6 einen eigenen Plan,
weil die Themen-Dateien aus Aufgabe 8 dann die Ordnernamen vorgeben.

## Ergebnis nach Abschluss

| Größe | Heute | Ziel |
|---|---|---|
| Markdown außerhalb `docs/archive/` | 1,45 MB | unter 300 KB |
| `CLAUDE.md` | 9 KB | höchstens 4 KB |
| `TODO.md` | 119 KB | unter 15 KB |
| Pflichtlektüre vor Arbeitsbeginn | 20–40k Tokens | unter 5k Tokens (`CLAUDE.md`, `tasks.py list --short`, eine Zeile aus `docs/KONTEXT.md`) |
| Bestätigungstests offen | 13 | höchstens 3 |
| Doppelte Codebasis Spike/Mod | ja | nein |
| Wiederwachsen | ungebremst | CI-Grenzen sinken nur |
