"""Aufgabentabellen in TODO.md, Abhaengigkeiten, Reservierungsschutz und README-Fortschritt."""
from contextlib import redirect_stdout
from io import StringIO
import os
from pathlib import Path
import tempfile
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
        extra += '## Real Coop logic\n' + OPEN + '| `c` | P2 | Text | – | parallel | wenig |\n'
        parsed = source.parse(todo(rows, extra=extra))
        counts = source.phase_counts(parsed)
        self.assertEqual(counts[1], [1, 2])
        self.assertEqual(counts[0], [1, 1])
        readme = ''.join(f'| [{p}](TODO.md#phase-{p}) | Ziel | 0 of 9 | x |\n' for p in range(7))
        readme += '| **Total** | | **0 of 99** | x |\n'
        result = source.update_readme(parsed, readme)
        self.assertIn('**7 of 8**', result)
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
        readme = ''.join(f'| [{p}](TODO.md#phase-{p}) | Ziel & Test | 0 of 9 | x |\n' for p in range(7))
        readme += '| **Total** | | **0 of 99** | x |\n'
        svg = source.progress_svg(parsed, readme)
        root = ET.fromstring(svg)
        ns = {'s': 'http://www.w3.org/2000/svg'}
        for phase in range(7):
            row = root.find(f'.//s:g[@id="phase-{phase}"]', ns)
            self.assertEqual(row.find('s:rect', ns).get('fill'), '#ffffff' if phase == 1 else '#dcfce7')
            text = ''.join(row.itertext())
            self.assertIn('1 of 2' if phase == 1 else '1 of 1', text)
            self.assertEqual('Completed' in text, phase != 1)
        self.assertIn('Ziel &amp; Test', svg)
        self.assertNotIn('#dcfce7', ET.tostring(root.find('.//s:g[@id="phase-total"]', ns)).decode())
        self.assertEqual(svg, source.progress_svg(parsed, source.update_readme(parsed, readme)))

    def test_svg_all_done_includes_green_total(self):
        parsed = source.parse(todo())
        readme = ''.join(f'| [{p}](TODO.md#phase-{p}) | Ziel | 0 of 9 | x |\n' for p in range(7))
        readme += '| **Total** | | **0 of 99** | x |\n'
        root = ET.fromstring(source.progress_svg(parsed, readme))
        ns = {'s': 'http://www.w3.org/2000/svg'}
        row = root.find('.//s:g[@id="phase-total"]', ns)
        self.assertEqual(row.find('s:rect', ns).get('fill'), '#dcfce7')
        self.assertIn('7 of 7', ''.join(row.itertext()))


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
        """Ergebnis of cmd_claim: Abbruchtext oder die Git-Aufrufe einer erfolgreichen Reservierung."""
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


class ConfirmationTests(unittest.TestCase):
    ROWS = ('| `first` | P1 | Zehn Dodges mit Ausdauer pruefen | – | parallel | wenig |\n'
            '| `second` | P1 | Gleiche Karte pruefen | – | zu zweit | wenig |\n'
            '| `blocked` | P1 | Wartet | `first` | parallel | wenig |\n')

    def pending(self):
        return source.confirmation_update(todo(self.ROWS), ['first', 'second'], 'pending', 'Test auf zwei PCs steht aus')

    def test_pending_keeps_requirements_phase_and_not_done(self):
        parsed = source.parse(self.pending())
        self.assertFalse(parsed['first'].done)
        self.assertEqual(parsed['first'].phase, '1')
        self.assertEqual(parsed['first'].confirmation, 'Test auf zwei PCs steht aus')
        self.assertIn('Zehn Dodges mit Ausdauer pruefen', parsed['first'].text)
        self.assertEqual(source.blockers(parsed['blocked'], parsed), ['first'])
        self.assertEqual(source.phase_counts(parsed)[1], [1, 4])

    def test_pending_is_separate_from_free_and_active_even_with_old_branch_or_pr(self):
        text = self.pending()
        parsed = source.parse(text)
        block = source.free_block(text, parsed)
        self.assertNotIn('| `first` |', block)
        self.assertIn('Bestätigungstest ausstehend (nicht in Arbeit)', block)
        self.assertEqual(tasks.status('first', {'first'}, None, parsed), 'Bestätigungstest ausstehend')
        prs = {'first': {'state': 'OPEN', 'number': 5, 'isDraft': True}}
        self.assertEqual(tasks.status('first', {'first'}, prs, parsed), 'Bestätigungstest ausstehend')
        with patch.object(tasks, 'git'), patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value={'first'}), \
                patch.object(tasks, 'pull_requests', return_value=prs), redirect_stdout(StringIO()) as out:
            tasks.cmd_list(active_only=True)
        self.assertNotIn('first', out.getvalue())

    def test_pending_cannot_be_claimed_even_without_github(self):
        with patch.object(tasks, 'git') as git, patch.object(tasks, 'todo_text', return_value=self.pending()), \
                patch.object(tasks, 'remote_branches', return_value=set()), \
                patch.object(tasks, 'pull_requests', return_value=None):
            with self.assertRaisesRegex(SystemExit, 'Bestätigungstest ausstehend'):
                tasks.cmd_claim('first', 'tester')
            self.assertEqual(git.call_args_list, [call('fetch', '-q', '--prune', 'origin')])

    def test_update_batch_preserves_original_text_and_does_not_stack_markers(self):
        once = source.confirmation_update(self.pending(), ['first', 'second'], 'update', 'Termin verschoben')
        twice = source.confirmation_update(once, ['second'], 'update', 'Tag 2 noch offen')
        parsed = source.parse(twice)
        self.assertEqual(parsed['first'].confirmation, 'Termin verschoben')
        self.assertEqual(parsed['second'].confirmation, 'Tag 2 noch offen')
        self.assertEqual(parsed['second'].text.count(source.CONFIRMATION_TASK), 1)
        self.assertIn('Gleiche Karte pruefen', parsed['second'].text)
        self.assertNotIn('Test auf zwei PCs steht aus', twice)

    def test_pass_batch_marks_done_only_with_explicit_result(self):
        parsed = source.parse(source.confirmation_update(self.pending(), ['second', 'first'], 'pass',
                                                        'Auf zwei PCs bestanden, siehe [Bericht](docs/TESTING.md)'))
        for name in ('first', 'second'):
            self.assertTrue(parsed[name].done)
            self.assertIn('Bericht', parsed[name].text)
        self.assertEqual(source.blockers(parsed['blocked'], parsed), [])

    def test_reopen_restores_development_and_retains_failed_test_note(self):
        parsed = source.parse(source.confirmation_update(self.pending(), ['first'], 'reopen', 'Client rutscht weiter'))
        self.assertFalse(parsed['first'].done)
        self.assertFalse(parsed['first'].confirmation)
        self.assertIn('Zehn Dodges mit Ausdauer pruefen', parsed['first'].text)
        self.assertIn('Client rutscht weiter', parsed['first'].text)
        self.assertTrue(parsed['second'].confirmation)

    def test_batch_rejects_invalid_done_duplicate_and_unmarked_ids(self):
        for names, action, note in ((['first', 'missing'], 'update', 'x'), (['first', 'base1'], 'update', 'x'),
                                    (['first', 'first'], 'update', 'x'), (['blocked'], 'update', 'x'),
                                    (['first'], 'update', ' '), (['first'], 'pass', 'a|b'),
                                    (['first'], 'update', 'a\nb'), (['first'], 'update', source.CONFIRMATION_TASK),
                                    ([], 'update', 'x')):
            with self.subTest(names=names, action=action, note=note), self.assertRaises(ValueError):
                source.confirmation_update(self.pending(), names, action, note)
        with self.assertRaisesRegex(ValueError, 'blockiert'):
            source.confirmation_update(self.pending(), ['blocked'], 'pending', 'x')

    def test_malformed_confirmation_rejected(self):
        row = '| `a` | P1 | ' + source.CONFIRMATION_PREFIX + 'ohne Abnahme | – | parallel | wenig |\n'
        with self.assertRaisesRegex(ValueError, 'Bestaetigungstest braucht'):
            source.parse(todo(row))

    def test_crlf_and_generated_free_list_are_preserved(self):
        text = source.update_todo(todo(self.ROWS), source.parse(todo(self.ROWS))).replace('\n', '\r\n')
        updated = source.confirmation_update(text, ['first'], 'pending', 'offen')
        self.assertNotIn('\n', updated.replace('\r\n', ''))
        self.assertEqual(source.parse(updated)['first'].confirmation, 'offen')
        self.assertIn('| `first` | P1 | [1]', updated) # Liste erneuert erst regenerate

    def test_active_work_protects_roles_and_unknown_github_state(self):
        self.assertTrue(tasks.active_work('first', {'first/host'}, None))
        self.assertTrue(tasks.active_work('first', set(), {'first/client': {'state': 'OPEN'}}))
        self.assertFalse(tasks.active_work('first', {'first'}, {'first': {'state': 'MERGED'}}))
        self.assertFalse(tasks.active_work('first', {'first'}, None, 'task/first'))
        self.assertTrue(tasks.active_work('first', {'first/client'}, None, 'task/first/host'))

    def run_command(self, root, args, branches=frozenset(), prs=None):
        with patch.object(tasks, 'git') as git, patch.object(tasks, 'remote_branches', return_value=set(branches)), \
                patch.object(tasks, 'pull_requests', return_value=prs if prs is not None else {}), \
                patch.object(tasks, 'worktree_root', return_value=root), \
                patch.object(tasks, 'todo_text', return_value=self.pending()), patch.object(tasks, 'regenerate', return_value=[]), \
                redirect_stdout(StringIO()) as out:
            git.return_value.stdout = 'task/batch-tests'
            tasks.cmd_tests(args)
        return out.getvalue()

    def test_plan_and_all_update_exclude_active_foreign_work(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'TODO.md'
            path.write_text(self.pending(), encoding='utf-8')
            output = self.run_command(root, ['plan'], {'first/client'})
            self.assertIn('Ausgeschlossen: first: aktive fremde Arbeit', output)
            self.assertIn('Gleiche Karte pruefen', output)
            output = self.run_command(root, ['update', '--all', '--note', 'Neuer Termin'], {'first/client'})
            parsed = source.parse(path.read_text(encoding='utf-8'))
            self.assertEqual(parsed['first'].confirmation, 'Test auf zwei PCs steht aus')
            self.assertEqual(parsed['second'].confirmation, 'Neuer Termin')
            self.assertIn('Ausgeschlossen: first', output)

    def test_explicit_busy_batch_fails_without_partial_write(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'TODO.md'
            path.write_text(self.pending(), encoding='utf-8')
            before = path.read_bytes()
            with self.assertRaisesRegex(SystemExit, 'Gesamter Batch unveraendert'):
                self.run_command(root, ['update', 'second', 'first', '--note', 'x'], {'first'})
            self.assertEqual(path.read_bytes(), before)
            with self.assertRaisesRegex(ValueError, 'ohne'):
                self.run_command(root, ['update', 'second', '--note', 'x|y'])
            self.assertEqual(path.read_bytes(), before)

    def test_pass_batch_can_complete_dependencies_together_but_not_without_them(self):
        text = self.pending()
        # Ein bereits vorbereiteter Test kann spaeter einen noch offenen Bestaetigungstest als Voraussetzung haben.
        text = text.replace('Gleiche Karte pruefen | – |', 'Gleiche Karte pruefen | `first` |')
        with self.assertRaisesRegex(ValueError, 'blockiert'):
            source.confirmation_update(text, ['second'], 'pass', 'Bericht')
        parsed = source.parse(source.confirmation_update(text, ['second', 'first'], 'pass', 'Bericht'))
        self.assertTrue(parsed['first'].done and parsed['second'].done)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'TODO.md'
            path.write_text(text, encoding='utf-8')
            before = path.read_bytes()
            output = self.run_command(root, ['pass', '--all', '--note', 'Bericht'], {'first'})
            self.assertEqual(path.read_bytes(), before)
            self.assertIn('Ausgeschlossen: first: aktive fremde Arbeit', output)
            self.assertIn('Ausgeschlossen: second: blockiert', output)

    def test_mutations_fail_closed_without_pr_status_and_on_main(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'TODO.md'
            path.write_text(self.pending(), encoding='utf-8')
            before = path.read_bytes()
            for prs, branch, message in ((None, 'task/tests', 'PR-Status unbekannt'), ({}, 'main', 'nie auf main')):
                with self.subTest(prs=prs, branch=branch), patch.object(tasks, 'git') as git, \
                        patch.object(tasks, 'remote_branches', return_value=set()), \
                        patch.object(tasks, 'pull_requests', return_value=prs), \
                        patch.object(tasks, 'worktree_root', return_value=root), \
                        self.assertRaisesRegex(SystemExit, message):
                    git.return_value.stdout = branch
                    tasks.cmd_tests(['update', '--all', '--note', 'x'])
                self.assertEqual(path.read_bytes(), before)


class ReferenceTests(unittest.TestCase):
    def setUp(self):
        self.tasks = source.parse(todo('| `offen` | P0 | Text | – | parallel | wenig |\n'))

    def test_done_task_announced_as_future_is_reported(self):
        for sentence in ('Das kommt mit `base1`.', 'Rest folgt erst in `base2`.', 'Absturz bleibt, bis `base3` ihn behebt.'):
            with self.subTest(sentence=sentence):
                found = source.stale_references(self.tasks, {'docs/X.md': 'Kopf\n' + sentence})
                self.assertEqual(len(found), 1)
                self.assertTrue(found[0].startswith('docs/X.md:2: `base'))

    def test_open_or_unknown_task_and_history_are_ignored(self):
        text = ('Das kommt mit `offen`.\nDas kommt mit `unbekannt`.\nMod-Stand `main` bis `base1` (#131).\n'
                'Tasten `1` bis `4` gesperrt.\nErledigt mit `base1`.')
        self.assertEqual(source.stale_references(self.tasks, {'docs/X.md': text}), [])

    def test_task_rows_in_todo_are_ignored(self):
        row = '| `x` | P0 | Rest kommt mit `base1` | – | parallel | wenig |'
        self.assertEqual(source.stale_references(self.tasks, {'TODO.md': row}), [])
        self.assertEqual(len(source.stale_references(self.tasks, {'TODO.md': 'Fließtext: kommt mit `base1`.'})), 1)

    def test_real_documents_have_no_stale_references(self):
        parsed = source.parse((ROOT / 'TODO.md').read_text(encoding='utf-8'))
        self.assertEqual(source.check_references(ROOT, parsed), [])

    def test_markdown_files_skip_foreign_and_local_folders(self):
        names = source.markdown_files(ROOT)
        self.assertIn('TODO.md', names)
        self.assertFalse([n for n in names if n.startswith(source.SKIPPED_DIRS)])


class MarkDoneTests(unittest.TestCase):
    ROW = '| `zeta` | P1 | Text | `base1` | parallel | wenig |\n| `alpha` | P0 | Text | – | parallel | keine |\n'

    def test_moves_row_alphabetically_with_phase(self):
        text = source.mark_done(todo(self.ROW), 'alpha', 'Fertig, siehe [Doku](docs/x.md)')
        parsed = source.parse(text)
        self.assertTrue(parsed['alpha'].done)
        self.assertEqual(parsed['alpha'].phase, '1')
        self.assertEqual(parsed['alpha'].text, 'Fertig, siehe [Doku](docs/x.md)')
        self.assertFalse(parsed['zeta'].done)
        self.assertEqual(text.count('`alpha`'), 1)
        self.assertLess(text.index('| `alpha` | 1 |'), text.index('| `base0` |'))

    def test_appends_after_last_done_row(self):
        text = source.mark_done(todo(self.ROW), 'zeta', 'Ergebnis')
        self.assertTrue(text.rstrip().endswith('| `zeta` | 1 | Ergebnis |'))
        source.parse(text)

    def test_keeps_crlf(self):
        text = source.mark_done(todo(self.ROW).replace('\n', '\r\n'), 'alpha', 'Ergebnis')
        self.assertNotIn('\n', text.replace('\r\n', ''))
        self.assertTrue(source.parse(text)['alpha'].done)

    def test_mixed_line_endings_find_newly_inserted_task(self):
        text = todo(self.ROW).replace('\n', '\r\n')
        text = text.replace('| `alpha` | P0 | Text | – | parallel | keine |\r\n',
                            '| `alpha` | P0 | Text | – | parallel | keine |\n')
        done = source.mark_done(text, 'alpha', 'Ergebnis')
        self.assertTrue(source.parse(done)['alpha'].done)
        self.assertNotIn('\n', done.replace('\r\n', ''))

    def test_ignores_free_list_row(self):
        base = todo(self.ROW)
        free = base.replace('<!-- frei:end -->', '| `alpha` | P0 | [1](#x) | parallel | keine |\n<!-- frei:end -->')
        text = source.mark_done(free, 'alpha', 'Ergebnis')
        self.assertIn('| `alpha` | P0 | [1](#x) |', text)
        self.assertTrue(source.parse(text)['alpha'].done)

    def test_rejects_bad_input(self):
        base = todo(self.ROW)
        for name, result, message in (('nope', 'x', 'nicht in TODO'), ('base1', 'x', 'schon erledigt'),
                                      ('alpha', 'a | b', 'ohne |'), ('alpha', ' ', 'ohne |'), ('alpha', 'a\nb', 'ohne |')):
            with self.subTest(name=name, result=result), self.assertRaisesRegex(ValueError, message):
                source.mark_done(base, name, result)

    def test_describe_open_and_done(self):
        parsed = source.parse(todo(self.ROW))
        lines = tasks.describe(parsed['zeta'], parsed, 'frei')
        self.assertEqual(lines[0], 'zeta: P1, Phase 1 – Test, parallel, Spielzeit wenig')
        self.assertEqual(lines[2], 'Braucht: base1 (erledigt)')
        parsed['zeta'].text = 'Siehe [Doku](docs/x.md) **fett**'
        self.assertEqual(tasks.describe(parsed['zeta'], parsed, 'frei')[3], 'Siehe Doku (docs/x.md) fett')
        self.assertEqual(tasks.describe(parsed['base1'], parsed, '')[0], 'base1: erledigt (Phase 1)')


if __name__ == '__main__':
    unittest.main()
