"""Synthetic memory snapshots for the read-only audit; no Win32/game access."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'spike'))
from players import audit, pointer_hash


class Memory:
    base = 0x140000000

    def __init__(self):
        self.data = bytearray(0x10000)
        self.pack(0x11A8, '<Q', 0x2000)
        self.pack(0x11B0, '<Q', 0x3000)
        self.pack(0x2010, '<Q', 0x4000)
        self.pack(0x32C0, '<Qi', 0x5000, 2)
        self.pack(0x5000, '<QQ', 0x6000, 0x6100)
        self.pack(0x6158, '<Q', 0x7000)
        self.pack(0x6258, '<Q', 0x7100)
        self.pack(0x2388, '<Qii', 0x8000, 2, 2)
        self.pack(0x2398, '<I', 3)
        self.pack(0x23A8, '<QiiiiiiQi', 0, 2, 128, -1, 0, 0, -1, 0, 1)
        self.pack(0x8000, '<QiiQii', 0x7000, 1, 0, 0x7100, -1, 0)

    def pack(self, address, fmt, *values):
        struct.pack_into(fmt, self.data, address, *values)

    def read(self, address, size):
        return bytes(self.data[address:address + size]) if address else None

    def ptr(self, address):
        return struct.unpack_from('<Q', self.data, address)[0]

    def i32(self, address):
        return struct.unpack_from('<i', self.data, address)[0]

    def world(self):
        return 0x1000

    def name(self, address):
        return 'DDMissionGameMode' if address == 0x4000 else str(address)


class AuditTests(unittest.TestCase):
    def test_current_roster(self):
        result = audit(Memory())
        self.assertTrue(result['hash_valid'])
        self.assertTrue(result['roster_equal'])
        self.assertEqual(result['allocated'], 2)

    def test_cycle_and_invalid_link(self):
        for link in (0, 99):
            game = Memory()
            game.pack(0x8008, '<i', link)
            self.assertFalse(audit(game)['hash_valid'])

    def test_free_slot_does_not_count(self):
        game = Memory()
        game.pack(0x2390, '<ii', 3, 3)
        game.pack(0x23B0, '<iiii', 3, 128, 2, 1)
        game.pack(0x8020, '<Qii', 0xFFFFFFFFFFFFFFFF, 99, 99)
        self.assertTrue(audit(game)['hash_valid'])
        self.assertTrue(audit(game)['roster_equal'])

    def test_gc_null_is_not_current(self):
        game = Memory()
        game.pack(0x8010, '<Q', 0)
        self.assertFalse(audit(game)['roster_equal'])

    def test_missing_player_and_duplicate(self):
        game = Memory()
        game.pack(0x6258, '<Q', 0)
        self.assertIn('incomplete_current_roster', audit(game)['errors'])
        game = Memory()
        game.pack(0x8010, '<Q', 0x7000)
        self.assertIn('duplicate_controller', audit(game)['errors'])

    def test_reject_non_mission_mode(self):
        game = Memory()
        game.pack(0x2010, '<Q', 0)
        with self.assertRaises(ValueError):
            audit(game)

    def test_pointer_hash_checks_actual_bucket(self):
        game = Memory()
        # Two occupied entries deliberately in the opposite bucket.
        controller = 0x7000
        bucket = 1 - (pointer_hash(controller) & 1)
        game.pack(0x23C0, '<ii', 0 if bucket == 0 else -1, 0 if bucket == 1 else -1)
        game.pack(0x23D0, '<i', 2)
        game.pack(0x8000, '<QiiQii', controller, 1, bucket, controller, -1, bucket)
        self.assertIn('key_hash_mismatch', audit(game)['errors'])


if __name__ == '__main__':
    unittest.main()
