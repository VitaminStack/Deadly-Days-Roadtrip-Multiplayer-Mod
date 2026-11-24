"""Einstieg fuer die Probe-Skripte unter tools/spike: Liste mit je einem Satz, Aufruf per Name."""
from pathlib import Path
import tempfile
import unittest

from tools.spike import probe

SPIKE = Path(__file__).resolve().parents[1] / 'tools' / 'spike'


class ProbeTests(unittest.TestCase):
    def test_every_probe_script_has_docstring(self):
        missing = [name for name, summary in probe.scripts(SPIKE).items() if not summary]
        self.assertEqual(missing, [], 'Skripte ohne Modul-Docstring')

    def test_list_is_sorted_one_line_each(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'zeta.py').write_text('"""Misst Z.\n\nMehr Text."""\n', encoding='utf-8')
            (root / 'alpha.py').write_text('"""Misst A."""\nimport sys\n', encoding='utf-8')
            (root / 'probe.py').write_text('"""Einstieg."""\n', encoding='utf-8')
            self.assertEqual(probe.list_lines(root), ['alpha  Misst A.', 'zeta   Misst Z.'])

    def test_docstring_read_without_running_script(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'boom.py').write_text('"""Liest nur."""\nraise SystemExit(3)\n', encoding='utf-8')
            self.assertEqual(probe.scripts(root), {'boom': 'Liest nur.'})


if __name__ == '__main__':
    unittest.main()
