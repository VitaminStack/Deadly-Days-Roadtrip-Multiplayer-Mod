"""Lokale TODO-Quelle fuer Aufgabenstatus und README-Fortschritt, ohne GitHub."""
from pathlib import Path
import re
import sys


def task_states(text):
    """Explizite tasks-Marker gelten fuer die unmittelbar folgende Checkbox.

    Ein Paket kann mehrere Punkte umfassen; fertig ist es erst, wenn alle fertig sind.
    Marker ausserhalb der Phasen zaehlen nicht zum README-Fortschritt.
    """
    states = {}
    pending = None
    for line in text.splitlines():
        marker = re.fullmatch(r'<!-- tasks: ([a-z0-9 -]+) -->', line)
        if marker:
            if pending is not None:
                raise ValueError('Aufgabenmarker ohne folgende Checkbox')
            pending = marker[1].split()
            if len(pending) != len(set(pending)):
                raise ValueError('Doppelte Kennung im Aufgabenmarker')
            continue
        if pending is not None:
            box = re.match(r'^- \[([ x])\] ', line)
            if not box:
                raise ValueError('Aufgabenmarker muss direkt vor einer Checkbox stehen')
            for name in pending:
                states[name] = states.get(name, True) and box[1] == 'x'
            pending = None
    if pending is not None:
        raise ValueError('Aufgabenmarker ohne folgende Checkbox')
    return states


def phase_counts(text):
    counts = {}
    phase = None
    for line in text.splitlines():
        if line.startswith('## '):
            match = re.match(r'^## Phase ([0-6]) – ', line)
            phase = int(match[1]) if match else None
            if phase is not None:
                if phase in counts:
                    raise ValueError('Doppelte Phase')
                counts[phase] = [0, 0]
        box = re.match(r'^- \[([ x])\] ', line)
        if phase is not None and box:
            counts[phase][0] += box[1] == 'x'
            counts[phase][1] += 1
    if set(counts) != set(range(7)) or any(total == 0 for _, total in counts.values()):
        raise ValueError('Phasen 0 bis 6 muessen vollstaendig vorhanden sein')
    return counts


def progress_bar(done, total):
    filled = (done * 11 + total // 2) // total
    return '█' * filled + '░' * (11 - filled)


def update_readme(todo, readme):
    counts = phase_counts(todo)
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
            cells = line.rstrip('\r\n').split('|')
            if len(cells) != 6:
                raise ValueError('Unerwartetes README-Tabellenformat')
            done, total = counts[key]
            value = f'{done} von {total}'
            cells[3] = f' **{value}** ' if key == 'total' else f' {value} '
            cells[4] = f' {progress_bar(done, total)} '
            line = '|'.join(cells) + ('\n' if line.endswith('\n') else '')
        lines.append(line)
    if seen != set(counts):
        raise ValueError('README-Fortschrittstabelle unvollstaendig')
    return ''.join(lines)


def validate_tasks(todo):
    states = task_states(todo)
    names = re.findall(r'^\| `([a-z0-9-]+)` \|', todo, re.M)
    if len(names) != len(set(names)):
        raise ValueError('Kennung mehrfach im Aufgabenkatalog')
    missing = set(names) - states.keys()
    if missing:
        raise ValueError('Checkbox-Zuordnung fehlt: ' + ', '.join(sorted(missing)))
    orphaned = states.keys() - set(names)
    if orphaned:
        raise ValueError('Katalogeintrag fehlt: ' + ', '.join(sorted(orphaned)))
    section = ''
    for line in todo.splitlines():
        if line.startswith('## '):
            section = line[3:]
        row = re.match(r'^\| `([a-z0-9-]+)` \|', line)
        if row:
            name = row[1]
            if states[name] != (section == 'Abgeschlossene Pakete'):
                raise ValueError(f'{name}: Katalogabschnitt widerspricht Checkbox-Status')
    return states


def main():
    if sys.argv[1:] not in (['check'], ['update']):
        sys.exit('Aufruf: py tools/task_status.py check|update')
    root = Path(__file__).resolve().parents[1]
    todo = (root / 'TODO.md').read_text(encoding='utf-8')
    path = root / 'README.md'
    readme = path.read_text(encoding='utf-8')
    try:
        validate_tasks(todo)
        expected = update_readme(todo, readme)
    except ValueError as error:
        sys.exit(str(error))
    if expected != readme:
        if sys.argv[1] == 'check':
            sys.exit('README-Fortschritt veraltet: py tools/task_status.py update ausfuehren.')
        path.write_text(expected, encoding='utf-8')
        print('README-Fortschritt aus TODO.md aktualisiert.')
    else:
        print('TODO-Aufgaben und README-Fortschritt konsistent.')


if __name__ == '__main__':
    main()
