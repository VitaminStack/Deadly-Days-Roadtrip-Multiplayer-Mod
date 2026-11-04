"""Aufgabentabellen in TODO.md, Abhaengigkeiten, Reservierungsschutz und README-Fortschritt."""
from contextlib import redirect_stdout
from io import StringIO
import os
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET
from unittest.mock import call, patch

from tools import task_status as source
from tools import tasks

ROOT = Path(__file__).resolve().parents[1]
OPEN = source.OPEN_HEADER + '\n|---|---|---|---|---|---|\n'
DONE = '## Erledigt\n' + source.DONE_HEADER + '\n|---|---|---|\n'


def todo(phase1_rows='', done_rows='', extra=''):
    """Minimales TODO.md mit allen Phasen; Phase 0 bis 6 haben je eine erledigte Aufgabe."""
    text = '<!-- frei:start -->\n<!-- frei:end -->\n'
    for phase in range(7):
        text += f'## Phase {phase} – Test\n'
        if phase == 1 and phase1_rows:
            text += OPEN + phase1_rows
    text += extra
    rows = [f'| `base{p}` | {p} | Ergebnis |' for p in range(7)] + [r for r in done_rows.splitlines() if r]
    return text + DONE + '\n'.join(sorted(rows)) + '\n'


class ParseTests(unittest.TestCase):
    def test_real_documents_are_consistent(self):
        text = (ROOT / 'TODO.md').read_text(encoding='utf-8')
        readme = (ROOT / 'README.md').read_text(encoding='utf-8')
        parsed = source.parse(text)
        self.assertEqual(source.update_todo(text, parsed), text)
        self.assertEqual(source.update_readme(parsed, readme), readme)
        self.assertEqual(source.progress_svg(parsed, readme),
                         (ROOT / 'docs/assets/progress.svg').read_text(encoding='utf-8'))
        for name in ('crash-reporter', 'overlay', 'deploy', 'ci', 'grundgeruest', 'testumgebung'):
            self.assertTrue(parsed[name].done, name)

    def test_open_and_done_rows(self):
        parsed = source.parse(todo('| `a` | P0 | Text | `base1` | parallel | wenig |\n'))
        self.assertFalse(parsed['a'].done)
        self.assertEqual(parsed['a'].needs, ['base1'])
        self.assertTrue(parsed['base1'].done)

    def test_checkbox_and_marker_rejected(self):
        for line in ('- [ ] alt\n', '- [x] alt\n', '<!-- tasks: a -->\n'):
            with self.subTest(line=line), self.assertRaisesRegex(ValueError, 'Checkboxen'):
                source.parse(todo(extra=line))

    def test_duplicate_id_rejected(self):
        with self.assertRaisesRegex(ValueError, 'mehrfach'):
            source.parse(todo('| `base1` | P0 | Text | – | parallel | wenig |\n'))

    def test_unknown_dependency_rejected(self):
        with self.assertRaisesRegex(ValueError, 'unbekannte'):
            source.parse(todo('| `a` | P0 | Text | `nope` | parallel | wenig |\n'))

    def test_cycle_rejected(self):
        rows = '| `a` | P0 | Text | `b` | parallel | wenig |\n| `b` | P0 | Text | `a` | parallel | wenig |\n'
        with self.assertRaisesRegex(ValueError, 'Kreis'):
            source.parse(todo(rows))

    def test_invalid_values_rejected(self):
        for row in ('| `a` | P3 | Text | – | parallel | wenig |\n',
                    '| `a` | P0 | Text | – | solo | wenig |\n',
                    '| `a` | P0 | Text | – | parallel | lang |\n',
                    '| `a` | P0 | Text | b | parallel | wenig |\n',
                    '| `a` | P0 | Text | – | parallel |\n'):
            with self.subTest(row=row), self.assertRaises(ValueError):
                source.parse(todo(row))

    def test_done_must_be_sorted(self):
        text = todo().replace('| `base0` | 0 |', '| `zzz` | 0 |')
        with self.assertRaisesRegex(ValueError, 'alphabetisch'):
            source.parse(text)

    def test_missing_phase_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Phasen fehlen'):
            source.parse(todo().replace('## Phase 3 – Test\n', ''))

    def test_wrong_table_header_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Kopf'):
            source.parse(todo(extra='## Phase 9\n| Kennung | Aufgabe |\n'))


class DerivedTests(unittest.TestCase):
    def test_free_block_lists_unblocked_and_team(self):
        rows = ('| `ready` | P1 | Text | `base1` | parallel | wenig |\n'
                '| `first` | P0 | Text | – | zentral | viel |\n'
                '| `waits` | P0 | Text | `ready` | parallel | wenig |\n'
                '| `vote` | P0 | Text | – | Team | keine |\n')
        text = todo(rows)
        block = source.free_block(text, source.parse(text))
        lines = block.splitlines()
        self.assertTrue(lines[2].startswith('| `first` | P0'))
        self.assertTrue(lines[3].startswith('| `ready` | P1'))
        self.assertNotIn('`waits`', block)
        self.assertIn('**Wartet auf das Team:** `vote`.', block)

    def test_update_todo_is_idempotent_and_keeps_crlf(self):
        text = todo('| `a` | P0 | Text | – | parallel | wenig |\n').replace('\n', '\r\n')
        parsed = source.parse(text)
        once = source.update_todo(text, parsed)
        self.assertIn('| `a` | P0 |', once)
        self.assertNotIn('\r\r', once)
        self.assertEqual(source.update_todo(once, source.parse(once)), once)

    def test_progress_counts_phases_only(self):
        rows = '| `a` | P0 | Text | – | parallel | wenig |\n'
        extra = '## Optionale Optimierungen\n' + OPEN + '| `o` | P2 | Text | – | parallel | wenig |\n'
        parsed = source.parse(todo(rows, extra=extra))
        counts = source.phase_counts(parsed)
        self.assertEqual(counts[1], [1, 2])
        self.assertEqual(counts[0], [1, 1])
        readme = ''.join(f'| [{p}](TODO.md#phase-{p}) | Ziel | 0 von 9 | x |\n' for p in range(7))
        readme += '| **Gesamt** | | **0 von 99** | x |\n'
        result = source.update_readme(parsed, readme)
        self.assertIn('**7 von 8**', result)
        self.assertEqual(source.update_readme(parsed, result), result)

    def test_missing_readme_row_fails(self):
        with self.assertRaises(ValueError):
            source.update_readme(source.parse(todo()), '')

    def test_progress_bar_boundaries(self):
        self.assertEqual(source.progress_bar(0, 8), '░' * 11)
        self.assertEqual(source.progress_bar(8, 8), '█' * 11)
        self.assertEqual(source.progress_bar(7, 8), '██████████░')

    def test_svg_only_completed_phases_have_green_background(self):
        parsed = source.parse(todo('| `a` | P0 | Text | – | parallel | wenig |\n'))
        readme = ''.join(f'| [{p}](TODO.md#phase-{p}) | Ziel & Test | 0 von 9 | x |\n' for p in range(7))
        readme += '| **Gesamt** | | **0 von 99** | x |\n'
        svg = source.progress_svg(parsed, readme)
        root = ET.fromstring(svg)
        ns = {'s': 'http://www.w3.org/2000/svg'}
        for phase in range(7):
            row = root.find(f'.//s:g[@id="phase-{phase}"]', ns)
            self.assertEqual(row.find('s:rect', ns).get('fill'), '#ffffff' if phase == 1 else '#dcfce7')
            text = ''.join(row.itertext())
            self.assertIn('1 von 2' if phase == 1 else '1 von 1', text)
            self.assertEqual('Abgeschlossen' in text, phase != 1)
        self.assertIn('Ziel &amp; Test', svg)
        self.assertNotIn('#dcfce7', ET.tostring(root.find('.//s:g[@id="phase-gesamt"]', ns)).decode())
        self.assertEqual(svg, source.progress_svg(parsed, source.update_readme(parsed, readme)))

    def test_svg_all_done_includes_green_total(self):
        parsed = source.parse(todo())
        readme = ''.join(f'| [{p}](TODO.md#phase-{p}) | Ziel | 0 von 9 | x |\n' for p in range(7))
        readme += '| **Gesamt** | | **0 von 99** | x |\n'
        root = ET.fromstring(source.progress_svg(parsed, readme))
        ns = {'s': 'http://www.w3.org/2000/svg'}
        row = root.find('.//s:g[@id="phase-gesamt"]', ns)
        self.assertEqual(row.find('s:rect', ns).get('fill'), '#dcfce7')
        self.assertIn('7 von 7', ''.join(row.itertext()))


class ReservationTests(unittest.TestCase):
    ROWS = ('| `open` | P0 | Text | – | parallel | wenig |\n'
            '| `blocked` | P0 | Text | `open` | parallel | wenig |\n'
            '| `vote` | P0 | Text | – | Team | keine |\n')

    def claim(self, name, branches=frozenset(), prs=None):
        text = todo(self.ROWS)
        with patch.object(tasks, 'git') as git, patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value=set(branches)), \
                patch.object(tasks, 'pull_requests', return_value=prs), \
                patch.object(tasks, 'claim_info', return_value=('other-agent', 0)):
            with self.assertRaises(SystemExit) as raised:
                tasks.cmd_claim(name, 'test')
            self.assertEqual(git.call_args_list, [call('fetch', '-q', '--prune', 'origin')])
        return str(raised.exception)

    def test_done_task_cannot_be_claimed(self):
        self.assertIn('erledigt', self.claim('base1'))

    def test_blocked_task_cannot_be_claimed(self):
        self.assertIn('blockiert', self.claim('blocked'))

    def test_team_task_cannot_be_claimed(self):
        self.assertIn('Team', self.claim('vote'))

    def test_existing_branch_prevents_claim_without_github(self):
        self.assertIn('reserviert', self.claim('open', branches={'open'}))

    def test_open_or_merged_pr_prevents_claim(self):
        for state in ('OPEN', 'MERGED'):
            with self.subTest(state=state):
                message = self.claim('open', prs={'open': {'number': 8, 'state': state, 'isDraft': False}})
                self.assertIn('Keine neue Reservierung', message)

    def test_status_values(self):
        parsed = source.parse(todo(self.ROWS))
        self.assertEqual(tasks.status('base1', set(), None, parsed), 'erledigt')
        self.assertEqual(tasks.status('open', set(), None, parsed), 'frei')
        self.assertEqual(tasks.status('blocked', set(), None, parsed), 'blockiert, braucht open')
        self.assertEqual(tasks.status('vote', set(), None, parsed), 'Team')
        self.assertEqual(tasks.status('missing', set(), None, parsed), 'nicht in TODO.md')

    def test_list_without_github_hides_done_and_shows_status(self):
        # Project tasks become unblocked as gates close; test rendering against stable fixture dependencies.
        text = todo(self.ROWS)
        with patch.object(tasks, 'git'), patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value=set()), \
                patch.object(tasks, 'pull_requests', return_value=None), redirect_stdout(StringIO()) as out:
            tasks.cmd_list()
        lines = out.getvalue().splitlines()
        self.assertFalse(any(line.strip().startswith('base1 ') for line in lines))
        row = next(line for line in lines if line.strip().startswith('blocked '))
        self.assertIn('blockiert', row)
        self.assertIn('frei', next(line for line in lines if line.strip().startswith('open ')))
        self.assertIn('Team', next(line for line in lines if line.strip().startswith('vote ')))


class PairReservationTests(unittest.TestCase):
    """Aufgaben "zu zweit": je eine Person als host und als client, Branch task/<kennung>/<rolle>."""
    ROWS = ('| `pair` | P1 | Test auf zwei PCs | – | zu zweit | wenig |\n'
            '| `solo` | P1 | Text | – | parallel | wenig |\n')

    def claim(self, name, role=None, branches=frozenset(), prs=None):
        """Ergebnis von cmd_claim: Abbruchtext oder die Git-Aufrufe einer erfolgreichen Reservierung."""
        text = todo(self.ROWS)
        with patch.object(tasks, 'git') as git, patch.object(tasks, 'run') as run, \
                patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value=set(branches)), \
                patch.object(tasks, 'pull_requests', return_value=prs), \
                patch.object(tasks, 'repo_root', return_value='root'), \
                patch.object(tasks, 'claim_info', return_value=('other-agent', 0)), redirect_stdout(StringIO()):
            git.return_value.returncode = 0
            run.return_value.returncode = 0
            try:
                tasks.cmd_claim(name, 'test', role)
            except SystemExit as raised:
                return str(raised)
        return git.call_args_list

    def test_mode_is_valid(self):
        self.assertEqual(source.parse(todo(self.ROWS))['pair'].mode, 'zu zweit')

    def test_role_claim_uses_role_branch_and_worktree(self):
        calls = self.claim('pair', 'host')
        self.assertIn(call('-C', 'root', 'worktree', 'add', '-q', '-b', 'task/pair/host',
                           os.path.join('root', '.worktrees', 'pair-host'), 'origin/main'), calls)
        self.assertIn(call('-C', os.path.join('root', '.worktrees', 'pair-host'), 'push', '-q', '-u', 'origin',
                           'task/pair/host', check=False), calls)

    def test_second_person_takes_other_role(self):
        self.assertIsInstance(self.claim('pair', 'client', branches={'pair/host'}), list)

    def test_same_role_twice_rejected(self):
        self.assertIn('Rolle host ist vergeben', self.claim('pair', 'host', branches={'pair/host'}))

    def test_role_with_open_pr_rejected(self):
        prs = {'pair/client': {'number': 9, 'state': 'OPEN', 'isDraft': True}}
        self.assertIn('Rolle client ist vergeben', self.claim('pair', 'client', prs=prs))

    def test_whole_claim_blocks_roles_and_roles_block_whole_claim(self):
        self.assertIn('existiert schon', self.claim('pair', 'host', branches={'pair'}))
        self.assertIn('Nur die freie Rolle', self.claim('pair', branches={'pair/client'}))

    def test_one_person_may_take_both_roles(self):
        self.assertIsInstance(self.claim('pair'), list)

    def test_role_only_for_pair_tasks(self):
        self.assertIn('keine Aufgabe "zu zweit"', self.claim('solo', 'host'))
        self.assertIn('Rolle muss host oder client sein', self.claim('pair', 'beide'))

    def test_status_shows_both_roles(self):
        parsed = source.parse(todo(self.ROWS))
        with patch.object(tasks, 'claim_info', return_value=('anna', 0)):
            self.assertEqual(tasks.status('pair', {'pair/host'}, None, parsed),
                             'zu zweit: host reserviert (anna), client frei')
            prs = {'pair/client': {'number': 9, 'state': 'OPEN', 'isDraft': False}}
            self.assertEqual(tasks.status('pair', {'pair/host', 'pair/client'}, prs, parsed),
                             'zu zweit: host reserviert (anna), client PR #9 wartet auf Review')
        self.assertEqual(tasks.status('pair', set(), None, parsed), 'frei')

    def test_role_branches_are_not_unknown_ids(self):
        self.assertEqual(tasks.split_branch('pair/host'), ('pair', 'host'))
        self.assertEqual(tasks.split_branch('pair'), ('pair', None))
        self.assertEqual(tasks.split_branch('pair/other'), ('pair/other', None))


if __name__ == '__main__':
    unittest.main()
