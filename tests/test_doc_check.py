"""Doku-Pruefung: Groessengrenzen, relative Links und Anker wie auf GitHub."""
from pathlib import Path
import tempfile
import unittest

from tools import doc_check


class DocCheckTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.encode('utf-8'))

    def test_anchor_slugs_match_github(self):
        text = '# Lager und Händler\n## Niederlage (`niederlage`)\n## Gleich\n## Gleich\n'
        self.assertEqual(doc_check.anchors(text), {'lager-und-händler', 'niederlage-niederlage', 'gleich', 'gleich-1'})

    def test_headings_in_code_fences_are_no_anchors(self):
        self.assertEqual(doc_check.anchors('```\n# Kein Titel\n```\n## Titel\n'), {'titel'})

    def test_inline_backticks_at_line_start_open_no_fence(self):
        self.write('a.md', '```` ``` ````-Blöcke\n[x](fehlt.md)\n````\n[y](auch.md)\n```\n[z](nein.md)\n````\n')
        self.assertEqual(doc_check.check_links(self.root, ['a.md']), ['a.md:2: fehlt.md: Datei fehlt'])

    def test_missing_file_and_anchor_reported(self):
        self.write('a.md', '[x](b.md#da) [y](b.md#weg) [z](fehlt.md)\n')
        self.write('b.md', '## Da\n')
        self.assertEqual(doc_check.check_links(self.root, ['a.md', 'b.md']),
                         ['a.md:1: b.md#weg: Anker fehlt', 'a.md:1: fehlt.md: Datei fehlt'])

    def test_code_fences_and_line_anchors(self):
        self.write('a.md', '```\n[x](nix.md)\n```\n[c](src/X.cpp#L12)\n')
        self.write('src/X.cpp', '')
        self.assertEqual(doc_check.check_links(self.root, ['a.md']), [])

    def test_external_and_same_file_links(self):
        self.write('a.md', '## Hier\n[w](https://example.com) [m](mailto:a@b.c) [h](#hier) [n](#nicht)\n')
        self.assertEqual(doc_check.check_links(self.root, ['a.md']), ['a.md:2: #nicht: Anker fehlt'])

    def test_relative_and_encoded_paths(self):
        self.write('docs/a.md', '[r](../README.md#start) [u](Mit%20Leerzeichen.md) [d](./)\n')
        self.write('README.md', '# Start\n')
        self.write('docs/Mit Leerzeichen.md', '')
        self.assertEqual(doc_check.check_links(self.root, ['docs/a.md']), [])

    def test_size_limits_default_override_and_archive(self):
        self.write('a.md', 'x' * 70_000)
        self.write('TODO.md', 'x' * 16_000)
        self.write('docs/archive/x.md', 'x' * 900_000)
        files = ['a.md', 'TODO.md', 'docs/archive/x.md']
        self.assertEqual(doc_check.check_sizes(self.root, files, {'TODO.md': 15_000}),
                         ['TODO.md: 16000 Bytes > Grenze 15000', 'a.md: 70000 Bytes > Grenze 60000'])

    def test_real_documents_pass(self):
        root = Path(__file__).resolve().parents[1]
        self.assertEqual(doc_check.run_checks(root), [])


if __name__ == '__main__':
    unittest.main()
