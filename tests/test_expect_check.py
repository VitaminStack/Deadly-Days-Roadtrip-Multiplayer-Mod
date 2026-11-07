"""Offline-Pruefung der Patch-Bytes ohne Spieldateien: Ausdruecke, Tabellenumrechnung und Sprungziele."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location('expect_check', TOOLS / 'expect_check.py')
check = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check)


class FakeImage:
    """Speicher ab RVA 0x1000 statt einer Exe."""

    def __init__(self, data, base=0x1000):
        self.data, self.base = bytes(data), base

    def bytes_at(self, rva, size):
        start = rva - self.base
        return self.data[start:start + size] if 0 <= start < len(self.data) else b''


def branch(site, opcode, target):
    return bytes([opcode]) + (target - site - 5).to_bytes(4, 'little', signed=True)


SOURCE = '''
bool Check()
{
    return Code::Expect(At(Function), {0x48, 0x89}, "Anfang") &&
           Code::Expect(At(Function, 2) - 1, {0x89, 0x5C}, "Mitte") &&
           Code::Expect(Site, {0x90}, "lokal") &&
           ExpectBranch(At(CallSite), CallRel32, Function, "Aufruf");
}
'''
NAMES = {'Function': 0x1000, 'CallSite': 0x1010}


class ExpectCheckTests(unittest.TestCase):
    def image(self, shift=0):
        data = bytearray(0x20 + shift)
        data[shift:shift + 3] = b'\x48\x89\x5C'
        site = 0x1010 + shift
        data[site - 0x1000:site - 0x1000 + 5] = branch(site, 0xE8, 0x1000 + shift)
        return FakeImage(data)

    def results(self, image, table):
        return {where: status for _, where, status, _ in check.evaluate(image, SOURCE, NAMES, table)}

    def test_reference_build_reads_addresses_unchanged(self):
        self.assertEqual(self.results(self.image(), None), {'Anfang': 'ok', 'Mitte': 'ok', 'lokal': 'uebersprungen', 'Aufruf': 'ok'})

    def test_shifted_build_maps_sites_and_branch_target(self):
        results = self.results(self.image(4), {0x1000: 0x1004, 0x1010: 0x1014})
        self.assertEqual((results['Anfang'], results['Mitte'], results['Aufruf']), ('ok', 'ok', 'ok'))

    def test_wrong_target_and_missing_row_are_reported(self):
        self.assertEqual(self.results(self.image(4), {0x1000: 0x1000, 0x1010: 0x1014})['Aufruf'], 'abweichend')
        self.assertEqual(self.results(self.image(4), {0x1010: 0x1014})['Anfang'], 'fehlt')

    def test_build_marked_checks_apply_only_to_their_build(self):
        source = """
bool Check()
{
    if (Reference)
        return Code::Expect(At(Function), {0x48, 0x89}, // Build 1
                            "alt");
    return Code::Expect(At(Function), {0x40, 0x53}, // Build 2
                        "neu");
}
"""
        image = FakeImage(bytes([0x48, 0x89]))
        statuses = lambda build: {where: status for _, where, status, _ in check.evaluate(image, source, NAMES, None, build)}
        self.assertEqual(statuses(1), {'alt': 'ok', 'neu': 'anderer-build'})
        self.assertEqual(statuses(2), {'alt': 'anderer-build', 'neu': 'abweichend'})
        self.assertEqual(statuses(None), {'alt': 'ok', 'neu': 'abweichend'})

    def test_table_skips_missing_comment_rows(self):
        with tempfile.TemporaryDirectory() as folder:
            (Path(folder) / 'RvaMap1.inc').write_text('// Kopf\n{0x00001000, 0x00001004}, // eindeutig\n// FEHLT 0x00002000 // fehlt\n',
                                                     encoding='utf-8')
            self.assertEqual(check.load_table(1, Path(folder)), {0x1000: 0x1004})


if __name__ == '__main__':
    unittest.main()
