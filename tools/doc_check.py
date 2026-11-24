"""Prueft die versionierte Markdown-Doku: Dateigroessen und relative Links samt Anker.

  py tools/doc_check.py check    Groessen und Links pruefen, Exit 1 bei Meldungen
  py tools/doc_check.py sizes    Die 20 groessten Markdown-Dateien mit Bytes
  py tools/doc_check.py move <alt> <neu> [--anchors=a,b]
                                 Links auf <alt> (nur diese Anker) auf <neu> umschreiben. Ohne --anchors wurde die
                                 ganze Datei verschoben (vorher git mv), dann werden auch ihre eigenen Links angepasst.
  py tools/doc_check.py rebase <datei> <herkunft>
                                 Links in <datei> anpassen, deren Text wortgleich aus <herkunft> stammt

Die Grenzen in LIMITS duerfen nur sinken. Sie halten die Doku klein genug, dass Agenten sie gezielt lesen koennen.
"""
from pathlib import Path
from urllib.parse import unquote
import os
import re
import sys

if __package__:
    from .task_status import markdown_files, slug
else:
    from task_status import markdown_files, slug

DEFAULT_LIMIT = 60_000
# Obergrenzen in Bytes fuer Dateien ueber DEFAULT_LIMIT. Nur absenken, nie anheben: Jede Aufraeum-Aufgabe
# setzt ihre Datei auf die neue Groesse (auf 1000 aufgerundet) oder entfernt den Eintrag.
LIMITS = {
    'CLAUDE.md': 4_000,
    'TODO.md': 28_000,
    'docs/OFFSETS.md': 99_000,
    'src/Game/README.md': 81_000,
}
# Verschachtelte CLAUDE.md laedt Claude Code bei jeder Arbeit im Ordner, sie bleiben darum sehr kurz.
for _module in ('src/Core', 'src/Game', 'src/Mod', 'src/Net', 'src/Sync', 'src/UI', 'tools'):
    LIMITS[f'{_module}/CLAUDE.md'] = 3_000
EXCLUDED = ('docs/archive/',)
ARCHIVE_DIR = 'docs/archive/'
ARCHIVE_BANNER = '> **Archiv.** Historischer Stand, nicht aktuell. Gilt nur, wenn eine aktuelle Doku darauf verweist.'

LINK = re.compile(r'(\[[^\]]*\]\()([^)\s]+)((?:\s+"[^"]*")?\))')
CODE_SPAN = re.compile(r'`[^`]*`')
HTML_ANCHOR = re.compile(r'<a\s+(?:id|name)="([^"]+)"')
HEADING = re.compile(r'^(#{1,6})\s+(.*?)\s*#*\s*$')
FENCE = re.compile(r'^ {0,3}(`{3,}|~{3,})(.*)$')


def outside_fences(text):
    """(Zeilennummer, Zeile) aller Zeilen ausserhalb von Codebloecken."""
    fence = None
    for number, line in enumerate(text.splitlines(), 1):
        match = FENCE.match(line)
        if fence is None:
            # Wie CommonMark: Nach Backticks darf der Rest keine Backticks enthalten, sonst ist es Inline-Code.
            if match and not (match[1][0] == '`' and '`' in match[2]):
                fence = match[1]
                continue
            yield number, line
        elif match and match[1][0] == fence[0] and len(match[1]) >= len(fence) and not match[2].strip():
            fence = None


def anchors(text):
    """GitHub-Anker aller Ueberschriften und HTML-Anker; doppelte Ueberschriften bekommen -1, -2, ..."""
    found, seen = set(), {}
    for _, line in outside_fences(text):
        found.update(HTML_ANCHOR.findall(line))
        match = HEADING.match(line)
        if not match:
            continue
        heading = re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', match[2])
        base = slug(heading)
        count = seen.get(base, 0)
        seen[base] = count + 1
        found.add(base if count == 0 else f'{base}-{count}')
    return found


def check_links(root, files):
    """Meldungen fuer relative Links auf fehlende Dateien oder fehlende Anker in Markdown-Zielen."""
    root = Path(root)
    cache, found = {}, []

    def anchors_of(path):
        if path not in cache:
            cache[path] = {a.lower() for a in anchors(path.read_bytes().decode('utf-8'))}
        return cache[path]

    for name in files:
        source = root / name
        text = source.read_bytes().decode('utf-8')
        for number, line in outside_fences(text):
            for _, target, _ in LINK.findall(CODE_SPAN.sub('', line)):
                if '://' in target or target.startswith('mailto:'):
                    continue
                path_part, _, anchor = target.partition('#')
                path = (source.parent / unquote(path_part)).resolve() if path_part else source
                if not path.exists():
                    found.append(f'{name}:{number}: {target}: Datei fehlt')
                elif anchor and path.suffix == '.md' and unquote(anchor).lower() not in anchors_of(path):
                    found.append(f'{name}:{number}: {target}: Anker fehlt')
    return sorted(found)


def check_sizes(root, files, limits=None):
    """Meldungen fuer Dateien ueber ihrer Grenze; Dateien unter EXCLUDED sind ausgenommen."""
    limits = LIMITS if limits is None else limits
    found = []
    for name in files:
        if name.startswith(EXCLUDED):
            continue
        size = (Path(root) / name).stat().st_size
        limit = limits.get(name, DEFAULT_LIMIT)
        if size > limit:
            found.append(f'{name}: {size} Bytes > Grenze {limit}')
    return sorted(found)


def check_archive(root, files):
    """Meldungen fuer Archivdateien, deren erste Zeile nicht das Archiv-Banner ist."""
    found = []
    for name in files:
        if name.startswith(ARCHIVE_DIR):
            first = (Path(root) / name).read_bytes().decode('utf-8').splitlines()[:1]
            if first != [ARCHIVE_BANNER]:
                found.append(f'{name}: Archiv-Banner fehlt')
    return sorted(found)


def relative(target, start_dir):
    return Path(os.path.relpath(target, start_dir)).as_posix()


def edit_links(root, name, change):
    """Ruft change(ziel_ohne_anker, anker) fuer jeden relativen Link ausserhalb von Codebloecken auf.

    change liefert den neuen Linktext oder None. Schreibt die Datei nur bei Aenderung, Zeilenenden bleiben.
    Liefert die Zahl der geaenderten Links.
    """
    path = Path(root) / name
    text = path.read_bytes().decode('utf-8')
    lines = text.splitlines(keepends=True)
    keep = {number for number, _ in outside_fences(text)}
    count = 0

    def replace(match):
        nonlocal count
        target = match[2]
        if '://' in target or target.startswith('mailto:'):
            return match[0]
        path_part, _, anchor = target.partition('#')
        new = change(path_part, anchor)
        if new is None or new == target:
            return match[0]
        count += 1
        return match[1] + new + match[3]

    for index, line in enumerate(lines):
        if index + 1 in keep:
            spans = [m.span() for m in CODE_SPAN.finditer(line)]
            lines[index] = LINK.sub(lambda m: m[0] if any(a <= m.start() < b for a, b in spans) else replace(m), line)
    if count:
        path.write_bytes(''.join(lines).encode('utf-8'))
    return count


def rewrite_links(root, files, old, new, only_anchors=None):
    """Schreibt relative Links auf die Repo-Datei old auf new um, nur fuer only_anchors, wenn gesetzt."""
    root = Path(root).resolve()
    old_path, new_path = (root / old).resolve(), (root / new).resolve()
    total = 0
    for name in files:
        source = (root / name).resolve()

        def change(path_part, anchor, source=source):
            target = (source.parent / unquote(path_part)).resolve() if path_part else source
            if target != old_path or (only_anchors is not None and unquote(anchor) not in only_anchors):
                return None
            link = '' if new_path == source else relative(new_path, source.parent)
            return link + (f'#{anchor}' if anchor else '')
        total += edit_links(root, name, change)
    return total


def rebase_links(root, name, origin):
    """Passt die Links in name an, deren Text wortgleich aus origin stammt. Liefert die Zahl der Aenderungen."""
    root = Path(root).resolve()
    source, origin_path = (root / name).resolve(), (root / origin).resolve()
    own = {a.lower() for a in anchors(source.read_bytes().decode('utf-8'))}

    def change(path_part, anchor):
        if not path_part:
            if unquote(anchor).lower() in own:
                return None
            return relative(origin_path, source.parent) + f'#{anchor}'
        target = (origin_path.parent / unquote(path_part)).resolve()
        link = '' if target == source else relative(target, source.parent)
        return link + (f'#{anchor}' if anchor else '')
    return edit_links(root, name, change)


def run_checks(root):
    root = Path(root)
    files = [n for n in markdown_files(root) if (root / n).is_file()]
    return check_sizes(root, files) + check_archive(root, files) + check_links(root, files)


def main(argv):
    root = Path(__file__).resolve().parents[1]
    if argv == ['check']:
        found = run_checks(root)
        print('\n'.join(found) if found else 'Doku-Groessen und Links in Ordnung.')
        return 1 if found else 0
    if argv == ['sizes']:
        files = [n for n in markdown_files(root) if (root / n).is_file()]
        for name in sorted(files, key=lambda n: -(root / n).stat().st_size)[:20]:
            print(f'{(root / name).stat().st_size:>9} {name}')
        return 0
    if len(argv) in (3, 4) and argv[0] == 'move':
        anchors_arg = None
        if len(argv) == 4:
            if not argv[3].startswith('--anchors='):
                print(__doc__.strip())
                return 2
            anchors_arg = {a for a in argv[3].split('=', 1)[1].split(',') if a}
        files = [n for n in markdown_files(root) if (root / n).is_file()]
        if argv[2] not in files and (root / argv[2]).is_file():
            files.append(argv[2])
        count = rewrite_links(root, files, argv[1], argv[2], anchors_arg)
        if anchors_arg is None and (root / argv[2]).is_file():
            count += rebase_links(root, argv[2], argv[1])
        print(f'{count} Links angepasst.')
        return 0
    if len(argv) == 3 and argv[0] == 'rebase':
        print(f'{rebase_links(root, argv[1], argv[2])} Links angepasst.')
        return 0
    print(__doc__.strip())
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
