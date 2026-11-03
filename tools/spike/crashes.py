"""Listet Abstuerze der Testinstanzen mit Fehler und Stack (RVAs).

  crashes.py                      alle Berichte, aelteste zuerst
  crashes.py --latest 1           nur den neuesten
  crashes.py --role client        nur Client-Berichte (host, client)
  crashes.py --session            nur Berichte seit dem Start der aktuellen Sitzung

Manche Abstuerze legen keinen Bericht an. Dann steht die Adresse in der Windows-Ereignisanzeige (Application, ID 1000).
"""
import argparse
import glob
import json
import os
import re

SESSION = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'research', 'local', 'session')


def parse_context(text):
    """Fehlertext und RVAs aus einer CrashContext.runtime-xml."""
    stack = re.search(r'<PCallStack>(.*?)</PCallStack>', text, re.S)
    message = re.search(r'<ErrorMessage>(.*?)</ErrorMessage>', text, re.S)
    return {
        'message': message.group(1).strip() if message else '',
        'rvas': re.findall(r'\+ ([0-9a-fA-F]+)', stack.group(1)) if stack else [],
    }


def find_reports(session=SESSION, role=None, since=0.0):
    """Berichte als Liste, aelteste zuerst. role ist host oder client, since eine Unix-Zeit."""
    reports = []
    for folder in glob.glob(os.path.join(session, f'ud_{role or "*"}', 'Saved', 'Crashes', '*')):
        context = os.path.join(folder, 'CrashContext.runtime-xml')
        if not os.path.isfile(context) or os.path.getmtime(context) < since:
            continue
        with open(context, encoding='utf-8', errors='replace') as file:
            report = parse_context(file.read())
        report['role'] = re.search(r'ud_([^\\/]+)', folder).group(1)
        report['time'] = os.path.getmtime(context)
        report['folder'] = os.path.basename(folder)
        reports.append(report)
    return sorted(reports, key=lambda report: report['time'])


def session_start(session=SESSION):
    """Startzeit aus session.json, bei alten Sitzungen ohne Feld die Dateizeit."""
    path = os.path.join(session, 'session.json')
    if not os.path.isfile(path):
        return 0.0
    with open(path, encoding='utf-8-sig') as file:
        started = json.load(file).get('Started')
    return float(started) if started else os.path.getmtime(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--latest', type=int, default=0, help='nur die neuesten N Berichte')
    parser.add_argument('--role', choices=['host', 'client'])
    parser.add_argument('--session', action='store_true', help='nur seit dem Start der aktuellen Sitzung')
    parser.add_argument('--frames', type=int, default=14, help='Anzahl der RVAs je Bericht')
    args = parser.parse_args()
    reports = find_reports(role=args.role, since=session_start() if args.session else 0.0)
    if args.latest:
        reports = reports[-args.latest:]
    if not reports:
        print('Keine Absturzberichte.')
    for report in reports:
        print(report['folder'], report['role'], report['message'][:90])
        print('   ', ' '.join(report['rvas'][:args.frames]))


if __name__ == '__main__':
    main()
