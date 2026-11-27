"""Liest die Aufgabentabellen aus TODO.md, prueft sie und erzeugt die abgeleiteten Abschnitte.

  py tools/task_status.py check    Format, Abhaengigkeiten, Abnahme je Aufgabe, erzeugte Abschnitte und veraltete Querverweise pruefen
  py tools/task_status.py update   "Jetzt bearbeitbar" in TODO.md und den Fortschritt im README neu erzeugen

Zum Abschliessen einer Aufgabe gibt es "py tools/tasks.py done <kennung> <ergebnis>", das mark_done nutzt.

Jede Aufgabe steht genau einmal: offen in TODO.md in der Tabelle ihrer Phase, erledigt in docs/ERLEDIGT.md.
Die Werkzeuge lesen beide Dateien als einen Text (combine) und schreiben sie getrennt zurueck (split).
"""
from dataclasses import dataclass, field
from pathlib import Path
from html import escape
import re
import sys

OPEN_HEADER = '| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |'
DONE_HEADER = '| Kennung | Phase | Ergebnis |'
DONE_SECTION = 'Erledigt'
DONE_FILE = 'docs/ERLEDIGT.md'
DONE_TITLE = '# Erledigte Aufgaben'
DONE_PREAMBLE = DONE_TITLE + '\n\nAbgeschlossene Kennungen mit Ergebnis, nie erneut reservieren. Neue Zeilen setzt `py tools/tasks.py done`.\n\n'
# Alle Phasen sind abgeschlossen. Offene Arbeit steht flach unter Probleme oder Verbesserungen, ohne Abhaengigkeiten.
LIST_HEADER = '| Kennung | Prio | Aufgabe | Bearbeitung | Spielzeit |'
LIST_SECTIONS = {'Probleme': 'Problem', 'Verbesserungen': 'Verbesserung'}
EXTRA_SECTIONS = {'Wartung': 'Wartung', 'Optionale Optimierungen': 'Optional', 'Real Coop logic': 'Coop'}
PRIOS = ('P0', 'P1', 'P2')
MODES = ('parallel', 'zentral', 'zu zweit', 'Team')
PLAYTIMES = ('keine', 'wenig', 'mittel', 'viel')
PHASES = tuple(str(n) for n in range(7))
GATE_PHASES = PHASES[:6]
OPTIONAL_PHASE = 'Optional'
FREE_START = '<!-- frei:start -->'
FREE_END = '<!-- frei:end -->'
ID_PATTERN = r'[a-z0-9][a-z0-9-]*'
MAX_TASK_TEXT = 800
ACCEPTANCE_MARK = '**Fertig, wenn**'
# Bestaetigungstests sind nur Info: Die Aufgabe ist erledigt, der offene Test steht in docs/ERLEDIGT.md.
INFO_SECTION = 'Bestätigungstests ausstehend'
INFO_HEADER = '| Kennung | Im Spiel prüfen |'
INFO_INTRO = ('Spicker für Live-Sitzungen: was bei erledigten Aufgaben noch im Spiel zu prüfen ist, ohne Datum und Vorgeschichte. '
              'Nur Info, blockiert nichts. Pflege mit `py tools/tasks.py tests update|pass|reopen`.')
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
            phase = match[1] if match else EXTRA_SECTIONS.get(section) or LIST_SECTIONS.get(section)
            if phase is not None:
                sections[phase] = section
            table = None
            continue
        if line.startswith('| Kennung |'):
            if line == OPEN_HEADER and phase is not None and section not in LIST_SECTIONS:
                table = 'open'
            elif line == LIST_HEADER and section in LIST_SECTIONS:
                table = 'list'
            elif line == DONE_HEADER and section == DONE_SECTION:
                table = 'done'
            elif line == INFO_HEADER and section == INFO_SECTION:
                table = 'info'
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
        if table == 'info':
            if len(row) != 2:
                raise ValueError(f'{where}: {name} braucht 2 Spalten')
            if name not in tasks or not tasks[name].done:
                raise ValueError(f'{where}: {name}: Bestätigungstest für eine nicht erledigte Aufgabe')
            tasks[name].confirmation = row[1]
            continue
        if name in tasks:
            raise ValueError(f'{where}: Kennung {name} steht mehrfach in TODO.md')
        if table in ('open', 'list'):
            if table == 'list':
                if len(row) != 5:
                    raise ValueError(f'{where}: {name} braucht 5 Spalten')
                _, prio, text, mode, playtime = row
                needs = '–'
            elif len(row) != 6:
                raise ValueError(f'{where}: {name} braucht 6 Spalten')
            else:
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
            # Alter Stand (etwa origin/main vor dem Umbau): offene Zeile mit Bestaetigungstest lesen, check lehnt sie ab.
            confirmation = text[len(CONFIRMATION_PREFIX):].partition(CONFIRMATION_TASK)[0] if text.startswith(CONFIRMATION_PREFIX) else ''
            tasks[name] = Task(name, phase, section, False, text, prio, need_list, mode, playtime, confirmation)
        else:
            if len(row) != 3:
                raise ValueError(f'{where}: {name} braucht 3 Spalten')
            _, done_phase, text = row
            if done_phase not in PHASES + tuple(EXTRA_SECTIONS.values()) + tuple(LIST_SECTIONS.values()):
                raise ValueError(f'{where}: {name}: Phase muss 0 bis 6, Wartung, Optional, Coop, Problem oder Verbesserung sein')
            tasks[name] = Task(name, done_phase, DONE_SECTION, True, text)
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


def deferred(task, tasks):
    """Optionale Aufgaben warten, bis Phase 0 bis 5 erledigt sind, also der Run zu zweit laeuft."""
    return task.phase == OPTIONAL_PHASE and any(not t.done for t in tasks.values() if t.phase in GATE_PHASES)


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


SECTION_RANK = {p: i for i, p in enumerate(PHASES + tuple(EXTRA_SECTIONS.values()) + tuple(LIST_SECTIONS.values()))}
# Wer neben anderen laufen kann, steht vor Aufgaben, die gemeinsame Schnittstellen festlegen oder zwei PCs brauchen.
MODE_ORDER = {'parallel': 0, 'zu zweit': 1, 'zentral': 2, 'Team': 3}


def work_order(task):
    """Sortierschluessel der Bearbeitungsreihenfolge fuer "Wo kann ich anfangen?" und tasks.py list.

    Prio, dann Probleme vor Verbesserungen, dann schnell Abschliessbares zuerst: wenig Spielzeit vor viel,
    parallel vor zentral. Die Kennung haelt die Reihenfolge stabil.
    """
    return (PRIOS.index(task.prio), SECTION_RANK[task.phase], PLAYTIMES.index(task.playtime),
            MODE_ORDER[task.mode], task.name)


def free_block(todo, tasks):
    """Inhalt zwischen den frei-Markern: offene Aufgaben, deren Voraussetzungen erledigt sind."""
    anchors = {}
    for line in todo.splitlines():
        if line.startswith('## '):
            heading = line[3:].strip()
            match = re.match(r'^Phase ([0-6]) – ', heading)
            key = match[1] if match else EXTRA_SECTIONS.get(heading) or LIST_SECTIONS.get(heading)
            if key:
                anchors[key] = slug(heading)
    ready = sorted((t for t in tasks.values() if not t.done and not blockers(t, tasks)), key=work_order)
    held = [t for t in ready if deferred(t, tasks)]
    ready = [t for t in ready if t not in held]
    work = [t for t in ready if t.mode != 'Team']
    team = [t for t in ready if t.mode == 'Team']
    lines = ['| Kennung | Prio | Phase | Bearbeitung | Spielzeit |', '|---|---|---|---|---|']
    for t in work:
        lines.append(f'| `{t.name}` | {t.prio} | [{t.phase}](#{anchors[t.phase]}) | {t.mode} | {t.playtime} |')
    if not work:
        lines.append('| – | | | | |')
    lines.append('')
    lines.append('**Wartet auf das Team:** ' + (', '.join(f'`{t.name}`' for t in team) if team else 'nichts') + '.')
    if held:
        lines.append('')
        lines.append(f'**Zurückgestellt, bis Phase 0 bis 5 erledigt sind:** {len(held)} Aufgaben unter '
                     f'[Optionale Optimierungen](#{anchors[OPTIONAL_PHASE]}).')
    pending = [t for t in tasks.values() if t.confirmation]
    lines.append('')
    lines.append(f'**Bestätigungstests ausstehend (nur Info, blockiert nichts):** {len(pending)}, siehe '
                 f'[docs/ERLEDIGT.md](docs/ERLEDIGT.md#{slug(INFO_SECTION)}).')
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
        match = re.match(r'^\| \[([0-6])\]\(', line)
        key = int(match[1]) if match else 'total' if line.startswith('| **Total** |') else None
        if key is not None:
            if key in seen:
                raise ValueError('Doppelte README-Fortschrittszeile')
            seen.add(key)
            row = line.rstrip('\r\n').split('|')
            if len(row) != 6:
                raise ValueError('Unerwartetes README-Tabellenformat')
            done, total = counts[key]
            value = f'{done} of {total}'
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
        match = re.match(r'^\| \[([0-6])\]\(', line)
        if match:
            phase = int(match[1])
            rows.append((str(phase), cells(line)[1], *counts[phase]))
    rows.append(('Total', '', sum(v[0] for v in counts.values()), sum(v[1] for v in counts.values())))
    height = 52 + 48 * len(rows)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="{height}" viewBox="0 0 1000 {height}" role="img" aria-labelledby="title desc">',
           '  <title id="title">Project progress per phase</title>',
           '  <desc id="desc">Fully completed phases have a green background. The linked text table is in the README.</desc>',
           '  <g font-family="Segoe UI, sans-serif" font-size="16" fill="#111827">',
           '    <rect width="1000" height="52" fill="#1f2937"/>']
    for x, label in ((20, 'Phase'), (125, 'Goal'), (490, 'Done'), (630, 'Progress')):
        svg.append(f'    <text x="{x}" y="33" fill="#ffffff" font-weight="600">{label}</text>')
    for index, (phase, goal, done, total) in enumerate(rows):
        y = 52 + index * 48
        complete = done == total
        background = '#dcfce7' if complete else '#f3f4f6' if phase == 'Total' else '#ffffff'
        bar = '#15803d' if complete else '#64748b'
        status = 'Completed' if complete else f'{done * 100 // total} %'
        svg += [f'    <g id="phase-{phase.lower()}">',
                f'      <rect y="{y}" width="1000" height="48" fill="{background}"/>',
                f'      <path d="M0 {y + 48}H1000" stroke="#d1d5db"/>']
        for x, label in ((20, phase), (125, goal), (490, f'{done} of {total}')):
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


def set_info(text, name, note):
    """Setzt (note) oder entfernt (None) die Info-Zeile von name im Abschnitt Bestaetigungstests ausstehend."""
    newline = '\r\n' if '\r\n' in text else '\n'
    lines = text.splitlines()
    rows, section, start = {}, '', None
    for index, line in enumerate(lines):
        if line.startswith('## '):
            section = line[3:].strip()
            if section == INFO_SECTION:
                start = index
        elif section == INFO_SECTION:
            match = re.match(rf'^\| `({ID_PATTERN})` \|', line)
            if match:
                rows[match[1]] = index
    if note is None:
        if name in rows:
            del lines[rows[name]]
    elif name in rows:
        lines[rows[name]] = f'| `{name}` | {note} |'
    else:
        if start is None:
            while lines and not lines[-1].strip():
                lines.pop()
            lines += ['', f'## {INFO_SECTION}', '', INFO_INTRO, '', INFO_HEADER, '|---|---|']
            start = len(lines) - 3
        names = sorted(list(rows) + [name])
        after = names.index(name)
        if after < len(rows):
            insert_at = rows[sorted(rows)[after]]
        else:
            insert_at = max(rows.values()) + 1 if rows else start + 5
        lines.insert(insert_at, f'| `{name}` | {note} |')
    return newline.join(lines) + (newline if text.endswith(('\n', '\r')) else '')


def append_result(text, name, suffix):
    """Haengt suffix an das Ergebnis der erledigten Zeile von name."""
    lines = text.splitlines(keepends=True)
    section = ''
    for index, line in enumerate(lines):
        if line.startswith('## '):
            section = line[3:].strip()
        elif section == DONE_SECTION and line.startswith(f'| `{name}` |'):
            body = line.rstrip('\r\n')
            lines[index] = body[:-1].rstrip() + f' {suffix} |' + line[len(body):]
            return ''.join(lines)
    raise ValueError(f'{name}: keine Zeile unter Erledigt')


def confirmation_update(todo, names, action, note):
    """Bestaetigungstests als Info pflegen. pending schliesst die Aufgabe ab, damit sie nichts blockiert."""
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
        if action == 'pending':
            if task is None or task.done:
                raise ValueError(f'{name}: keine offene Aufgabe')
            if task.mode == 'Team':
                raise ValueError(f'{name}: Teamentscheidung, kein Bestaetigungstest')
            waiting = blockers(task, tasks)
            if waiting:
                raise ValueError(f'{name}: blockiert, braucht ' + ', '.join(waiting))
        elif task is None or not task.confirmation:
            raise ValueError(f'{name}: kein ausstehender Bestaetigungstest')
    for name in names:
        if action == 'pending':
            todo = mark_done(todo, name, f'{tasks[name].text} Bestätigungstest ausstehend, siehe unten.')
            todo = set_info(todo, name, note)
        elif action == 'update':
            todo = set_info(todo, name, note)
        else:
            label = 'Bestätigt' if action == 'pass' else 'Bestätigungstest fehlgeschlagen'
            todo = append_result(set_info(todo, name, None), name, f'{label}: {note}')
    parse(todo)
    return todo


FUTURE_REFERENCE = re.compile(
    rf'\b(?:folgt|folgen|kommt|kommen)(?: erst| später| noch| dann)? (?:mit|in|über|durch) `({ID_PATTERN})`'
    rf'|\bbis `({ID_PATTERN})`(?! \(#)')
SKIPPED_DIRS = ('third_party/', 'research/local/', '.worktrees/', '.claude/')


def combine(todo, done):
    """Ein Text aus TODO.md und docs/ERLEDIGT.md, wie ihn parse, mark_done und update_todo erwarten."""
    if not done:
        return todo
    if not todo.endswith('\n'):
        todo += '\n'
    return todo + done


def split(text):
    """(TODO.md, docs/ERLEDIGT.md) aus dem kombinierten Text.

    Steht der Abschnitt "Erledigt" noch in TODO.md (alter Stand), wird er herausgeschnitten und bekommt den Vorspann.
    """
    lines = text.splitlines(keepends=True)
    for index, line in enumerate(lines):
        if line.rstrip('\r\n') == DONE_TITLE:
            return ''.join(lines[:index]), ''.join(lines[index:])
    start = next((i for i, line in enumerate(lines) if line.rstrip('\r\n') == f'## {DONE_SECTION}'), None)
    if start is None:
        return text, ''
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith('## ')
                and lines[i].rstrip('\r\n') != f'## {INFO_SECTION}'), len(lines))
    newline = '\r\n' if lines[start].endswith('\r\n') else '\n'
    section = ''.join(lines[start:end]).rstrip('\r\n') + newline
    return ''.join(lines[:start] + lines[end:]), DONE_PREAMBLE.replace('\n', newline) + section


def newline_of(path, default='\n'):
    if not path.exists():
        return default
    return '\r\n' if b'\r\n' in path.read_bytes() else '\n'


def read_local(root):
    """Kombinierter Text aus TODO.md und docs/ERLEDIGT.md im Arbeitsordner; fehlt ERLEDIGT.md, nur TODO.md."""
    root = Path(root)
    todo = (root / 'TODO.md').read_bytes().decode('utf-8')
    done_path = root / DONE_FILE
    return combine(todo, done_path.read_bytes().decode('utf-8') if done_path.exists() else '')


def write_local(root, text):
    """Schreibt den kombinierten Text getrennt zurueck, jede Datei im Zeilenende ihres bisherigen Stands."""
    root = Path(root)
    for name, part in zip(('TODO.md', DONE_FILE), split(text)):
        path = root / name
        newline = newline_of(path)
        part = part.replace('\r\n', '\n').replace('\n', newline)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(part.encode('utf-8'))


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
            if path in ('TODO.md', DONE_FILE) and line.startswith('| `'):
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
    readme_path = root / 'README.md'
    svg_path = root / 'docs/assets/progress.svg'
    text = read_local(root)
    readme = readme_path.read_bytes().decode('utf-8')
    tasks = parse(text)
    expected_text = update_todo(text, tasks)
    expected_readme = update_readme(tasks, readme)
    expected_svg = progress_svg(tasks, expected_readme)
    stale = []
    if expected_text != text:
        stale += [root / name for name, old, new in zip(('TODO.md', DONE_FILE), split(text), split(expected_text))
                  if old != new]
    others = ((readme_path, readme, expected_readme),
              (svg_path, svg_path.read_text(encoding='utf-8') if svg_path.exists() else '', expected_svg))
    stale += [p for p, old, new in others if old != new]
    if stale and not check:
        if expected_text != text:
            write_local(root, expected_text)
        for path, _, new in others:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(new.encode('utf-8'))
    return stale


def check_flat_list(tasks):
    """Offene Aufgaben stehen nur unter Probleme oder Verbesserungen und haben keine Abhaengigkeiten."""
    for task in tasks.values():
        if not task.done and task.confirmation:
            raise ValueError(f'{task.name}: Bestätigungstests stehen als Info in docs/ERLEDIGT.md, nicht als offene Zeile')
        if not task.done and (task.phase not in LIST_SECTIONS.values() or task.needs):
            raise ValueError(f'{task.name}: offene Aufgaben stehen unter Probleme oder Verbesserungen, ohne Braucht')


def check_text_lengths(tasks):
    """Offene Aufgabenzeilen bleiben kurz, Details stehen in der Fallakte. Nur beim Pruefen, nicht beim Lesen."""
    for task in tasks.values():
        if not task.done and len(task.text) > MAX_TASK_TEXT:
            raise ValueError(f'{task.name}: Aufgabentext über {MAX_TASK_TEXT} Zeichen, Details in docs/faelle/{task.name}.md')


def check_acceptance(tasks):
    """Jede offene Aufgabe nennt in ihrer Zeile, woran man ihren Abschluss prueft."""
    for task in tasks.values():
        if not task.done and ACCEPTANCE_MARK not in task.text:
            raise ValueError(f'{task.name}: Aufgabentext braucht eine Abnahme mit {ACCEPTANCE_MARK}')


def check_all(root, check):
    """(veraltete Dateien, veraltete Querverweise). Wirft ValueError bei ungueltigen Aufgabentabellen."""
    tasks = parse(read_local(root))
    check_text_lengths(tasks)
    check_acceptance(tasks)
    check_flat_list(tasks)
    references = check_references(root, tasks)
    return regenerate(root, check=check), references


def main():
    if sys.argv[1:] not in (['check'], ['update']):
        sys.exit('Aufruf: py tools/task_status.py check|update')
    sys.stdout.reconfigure(encoding='utf-8')
    root = Path(__file__).resolve().parents[1]
    try:
        stale, references = check_all(root, check=sys.argv[1] == 'check')
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
