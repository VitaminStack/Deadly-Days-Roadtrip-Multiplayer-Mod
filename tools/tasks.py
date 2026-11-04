"""Reservierung von Aufgaben, damit niemand doppelt arbeitet.

  py tools/tasks.py list                              Offene Kennungen mit Status
  py tools/tasks.py claim <kennung> <name> [<rolle>]   Reservieren: Worktree, Branch, Push, Draft-PR
  py tools/tasks.py release <kennung> [<rolle>]        Reservierung aufgeben: Draft-PR schliessen, Branch loeschen

Quelle der Kennungen, Abhaengigkeiten und Abschluesse ist TODO.md auf origin/main. Branches task/<kennung>
und Pull Requests ergaenzen nur Reservierung und Review. Blockierte und Team-Aufgaben lassen sich nicht reservieren.

Aufgaben mit Bearbeitung "zu zweit" (Test auf zwei PCs) duerfen zwei Personen nehmen, je eine Rolle host oder client:
Branch task/<kennung>/<rolle>, Worktree .worktrees/<kennung>-<rolle>. Ohne Rolle nimmt eine Person beide Seiten.
"""
import json
import os
import random
import re
import subprocess
import sys
import time

if __package__:
    from .task_status import blockers, parse
else:
    from task_status import blockers, parse

STALE_DAYS = 3
ROLES = ('host', 'client')
PAIR_MODE = 'zu zweit'


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


def todo_text():
    return git('show', 'origin/main:TODO.md').stdout


def plain(text):
    """Markdown-Text ohne Links und Fettdruck fuer die Konsole."""
    return re.sub(r'\[([^\]]*)\]\([^)]*\)', r'\1', text).replace('**', '')


def todo_tasks(text=None):
    """Kennung -> Task aus TODO.md, Reihenfolge wie in der Datei."""
    return parse(todo_text() if text is None else text)


def remote_branches():
    """Namen nach task/, also <kennung> oder <kennung>/<rolle>."""
    out = git('ls-remote', '--heads', 'origin', 'task/*').stdout
    return {line.split('refs/heads/task/')[1] for line in out.splitlines() if 'refs/heads/task/' in line}


def split_branch(name):
    """<kennung>/<rolle> -> (kennung, rolle), sonst (name, None)."""
    kennung, _, role = name.partition('/')
    return (kennung, role) if role in ROLES else (name, None)


def branch_name(kennung, role=None):
    return f'{kennung}/{role}' if role else kennung


def role_branches(kennung, branches):
    """Rollen, die fuer eine Kennung schon einen Branch haben, in fester Reihenfolge."""
    return [role for role in ROLES if branch_name(kennung, role) in branches]


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


def claim_info(name):
    """Wer hat reserviert und wann war der letzte Commit. name ist <kennung> oder <kennung>/<rolle>."""
    ref = f'origin/task/{name}'
    claim = git('log', '-1', '--format=%s', '--grep=^task: claim', ref, check=False).stdout.strip()
    who = re.sub(r'-\d+$', '', claim.split(' by ', 1)[1]) if ' by ' in claim else '?'
    last = int(git('log', '-1', '--format=%ct', ref).stdout.strip() or 0)
    return who, (time.time() - last) / 86400


def claim_status(name, branches, prs):
    """Stand einer Reservierung oder None, wenn es weder Branch noch offenen oder gemergten PR gibt."""
    pr = prs.get(name) if prs else None
    if pr and pr['state'] == 'OPEN':
        who, _ = claim_info(name) if name in branches else ('?', 0)
        return f"in Arbeit, Draft-PR #{pr['number']} ({who})" if pr['isDraft'] else f"PR #{pr['number']} wartet auf Review"
    if pr and pr['state'] == 'MERGED':
        return f"PR #{pr['number']} gemergt"
    if name in branches:
        who, days = claim_info(name)
        stale = f', seit {days:.0f} Tagen ohne Commit, erst nachfragen' if days >= STALE_DAYS else ''
        return f'reserviert ({who}{stale})'
    return None


def pair_status(kennung, branches, prs):
    """Stand beider Rollen, etwa 'zu zweit: host reserviert (a), client frei'."""
    parts = []
    for role in ROLES:
        state = claim_status(branch_name(kennung, role), branches, prs)
        parts.append(f'{role} {state}' if state else f'{role} frei')
    return 'zu zweit: ' + ', '.join(parts)


def status(kennung, branches, prs, tasks):
    task = tasks.get(kennung)
    roles = role_branches(kennung, branches)
    if task and task.done:
        return 'erledigt' + (' (Branch noch vorhanden)' if kennung in branches or roles else '')
    if roles:
        return pair_status(kennung, branches, prs)
    pr = prs.get(kennung) if prs else None
    if pr and pr['state'] == 'OPEN':
        who, _ = claim_info(kennung) if kennung in branches else ('?', 0)
        return f"in Arbeit, Draft-PR #{pr['number']} ({who})" if pr['isDraft'] else f"PR #{pr['number']} wartet auf Review"
    if pr and pr['state'] == 'MERGED':
        return f"erst nachfragen: PR #{pr['number']} gemergt, Aufgabe noch offen"
    if kennung in branches:
        who, days = claim_info(kennung)
        stale = f', seit {days:.0f} Tagen ohne Commit, erst nachfragen' if days >= STALE_DAYS else ''
        return f'reserviert ({who}{stale})'
    if task is None:
        return 'nicht in TODO.md'
    if task.mode == 'Team':
        return 'Team'
    waiting = blockers(task, tasks)
    if waiting:
        return 'blockiert, braucht ' + ', '.join(waiting)
    return 'frei'


def cmd_list():
    git('fetch', '-q', '--prune', 'origin')
    tasks = todo_tasks()
    branches, prs = remote_branches(), pull_requests()
    if prs is None:
        print('Hinweis: GitHub CLI fehlt oder ist nicht angemeldet. Abschluss aus TODO.md, '
              'Reservierungen aus Branches; PR-Status unbekannt.')
    section = None
    for kennung, task in tasks.items():
        if task.done:
            continue
        if task.section != section:
            print(f'\n{task.section}')
            section = task.section
        print(f'  {kennung:<20} {status(kennung, branches, prs, tasks):<44} {plain(task.text)[:70]}')
    done = sum(task.done for task in tasks.values())
    print(f'\nErledigt: {done} Aufgaben, siehe TODO.md#erledigt')
    extra = sorted({split_branch(name)[0] for name in branches} - set(tasks))
    if extra:
        print('\nBranches ohne Kennung in TODO.md')
        for kennung in extra:
            print(f'  {kennung:<20} {status(kennung, branches, prs, tasks)}')


def cmd_claim(kennung, name, role=None):
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]*', kennung):
        sys.exit('Kennung nur aus Kleinbuchstaben, Ziffern und Bindestrich.')
    if role is not None and role not in ROLES:
        sys.exit('Rolle muss host oder client sein.')
    git('fetch', '-q', '--prune', 'origin')
    tasks = todo_tasks()
    branches, prs = remote_branches(), pull_requests()
    task = tasks.get(kennung)
    if task and task.done:
        sys.exit(f'{kennung} ist laut TODO.md erledigt. Keine neue Reservierung.')
    if role and (task is None or task.mode != PAIR_MODE):
        sys.exit(f'{kennung} ist keine Aufgabe "{PAIR_MODE}". Ohne Rolle reservieren.')
    pr = prs.get(kennung) if prs else None
    if pr and pr['state'] in ('OPEN', 'MERGED'):
        sys.exit(f'{kennung}: {status(kennung, branches, prs, tasks)}. Keine neue Reservierung.')
    if kennung in branches:
        sys.exit(f'task/{kennung} existiert schon: {status(kennung, branches, prs, tasks)}. Andere Aufgabe waehlen.')
    taken = role_branches(kennung, branches)
    if role is None and taken:
        sys.exit(f'{kennung}: {status(kennung, branches, prs, tasks)}. Nur die freie Rolle reservieren.')
    if role:
        own_pr = prs.get(branch_name(kennung, role)) if prs else None
        if role in taken or (own_pr and own_pr['state'] in ('OPEN', 'MERGED')):
            sys.exit(f'{kennung}: {status(kennung, branches, prs, tasks)}. Rolle {role} ist vergeben.')
    if task is None:
        print(f'Hinweis: {kennung} steht nicht in TODO.md. Nur fuer kurze Wartung ohne eigene Aufgabe.')
    elif task.mode == 'Team':
        sys.exit(f'{kennung} entscheidet das Team. Keine Reservierung.')
    elif blockers(task, tasks):
        sys.exit(f'{kennung} ist blockiert, erst erledigen: ' + ', '.join(blockers(task, tasks)))

    root = repo_root()
    branch = f'task/{branch_name(kennung, role)}'
    path = os.path.join(root, '.worktrees', f'{kennung}-{role}' if role else kennung)
    label = f'{kennung} ({role})' if role else kennung
    git('-C', root, 'worktree', 'add', '-q', '-b', branch, path, 'origin/main')
    git('-C', path, 'commit', '-q', '--allow-empty', '-m', f'task: claim {label} by {name}-{random.randint(0, 32767)}')
    if git('-C', path, 'push', '-q', '-u', 'origin', branch, check=False).returncode != 0:
        git('-C', root, 'worktree', 'remove', '--force', path)
        git('-C', root, 'branch', '-D', branch)
        sys.exit('Push abgelehnt, jemand anderes war schneller. Andere Aufgabe waehlen.')
    print(f'Reserviert: {branch}, Worktree {os.path.relpath(path, root)}')
    if role:
        other = ROLES[1 - ROLES.index(role)]
        print(f'Zu zweit: Die Rolle {other} reserviert die zweite Person mit "claim {kennung} <name> {other}".')
        print('Ablauf des Tests: docs/TESTING.md, Abschnitt "Test auf zwei PCs".')

    text = plain(task.text)[:60] if task else kennung
    body = (f'Reserviert von {name}. Dieser Draft-PR zeigt, dass an `{label}` gearbeitet wird.\n\n'
            'Zum Abschluss Titel und Beschreibung ersetzen und den PR auf „Ready for review“ stellen.')
    try:
        pr = run('gh', 'pr', 'create', '--draft', '--base', 'main', '--head', branch,
                 '--title', f'[WIP] {label}: {text}', '--body', body, check=False)
    except FileNotFoundError:
        pr = None
    if pr and pr.returncode == 0:
        print(f'Draft-PR: {pr.stdout.strip()}')
    else:
        print('Draft-PR nicht angelegt (GitHub CLI fehlt oder nicht angemeldet). Die Reservierung gilt trotzdem,')
        print(f'bitte den Draft-PR fuer {branch} auf GitHub von Hand oeffnen.')


def cmd_release(kennung, role=None):
    if role is not None and role not in ROLES:
        sys.exit('Rolle muss host oder client sein.')
    root = repo_root()
    name = branch_name(kennung, role)
    pr = (pull_requests() or {}).get(name)
    # Nur eine Reservierung aufgeben, nie einen fertigen PR: Das Loeschen des Branches wuerde ihn ebenfalls schliessen.
    if pr and pr['state'] == 'OPEN' and not pr['isDraft']:
        sys.exit(f"PR #{pr['number']} wartet auf Review und ist keine reine Reservierung. Nichts geaendert.")
    if pr and pr['state'] == 'OPEN':
        run('gh', 'pr', 'close', str(pr['number']), '--comment', 'Reservierung aufgegeben.')
    if git('push', '-q', 'origin', '--delete', f'task/{name}', check=False).returncode == 0:
        print(f'task/{name} auf origin geloescht.')
    else:
        print(f'task/{name} war auf origin schon geloescht.')
    path = os.path.join(root, '.worktrees', f'{kennung}-{role}' if role else kennung)
    if os.path.isdir(path):
        if git('-C', root, 'worktree', 'remove', path, check=False).returncode == 0:
            git('-C', root, 'branch', '-D', f'task/{name}', check=False)
        else:
            print(f'Worktree {path} hat Aenderungen und bleibt erhalten.')


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    args = sys.argv[1:]
    try:
        if args[:1] == ['list'] and len(args) == 1:
            cmd_list()
        elif args[:1] == ['claim'] and len(args) in (3, 4):
            cmd_claim(*args[1:])
        elif args[:1] == ['release'] and len(args) in (2, 3):
            cmd_release(*args[1:])
        else:
            sys.exit(__doc__)
    except ValueError as error:
        sys.exit(f'TODO.md ungueltig: {error}')


if __name__ == '__main__':
    main()
