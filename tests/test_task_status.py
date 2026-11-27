"""Aufgabentabellen in TODO.md, Reservierungsschutz und README-Fortschritt."""
from contextlib import redirect_stdout
from io import StringIO
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
import xml.etree.ElementTree as ET
from unittest.mock import call, patch

from tools import task_status as source
from tools import tasks

ROOT = Path(__file__).resolve().parents[1]
OPEN = source.LIST_HEADER + '\n|---|---|---|---|---|\n'
DONE = '## Erledigt\n' + source.DONE_HEADER + '\n|---|---|---|\n'
FLAT = ('<!-- frei:start -->\n<!-- frei:end -->\n## Probleme\n' + source.LIST_HEADER + '\n|---|---|---|---|---|\n{problems}'
        '## Verbesserungen\n' + source.LIST_HEADER + '\n|---|---|---|---|---|\n{ideas}'
        + DONE + '\n'.join(f'| `base{p}` | {p} | Ergebnis |' for p in range(7)) + '\n')


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
        text = source.read_local(ROOT)
        readme = (ROOT / 'README.md').read_text(encoding='utf-8')
        parsed = source.parse(text)
        self.assertEqual(source.update_todo(text, parsed), text)
        self.assertEqual(source.update_readme(parsed, readme), readme)
        self.assertEqual(source.progress_svg(parsed, readme),
                         (ROOT / 'docs/assets/progress.svg').read_text(encoding='utf-8'))
        for name in ('crash-reporter', 'overlay', 'deploy', 'ci', 'grundgeruest', 'testumgebung'):
            self.assertTrue(parsed[name].done, name)

    def test_long_open_task_text_rejected(self):
        long_text = todo('| `a` | P0 | ' + 'x' * 801 + ' | parallel | wenig |\n')
        parsed = source.parse(long_text)  # Lesen bleibt tolerant, etwa fuer einen aelteren Stand von origin/main
        with self.assertRaisesRegex(ValueError, 'über 800 Zeichen'):
            source.check_text_lengths(parsed)
        source.check_text_lengths(source.parse(todo('| `a` | P0 | ' + 'x' * 800 + ' | parallel | wenig |\n',
                                                    done_rows='| `z` | 1 | ' + 'y' * 2000 + ' |')))

    def test_open_task_needs_acceptance(self):
        missing = FLAT.format(problems='| `bug` | P1 | Absturz beim Start | parallel | wenig |\n', ideas='')
        with self.assertRaisesRegex(ValueError, 'bug: Aufgabentext braucht eine Abnahme'):
            source.check_acceptance(source.parse(missing))
        # Erledigte Zeilen und Zeilen mit Abnahme bleiben gueltig.
        source.check_acceptance(source.parse(FLAT.format(
            problems='| `bug` | P1 | Absturz beim Start. **Fertig, wenn** der Start zehnmal gelingt. | parallel | wenig |\n', ideas='')))

    def test_check_all_rejects_long_rows(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source.write_local(root, todo('| `a` | P0 | ' + 'x' * 801 + ' | parallel | wenig |\n'))
            with self.assertRaisesRegex(ValueError, 'über 800 Zeichen'):
                source.check_all(root, check=True)

    def test_open_and_done_rows(self):
        parsed = source.parse(todo('| `a` | P0 | Text | parallel | wenig |\n'))
        self.assertFalse(parsed['a'].done)
        self.assertFalse(hasattr(parsed['a'], 'needs'))  # Aufgaben haengen nicht voneinander ab
        self.assertTrue(parsed['base1'].done)

    def test_checkbox_and_marker_rejected(self):
        for line in ('- [ ] alt\n', '- [x] alt\n', '<!-- tasks: a -->\n'):
            with self.subTest(line=line), self.assertRaisesRegex(ValueError, 'Checkboxen'):
                source.parse(todo(extra=line))

    def test_duplicate_id_rejected(self):
        with self.assertRaisesRegex(ValueError, 'mehrfach'):
            source.parse(todo('| `base1` | P0 | Text | parallel | wenig |\n'))

    def test_invalid_values_rejected(self):
        for row in ('| `a` | P3 | Text | parallel | wenig |\n',
                    '| `a` | P0 | Text | solo | wenig |\n',
                    '| `a` | P0 | Text | parallel | lang |\n',
                    '| `a` | P0 | Text | parallel |\n'):
            with self.subTest(row=row), self.assertRaises(ValueError):
                source.parse(todo(row))

    def test_dependency_column_rejected(self):
        """Die Spalte "Braucht" gibt es nicht mehr: weder als Tabellenkopf noch als sechste Zelle."""
        old_header = '| Kennung | Prio | Aufgabe | Braucht | Bearbeitung | Spielzeit |\n|---|---|---|---|---|---|\n'
        with self.assertRaisesRegex(ValueError, 'Kopf'):
            source.parse(todo(extra='## Probleme\n' + old_header + '| `a` | P0 | Text | `base1` | parallel | wenig |\n'))
        with self.assertRaisesRegex(ValueError, '5 Spalten'):
            source.parse(todo('| `a` | P0 | Text | `base1` | parallel | wenig |\n'))

    def test_done_must_be_sorted(self):
        text = todo().replace('| `base0` | 0 |', '| `zzz` | 0 |')
        with self.assertRaisesRegex(ValueError, 'alphabetisch'):
            source.parse(text)

    def test_phase_sections_optional(self):
        # Alle Phasen sind abgeschlossen; ihre Aufgaben stehen nur noch in docs/ERLEDIGT.md.
        parsed = source.parse(todo().replace('## Phase 3 – Test\n', ''))
        self.assertTrue(parsed['base3'].done)

    def test_flat_list_rows(self):
        text = FLAT.format(problems='| `bug` | P1 | Absturz | parallel | wenig |\n',
                           ideas='| `idee` | P2 | Besser | parallel | keine |\n')
        parsed = source.parse(text)
        self.assertEqual((parsed['bug'].phase, parsed['bug'].done), ('Problem', False))
        self.assertEqual(parsed['idee'].phase, 'Verbesserung')
        source.check_flat_list(parsed)
        self.assertEqual(source.phase_counts(parsed), {p: [1, 1] for p in range(7)})

    def test_flat_list_done_row_keeps_section_as_phase(self):
        text = FLAT.format(problems='| `bug` | P1 | Absturz | parallel | wenig |\n', ideas='')
        parsed = source.parse(source.mark_done(text, 'bug', 'Behoben.'))
        self.assertTrue(parsed['bug'].done)
        self.assertEqual(parsed['bug'].phase, 'Problem')

    def test_check_flat_list_rejects_phases(self):
        with self.assertRaisesRegex(ValueError, 'Probleme oder Verbesserungen'):
            source.check_flat_list(source.parse(todo('| `a` | P1 | Text | parallel | wenig |\n')))

    def test_wrong_table_header_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Kopf'):
            source.parse(todo(extra='## Phase 9\n| Kennung | Aufgabe |\n'))


class DerivedTests(unittest.TestCase):
    def test_free_block_lists_every_open_task_and_team(self):
        rows = ('| `ready` | P1 | Text | parallel | wenig |\n'
                '| `first` | P0 | Text | zentral | viel |\n'
                '| `vote` | P0 | Text | Team | keine |\n')
        text = todo(rows)
        block = source.free_block(text, source.parse(text))
        lines = block.splitlines()
        self.assertTrue(lines[2].startswith('| `first` | P0'))
        self.assertTrue(lines[3].startswith('| `ready` | P1'))
        self.assertNotIn('Zurückgestellt', block)
        self.assertIn('**Wartet auf das Team:** `vote`.', block)

    WORK_ORDER = FLAT.format(
        problems=('| `bug-spaet` | P2 | Text | parallel | mittel |\n'
                  '| `bug-kurz` | P2 | Text | parallel | wenig |\n'
                  '| `bug-wichtig` | P1 | Text | zentral | viel |\n'),
        ideas=('| `idee-zentral` | P1 | Text | zentral | keine |\n'
               '| `idee-viel` | P1 | Text | parallel | viel |\n'
               '| `idee-b` | P1 | Text | parallel | keine |\n'
               '| `idee-a` | P1 | Text | parallel | keine |\n'
               '| `idee-zwei` | P1 | Text | zu zweit | keine |\n'
               '| `idee-team` | P1 | Text | Team | keine |\n'))
    EXPECTED_ORDER = ['bug-wichtig', 'idee-a', 'idee-b', 'idee-zwei', 'idee-zentral', 'idee-viel', 'bug-kurz', 'bug-spaet']

    def test_free_block_orders_by_prio_section_playtime_mode(self):
        """Prio, Probleme vor Verbesserungen, wenig Spielzeit vor viel, parallel vor zentral, dann Kennung."""
        block = source.free_block(self.WORK_ORDER, source.parse(self.WORK_ORDER))
        names = [line.split('`')[1] for line in block.splitlines()[2:] if line.startswith('| `')]
        self.assertEqual(names, self.EXPECTED_ORDER)
        self.assertIn('**Wartet auf das Team:** `idee-team`.', block)

    def test_list_uses_the_same_order(self):
        with (patch.object(tasks, 'git'), patch.object(tasks, 'todo_text', return_value=self.WORK_ORDER),
              patch.object(tasks, 'remote_branches', return_value=set()),
              patch.object(tasks, 'pull_requests', return_value=None)):
            with redirect_stdout(StringIO()) as short:
                tasks.cmd_list(short=True)
            with redirect_stdout(StringIO()) as full:
                tasks.cmd_list()
        listed = [line.split()[0] for line in short.getvalue().splitlines() if line.startswith('  ')]
        self.assertEqual([n for n in listed if n != 'idee-team'], self.EXPECTED_ORDER)
        self.assertEqual(listed[5], 'idee-team')  # Team zuletzt innerhalb gleicher Prio und Spielzeit
        rows = [line.split()[0] for line in full.getvalue().splitlines() if line.startswith('  ')]
        # Je Abschnitt, darin dieselbe Reihenfolge.
        self.assertEqual(rows, ['bug-wichtig', 'bug-kurz', 'bug-spaet',
                                'idee-a', 'idee-b', 'idee-zwei', 'idee-zentral', 'idee-team', 'idee-viel'])

    def test_optional_section_is_listed_like_any_other(self):
        """Es gibt keine zurueckgestellten Aufgaben: Auch eine offene Zeile unter Optionen steht in der Liste."""
        optional = '## Optionale Optimierungen\n' + OPEN + '| `opt-x` | P2 | Text | parallel | wenig |\n'
        text = todo('| `run` | P0 | Text | zentral | viel |\n', extra=optional)
        block = source.free_block(text, source.parse(text))
        self.assertIn('| `opt-x` | P2', block)
        self.assertNotIn('Zurückgestellt', block)

    def test_update_todo_is_idempotent_and_keeps_crlf(self):
        text = todo('| `a` | P0 | Text | parallel | wenig |\n').replace('\n', '\r\n')
        parsed = source.parse(text)
        once = source.update_todo(text, parsed)
        self.assertIn('| `a` | P0 |', once)
        self.assertNotIn('\r\r', once)
        self.assertEqual(source.update_todo(once, source.parse(once)), once)

    def test_progress_counts_phases_only(self):
        rows = '| `a` | P0 | Text | parallel | wenig |\n'
        extra = '## Optionale Optimierungen\n' + OPEN + '| `o` | P2 | Text | parallel | wenig |\n'
        extra += '## Real Coop logic\n' + OPEN + '| `c` | P2 | Text | parallel | wenig |\n'
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
        parsed = source.parse(todo('| `a` | P0 | Text | parallel | wenig |\n'))
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
    ROWS = ('| `open` | P0 | Text | parallel | wenig |\n'
            '| `other` | P0 | Text | parallel | wenig |\n'
            '| `vote` | P0 | Text | Team | keine |\n')

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

    def test_team_task_cannot_be_claimed(self):
        self.assertIn('Team', self.claim('vote'))

    def test_existing_branch_prevents_claim_without_github(self):
        self.assertIn('reserviert', self.claim('open', branches={'open'}))

    def test_open_pr_prevents_claim(self):
        message = self.claim('open', prs={'open': {'number': 8, 'state': 'OPEN', 'isDraft': False}})
        self.assertIn('Keine neue Reservierung', message)

    def test_merged_leftover_compares_branch_tip_with_merged_head(self):
        pr = {'number': 8, 'state': 'MERGED', 'isDraft': False, 'headRefOid': 'abc123'}
        for tip, expected in (('abc123\n', True), ('def456\n', False), ('', False)):
            with self.subTest(tip=tip), patch.object(tasks, 'git', return_value=SimpleNamespace(stdout=tip)):
                self.assertEqual(tasks.merged_leftover('open', pr), expected)
        with patch.object(tasks, 'git', return_value=SimpleNamespace(stdout='abc123\n')):
            self.assertFalse(tasks.merged_leftover('open', {'number': 8, 'state': 'MERGED'}))  # Stand des PRs unbekannt

    def test_merged_pr_does_not_lock_open_task(self):
        """Erledigtes ist nur Info: Ein gemergter PR sperrt die offene Aufgabe nicht."""
        merged = {'open': {'number': 8, 'state': 'MERGED', 'isDraft': False}}
        parsed = source.parse(todo(self.ROWS))
        self.assertEqual(tasks.status('open', set(), merged, parsed), 'frei (früherer PR #8 gemergt)')
        # Der alte Branch liegt unveraendert auf origin: nur der Name ist belegt, die Meldung sagt, wie man ihn freimacht.
        with patch.object(tasks, 'merged_leftover', return_value=True):
            self.assertEqual(tasks.status('open', {'open'}, merged, parsed), 'frei (früherer PR #8 gemergt)')
            message = self.claim('open', branches={'open'}, prs=merged)
        self.assertIn('git push origin --delete task/open', message)
        self.assertNotIn('nachfragen', message)
        # Auf dem Branch liegen neue Commits (etwa nach einem Teil-PR): Das ist eine Reservierung, kein Rest.
        with patch.object(tasks, 'merged_leftover', return_value=False), \
                patch.object(tasks, 'claim_info', return_value=('other-agent', 0)):
            self.assertEqual(tasks.status('open', {'open'}, merged, parsed), 'reserviert (other-agent)')
            message = self.claim('open', branches={'open'}, prs=merged)
        self.assertIn('existiert schon', message)
        self.assertNotIn('--delete', message)
        # Ohne alten Branch laeuft die Reservierung durch.
        text = todo(self.ROWS)
        done = SimpleNamespace(returncode=0, stdout='https://example.invalid/pr/9\n')
        with patch.object(tasks, 'git', return_value=done) as git, patch.object(tasks, 'run', return_value=done), \
                patch.object(tasks, 'todo_text', return_value=text), patch.object(tasks, 'repo_root', return_value='repo'), \
                patch.object(tasks, 'remote_branches', return_value=set()), \
                patch.object(tasks, 'pull_requests', return_value=merged), redirect_stdout(StringIO()) as out:
            tasks.cmd_claim('open', 'test')
        self.assertIn('Reserviert: task/open', out.getvalue())
        self.assertTrue(any(c.args[1:4] == ('repo', 'worktree', 'add') for c in git.call_args_list))

    def test_status_values(self):
        parsed = source.parse(todo(self.ROWS))
        self.assertEqual(tasks.status('base1', set(), None, parsed), 'erledigt')
        self.assertEqual(tasks.status('open', set(), None, parsed), 'frei')
        self.assertEqual(tasks.status('other', set(), None, parsed), 'frei')
        self.assertEqual(tasks.status('vote', set(), None, parsed), 'Team')
        self.assertEqual(tasks.status('missing', set(), None, parsed), 'nicht in TODO.md')

    def test_list_without_github_hides_done_and_shows_status(self):
        text = todo(self.ROWS)
        with patch.object(tasks, 'git'), patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value=set()), \
                patch.object(tasks, 'pull_requests', return_value=None), redirect_stdout(StringIO()) as out:
            tasks.cmd_list()
        lines = out.getvalue().splitlines()
        self.assertFalse(any(line.strip().startswith('base1 ') for line in lines))
        self.assertIn('frei', next(line for line in lines if line.strip().startswith('other ')))
        self.assertIn('frei', next(line for line in lines if line.strip().startswith('open ')))
        self.assertIn('Team', next(line for line in lines if line.strip().startswith('vote ')))

    def test_list_short_has_no_descriptions_and_lists_all_open(self):
        extra = ('## Optionale Optimierungen\n' + OPEN + '| `opt-x` | P2 | Langer Text | parallel | wenig |\n'
                 '## Real Coop logic\n' + OPEN + '| `coop-x` | P2 | Text | parallel | wenig |\n')
        text = todo(self.ROWS.replace('| `vote` | P0 | Text |', '| `vote` | P0 | ' + 'Sehr lang ' * 20 + '|'), extra=extra)
        with patch.object(tasks, 'git'), patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value=set()), \
                patch.object(tasks, 'pull_requests', return_value=None), redirect_stdout(StringIO()) as out:
            tasks.cmd_list(short=True)
        lines = out.getvalue().splitlines()
        self.assertTrue(any(line.split() == ['open', 'frei'] for line in lines))
        self.assertTrue(all(len(line) <= 80 for line in lines))
        self.assertFalse(any('Sehr lang' in line for line in lines))
        self.assertTrue(any(line.split() == ['opt-x', 'frei'] for line in lines))  # zurueckgestellt gibt es nicht mehr
        self.assertIn('Bestätigungstests: 0 (tasks.py tests list)', lines)


class PairReservationTests(unittest.TestCase):
    """Aufgaben "zu zweit": je eine Person als host und als client, Branch task/<kennung>/<rolle>."""
    ROWS = ('| `pair` | P1 | Test auf zwei PCs | zu zweit | wenig |\n'
            '| `solo` | P1 | Text | parallel | wenig |\n')

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

    def test_cleanup_removes_only_safe_worktrees(self):
        merged = {'a': {'number': 1, 'state': 'MERGED', 'isDraft': False}}
        closed = {'a': {'number': 2, 'state': 'CLOSED', 'isDraft': False}}
        opened = {'a': {'number': 3, 'state': 'OPEN', 'isDraft': True}}
        verdict = tasks.worktree_verdict
        self.assertTrue(verdict('task/a', False, False, set(), merged)[0])
        self.assertFalse(verdict('task/a', True, True, set(), merged)[0])
        self.assertFalse(verdict('task/a', False, False, {'a'}, merged)[0])
        self.assertFalse(verdict('task/a', False, False, set(), closed)[0])
        self.assertFalse(verdict('task/a', False, True, set(), opened)[0])
        self.assertFalse(verdict('task/a', False, False, set(), {})[0])
        self.assertTrue(verdict(None, False, True, set(), {})[0])
        self.assertFalse(verdict(None, False, False, set(), {})[0])

    def test_merge_order_prefers_few_conflicts(self):
        # Stand = Menge gemergter PRs; 'hub' kollidiert mit allen, 'late' nur nach 'x'.
        clash = {frozenset({'hub', 'x'}), frozenset({'hub', 'y'}), frozenset({'late', 'x'})}

        def merge(state, name):
            merged = set() if state == 'origin/main' else set(state)
            if any(frozenset({name, m}) in clash for m in merged):
                return ['TODO.md'], None
            return [], tuple(sorted(merged | {name}))

        pairs = {'hub': {'x', 'y'}, 'x': {'hub', 'late'}, 'y': {'hub'}, 'late': {'x'}, 'key': set()}
        rank = {n: (0, 2, i) for i, n in enumerate(['hub', 'x', 'y', 'late', 'key'])}
        order, rest = tasks.merge_order(['hub', 'x', 'y', 'late', 'key'], merge, rank, pairs, {})
        self.assertEqual(order[0], 'key')
        self.assertNotIn('hub', order[:2])
        self.assertEqual(set(order) | set(rest), {'hub', 'x', 'y', 'late', 'key'})
        order, _ = tasks.merge_order(['b', 'a'], lambda s, n: ([], (s, n)), {'a': (1,), 'b': (0,)}, {}, {'b': ['a']})
        self.assertEqual(order, ['a', 'b'])

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
    """Bestaetigungstests sind nur Info: die Aufgabe gilt als erledigt, der offene Test steht in docs/ERLEDIGT.md."""
    ROWS = ('| `first` | P1 | Zehn Dodges mit Ausdauer pruefen | parallel | wenig |\n'
            '| `second` | P1 | Gleiche Karte pruefen | zu zweit | wenig |\n'
            '| `third` | P1 | Noch offen | parallel | wenig |\n')

    def pending(self):
        return source.confirmation_update(todo(self.ROWS), ['first', 'second'], 'pending', 'Test auf zwei PCs steht aus')

    def test_pending_marks_done(self):
        parsed = source.parse(self.pending())
        self.assertTrue(parsed['first'].done)
        self.assertEqual(parsed['first'].phase, '1')
        self.assertEqual(parsed['first'].confirmation, 'Test auf zwei PCs steht aus')
        self.assertFalse(parsed['third'].done)
        self.assertEqual(tasks.status('first', set(), None, parsed), 'erledigt')

    def test_pending_is_info_in_done_file_not_open_row(self):
        todo_part, done_part = source.split(self.pending())
        self.assertFalse(any(line.startswith('| `first` |') for line in todo_part.splitlines()))
        self.assertNotIn(source.INFO_SECTION, todo_part)
        self.assertIn('## ' + source.INFO_SECTION, done_part)
        self.assertIn('| `first` | Test auf zwei PCs steht aus |', done_part)

    def test_no_limit_for_pending_tests(self):
        rows = ''.join(f'| `t{i}` | P1 | Text | parallel | wenig |\n' for i in range(6))
        parsed = source.parse(source.confirmation_update(todo(rows), [f't{i}' for i in range(6)], 'pending', 'Stand'))
        self.assertEqual(sum(bool(t.confirmation) for t in parsed.values()), 6)

    def test_free_block_counts_pending_as_info(self):
        text = self.pending()
        block = source.free_block(text, source.parse(text))
        self.assertIn('| `third` |', block)
        self.assertIn('Bestätigungstests ausstehend (nur Info, blockiert nichts):** 2', block)

    def test_update_replaces_note(self):
        once = source.confirmation_update(self.pending(), ['first', 'second'], 'update', 'Termin verschoben')
        twice = source.confirmation_update(once, ['second'], 'update', 'Tag 2 noch offen')
        parsed = source.parse(twice)
        self.assertEqual(parsed['first'].confirmation, 'Termin verschoben')
        self.assertEqual(parsed['second'].confirmation, 'Tag 2 noch offen')
        self.assertNotIn('Test auf zwei PCs steht aus', twice)

    def test_pass_removes_info_and_records_result(self):
        parsed = source.parse(source.confirmation_update(self.pending(), ['second', 'first'], 'pass',
                                                        'Auf zwei PCs bestanden, siehe [Bericht](docs/TESTING.md)'))
        for name in ('first', 'second'):
            self.assertTrue(parsed[name].done)
            self.assertFalse(parsed[name].confirmation)
            self.assertIn('Bestätigt: Auf zwei PCs bestanden', parsed[name].text)

    def test_reopen_removes_info_and_records_failure(self):
        parsed = source.parse(source.confirmation_update(self.pending(), ['first'], 'reopen', 'Client rutscht weiter'))
        self.assertTrue(parsed['first'].done)
        self.assertFalse(parsed['first'].confirmation)
        self.assertIn('Bestätigungstest fehlgeschlagen: Client rutscht weiter', parsed['first'].text)
        self.assertTrue(parsed['second'].confirmation)

    def test_batch_rejects_invalid_ids_and_notes(self):
        for names, action, note in ((['first', 'missing'], 'update', 'x'), (['first', 'base1'], 'update', 'x'),
                                    (['first', 'first'], 'update', 'x'), (['third'], 'update', 'x'),
                                    (['first'], 'update', ' '), (['first'], 'pass', 'a|b'),
                                    (['first'], 'update', 'a\nb'), (['first'], 'pending', 'x'), ([], 'update', 'x')):
            with self.subTest(names=names, action=action, note=note), self.assertRaises(ValueError):
                source.confirmation_update(self.pending(), names, action, note)
        # Jede offene Aufgabe darf fuer sich auf Bestaetigungstest gesetzt werden, nichts haengt an einer anderen.
        self.assertTrue(source.parse(source.confirmation_update(todo(self.ROWS), ['third'], 'pending', 'x'))['third'].done)

    def test_open_row_with_confirmation_marker_rejected(self):
        row = '| `a` | P1 | ' + source.CONFIRMATION_PREFIX + 'Stand' + source.CONFIRMATION_TASK + 'Text | parallel | wenig |\n'
        parsed = source.parse(todo(row))  # Lesen bleibt tolerant fuer einen aelteren Stand von origin/main
        self.assertEqual(parsed['a'].confirmation, 'Stand')
        with self.assertRaisesRegex(ValueError, 'docs/ERLEDIGT.md'):
            source.check_flat_list(parsed)

    def test_info_row_for_open_task_rejected(self):
        text = todo(self.ROWS) + '## ' + source.INFO_SECTION + '\n' + source.INFO_HEADER + '\n|---|---|\n| `first` | x |\n'
        with self.assertRaisesRegex(ValueError, 'nicht erledigt'):
            source.parse(text)

    def test_crlf_is_preserved(self):
        text = source.update_todo(todo(self.ROWS), source.parse(todo(self.ROWS))).replace('\n', '\r\n')
        updated = source.confirmation_update(text, ['first'], 'pending', 'offen')
        self.assertNotIn('\n', updated.replace('\r\n', ''))
        self.assertEqual(source.parse(updated)['first'].confirmation, 'offen')

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

    def make_root(self, directory):
        root = Path(directory)
        source.write_local(root, self.pending())
        return root

    def test_plan_and_all_update_exclude_active_foreign_work(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_root(directory)
            output = self.run_command(root, ['plan'], {'first/client'})
            self.assertIn('Ausgeschlossen: first: aktive fremde Arbeit', output)
            self.assertIn('Gleiche Karte pruefen', output)
            output = self.run_command(root, ['update', '--all', '--note', 'Neuer Termin'], {'first/client'})
            parsed = source.parse(source.read_local(root))
            self.assertEqual(parsed['first'].confirmation, 'Test auf zwei PCs steht aus')
            self.assertEqual(parsed['second'].confirmation, 'Neuer Termin')
            self.assertIn('Ausgeschlossen: first', output)

    def test_explicit_busy_batch_fails_without_partial_write(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_root(directory)
            before = source.read_local(root)
            with self.assertRaisesRegex(SystemExit, 'Gesamter Batch unveraendert'):
                self.run_command(root, ['update', 'second', 'first', '--note', 'x'], {'first'})
            self.assertEqual(source.read_local(root), before)
            with self.assertRaisesRegex(ValueError, 'ohne'):
                self.run_command(root, ['update', 'second', '--note', 'x|y'])
            self.assertEqual(source.read_local(root), before)

    def test_mutations_fail_closed_without_pr_status_and_on_main(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_root(directory)
            before = source.read_local(root)
            for prs, branch, message in ((None, 'task/tests', 'PR-Status unbekannt'), ({}, 'main', 'nie auf main')):
                with self.subTest(prs=prs, branch=branch), patch.object(tasks, 'git') as git, \
                        patch.object(tasks, 'remote_branches', return_value=set()), \
                        patch.object(tasks, 'pull_requests', return_value=prs), \
                        patch.object(tasks, 'worktree_root', return_value=root), \
                        self.assertRaisesRegex(SystemExit, message):
                    git.return_value.stdout = branch
                    tasks.cmd_tests(['update', '--all', '--note', 'x'])
                self.assertEqual(source.read_local(root), before)


class ReferenceTests(unittest.TestCase):
    def setUp(self):
        self.tasks = source.parse(todo('| `offen` | P0 | Text | parallel | wenig |\n'))

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
        row = '| `x` | P0 | Rest kommt mit `base1` | parallel | wenig |'
        self.assertEqual(source.stale_references(self.tasks, {'TODO.md': row}), [])
        self.assertEqual(source.stale_references(self.tasks, {source.DONE_FILE: row}), [])
        self.assertEqual(len(source.stale_references(self.tasks, {'TODO.md': 'Fließtext: kommt mit `base1`.'})), 1)

    def test_real_documents_have_no_stale_references(self):
        parsed = source.parse(source.read_local(ROOT))
        self.assertEqual(source.check_references(ROOT, parsed), [])

    def test_markdown_files_skip_foreign_and_local_folders(self):
        names = source.markdown_files(ROOT)
        self.assertIn('TODO.md', names)
        self.assertFalse([n for n in names if n.startswith(source.SKIPPED_DIRS)])


class MarkDoneTests(unittest.TestCase):
    ROW = '| `zeta` | P1 | Text | parallel | wenig |\n| `alpha` | P0 | Text | parallel | keine |\n'

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
        text = text.replace('| `alpha` | P0 | Text | parallel | keine |\r\n',
                            '| `alpha` | P0 | Text | parallel | keine |\n')
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
        self.assertEqual(lines[1], 'Status: frei')
        self.assertFalse(any(line.startswith('Braucht') for line in lines))
        parsed['zeta'].text = 'Siehe [Doku](docs/x.md) **fett**'
        self.assertEqual(tasks.describe(parsed['zeta'], parsed, 'frei')[2], 'Siehe Doku (docs/x.md) fett')
        self.assertEqual(tasks.describe(parsed['base1'], parsed, '')[0], 'base1: erledigt (Phase 1)')


if __name__ == '__main__':
    unittest.main()


class DoneFileTests(unittest.TestCase):
    """Erledigte Aufgaben stehen in docs/ERLEDIGT.md, die Werkzeuge arbeiten auf beiden Dateien zusammen."""
    OLD = todo('| `frei-a` | P0 | Text | parallel | wenig |\n') + '## Risiken\n\nText\n'

    def test_split_and_combine_roundtrip(self):
        todo_part, done_part = source.split(self.OLD)
        self.assertNotIn('## Erledigt', todo_part)
        self.assertIn('## Risiken', todo_part)
        self.assertTrue(done_part.startswith(source.DONE_TITLE))
        self.assertIn('## Erledigt', done_part)
        combined = source.combine(todo_part, done_part)
        self.assertEqual(source.parse(combined), source.parse(self.OLD))
        self.assertEqual(source.split(combined), (todo_part, done_part))

    def test_split_keeps_line_endings_per_file(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'docs').mkdir()
            todo_part, done_part = source.split(self.OLD)
            (root / 'TODO.md').write_bytes(todo_part.replace('\n', '\r\n').encode('utf-8'))
            (root / source.DONE_FILE).write_bytes(done_part.encode('utf-8'))
            text = source.read_local(root)
            source.write_local(root, source.mark_done(text, 'frei-a', 'Ergebnis.'))
            todo_bytes, done_bytes = (root / 'TODO.md').read_bytes(), (root / source.DONE_FILE).read_bytes()
            self.assertIn(b'\r\n', todo_bytes)
            self.assertNotIn(b'\n', todo_bytes.replace(b'\r\n', b''))
            self.assertNotIn(b'\r', done_bytes)

    def test_mark_done_moves_row_into_done_file(self):
        todo_part, done_part = source.split(source.mark_done(source.combine(*source.split(self.OLD)), 'frei-a', 'Ergebnis.'))
        self.assertNotIn('`frei-a`', todo_part)
        self.assertIn('| `frei-a` | 1 | Ergebnis. |', done_part)

    def test_missing_done_file_reads_as_empty(self):
        # Echtes run(): nur subprocess.run ist ersetzt; git show auf eine fehlende Datei endet mit Code 128.
        def fake_run(args, **kwargs):
            if args[-1].endswith(source.DONE_FILE):
                return subprocess.CompletedProcess(args, 128, '', f"fatal: path '{source.DONE_FILE}' does not exist")
            return subprocess.CompletedProcess(args, 0, self.OLD, '')
        with patch.object(tasks.subprocess, 'run', side_effect=fake_run):
            self.assertEqual(tasks.todo_text(), self.OLD)

    def test_todo_text_combines_both_files(self):
        todo_part, done_part = source.split(self.OLD)
        files = {'origin/main:TODO.md': todo_part, f'origin/main:{source.DONE_FILE}': done_part}
        with patch.object(tasks, 'git', side_effect=lambda *a, check=True: SimpleNamespace(stdout=files[a[-1]], returncode=0)):
            self.assertEqual(tasks.todo_text(), source.combine(todo_part, done_part))

    def test_check_all_reads_done_file_for_dependencies(self):
        # Ohne docs/ERLEDIGT.md fehlten die erledigten Phasen und phase_counts scheiterte vor der README-Pruefung.
        old = FLAT.format(problems='| `frei-a` | P0 | Text. **Fertig, wenn** es läuft. | parallel | wenig |\n', ideas='')
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'docs/assets').mkdir(parents=True)
            source.write_local(root, old)
            (root / 'README.md').write_text('', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'README'):
                source.check_all(root, check=True)  # erledigte Phasen gelesen, erst das leere README stoert

