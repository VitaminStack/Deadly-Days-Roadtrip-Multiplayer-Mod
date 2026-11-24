"""Prueft die versionierte Markdown-Doku: Dateigroessen und relative Links samt Anker.

  py tools/doc_check.py check    Groessen und Links pruefen, Exit 1 bei Meldungen
  py tools/doc_check.py sizes    Die 20 groessten Markdown-Dateien mit Bytes

Die Grenzen in LIMITS duerfen nur sinken. Sie halten die Doku klein genug, dass Agenten sie gezielt lesen koennen.
"""
from pathlib import Path
from urllib.parse import unquote
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
    'CLAUDE.md': 10_000,
    'TODO.md': 120_000,
    'docs/GAME_ANALYSIS.md': 545_000,
    'docs/OFFSETS.md': 156_000,
    'docs/ROADMAP.md': 99_000,
    'src/Game/README.md': 81_000,
}
EXCLUDED = ('docs/archive/',)

LINK = re.compile(r'\[[^\]]*\]\(([^)\s]+)(?:\s+"[^"]*")?\)')
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
            for target in LINK.findall(CODE_SPAN.sub('', line)):
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


def run_checks(root):
    root = Path(root)
    files = [n for n in markdown_files(root) if (root / n).is_file()]
    return check_sizes(root, files) + check_links(root, files)


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
    print(__doc__.strip())
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
