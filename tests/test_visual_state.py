"""Reine Decodier-Tests, kein Zugriff auf das Spiel oder Prozessspeicher."""
import importlib.util
from pathlib import Path
import struct
import unittest


PATH = Path(__file__).resolve().parents[1] / 'tools' / 'spike' / 'visual_state.py'
SPEC = importlib.util.spec_from_file_location('visual_state', PATH)
visual = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(visual)


class VisualStateTests(unittest.TestCase):
    def test_empty_array_is_known_zero(self):
        self.assertEqual(visual.array_header(struct.pack('<Qii', 0, 0, 0)), 0)

    def test_nonempty_array_count(self):
        self.assertEqual(visual.array_header(struct.pack('<Qii', 0x1000, 2, 8)), 2)

    def test_unreadable_or_short_array_is_unknown(self):
        for data in (None, b'', b'\0' * 15, b'\0' * 17):
            with self.subTest(data=data):
                self.assertIsNone(visual.array_header(data))

    def test_malformed_array_is_unknown(self):
        for pointer, count, capacity in ((0, 1, 1), (0x1000, -1, 8),
                                         (0x1000, 9, 8), (0x1000, 0, -1),
                                         (0x1000, 1, 100001)):
            with self.subTest(count=count, capacity=capacity):
                self.assertIsNone(visual.array_header(struct.pack('<Qii', pointer, count, capacity)))

    def test_hidden_does_not_imply_movement_replication(self):
        self.assertEqual(visual.actor_flags(bytes([0xe0, 0, 0, 0, 0, 0])),
                         {'replicate_movement': False, 'hidden': True, 'destroying': False})

    def test_destroying_is_separate_from_hidden(self):
        self.assertEqual(visual.actor_flags(bytes([0x10, 0, 0, 0, 0, 2])),
                         {'replicate_movement': True, 'hidden': False, 'destroying': True})

    def test_unreadable_flags_are_unknown(self):
        self.assertIsNone(visual.actor_flags(None))
        self.assertIsNone(visual.actor_flags(b'\0'))

    def test_unreadable_vector_is_unknown(self):
        class Memory:
            def read(self, address, size):
                return b'\0' * 23
        self.assertIsNone(visual.vector(Memory(), 0x1000))

    def test_nonfinite_vector_is_unknown(self):
        class Memory:
            def read(self, address, size):
                return struct.pack('<ddd', 1, float('nan'), 0)
        self.assertIsNone(visual.vector(Memory(), 0x1000))


if __name__ == '__main__':
    unittest.main()
