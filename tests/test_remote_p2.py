"""Synthetic read-only snapshots; no Win32 imports, game, SDK or memory writes to another process."""
import struct
import unittest

from tools.spike.remote_p2 import snapshot


class Game:
    base = 0x10000000

    def __init__(self):
        self.memory = {}
        self.names = {0x11000: 'LocalCoopSubsystem', 0x12000: 'CoopInputRouterSubsystem',
                      0x13000: 'DDPlayerController', 0x14000: 'LocalPlayer', 0x15000: 'IpConnection',
                      0x1000: 'TestWorld', 0x5000: 'Host', 0x6000: 'Remote'}
        for address, size in ((0x1000, 0x300), (0x2000, 0x100), (0x3000, 0x300), (0x4000, 0x200),
                              (0x5000, 0x600), (0x6000, 0x600), (0x7000, 0x100), (0x8000, 0x100),
                              (0x9000, 0x200), (0xA000, 0x200), (0xB000, 0x100), (0xC000, 0x100)):
            self.put(address, bytes(size))
        for address, value in ((0x1000 + 0x228, 0x2000), (0x1000 + 0x38, 0x4000),
                               (0x1000 + 0x1B0, 0x3000), (0x3000 + 0x2C0, 0xB000),
                               (0xB000, 0xD000), (0xB008, 0xE000), (0xD000 + 0x158, 0x5000),
                               (0xE000 + 0x158, 0x6000), (0x5000 + 0x348, 0x7000),
                               (0x6000 + 0x348, 0x8000), (0x6000 + 0x520, 0x8000),
                               (0x5000 + 0x10, 0x13000), (0x6000 + 0x10, 0x13000),
                               (0x7000 + 0x10, 0x14000), (0x8000 + 0x10, 0x15000),
                               (0x9000 + 0x10, 0x11000), (0xA000 + 0x10, 0x12000),
                               (0x9000 + 0x20, 0x2000), (0xA000 + 0x20, 0x2000),
                               (self.base + 0xAA67B80, 0x30000), (0x30000, 0x40000),
                               (0x40000 + 0x18 + 8, 0x5000), (0x40000 + 0x30 + 8, 0x6000)):
            self.put(address, struct.pack('<Q', value))
        for address, value in ((0x2000 + 0x40, 1), (0x3000 + 0x2C8, 2),
                               (self.base + 0xAA67B80 + 8, 3), (0x40000 + 0x18 + 0x10, 11),
                               (0x40000 + 0x30 + 0x10, 22)):
            self.put(address, struct.pack('<i', value))
        self.put(0x9100, struct.pack('<Qii', 0xC000, 2, 2))
        self.put(0x9110, struct.pack('<I', 3))
        self.put(0x9120, struct.pack('<Qiiii', 0, 2, 128, -1, 0))
        self.put(0x9138, struct.pack('<ii', 0, 1))
        self.put(0x9148, struct.pack('<i', 2))
        self.put(0xC000, struct.pack('<iiiii', 0, 1, 11, -1, 0))
        self.put(0xC014, struct.pack('<iiiii', 1, 2, 22, -1, 1))

    def put(self, address, data):
        self.memory.update({address + i: value for i, value in enumerate(data)})

    def read(self, address, size):
        try:
            return bytes(self.memory[address + i] for i in range(size))
        except KeyError:
            return None

    def ptr(self, address):
        return struct.unpack('<Q', self.read(address, 8) or bytes(8))[0]

    def i32(self, address):
        return struct.unpack('<i', self.read(address, 4) or bytes(4))[0]

    def name(self, obj):
        return self.names.get(obj) if obj else None

    def world(self):
        return 0x1000

    def objects(self):
        return [0x9000, 0xA000]


class RemoteP2Tests(unittest.TestCase):
    def test_current_remote_and_one_local_player(self):
        data = snapshot(Game())
        self.assertEqual((data['role'], data['local_players'], data['player_states']), ('host', 1, 2))
        self.assertTrue(data['coop']['hash_valid'])
        self.assertEqual([entry['controller'] for entry in data['coop']['active_players']], ['0x5000', '0x6000'])
        self.assertFalse(data['controllers'][1]['local_player'])

    def test_stale_serial_is_not_a_live_controller(self):
        game = Game()
        game.put(0xC014 + 8, struct.pack('<i', 99))
        self.assertEqual(snapshot(game)['coop']['active_players'][1]['controller'], '0x0')

    def test_invalid_hash_link_is_rejected(self):
        game = Game()
        game.put(0xC014 + 12, struct.pack('<i', 1))
        with self.assertRaisesRegex(ValueError, 'cyclic'):
            snapshot(game)

    def test_incomplete_map_read_is_not_zero_or_success(self):
        game = Game()
        del game.memory[0x9100]
        with self.assertRaisesRegex(ValueError, 'incomplete read'):
            snapshot(game)

    def test_other_game_instance_subsystem_is_not_selected(self):
        game = Game()
        game.put(0x9020, struct.pack('<Q', 0x99000))
        self.assertIsNone(snapshot(game)['coop'])

    def test_local_router_session_authority(self):
        game = Game()
        game.put(0xA0A8, struct.pack('<Qii', 0xF000, 1, 1))
        raw = bytearray(0x1C)
        struct.pack_into('<i', raw, 0, 5)
        struct.pack_into('<i', raw, 0x10, 1)
        raw[0x1A] = 1
        game.put(0xF000, raw)
        self.assertEqual(snapshot(game)['router']['sessions'], [{'id': 5, 'authority': 1, 'blocks_gameplay': True}])


if __name__ == '__main__':
    unittest.main()
