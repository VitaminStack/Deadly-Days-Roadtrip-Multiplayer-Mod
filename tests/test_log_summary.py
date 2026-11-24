"""Zusammenfassung eines Mod-Logs fuer Agenten: Warnungen, Fehler, Abstuerze und Patch-Listen."""
import unittest

from tools.log_summary import summarize

L = '2026-11-24 15:41:00.123 [42] [{lvl}] {msg}'


class LogSummaryTests(unittest.TestCase):
    def test_keeps_warnings_errors_and_patch_lists(self):
        out = summarize([L.format(lvl='INFO', msg='Start'), L.format(lvl='INFO', msg='Tick'),
                         L.format(lvl='INFO', msg='Liste Reward gesetzt'), L.format(lvl='WARN', msg='x'),
                         L.format(lvl='INFO', msg='Ende')])
        self.assertEqual(out, [L.format(lvl='INFO', msg='Start'), L.format(lvl='INFO', msg='Liste Reward gesetzt'),
                               L.format(lvl='WARN', msg='x'), L.format(lvl='INFO', msg='Ende')])

    def test_keeps_crash_lines_at_any_level(self):
        crash = L.format(lvl='INFO', msg='EXCEPTION_ACCESS_VIOLATION bei 0x5A7008C')
        out = summarize([L.format(lvl='INFO', msg='a'), crash, L.format(lvl='INFO', msg='b'), L.format(lvl='INFO', msg='c')])
        self.assertIn(crash, out)
        self.assertNotIn(L.format(lvl='INFO', msg='b'), out)

    def test_collapses_repeats(self):
        out = summarize([L.format(lvl='WARN', msg='gleich')] * 5)
        self.assertEqual(out, [L.format(lvl='WARN', msg='gleich') + ' (×5)'])

    def test_repeats_with_other_timestamp_collapse(self):
        lines = [f'2026-11-24 15:41:0{i}.000 [42] [WARN] gleich' for i in range(3)]
        self.assertEqual(summarize(lines), [lines[0] + ' (×3)'])

    def test_limit_adds_rest_line(self):
        out = summarize([L.format(lvl='ERROR', msg=f'e{i}') for i in range(10)], limit=3)
        self.assertEqual(out[-1], '… 7 weitere Zeilen')
        self.assertEqual(len(out), 4)

    def test_empty_log(self):
        self.assertEqual(summarize([]), [])


if __name__ == '__main__':
    unittest.main()
