"""Acceptance decisions without attaching to Windows/game processes."""
import ast
import copy
import math
from pathlib import Path
import unittest


SOURCE = Path(__file__).resolve().parents[1] / 'tools' / 'spike' / 'check_round.py'
TREE = ast.parse(SOURCE.read_text(encoding='utf-8'))
EVALUATE = next(node for node in TREE.body if isinstance(node, ast.FunctionDef) and node.name == 'evaluate')
NAMESPACE = {'math': math, 'MISSION_WORLD': 'mission', 'BUS_RADIUS': 100, 'POSITION_TOLERANCE': 100}
exec(compile(ast.Module(body=[EVALUATE], type_ignores=[]), str(SOURCE), 'exec'), NAMESPACE)


def report():
    return {'world': 'base', 'player_array': 2, 'player_array_valid': True, 'player_states': 2,
            'paused_by': [], 'characters': [{'position': None}, {'position': None}], 'buses': [],
            'guid_player': ['0x125', '0x127'], 'new_actor_failed': 0, 'guid_entries': 100}


class RoundAcceptanceTests(unittest.TestCase):
    def test_legitimate_cache_cleanup_is_not_a_failure(self):
        host, client = report(), report()
        client['guid_entries'] = 80
        self.assertEqual(NAMESPACE['evaluate'](host, client, {'guid_entries': 100}, 'base'), [])

    def test_required_identity_loss_still_fails(self):
        host, client = report(), report()
        client['guid_player'] = ['0x127']
        failures = NAMESPACE['evaluate'](host, client, {}, 'base')
        self.assertTrue(any('0x125' in problem for problem in failures))

    def test_new_actor_failure_still_fails_even_if_cache_grows(self):
        host, client = report(), report()
        client['new_actor_failed'] = 2
        client['guid_entries'] = 200
        failures = NAMESPACE['evaluate'](host, client, {'new_actor_failed': 1, 'guid_entries': 100}, 'base')
        self.assertTrue(any('1 neue NewActor failed' in problem for problem in failures))

    def test_godmode_cannot_hide_empty_player_roster(self):
        host, client = report(), copy.deepcopy(report())
        client['player_array'] = client['player_states'] = 0
        failures = NAMESPACE['evaluate'](host, client, {}, 'base')
        self.assertTrue(any('PlayerArray 0' in problem for problem in failures))
        self.assertTrue(any('0 PlayerStates' in problem for problem in failures))


WAIT = next(node for node in TREE.body if isinstance(node, ast.FunctionDef) and node.name == 'wait_new_world')


class FakeClock:
    def __init__(self):
        self.now = 0.0

    def time(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class FakeGame:
    """World je Abfrage aus einer Liste: 0 heisst, die Instanz laedt gerade."""

    def __init__(self, worlds):
        self.worlds = list(worlds)

    def world(self):
        return self.worlds.pop(0) if len(self.worlds) > 1 else self.worlds[0]

    def name(self, world):
        return {0x100: 'MetaBase', 0x200: 'mission'}.get(world)

    def players(self, world):
        """Erst die zweite Abfrage der Missionswelt kennt beide Spieler, wie beim Client nach der Replikation."""
        if world != 0x200:
            return 2 if world else 0
        self.mission_polls = getattr(self, 'mission_polls', 0) + 1
        return 2 if self.mission_polls > 1 else 1


def wait_namespace(games, running):
    clock = FakeClock()
    namespace = {'time': clock, 'open_game': lambda pid: games.get(pid), 'process_running': lambda pid: running(pid, clock),
                 'players': lambda game, world: game.players(world)}
    exec(compile(ast.Module(body=[WAIT], type_ignores=[]), str(SOURCE), 'exec'), namespace)
    return namespace['wait_new_world']


class LoadTimeTests(unittest.TestCase):
    BEFORE = {'0': hex(0x100), '1': hex(0x100)}

    def test_loading_world_zero_is_measured_not_treated_as_ended(self):
        games = {1: FakeGame([0x100, 0, 0, 0, 0x200]), 2: FakeGame([0x100, 0, 0, 0, 0, 0, 0x200])}
        wait = wait_namespace(games, lambda pid, clock: True)
        self.assertEqual(wait(1, 2, self.BEFORE, 120, 'mission'), 3.5)

    def test_ended_instance_returns_none(self):
        games = {1: FakeGame([0x100, 0]), 2: FakeGame([0x100, 0])}
        wait = wait_namespace(games, lambda pid, clock: not (pid == 2 and clock.now >= 1.0))
        self.assertIsNone(wait(1, 2, self.BEFORE, 120, 'mission'))

    def test_timeout_returns_none_instead_of_a_load_time(self):
        games = {1: FakeGame([0]), 2: FakeGame([0])}
        wait = wait_namespace(games, lambda pid, clock: True)
        self.assertIsNone(wait(1, 2, self.BEFORE, 5, 'mission'))
