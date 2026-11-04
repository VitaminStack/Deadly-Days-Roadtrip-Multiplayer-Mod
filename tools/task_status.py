"""Liest die Aufgabentabellen aus TODO.md, prueft sie und erzeugt die abgeleiteten Abschnitte.

  py tools/task_status.py check    Format, Abhaengigkeiten und erzeugte Abschnitte pruefen
  py tools/task_status.py update   "Jetzt bearbeitbar" in TODO.md und den Fortschritt im README neu erzeugen

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
EXTRA_SECTIONS = {'Wartung': 'Wartung', 'Optionale Optimierungen': 'Optional'}
PRIOS = ('P0', 'P1', 'P2')
MODES = ('parallel', 'zentral', 'zu zweit', 'Team')
PLAYTIMES = ('keine', 'wenig', 'mittel', 'viel')
PHASES = tuple(str(n) for n in range(7))
FREE_START = '<!-- frei:start -->'
FREE_END = '<!-- frei:end -->'
ID_PATTERN = r'[a-z0-9][a-z0-9-]*'


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
            tasks[name] = Task(name, phase, section, False, text, prio, need_list, mode, playtime)
        else:
            if len(row) != 3:
                raise ValueError(f'{where}: {name} braucht 3 Spalten')
            _, done_phase, text = row
            if done_phase not in PHASES + tuple(EXTRA_SECTIONS.values()):
                raise ValueError(f'{where}: {name}: Phase muss 0 bis 6, Wartung oder Optional sein')
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
    work = [t for t in ready if t.mode != 'Team']
    team = [t for t in ready if t.mode == 'Team']
    lines = ['| Kennung | Prio | Phase | Bearbeitung | Spielzeit |', '|---|---|---|---|---|']
    for t in work:
        lines.append(f'| `{t.name}` | {t.prio} | [{t.phase}](#{anchors[t.phase]}) | {t.mode} | {t.playtime} |')
    if not work:
        lines.append('| – | | | | |')
    lines.append('')
    lines.append('**Wartet auf das Team:** ' + (', '.join(f'`{t.name}`' for t in team) if team else 'nichts') + '.')
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


def main():
    if sys.argv[1:] not in (['check'], ['update']):
        sys.exit('Aufruf: py tools/task_status.py check|update')
    sys.stdout.reconfigure(encoding='utf-8')
    root = Path(__file__).resolve().parents[1]
    todo_path, readme_path = root / 'TODO.md', root / 'README.md'
    svg_path = root / 'docs/assets/progress.svg'
    todo = todo_path.read_bytes().decode('utf-8')
    readme = readme_path.read_bytes().decode('utf-8')
    try:
        tasks = parse(todo)
        expected_todo = update_todo(todo, tasks)
        expected_readme = update_readme(tasks, readme)
        expected_svg = progress_svg(tasks, expected_readme)
    except ValueError as error:
        sys.exit(f'TODO.md ungueltig: {error}')
    outputs = ((todo_path, todo, expected_todo), (readme_path, readme, expected_readme),
               (svg_path, svg_path.read_text(encoding='utf-8') if svg_path.exists() else '', expected_svg))
    stale = [p for p, old, new in outputs if old != new]
    if not stale:
        print('TODO.md, README-Fortschritt und SVG sind konsistent.')
    elif sys.argv[1] == 'check':
        sys.exit('Veraltet: ' + ', '.join(p.name for p in stale) + '. py tools/task_status.py update ausfuehren.')
    else:
        for path, _, new in outputs:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(new.encode('utf-8'))
        print('Aktualisiert: ' + ', '.join(p.name for p in stale))


if __name__ == '__main__':
    main()
