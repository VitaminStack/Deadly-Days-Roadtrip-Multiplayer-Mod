"""Aufgabenabschluss ohne gh, Reservierungsschutz und README-Fortschritt."""
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import unittest
from unittest.mock import patch

from tools import task_status as source
from tools import tasks

ROOT = Path(__file__).resolve().parents[1]


class TaskStatusTests(unittest.TestCase):
    def test_real_documents_are_consistent(self):
        todo = (ROOT / 'TODO.md').read_text(encoding='utf-8')
        readme = (ROOT / 'README.md').read_text(encoding='utf-8')
        states = source.validate_tasks(todo)
        self.assertEqual(source.update_readme(todo, readme), readme)
        self.assertEqual(set(tasks.todo_tasks(todo)), set(states))
        for name in ('crash-reporter', 'overlay', 'deploy', 'ci', 'grundgeruest'):
            self.assertTrue(states[name])

    def test_package_requires_all_checkboxes(self):
        self.assertFalse(source.task_states(
            '<!-- tasks: package -->\n- [x] first\n'
            '<!-- tasks: package -->\n- [ ] second')['package'])
        self.assertTrue(source.task_states(
            '<!-- tasks: package -->\n- [x] first\n'
            '<!-- tasks: package -->\n- [x] second')['package'])

    def test_mentions_in_description_do_not_change_status(self):
        self.assertEqual(source.task_states(
            '<!-- tasks: done -->\n- [x] Follow-up `open` remains\n'
            '<!-- tasks: open -->\n- [ ] Later'), {'done': True, 'open': False})

    def test_malformed_markers_fail_closed(self):
        for text in ('<!-- tasks: a -->', '<!-- tasks: a -->\ntext',
                     '<!-- tasks: a a -->\n- [x] done',
                     '<!-- tasks: a -->\n<!-- tasks: b -->\n- [x] done'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                source.task_states(text)

    def test_catalog_requires_explicit_checkbox(self):
        with self.assertRaisesRegex(ValueError, 'Zuordnung fehlt'):
            source.validate_tasks('| `unknown` | Description |')

    def test_marker_requires_catalog_entry(self):
        with self.assertRaisesRegex(ValueError, 'Katalogeintrag fehlt'):
            source.validate_tasks('<!-- tasks: unknown -->\n- [ ] Text')

    def test_catalog_section_must_match_checkbox(self):
        for section, box in (('Wo kann ich anfangen?', 'x'), ('Abgeschlossene Pakete', ' ')):
            with self.subTest(section=section), self.assertRaisesRegex(ValueError, 'widerspricht'):
                source.validate_tasks(f'## {section}\n| `a` | Text |\n'
                                      f'<!-- tasks: a -->\n- [{box}] Text')

    def test_duplicate_catalog_id_rejected(self):
        with self.assertRaisesRegex(ValueError, 'mehrfach'):
            source.validate_tasks('| `a` | Text |\n| `a` | Text |\n'
                                  '<!-- tasks: a -->\n- [ ] Text')

    def test_done_without_github_or_branch_is_not_free(self):
        self.assertEqual(tasks.status('done', set(), None, {'done': True}), 'erledigt (TODO.md)')
        self.assertIn('erledigt', tasks.status('done', {'done'}, None, {'done': True}))

    def test_merged_partial_pr_is_not_completion_or_free(self):
        prs = {'partial': {'number': 7, 'state': 'MERGED'}}
        self.assertIn('TODO noch offen', tasks.status('partial', set(), prs, {'partial': False}))

    def test_missing_status_never_free(self):
        self.assertIn('ungeklärt', tasks.status('unknown', set(), None, {}))

    def test_open_task_remains_free(self):
        self.assertEqual(tasks.status('open', set(), None, {'open': False}), 'frei')

    def test_done_claim_has_no_worktree_commit_or_push(self):
        text = '## Abgeschlossene Pakete\n| Kennung | Aufgabe |\n|---|---|\n| `done` | Text |\n'
        text += '<!-- tasks: done -->\n- [x] Text'
        with patch.object(tasks, 'git') as git, patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value=set()), \
                patch.object(tasks, 'pull_requests', return_value=None):
            with self.assertRaisesRegex(SystemExit, 'erledigt'):
                tasks.cmd_claim('done', 'test')
            self.assertEqual(git.call_args_list, [unittest.mock.call('fetch', '-q', '--prune', 'origin')])

    def test_open_pr_without_branch_prevents_claim(self):
        text = '| Kennung | Aufgabe |\n|---|---|\n| `open` | Text |\n<!-- tasks: open -->\n- [ ] Text'
        prs = {'open': {'number': 8, 'state': 'OPEN', 'isDraft': False}}
        with patch.object(tasks, 'git') as git, patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value=set()), \
                patch.object(tasks, 'pull_requests', return_value=prs):
            with self.assertRaisesRegex(SystemExit, 'Keine neue Reservierung'):
                tasks.cmd_claim('open', 'test')
            self.assertEqual(git.call_count, 1)

    def test_merged_partial_pr_prevents_claim(self):
        text = '| Kennung | Aufgabe |\n|---|---|\n| `open` | Text |\n<!-- tasks: open -->\n- [ ] Text'
        prs = {'open': {'number': 8, 'state': 'MERGED'}}
        with patch.object(tasks, 'git') as git, patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value=set()), \
                patch.object(tasks, 'pull_requests', return_value=prs):
            with self.assertRaisesRegex(SystemExit, 'TODO noch offen'):
                tasks.cmd_claim('open', 'test')
            self.assertEqual(git.call_count, 1)

    def test_existing_branch_still_prevents_claim_without_github(self):
        text = '| Kennung | Aufgabe |\n|---|---|\n| `open` | Text |\n<!-- tasks: open -->\n- [ ] Text'
        with patch.object(tasks, 'git') as git, patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value={'open'}), \
                patch.object(tasks, 'pull_requests', return_value=None), \
                patch.object(tasks, 'claim_info', return_value=('other-agent', 0)):
            with self.assertRaisesRegex(SystemExit, 'reserviert'):
                tasks.cmd_claim('open', 'test')
            self.assertEqual(git.call_count, 1)

    def test_list_without_github_uses_checkbox_status(self):
        text = (ROOT / 'TODO.md').read_text(encoding='utf-8')
        with patch.object(tasks, 'git'), patch.object(tasks, 'todo_text', return_value=text), \
                patch.object(tasks, 'remote_branches', return_value=set()), \
                patch.object(tasks, 'pull_requests', return_value=None), redirect_stdout(StringIO()) as out:
            tasks.cmd_list()
        row = next(line for line in out.getvalue().splitlines() if line.strip().startswith('crash-reporter '))
        self.assertIn('erledigt (TODO.md)', row)
        self.assertNotIn('frei', row)

    def test_progress_counts_only_phases_and_excludes_optional_and_milestones(self):
        todo = '## Optionale Optimierungen\n- [x] Optional\n## Meilensteine\n- [ ] Gate\n'
        readme = ''
        for phase in range(7):
            todo += f'## Phase {phase} – Test\n- [x] Done\n- [ ] Open\n'
            readme += f'| [{phase}](TODO.md#phase-{phase}) | Ziel | 0 von 9 | wrong |\n'
        readme += '| **Gesamt** | | **0 von 99** | wrong |\n'
        counts = source.phase_counts(todo)
        self.assertEqual(counts, {phase: [1, 2] for phase in range(7)})
        result = source.update_readme(todo, readme)
        self.assertIn('**7 von 14**', result)
        self.assertEqual(result.count('██████░░░░░'), 8)
        self.assertEqual(source.update_readme(todo, result), result)

    def test_missing_phase_or_readme_row_fails(self):
        with self.assertRaises(ValueError):
            source.phase_counts('## Phase 0 – Setup\n- [x] Done')
        todo = (ROOT / 'TODO.md').read_text(encoding='utf-8')
        with self.assertRaises(ValueError):
            source.update_readme(todo, '')

    def test_progress_bar_boundaries(self):
        self.assertEqual(source.progress_bar(0, 8), '░' * 11)
        self.assertEqual(source.progress_bar(8, 8), '█' * 11)
        self.assertEqual(source.progress_bar(7, 8), '██████████░')


if __name__ == '__main__':
    unittest.main()
