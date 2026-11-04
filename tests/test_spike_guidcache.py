"""SparseArray decoding with fake memory, without Windows/process access."""
import ast
from pathlib import Path
import struct
import unittest


SOURCE = Path(__file__).resolve().parents[1] / 'tools' / 'spike' / 'guidcache.py'
TREE = ast.parse(SOURCE.read_text(encoding='utf-8'))
FUNCTION = next(node for node in TREE.body if isinstance(node, ast.FunctionDef) and node.name == 'entries')
NAMESPACE = {'struct': struct, 'DRIVER_GUID_CACHE': 0x1C8, 'ENTRY_SIZE': 0x40, 'GOBJECTS': 0xAA61B00}
exec(compile(ast.Module(body=[FUNCTION], type_ignores=[]), str(SOURCE), 'exec'), NAMESPACE)


class FakeGame:
    base = 0

    def __init__(self):
        self.reads = []
        self.raw = bytearray(8 * 0x40)
        for slot in range(8):
            struct.pack_into('<QiiQi', self.raw, slot * 0x40, 293 + slot * 2, 0, 0, 0, 1)
            self.raw[slot * 0x40 + 0x30] = 16

    def world(self):
        return 0x1000

    def ptr(self, address):
        return {0x1038: 0x2000, 0x21C8: 0x3000}.get(address, 0)

    def read(self, address, size):
        self.reads.append((address, size))
        if address == 0x3008:
            return struct.pack('<Qii', 0x4000, 3, 8)
        if address == 0x4000:
            return self.raw[:size]
        if address == 0x3018:
            return b'\xfd'[:size]  # Slot 1 is free; unused capacity bits deliberately set.
        return None

    def name_from_index(self, index):
        return 'asset'


class GuidCacheTests(unittest.TestCase):
    def test_capacity_and_holes_are_not_live_entries(self):
        game = FakeGame()
        slots, rows = NAMESPACE['entries'](game)
        self.assertEqual(slots, 3)
        self.assertEqual(set(rows), {293, 297})
        self.assertIn((0x4000, 3 * 0x40), game.reads)

    def test_short_read_does_not_invent_missing_entries(self):
        game = FakeGame()
        game.raw = game.raw[:0x40]
        slots, rows = NAMESPACE['entries'](game)
        self.assertEqual(slots, 3)
        self.assertEqual(set(rows), {293})
