"""Offline-Tests für tools/patch_signatures.py ohne Spieldateien: Tabellenformat, Zusammenlegen und Prüfung."""
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
import patch_signatures as sig  # noqa: E402


class FakeImage:
    """Nur ein .text-Abschnitt ab RVA 0x1000 statt einer Exe."""

    def __init__(self, data, base=0x1000):
        self.data, self.base = bytes(data), base

    def section_bytes(self, name):
        return self.base, self.data


class TableTests(unittest.TestCase):
    def test_pattern_round_trip(self):
        pattern = sig.parse_pattern('48 ?? 5C ? 08')
        self.assertEqual(pattern, [0x48, None, 0x5C, None, 0x08])
        self.assertEqual(sig.render_pattern(pattern), '48 ?? 5C ?? 08')

    def test_rows_round_trip_sorted(self):
        rows = {
            (0x2000, 26495302): ([0x90, None], 0, 'B', ''),
            (0x2000, 0): ([0x48, 0x89], 2, 'B', 'X.cpp'),
            (0x1000, 0): ([0xCC], 0, 'A', ''),
        }
        text = sig.render_rows(rows)
        lines = [line for line in text.splitlines() if line.startswith('{')]
        self.assertEqual(lines[0], '{0x00001000, 0, "CC", 0, "A"},')
        self.assertTrue(lines[1].startswith('{0x00002000, 0, ') and lines[1].endswith('// X.cpp'))
        self.assertIn('26495302', lines[2])
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'T.inc'
            path.write_text(text, encoding='utf-8')
            self.assertEqual(sig.load_rows(path), rows)

    def test_row_for_prefers_own_build(self):
        rows = {(0x10, 0): 'all', (0x10, 7): 'own'}
        self.assertEqual(sig.row_for(rows, 0x10, 7), 'own')
        self.assertEqual(sig.row_for(rows, 0x10, 8), 'all')
        self.assertIsNone(sig.row_for(rows, 0x20, 7))


class PatternTests(unittest.TestCase):
    def test_merge_masks_differences_and_shortens(self):
        self.assertEqual(sig.merge([[1, 2, 3, 4], [1, 9, 3]]), [1, None, 3])

    def test_strong_needs_fixed_bytes_at_site(self):
        self.assertTrue(sig.strong([1] * 8, 0))
        self.assertFalse(sig.strong([1] * 7, 0))
        self.assertFalse(sig.strong([1] * 8 + [None] * 9, 0))
        self.assertFalse(sig.strong([1] * 8 + [None] * 4, 8))

    def test_hits_with_wildcards(self):
        image = FakeImage(b'\x00\xAA\x01\xBB\x00\xAA\x02\xBB')
        self.assertEqual(sig.hits(image, [0xAA, None, 0xBB]), [0x1001, 0x1005])
        self.assertEqual(sig.hits(image, [0xAA, 0x02]), [0x1005])
        self.assertEqual(sig.hits(image, [0xCC]), [])


class CheckTests(unittest.TestCase):
    IMAGE = FakeImage(b'\x90' * 4 + b'\x48\x89\x5C\x24' + b'\xCC' * 4 + b'\x48\x89\x5C\x24')

    def statuses(self, rows, table=None, build=0):
        return [(rva, status) for rva, _, status, _ in sig.check_rows(self.IMAGE, rows, table, build)]

    def test_reference_build(self):
        rows = {
            (0x1004, 0): ([0x90, 0x48, 0x89], 1, 'Ok', ''),
            (0x1008, 0): ([0x48, 0x89, 0x5C], 0, 'Doppelt', ''),
            (0x1010, 0): ([0xEE], 0, 'Fehlt', ''),
        }
        self.assertEqual(self.statuses(rows), [(0x1004, 'ok'), (0x1008, 'mehrdeutig'), (0x1010, 'abweichend')])

    def test_other_build_uses_map_and_own_rows(self):
        rows = {
            (0x5000, 0): ([0x90, 0x48, 0x89], 1, 'Verschoben', ''),
            (0x6000, 0): ([0xCC], 0, 'Alt', ''),
            (0x6000, 3): ([0x24, 0xCC], 0, 'Eigen', ''),
            (0x7000, 0): ([0x90], 0, 'Ohne Karte', ''),
        }
        table = {0x5000: 0x1004, 0x6000: 0x1007}
        self.assertEqual(self.statuses(rows, table, 3), [(0x5000, 'ok'), (0x6000, 'ok'), (0x7000, 'fehlt')])


SOURCE = '''constexpr uintptr_t Site = 0x5000000;
constexpr uintptr_t Helper = 0x5000100;
constexpr uintptr_t Other = 0x5000200;
void __fastcall Hook(uintptr_t A)
{
    reinterpret_cast<Fn>(At(Helper))(A);
}
bool CheckOne()
{
    return Code::Expect(At(Site), {0x48}, "x");
}
bool ApplyOne(Code::Writer&)
{
    return Install(At(Site), &Hook);
}
bool CheckTwo()
{
    return At(0x5000300) != 0;
}
bool ApplyTwo(Code::Writer&)
{
    return At(Other) != 0;
}
const Patch List[] = {
    {"Eins", &CheckOne, &ApplyOne},
    {"Zwei", CheckTwo, ApplyTwo},
};
'''


class SitesTests(unittest.TestCase):
    LISTED = {0x5000000, 0x5000100, 0x5000300}

    def test_follows_hooks_and_ignores_unlisted(self):
        entries = sig.file_sites(SOURCE, self.LISTED)
        self.assertEqual([(check, rvas) for check, _, _, rvas in entries],
                         [('CheckOne', [0x5000000, 0x5000100]), ('CheckTwo', [0x5000300])])

    def test_write_is_stable(self):
        written = sig.write_sites(SOURCE, sig.file_sites(SOURCE, self.LISTED))
        self.assertIn('constexpr uintptr_t OneSites[] = {Site, Helper};', written)
        self.assertIn('constexpr uintptr_t TwoSites[] = {0x5000300};', written)
        self.assertIn('{"Zwei", CheckTwo, ApplyTwo, TwoSites},\n};', written)
        again = sig.write_sites(written, sig.file_sites(written, self.LISTED))
        self.assertEqual(again, written)


if __name__ == '__main__':
    unittest.main()
