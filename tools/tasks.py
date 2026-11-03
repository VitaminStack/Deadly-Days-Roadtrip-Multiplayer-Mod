"""Reservierung von Aufgaben, damit niemand doppelt arbeitet.

  py tools/tasks.py list                     Alle Kennungen mit Status
  py tools/tasks.py claim <kennung> <name>   Reservieren: Worktree, Branch, Push, Draft-PR
  py tools/tasks.py release <kennung>        Reservierung aufgeben: Draft-PR schliessen, Branch loeschen

Quelle der Kennungen sind die Tabellen in TODO.md auf origin/main. Der Status kommt aus den Branches task/<kennung>
und, wenn die GitHub CLI angemeldet ist, aus den Pull Requests. Ohne gh sieht man nur die Branches.
"""
import json
import os
import random
import re
import subprocess
import sys
import time

STALE_DAYS = 3


def run(*args, check=True):
    result = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if check and result.returncode != 0:
        sys.exit(f"Fehler bei {' '.join(args)}:\n{result.stderr.strip()}")
    return result


def git(*args, check=True):
    return run('git', *args, check=check)


def repo_root():
    # Auch aus einem Worktree heraus: der gemeinsame .git-Ordner liegt im Hauptordner.
    common = git('rev-parse', '--path-format=absolute', '--git-common-dir').stdout.strip()
    return os.path.dirname(common)


def todo_tasks():
    """Kennung -> (Abschnitt, Beschreibung) aus allen Tabellen mit Spalte Kennung."""
    tasks, section, in_table, column = {}, '', False, 1
    for line in git('show', 'origin/main:TODO.md').stdout.splitlines():
        if line.startswith('## '):
            section, in_table = line[3:].strip(), False
        elif line.startswith('| Kennung |'):
            header = [c.strip() for c in line.strip('|').split('|')]
            column = next(i for i, c in enumerate(header) if c in ('Aufgabe', 'Paket', 'Arbeit'))
            in_table = True
        elif in_table and line.startswith('| `'):
            cells = [c.strip() for c in line.strip('|').split('|')]
            tasks[cells[0].strip('`')] = (section, re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', cells[column]))
        elif not line.startswith('|'):
            in_table = False
    return tasks


def remote_branches():
    out = git('ls-remote', '--heads', 'origin', 'task/*').stdout
    return {line.split('refs/heads/task/')[1] for line in out.splitlines() if 'refs/heads/task/' in line}


def pull_requests():
    """Kennung -> neuester PR, oder None ohne angemeldete GitHub CLI."""
    try:
        result = run('gh', 'pr', 'list', '--state', 'all', '--limit', '500',
                     '--json', 'number,headRefName,state,isDraft', check=False)
    except FileNotFoundError:
        return None
    if result.returncode != 0:
        return None
    prs = {}
    for pr in json.loads(result.stdout):
        name = pr['headRefName']
        if name.startswith('task/') and pr['number'] > prs.get(name[5:], {}).get('number', 0):
            prs[name[5:]] = pr
    return prs


def claim_info(kennung):
    """Wer hat reserviert und wann war der letzte Commit."""
    ref = f'origin/task/{kennung}'
    claim = git('log', '-1', '--format=%s', '--grep=^task: claim', ref, check=False).stdout.strip()
    who = re.sub(r'-\d+$', '', claim.split(' by ', 1)[1]) if ' by ' in claim else '?'
    last = int(git('log', '-1', '--format=%ct', ref).stdout.strip() or 0)
    return who, (time.time() - last) / 86400


def status(kennung, branches, prs):
    pr = prs.get(kennung) if prs else None
    if pr and pr['state'] == 'OPEN':
        who, _ = claim_info(kennung) if kennung in branches else ('?', 0)
        return f"in Arbeit, Draft-PR #{pr['number']} ({who})" if pr['isDraft'] else f"PR #{pr['number']} wartet auf Review"
    if pr and pr['state'] == 'MERGED':
        return f"erledigt, PR #{pr['number']}" + (' (Branch nicht geloescht)' if kennung in branches else '')
    if kennung in branches:
        who, days = claim_info(kennung)
        stale = f', seit {days:.0f} Tagen ohne Commit, erst nachfragen' if days >= STALE_DAYS else ''
        return f'reserviert ({who}{stale})'
    return 'frei'


def cmd_list():
    git('fetch', '-q', '--prune', 'origin')
    tasks, branches, prs = todo_tasks(), remote_branches(), pull_requests()
    if prs is None:
        print('Hinweis: GitHub CLI fehlt oder ist nicht angemeldet, Status nur aus Branches.\n')
    section = None
    for kennung, (sec, text) in tasks.items():
        if sec != section:
            print(f'\n{sec}')
            section = sec
        print(f'  {kennung:<18} {status(kennung, branches, prs):<44} {text}')
    extra = sorted(branches - set(tasks))
    if extra:
        print('\nBranches ohne Kennung in TODO.md')
        for kennung in extra:
            print(f'  {kennung:<18} {status(kennung, branches, prs)}')


def cmd_claim(kennung, name):
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]*', kennung):
        sys.exit('Kennung nur aus Kleinbuchstaben, Ziffern und Bindestrich.')
    git('fetch', '-q', '--prune', 'origin')
    tasks, branches = todo_tasks(), remote_branches()
    if kennung in branches:
        sys.exit(f'task/{kennung} existiert schon: {status(kennung, branches, pull_requests())}. Andere Aufgabe waehlen.')
    if kennung not in tasks:
        print(f'Hinweis: {kennung} steht in keiner Tabelle in TODO.md.')
    elif tasks[kennung][0] == 'Spätere Arbeitspakete':
        print('Hinweis: Spaeteres Arbeitspaket, nur nehmen, wenn seine Voraussetzung auf main ist.')

    root = repo_root()
    path = os.path.join(root, '.worktrees', kennung)
    git('-C', root, 'worktree', 'add', '-q', '-b', f'task/{kennung}', path, 'origin/main')
    git('-C', path, 'commit', '-q', '--allow-empty', '-m', f'task: claim {kennung} by {name}-{random.randint(0, 32767)}')
    if git('-C', path, 'push', '-q', '-u', 'origin', f'task/{kennung}', check=False).returncode != 0:
        git('-C', root, 'worktree', 'remove', '--force', path)
        git('-C', root, 'branch', '-D', f'task/{kennung}')
        sys.exit('Push abgelehnt, jemand anderes war schneller. Andere Aufgabe waehlen.')
    print(f'Reserviert: task/{kennung}, Worktree {os.path.relpath(path, root)}')

    text = tasks.get(kennung, ('', kennung))[1]
    body = (f'Reserviert von {name}. Dieser Draft-PR zeigt, dass an `{kennung}` gearbeitet wird.\n\n'
            'Zum Abschluss Titel und Beschreibung ersetzen und den PR auf „Ready for review“ stellen.')
    try:
        pr = run('gh', 'pr', 'create', '--draft', '--base', 'main', '--head', f'task/{kennung}',
                 '--title', f'[WIP] {kennung}: {text}', '--body', body, check=False)
    except FileNotFoundError:
        pr = None
    if pr and pr.returncode == 0:
        print(f'Draft-PR: {pr.stdout.strip()}')
    else:
        print('Draft-PR nicht angelegt (GitHub CLI fehlt oder nicht angemeldet). Die Reservierung gilt trotzdem,')
        print(f'bitte den Draft-PR fuer task/{kennung} auf GitHub von Hand oeffnen.')


def cmd_release(kennung):
    root = repo_root()
    pr = (pull_requests() or {}).get(kennung)
    # Nur eine Reservierung aufgeben, nie einen fertigen PR: Das Loeschen des Branches wuerde ihn ebenfalls schliessen.
    if pr and pr['state'] == 'OPEN' and not pr['isDraft']:
        sys.exit(f"PR #{pr['number']} wartet auf Review und ist keine reine Reservierung. Nichts geaendert.")
    if pr and pr['state'] == 'OPEN':
        run('gh', 'pr', 'close', str(pr['number']), '--comment', 'Reservierung aufgegeben.')
    if git('push', '-q', 'origin', '--delete', f'task/{kennung}', check=False).returncode == 0:
        print(f'task/{kennung} auf origin geloescht.')
    else:
        print(f'task/{kennung} war auf origin schon geloescht.')
    path = os.path.join(root, '.worktrees', kennung)
    if os.path.isdir(path):
        if git('-C', root, 'worktree', 'remove', path, check=False).returncode == 0:
            git('-C', root, 'branch', '-D', f'task/{kennung}', check=False)
        else:
            print(f'Worktree {path} hat Aenderungen und bleibt erhalten.')


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    args = sys.argv[1:]
    if args[:1] == ['list'] and len(args) == 1:
        cmd_list()
    elif args[:1] == ['claim'] and len(args) == 3:
        cmd_claim(args[1], args[2])
    elif args[:1] == ['release'] and len(args) == 2:
        cmd_release(args[1])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
