"""Kurzfassung eines Mod-Logs fuer Agenten: nur Warnungen, Fehler, Abstuerze, Patch-Listen, erste und letzte Zeile.

  py tools/log_summary.py <log> [--limit N]

Logformat (src/Core/Log.cpp): "YYYY-MM-DD HH:MM:SS.fff [<thread>] [<LEVEL>] <text>". Gleiche Nachrichten hintereinander
werden zu einer Zeile mit "(×n)". Das volle Log nur abschnittsweise lesen, wenn die Kurzfassung nicht reicht.
"""
from pathlib import Path
import re
import sys

LINE = re.compile(r'^\S+ \S+ \[[^\]]*\] \[(\w+)\] (.*)$')
CRASH = re.compile(r'EXCEPTION|Absturz|Crash')
PATCH_LIST = re.compile(r'Liste .* (gesetzt|ausgelassen)')


def keep(line):
    match = LINE.match(line)
    if match and match[1] in ('WARN', 'ERROR'):
        return True
    return bool(CRASH.search(line) or PATCH_LIST.search(line))


def key(line):
    match = LINE.match(line)
    return (match[1], match[2]) if match else ('', line)


def summarize(lines, limit=200):
    """Gekuerzte Zeilen; ab limit Zeilen folgt "… <k> weitere Zeilen"."""
    lines = [line.rstrip('\r\n') for line in lines]
    picked = [line for i, line in enumerate(lines) if i in (0, len(lines) - 1) or keep(line)]
    out = []
    for line in picked:
        if out and key(line) == key(out[-1][0]):
            out[-1][1] += 1
        else:
            out.append([line, 1])
    result = [line + (f' (×{n})' if n > 1 else '') for line, n in out]
    if len(result) > limit:
        result = result[:limit] + [f'… {len(result) - limit} weitere Zeilen']
    return result


def main(argv):
    if not argv or len(argv) not in (1, 3) or (len(argv) == 3 and argv[1] != '--limit'):
        print(__doc__.strip())
        return 2
    limit = int(argv[2]) if len(argv) == 3 else 200
    text = Path(argv[0]).read_bytes().decode('utf-8', errors='replace')
    sys.stdout.reconfigure(encoding='utf-8')
    print('\n'.join(summarize(text.splitlines(), limit)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
