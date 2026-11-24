"""Einstieg fuer die Probe-Skripte in diesem Ordner, ohne ihren Quelltext zu lesen.

  py tools/spike/probe.py list               Alle Proben mit einem Satz, was sie messen
  py tools/spike/probe.py <name> [args...]   Probe ausfuehren, etwa: probe.py players <pid>
"""
from pathlib import Path
import ast
import runpy
import sys

HERE = Path(__file__).resolve().parent


def scripts(folder):
    """Name ohne .py -> erste Zeile des Modul-Docstrings (leer, wenn keiner da ist). Fuehrt nichts aus."""
    found = {}
    for path in sorted(Path(folder).glob('*.py')):
        if path.name == 'probe.py':
            continue
        doc = ast.get_docstring(ast.parse(path.read_text(encoding='utf-8'))) or ''
        found[path.stem] = doc.strip().splitlines()[0].strip() if doc.strip() else ''
    return found


def list_lines(folder):
    found = scripts(folder)
    width = max((len(name) for name in found), default=0)
    return [f'{name:<{width}}  {summary}' for name, summary in found.items()]


def main(argv):
    if argv == ['list']:
        sys.stdout.reconfigure(encoding='utf-8')
        print('\n'.join(list_lines(HERE)))
        return 0
    if argv and argv[0] in scripts(HERE):
        sys.argv = [str(HERE / f'{argv[0]}.py')] + argv[1:]
        sys.path.insert(0, str(HERE))
        runpy.run_path(sys.argv[0], run_name='__main__')
        return 0
    print(__doc__.strip())
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
