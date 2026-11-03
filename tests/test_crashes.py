import os
import sys
import tempfile
import time
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools', 'spike'))
import crashes  # noqa: E402

CONTEXT = """<FGenericCrashContext>
<ErrorMessage>Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000001080</ErrorMessage>
<PCallStack>
DDSurvivors-Win64-Shipping 0x00007ff6a0000000 + 4b68195
DDSurvivors-Win64-Shipping 0x00007ff6a0000000 + 59a4b90
</PCallStack>
</FGenericCrashContext>"""


def write_report(session, role, name, mtime):
    folder = os.path.join(session, f'ud_{role}', 'Saved', 'Crashes', name)
    os.makedirs(folder)
    path = os.path.join(folder, 'CrashContext.runtime-xml')
    with open(path, 'w', encoding='utf-8') as file:
        file.write(CONTEXT)
    os.utime(path, (mtime, mtime))


class CrashesTest(unittest.TestCase):
    def test_parse_context(self):
        report = crashes.parse_context(CONTEXT)
        self.assertIn('0x0000000000001080', report['message'])
        self.assertEqual(report['rvas'], ['4b68195', '59a4b90'])

    def test_parse_context_without_stack(self):
        self.assertEqual(crashes.parse_context('<ErrorMessage>x</ErrorMessage>'), {'message': 'x', 'rvas': []})

    def test_find_reports_filters_and_sorts(self):
        with tempfile.TemporaryDirectory() as session:
            now = time.time()
            write_report(session, 'host', 'new', now)
            write_report(session, 'client', 'old', now - 100)
            write_report(session, 'client', 'mid', now - 10)
            self.assertEqual([r['folder'] for r in crashes.find_reports(session)], ['old', 'mid', 'new'])
            self.assertEqual([r['folder'] for r in crashes.find_reports(session, role='client')], ['old', 'mid'])
            self.assertEqual([r['folder'] for r in crashes.find_reports(session, since=now - 50)], ['mid', 'new'])
            self.assertEqual(crashes.find_reports(session)[0]['role'], 'client')

    def test_session_start_without_session(self):
        with tempfile.TemporaryDirectory() as session:
            self.assertEqual(crashes.session_start(session), 0.0)

    def test_session_start_prefers_field(self):
        with tempfile.TemporaryDirectory() as session:
            path = os.path.join(session, 'session.json')
            with open(path, 'w', encoding='utf-8') as file:
                file.write('{"Host": 1, "Started": 1700000000}')
            self.assertEqual(crashes.session_start(session), 1700000000.0)
            with open(path, 'w', encoding='utf-8') as file:
                file.write('{"Host": 1}')
            self.assertEqual(crashes.session_start(session), os.path.getmtime(path))


if __name__ == '__main__':
    unittest.main()
