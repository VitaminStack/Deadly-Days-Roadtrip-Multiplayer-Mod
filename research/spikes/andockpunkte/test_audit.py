"""Synthetische Tests; keine Spieldateien als Fixtures."""
import unittest
from audit import PE, matches


class PatternTests(unittest.TestCase):
    def test_exact_and_overlap(self):
        self.assertEqual(matches(b"\x40\x40\x40", "40 40"), [0, 1])

    def test_wildcard(self):
        self.assertEqual(matches(b"\x48\x12\x90\x48\x34\x90", "48 ?? 90"), [0, 3])

    def test_no_partial_match(self):
        self.assertEqual(matches(b"\x48\x12", "48 ?? 90"), [])

    def test_missing(self):
        self.assertEqual(matches(b"\x90\x90", "40 55"), [])

    def test_bad_pattern(self):
        with self.assertRaises(ValueError):
            matches(b"\x90", "?? 90")

    def test_non_pe(self):
        with self.assertRaises(ValueError):
            PE(b"not an executable")


if __name__ == "__main__":
    unittest.main()
