"""Liest die Aufgabentabellen aus TODO.md, prueft sie und erzeugt die abgeleiteten Abschnitte.

  py tools/task_status.py check    Format, Abhaengigkeiten, erzeugte Abschnitte und veraltete Querverweise pruefen
  py tools/task_status.py update   "Jetzt bearbeitbar" in TODO.md und den Fortschritt im README neu erzeugen

Zum Abschliessen einer Aufgabe gibt es "py tools/tasks.py done <kennung> <ergebnis>", das mark_done nutzt.

Jede Aufgabe steht genau einmal in TODO.md: offen in der Tabelle ihrer Phase, erledigt in der Tabelle "Erledigt".
"""
from dataclasses import dataclass, field
from pathlib import Path
from html import escape
import re
import sys

OPEN_HEADER = '| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |'
DONE_HEADER = '| Kennung | Phase | Ergebnis |'
DONE_SECTION = 'Erledigt'
EXTRA_SECTIONS = {'Wartung': 'Wartung', 'Optionale Optimierungen': 'Optional', 'Real Coop logic': 'Coop'}
PRIOS = ('P0', 'P1', 'P2')
MODES = ('parallel', 'zentral', 'zu zweit', 'Team')
PLAYTIMES = ('keine', 'wenig', 'mittel', 'viel')
PHASES = tuple(str(n) for n in range(7))
FREE_START = '<!-- frei:start -->'
FREE_END = '<!-- frei:end -->'
ID_PATTERN = r'[a-z0-9][a-z0-9-]*'
CONFIRMATION_PREFIX = '**Bestätigungstest ausstehend:** '
CONFIRMATION_TASK = ' **Aufgabe und Abnahme:** '


@dataclass
class Task:
    name: str
    phase: str
    section: str
    done: bool
    text: str
    prio: str = ''
    needs: list = field(default_factory=list)
    mode: str = ''
    playtime: str = ''
    confirmation: str = ''


def slug(heading):
    """GitHub-Anker einer Ueberschrift."""
    text = re.sub(r'[^\w\- ]', '', heading.strip().lower())
    return text.replace(' ', '-')


def cells(line):
    return [c.strip() for c in line.strip().strip('|').split('|')]


def parse(todo):
    """Kennung -> Task. Wirft ValueError bei jedem Formatfehler."""
    tasks, sections = {}, {}
    section, phase, table, in_free = '', None, None, False
    for number, line in enumerate(todo.splitlines(), 1):
        where = f'TODO.md Zeile {number}'
        if line == FREE_START:
            in_free = True
            continue
        if line == FREE_END:
            in_free = False
            continue
        if in_free:
            continue
        if re.match(r'^- \[[ xX]\] ', line) or '<!-- tasks:' in line:
            raise ValueError(f'{where}: Checkboxen und tasks-Marker gibt es nicht mehr, Aufgaben stehen in Tabellen')
        if line.startswith('## '):
            section = line[3:].strip()
            match = re.match(r'^Phase ([0-6]) – ', section)
            phase = match[1] if match else EXTRA_SECTIONS.get(section)
            if phase is not None:
                sections[phase] = section
            table = None
            continue
        if line.startswith('| Kennung |'):
            if line == OPEN_HEADER and phase is not None:
                table = 'open'
            elif line == DONE_HEADER and section == DONE_SECTION:
                table = 'done'
            else:
                raise ValueError(f'{where}: Aufgabentabelle mit falschem Kopf oder im falschen Abschnitt')
            continue
        if not line.startswith('|'):
            table = None
            continue
        if table is None or re.match(r'^\|[-| ]+\|$', line):
            continue
        row = cells(line)
        match = re.fullmatch(rf'`({ID_PATTERN})`', row[0])
        if not match:
            raise ValueError(f'{where}: Kennung fehlt oder ist ungueltig')
        name = match[1]
        if name in tasks:
            raise ValueError(f'{where}: Kennung {name} steht mehrfach in TODO.md')
        if table == 'open':
            if len(row) != 6:
                raise ValueError(f'{where}: {name} braucht 6 Spalten')
            _, prio, text, needs, mode, playtime = row
            if prio not in PRIOS:
                raise ValueError(f'{where}: {name}: Prio muss P0, P1 oder P2 sein')
            if mode not in MODES:
                raise ValueError(f'{where}: {name}: Bearbeitung muss parallel, zentral, zu zweit oder Team sein')
            if playtime not in PLAYTIMES:
                raise ValueError(f'{where}: {name}: Spielzeit muss keine, wenig, mittel oder viel sein')
            if needs == '–':
                need_list = []
            else:
                need_list = [n.strip() for n in needs.split(',')]
                if not all(re.fullmatch(rf'`{ID_PATTERN}`', n) for n in need_list):
                    raise ValueError(f'{where}: {name}: Braucht ist "–" oder eine Liste wie `a`, `b`')
                need_list = [n.strip('`') for n in need_list]
            confirmation = ''
            if text.startswith(CONFIRMATION_PREFIX):
                confirmation, separator, original = text[len(CONFIRMATION_PREFIX):].partition(CONFIRMATION_TASK)
                if not confirmation.strip() or not separator or not original.strip():
                    raise ValueError(f'{where}: {name}: Bestaetigungstest braucht Stand sowie Aufgabe und Abnahme')
            tasks[name] = Task(name, phase, section, False, text, prio, need_list, mode, playtime, confirmation)
        else:
            if len(row) != 3:
                raise ValueError(f'{where}: {name} braucht 3 Spalten')
            _, done_phase, text = row
            if done_phase not in PHASES + tuple(EXTRA_SECTIONS.values()):
                raise ValueError(f'{where}: {name}: Phase muss 0 bis 6, Wartung, Optional oder Coop sein')
            tasks[name] = Task(name, done_phase, DONE_SECTION, True, text)
    missing = [p for p in PHASES if p not in sections]
    if missing:
        raise ValueError('Phasen fehlen in TODO.md: ' + ', '.join(missing))
    validate_graph(tasks)
    validate_done_order(todo)
    return tasks


def validate_graph(tasks):
    for task in tasks.values():
        for need in task.needs:
            if need not in tasks:
                raise ValueError(f'{task.name}: braucht unbekannte Kennung {need}')
            if need == task.name:
                raise ValueError(f'{task.name}: braucht sich selbst')
    state = {}

    def visit(name, path):
        if state.get(name) == 'done':
            return
        if state.get(name) == 'active':
            raise ValueError('Kreis in Braucht: ' + ' -> '.join(path + [name]))
        state[name] = 'active'
        for need in tasks[name].needs:
            visit(need, path + [name])
        state[name] = 'done'

    for name in tasks:
        visit(name, [])


def validate_done_order(todo):
    names, section = [], ''
    for line in todo.splitlines():
        if line.startswith('## '):
            section = line[3:].strip()
        elif section == DONE_SECTION:
            match = re.match(rf'^\| `({ID_PATTERN})` \|', line)
            if match:
                names.append(match[1])
    if names != sorted(names):
        wrong = next(a for a, b in zip(names, sorted(names)) if a != b)
        raise ValueError(f'Erledigt ist nicht alphabetisch sortiert, zuerst falsch: {wrong}')


def blockers(task, tasks):
    return [n for n in task.needs if not tasks[n].done]


def phase_counts(tasks):
    counts = {int(p): [0, 0] for p in PHASES}
    for task in tasks.values():
        if task.phase in PHASES:
            counts[int(task.phase)][0] += task.done
            counts[int(task.phase)][1] += 1
    empty = [str(p) for p, (_, total) in counts.items() if total == 0]
    if empty:
        raise ValueError('Phasen ohne Aufgaben: ' + ', '.join(empty))
    return counts


def free_block(todo, tasks):
    """Inhalt zwischen den frei-Markern: offene Aufgaben, deren Voraussetzungen erledigt sind."""
    anchors = {}
    for line in todo.splitlines():
        if line.startswith('## '):
            heading = line[3:].strip()
            match = re.match(r'^Phase ([0-6]) – ', heading)
            key = match[1] if match else EXTRA_SECTIONS.get(heading)
            if key:
                anchors[key] = slug(heading)
    ready = [t for t in tasks.values() if not t.done and not blockers(t, tasks)]
    order = {p: i for i, p in enumerate(PHASES + tuple(EXTRA_SECTIONS.values()))}
    ready.sort(key=lambda t: (PRIOS.index(t.prio), order[t.phase], t.name))
    work = [t for t in ready if t.mode != 'Team' and not t.confirmation]
    team = [t for t in ready if t.mode == 'Team' and not t.confirmation]
    lines = ['| Kennung | Prio | Phase | Bearbeitung | Spielzeit |', '|---|---|---|---|---|']
    for t in work:
        lines.append(f'| `{t.name}` | {t.prio} | [{t.phase}](#{anchors[t.phase]}) | {t.mode} | {t.playtime} |')
    if not work:
        lines.append('| – | | | | |')
    lines.append('')
    lines.append('**Wartet auf das Team:** ' + (', '.join(f'`{t.name}`' for t in team) if team else 'nichts') + '.')
    pending = [t for t in tasks.values() if t.confirmation]
    lines.append('')
    lines.append('**Bestätigungstest ausstehend (nicht in Arbeit):** ' +
                 (', '.join(f'`{t.name}`' for t in pending) if pending else 'nichts') + '.')
    return '\n'.join(lines)


def update_todo(todo, tasks):
    start, end = todo.find(FREE_START), todo.find(FREE_END)
    if start < 0 or end < start or todo.count(FREE_START) != 1 or todo.count(FREE_END) != 1:
        raise ValueError('TODO.md braucht genau einen Block zwischen <!-- frei:start --> und <!-- frei:end -->')
    newline = '\r\n' if '\r\n' in todo else '\n'
    block = free_block(todo, tasks).replace('\n', newline)
    return todo[:start + len(FREE_START)] + newline + block + newline + todo[end:]


def progress_bar(done, total):
    filled = (done * 11 + total // 2) // total
    return '█' * filled + '░' * (11 - filled)


def update_readme(tasks, readme):
    counts = phase_counts(tasks)
    counts['total'] = [sum(v[0] for v in counts.values()), sum(v[1] for v in counts.values())]
    seen = set()
    lines = []
    for line in readme.splitlines(keepends=True):
        match = re.match(r'^\| \[([0-6])\]\(TODO.md#phase-', line)
        key = int(match[1]) if match else 'total' if line.startswith('| **Gesamt** |') else None
        if key is not None:
            if key in seen:
                raise ValueError('Doppelte README-Fortschrittszeile')
            seen.add(key)
            row = line.rstrip('\r\n').split('|')
            if len(row) != 6:
                raise ValueError('Unerwartetes README-Tabellenformat')
            done, total = counts[key]
            value = f'{done} von {total}'
            row[3] = f' **{value}** ' if key == 'total' else f' {value} '
            row[4] = f' {progress_bar(done, total)} '
            line = '|'.join(row) + line[len(line.rstrip('\r\n')):]
        lines.append(line)
    if seen != set(counts):
        raise ValueError('README-Fortschrittstabelle unvollstaendig')
    return ''.join(lines)


def progress_svg(tasks, readme):
    """GitHub entfernt Tabellen-CSS; eine lokale SVG kann ganze abgeschlossene Zeilen gruen hinterlegen."""
    readme = update_readme(tasks, readme)
    counts = phase_counts(tasks)
    rows = []
    for line in readme.splitlines():
        match = re.match(r'^\| \[([0-6])\]\(TODO.md#phase-', line)
        if match:
            phase = int(match[1])
            rows.append((str(phase), cells(line)[1], *counts[phase]))
    rows.append(('Gesamt', '', sum(v[0] for v in counts.values()), sum(v[1] for v in counts.values())))
    height = 52 + 48 * len(rows)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="{height}" viewBox="0 0 1000 {height}" role="img" aria-labelledby="title desc">',
           '  <title id="title">Projektfortschritt je Phase</title>',
           '  <desc id="desc">Vollständig abgeschlossene Phasen haben einen grünen Hintergrund. Die verlinkte Texttabelle steht im README.</desc>',
           '  <g font-family="Segoe UI, sans-serif" font-size="16" fill="#111827">',
           '    <rect width="1000" height="52" fill="#1f2937"/>']
    for x, label in ((20, 'Phase'), (125, 'Ziel'), (490, 'Erledigt'), (630, 'Fortschritt')):
        svg.append(f'    <text x="{x}" y="33" fill="#ffffff" font-weight="600">{label}</text>')
    for index, (phase, goal, done, total) in enumerate(rows):
        y = 52 + index * 48
        complete = done == total
        background = '#dcfce7' if complete else '#f3f4f6' if phase == 'Gesamt' else '#ffffff'
        bar = '#15803d' if complete else '#64748b'
        status = 'Abgeschlossen' if complete else f'{done * 100 // total} %'
        svg += [f'    <g id="phase-{phase.lower()}">',
                f'      <rect y="{y}" width="1000" height="48" fill="{background}"/>',
                f'      <path d="M0 {y + 48}H1000" stroke="#d1d5db"/>']
        for x, label in ((20, phase), (125, goal), (490, f'{done} von {total}')):
            svg.append(f'      <text x="{x}" y="{y + 30}">{escape(label)}</text>')
        svg.append(f'      <rect x="630" y="{y + 15}" width="200" height="18" rx="4" fill="#e5e7eb"/>')
        if done:
            svg.append(f'      <rect x="630" y="{y + 15}" width="{200 * done // total}" height="18" rx="4" fill="{bar}"/>')
        svg += [f'      <text x="846" y="{y + 30}">{status}</text>', '    </g>']
    return '\n'.join(svg + ['  </g>', '</svg>', ''])


def mark_done(todo, name, result):
    """Verschiebt die offene Zeile von name alphabetisch nach Erledigt, mit Phase und Ergebnis. Liefert den neuen Text."""
    task = parse(todo).get(name)
    if task is None:
        raise ValueError(f'{name} steht nicht in TODO.md')
    if task.done:
        raise ValueError(f'{name} ist schon erledigt')
    result = result.strip()
    if not result or '|' in result or '\n' in result or '\r' in result:
        raise ValueError('Ergebnis ist ein Satz ohne | und ohne Zeilenumbruch')
    newline = '\r\n' if '\r\n' in todo else '\n'
    # Patch-Werkzeuge koennen einzelne LF-Zeilen in eine CRLF-Datei einfuegen. Auch diese Aufgabe finden,
    # statt beim Abschliessen irrtuemlich nur die generierte Liste oder gar keine offene Zeile zu sehen.
    lines = todo.splitlines()
    if todo.endswith(('\n', '\r')):
        lines.append('')
    prefix = f'| `{name}` |'
    section, in_free, open_index = '', False, None
    for index, line in enumerate(lines):
        if line == FREE_START:
            in_free = True
        elif line == FREE_END:
            in_free = False
        elif line.startswith('## '):
            section = line[3:].strip()
        elif not in_free and section != DONE_SECTION and line.startswith(prefix):
            open_index = index
    if open_index is None:
        raise ValueError(f'Offene Zeile von {name} nicht gefunden')
    del lines[open_index]
    section, insert_at, last_row = '', None, None
    for index, line in enumerate(lines):
        if line.startswith('## '):
            section = line[3:].strip()
            continue
        match = re.match(rf'^\| `({ID_PATTERN})` \|', line) if section == DONE_SECTION else None
        if match:
            last_row = index
            if insert_at is None and match[1] > name:
                insert_at = index
    if last_row is None:
        raise ValueError('Tabelle Erledigt nicht gefunden')
    lines.insert(insert_at if insert_at is not None else last_row + 1, f'| `{name}` | {task.phase} | {result} |')
    return newline.join(lines)


def confirmation_update(todo, names, action, note):
    """Batch nur im Speicher validieren; Abnahme bleibt beim Warten offen und blockiert Abhaengigkeiten."""
    if action not in ('pending', 'update', 'pass', 'reopen'):
        raise ValueError('Unbekannte Bestaetigungstest-Aktion')
    if not names or len(names) != len(set(names)):
        raise ValueError('Mindestens eine Kennung, keine doppelten Kennungen')
    note = note.strip()
    if not note or any(c in note for c in ('|', '\n', '\r', CONFIRMATION_PREFIX.strip(), CONFIRMATION_TASK.strip())):
        raise ValueError('Stand/Ergebnis ist ein Satz ohne |, Zeilenumbruch oder Statusmarker')
    tasks = parse(todo)
    for name in names:
        task = tasks.get(name)
        if task is None or task.done:
            raise ValueError(f'{name}: keine offene Aufgabe')
        if task.mode == 'Team':
            raise ValueError(f'{name}: Teamentscheidung, kein Bestaetigungstest')
        if action != 'pending' and not task.confirmation:
            raise ValueError(f'{name}: kein ausstehender Bestaetigungstest')
        if action in ('pending', 'pass'):
            waiting = [n for n in blockers(task, tasks) if action != 'pass' or n not in names]
            if waiting:
                raise ValueError(f'{name}: blockiert, braucht ' + ', '.join(waiting))
    if action == 'pass':
        for name in names:
            todo = mark_done(todo, name, note)
        return todo
    replacements = {}
    for name in names:
        task = tasks[name]
        original = task.text.partition(CONFIRMATION_TASK)[2] if task.confirmation else task.text
        text = (f'{original} **Letzter Bestätigungstest:** {note}' if action == 'reopen' else
                f'{CONFIRMATION_PREFIX}{note}{CONFIRMATION_TASK}{original}')
        replacements[name] = f'| `{name}` | {task.prio} | {text} | ' + (
            ', '.join(f'`{n}`' for n in task.needs) or '–') + f' | {task.mode} | {task.playtime} |'
    in_free = False
    lines = []
    for line in todo.splitlines(keepends=True):
        if line.rstrip('\r\n') == FREE_START:
            in_free = True
        elif line.rstrip('\r\n') == FREE_END:
            in_free = False
        match = re.match(rf'^\| `({ID_PATTERN})` \|', line) if not in_free else None
        if match and match[1] in replacements:
            line = replacements[match[1]] + line[len(line.rstrip('\r\n')):]
        lines.append(line)
    result = ''.join(lines)
    parse(result)
    return result


FUTURE_REFERENCE = re.compile(
    rf'\b(?:folgt|folgen|kommt|kommen)(?: erst| später| noch| dann)? (?:mit|in|über|durch) `({ID_PATTERN})`'
    rf'|\bbis `({ID_PATTERN})`(?! \(#)')
SKIPPED_DIRS = ('third_party/', 'research/local/', '.worktrees/', '.claude/')


def markdown_files(root):
    """Versionierte Markdown-Dateien; ohne Git alle .md ausser Fremd- und lokalen Ordnern."""
    import subprocess
    try:
        listed = subprocess.run(['git', 'ls-files', '-z', '*.md'], cwd=root, capture_output=True, check=True)
        names = [n for n in listed.stdout.decode('utf-8').split('\0') if n]
    except (OSError, subprocess.CalledProcessError):
        names = [p.relative_to(root).as_posix() for p in root.rglob('*.md')]
    return sorted(n for n in names if not n.startswith(SKIPPED_DIRS))


def stale_references(tasks, documents):
    """Saetze, die eine schon erledigte Aufgabe als kuenftig ankuendigen ("kommt mit `x`", "bis `x`").

    documents: Pfad -> Text. Liefert "Pfad:Zeile: `kennung` ist erledigt: Auszug". Die Aufgabenzeilen in TODO.md
    selbst sind ausgenommen, dort beschreiben Kennungen Abhaengigkeiten und Ergebnisse. "bis `x` (#123)" nennt
    einen gemergten Stand und zaehlt nicht.
    """
    found = []
    for path, text in documents.items():
        for number, line in enumerate(text.splitlines(), 1):
            if path == 'TODO.md' and line.startswith('| `'):
                continue
            for match in FUTURE_REFERENCE.finditer(line):
                name = match[1] or match[2]
                task = tasks.get(name)
                if task and task.done:
                    found.append(f'{path}:{number}: `{name}` ist erledigt: {match[0]}')
    return found


def check_references(root, tasks):
    documents = {name: (root / name).read_text(encoding='utf-8') for name in markdown_files(root)
                 if (root / name).is_file()}
    return stale_references(tasks, documents)


def regenerate(root, check=False):
    """Erneuert die abgeleiteten Abschnitte. Liefert die veralteten Dateien; mit check wird nichts geschrieben."""
    todo_path, readme_path = root / 'TODO.md', root / 'README.md'
    svg_path = root / 'docs/assets/progress.svg'
    todo = todo_path.read_bytes().decode('utf-8')
    readme = readme_path.read_bytes().decode('utf-8')
    tasks = parse(todo)
    expected_todo = update_todo(todo, tasks)
    expected_readme = update_readme(tasks, readme)
    expected_svg = progress_svg(tasks, expected_readme)
    outputs = ((todo_path, todo, expected_todo), (readme_path, readme, expected_readme),
               (svg_path, svg_path.read_text(encoding='utf-8') if svg_path.exists() else '', expected_svg))
    stale = [p for p, old, new in outputs if old != new]
    if stale and not check:
        for path, _, new in outputs:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(new.encode('utf-8'))
    return stale


def main():
    if sys.argv[1:] not in (['check'], ['update']):
        sys.exit('Aufruf: py tools/task_status.py check|update')
    sys.stdout.reconfigure(encoding='utf-8')
    root = Path(__file__).resolve().parents[1]
    try:
        stale = regenerate(root, check=sys.argv[1] == 'check')
        references = check_references(root, parse(root.joinpath('TODO.md').read_bytes().decode('utf-8')))
    except ValueError as error:
        sys.exit(f'TODO.md ungueltig: {error}')
    if references:
        print('Veraltete Querverweise (Aufgabe schon erledigt, Text kündigt sie noch an):')
        print('\n'.join('  ' + r for r in references))
        if sys.argv[1] == 'check':
            sys.exit('Querverweise anpassen, siehe oben.')
    if not stale:
        print('TODO.md, README-Fortschritt und SVG sind konsistent.')
    elif sys.argv[1] == 'check':
        sys.exit('Veraltet: ' + ', '.join(p.name for p in stale) + '. py tools/task_status.py update ausfuehren.')
    else:
        print('Aktualisiert: ' + ', '.join(p.name for p in stale))

if __name__ == '__main__':
    main()
