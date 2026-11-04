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
