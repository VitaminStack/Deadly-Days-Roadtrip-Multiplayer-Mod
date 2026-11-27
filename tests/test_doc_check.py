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

    def test_size_ignores_crlf(self):
        # Windows-Checkout mit CRLF darf nicht groesser zaehlen als der Stand im Repo
        self.write('a.md', 'x\r\n' * 20_000)
        self.assertEqual(doc_check.check_sizes(self.root, ['a.md'], {'a.md': 40_000}), [])

    def test_size_limits_default_override_and_archive(self):
        self.write('a.md', 'x' * 70_000)
        self.write('TODO.md', 'x' * 16_000)
        self.write('docs/archive/x.md', 'x' * 900_000)
        files = ['a.md', 'TODO.md', 'docs/archive/x.md']
        self.assertEqual(doc_check.check_sizes(self.root, files, {'TODO.md': 15_000}),
                         ['TODO.md: 16000 Bytes > Grenze 15000', 'a.md: 70000 Bytes > Grenze 60000'])

    def test_prozess_limit_is_6000_bytes(self):
        """docs/PROZESS.md liest jede Sitzung: Die Standardgrenzen melden sie ab 6001 Bytes (LF gezaehlt)."""
        self.assertEqual(doc_check.LIMITS['docs/PROZESS.md'], 6_000)
        name = 'docs/PROZESS.md'
        self.write(name, 'ä' * 3_000)  # 6000 Bytes in UTF-8
        self.assertEqual(doc_check.check_sizes(self.root, [name]), [])
        self.write(name, 'ä' * 2_999 + 'x\r\n' + 'x')  # CRLF zaehlt wie LF: 5998 + 2 + 1 = 6001 Bytes
        self.assertEqual(doc_check.check_sizes(self.root, [name]), [f'{name}: 6001 Bytes > Grenze 6000'])

    def test_archive_files_need_banner(self):
        self.write('docs/archive/a.md', '# Alt\n')
        self.write('docs/archive/b.md', doc_check.ARCHIVE_BANNER + '\n\n# Alt\n')
        self.write('docs/c.md', '# Aktuell\n')
        files = ['docs/archive/a.md', 'docs/archive/b.md', 'docs/c.md']
        self.assertEqual(doc_check.check_archive(self.root, files), ['docs/archive/a.md: Archiv-Banner fehlt'])

    def test_rewrite_links_relative_per_source(self):
        self.write('docs/X.md', '[a](GAME_ANALYSIS.md#k) [s](OFFSETS.md)\n')
        self.write('src/Game/README.md', '[b](../../docs/GAME_ANALYSIS.md)\n')
        files = ['docs/X.md', 'src/Game/README.md']
        count = doc_check.rewrite_links(self.root, files, 'docs/GAME_ANALYSIS.md', 'docs/archive/GAME_ANALYSIS.md')
        self.assertEqual(count, 2)
        self.assertEqual((self.root / 'docs/X.md').read_text(encoding='utf-8'),
                         '[a](archive/GAME_ANALYSIS.md#k) [s](OFFSETS.md)\n')
        self.assertEqual((self.root / 'src/Game/README.md').read_text(encoding='utf-8'),
                         '[b](../../docs/archive/GAME_ANALYSIS.md)\n')

    def test_rewrite_links_inside_moved_target_and_same_file(self):
        self.write('docs/archive/A.md', '[i](#k) [o](../OFFSETS.md#x)\n')
        count = doc_check.rewrite_links(self.root, ['docs/archive/A.md'], 'docs/OFFSETS.md', 'docs/archive/OFFSETS.md',
                                        {'x'})
        self.assertEqual(count, 1)
        self.assertEqual((self.root / 'docs/archive/A.md').read_text(encoding='utf-8'), '[i](#k) [o](OFFSETS.md#x)\n')

    def test_rewrite_same_file_anchor_links_when_section_moves_out(self):
        self.write('TODO.md', '[e](#erledigt) [p](#phase)\n')
        count = doc_check.rewrite_links(self.root, ['TODO.md'], 'TODO.md', 'docs/ERLEDIGT.md', {'erledigt'})
        self.assertEqual(count, 1)
        self.assertEqual((self.root / 'TODO.md').read_text(encoding='utf-8'), '[e](docs/ERLEDIGT.md#erledigt) [p](#phase)\n')

    def test_rewrite_only_selected_anchors_and_idempotent(self):
        self.write('docs/a.md', '[a](OFFSETS.md#alt) [b](OFFSETS.md#neu)\n')
        args = (self.root, ['docs/a.md'], 'docs/OFFSETS.md', 'docs/archive/OFFSETS.md', {'alt'})
        self.assertEqual(doc_check.rewrite_links(*args), 1)
        self.assertEqual(doc_check.rewrite_links(*args), 0)
        self.assertEqual((self.root / 'docs/a.md').read_text(encoding='utf-8'),
                         '[a](archive/OFFSETS.md#alt) [b](OFFSETS.md#neu)\n')

    def test_rewrite_keeps_crlf_and_code_fences(self):
        self.write('docs/a.md', '[a](OFFSETS.md)\r\n```\n[c](OFFSETS.md)\n```\r\n')
        doc_check.rewrite_links(self.root, ['docs/a.md'], 'docs/OFFSETS.md', 'docs/archive/OFFSETS.md')
        self.assertEqual((self.root / 'docs/a.md').read_bytes(),
                         b'[a](archive/OFFSETS.md)\r\n```\n[c](OFFSETS.md)\n```\r\n')

    def test_rebase_links_of_moved_text(self):
        self.write('src/A.cpp', '')
        self.write('docs/OFFSETS.md', '## X\n')
        self.write('docs/G.md', '## Gone\n')
        self.write('docs/archive/G.md', '[s](../src/A.cpp) [o](OFFSETS.md#x) [h](#here) [g](#gone) [w](https://a.b)\n## Here\n')
        count = doc_check.rebase_links(self.root, 'docs/archive/G.md', 'docs/G.md')
        self.assertEqual(count, 3)
        self.assertEqual((self.root / 'docs/archive/G.md').read_text(encoding='utf-8'),
                         '[s](../../src/A.cpp) [o](../OFFSETS.md#x) [h](#here) [g](../G.md#gone) [w](https://a.b)\n## Here\n')

    def test_finished_plan_must_be_archived(self):
        self.write('docs/superpowers/plans/x.md', '# Plan\n')
        self.write('TODO.md', '| `andere` | P1 | Text | – | zentral | wenig |\n')
        self.assertEqual(doc_check.check_plans(self.root),
                         ['docs/superpowers/plans/x.md: Arbeitsplan nach Abschluss archivieren oder löschen'])

    def test_plan_allowed_while_task_open(self):
        self.write('docs/superpowers/plans/x.md', '# Plan\n')
        self.write('TODO.md', '| `projekt-aufraeumen` | P1 | Text | – | zentral | wenig |\n')
        self.assertEqual(doc_check.check_plans(self.root), [])

    def test_real_documents_pass(self):
        root = Path(__file__).resolve().parents[1]
        self.assertEqual(doc_check.run_checks(root), [])


if __name__ == '__main__':
    unittest.main()
