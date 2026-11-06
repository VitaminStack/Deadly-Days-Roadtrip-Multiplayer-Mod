"""Zuordnung von Gegner-Toden ohne Spiel und ohne Prozessspeicher."""
import importlib.util
from pathlib import Path
import unittest


PATH = Path(__file__).resolve().parents[1] / 'tools' / 'spike' / 'enemy_sync.py'
SPEC = importlib.util.spec_from_file_location('enemy_sync', PATH)
enemy_sync = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(enemy_sync)


class MatchDeathsTests(unittest.TestCase):
    def test_same_place_shortly_after_matches(self):
        self.assertEqual(enemy_sync.match_events([(10.0, (0, 0, 0))], [(10.25, (50, 0, 90))]), [0.25])

    def test_far_away_or_too_late_does_not_match(self):
        self.assertEqual(enemy_sync.match_events([(10.0, (0, 0, 0))], [(10.0, (1000, 0, 0))]), [])
        self.assertEqual(enemy_sync.match_events([(10.0, (0, 0, 0))], [(14.0, (0, 0, 0))]), [])
        self.assertEqual(enemy_sync.match_events([(10.0, (0, 0, 0))], [(8.5, (0, 0, 0))]), [])

    def test_each_client_death_is_used_once_and_closest_time_wins(self):
        host = [(10.0, (0, 0, 0)), (10.1, (10, 0, 0))]
        client = [(10.5, (5, 0, 0)), (10.1, (5, 0, 0))]
        # Der fruehere Host-Tod nimmt den zeitlich naechsten Client-Tod, der zweite bekommt den uebrigen.
        delays = enemy_sync.match_events(host, client)
        self.assertEqual(len(delays), 2)
        self.assertAlmostEqual(delays[0], 0.1)
        self.assertAlmostEqual(delays[1], 0.4)

    def test_missing_position_never_matches(self):
        self.assertEqual(enemy_sync.match_events([(1.0, None)], [(1.0, (0, 0, 0))]), [])
        self.assertEqual(enemy_sync.match_events([(1.0, (0, 0, 0))], [(1.0, None)]), [])

    def test_summary_has_no_addresses(self):
        line = enemy_sync.summary('Tode', 3, 3, [0.0, 0.25, 0.5])
        self.assertTrue(line.startswith('Tode: Host 3, Client 3, zugeordnet 3'))
        self.assertIn('Median 0.25 s', line)
        self.assertEqual(enemy_sync.summary('Spawns', 2, 0, []), 'Spawns: Host 2, Client 0, zugeordnet 0')


if __name__ == '__main__':
    unittest.main()
