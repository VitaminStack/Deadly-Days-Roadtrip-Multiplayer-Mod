"""Reservierung von Aufgaben, damit niemand doppelt arbeitet.

  py tools/tasks.py list                              Offene Kennungen mit Status
  py tools/tasks.py list --active                     Nur aktiv reservierte Entwicklungsaufgaben
  py tools/tasks.py list --short                      Kurz: Kennung und Status, Optionen und Tests nur gezaehlt
  py tools/tasks.py tests [list|plan] [<kennung> ...]  Bestaetigungstests getrennt anzeigen oder buendeln
  py tools/tasks.py tests pending <kennung> --note "Stand"   Auf Bestaetigungstest setzen
  py tools/tasks.py tests update <kennungen> --note "Stand"  Teststand aktualisieren
  py tools/tasks.py tests pass <kennungen> --note "Beleg"    Nach echter Abnahme abschliessen
  py tools/tasks.py tests reopen <kennungen> --note "Befund" Zurueck zur Entwicklungsaufgabe
  Statt Kennungen: --all (ausser pending); aktive fremde Aufgaben werden dabei ausgelassen.
  py tools/tasks.py claim <kennung> <name> [<rolle>]   Reservieren: Worktree, Branch, Push, Draft-PR
  py tools/tasks.py release <kennung> [<rolle>]        Reservierung aufgeben: Draft-PR schliessen, Branch loeschen
  py tools/tasks.py merge-plan                         Empfohlene Reihenfolge fuer offene PRs, ohne etwas zu mergen
  py tools/tasks.py cleanup [--apply]                 Verwaiste Worktrees/Branches zeigen; --apply entfernt sichere lokale Worktrees
  py tools/tasks.py show <kennung>                     Nur diese Aufgabe: Text, Braucht, Bearbeitung, Status
  py tools/tasks.py done <kennung> "<ergebnis>"        Im eigenen Worktree: Zeile nach Erledigt, Status erneuern, pruefen

show und done ersparen das Lesen der ganzen TODO.md. done nur bei erfuellter Abnahme, das Ergebnis ist ein Satz mit Link.

Quelle der Kennungen, Abhaengigkeiten und Abschluesse ist TODO.md auf origin/main. Branches task/<kennung>
und Pull Requests ergaenzen nur Reservierung und Review. Blockierte und Team-Aufgaben lassen sich nicht reservieren.

Aufgaben mit Bearbeitung "zu zweit" (Test auf zwei PCs) duerfen zwei Personen nehmen, je eine Rolle host oder client:
Branch task/<kennung>/<rolle>, Worktree .worktrees/<kennung>-<rolle>. Ohne Rolle nimmt eine Person beide Seiten.
"""
import argparse
import json
import os
import random
import re
import subprocess
import sys
import time

from pathlib import Path

if __package__:
    from .task_status import PHASES, PRIOS, EXTRA_SECTIONS, blockers, confirmation_update, deferred, mark_done, parse, regenerate, combine, read_local, write_local, DONE_FILE
else:
    from task_status import PHASES, PRIOS, EXTRA_SECTIONS, blockers, confirmation_update, deferred, mark_done, parse, regenerate, combine, read_local, write_local, DONE_FILE

STALE_DAYS = 3
ROLES = ('host', 'client')
PAIR_MODE = 'zu zweit'
DEFERRED = 'zurückgestellt bis Phase 0 bis 5 erledigt'


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
    """TODO.md und docs/ERLEDIGT.md von origin/main als ein Text; fehlt ERLEDIGT.md dort noch, nur TODO.md."""
    todo = git('show', 'origin/main:TODO.md').stdout
    shown = git('show', f'origin/main:{DONE_FILE}', check=False)
    return combine(todo, shown.stdout if shown.returncode == 0 else '')


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
    if task and task.confirmation:
        waiting = blockers(task, tasks)
        return 'Bestätigungstest ausstehend' + (', braucht ' + ', '.join(waiting) if waiting else '')
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
    if deferred(task, tasks):
        return DEFERRED
    return 'frei'


def active_work(kennung, branches, prs, own_branch=None):
    """Offene PRs/Reservierungen schuetzen; gemergte Altbranches sind keine aktive Arbeit."""
    for name in [kennung] + [branch_name(kennung, role) for role in ROLES]:
        if own_branch == f'task/{name}':
            continue
        pr = prs.get(name) if prs else None
        if pr and pr['state'] == 'OPEN':
            return True
        if name in branches and (pr is None or pr['state'] != 'MERGED'):
            return True
    return False


def cmd_list_short(tasks, branches, prs):
    """Eine Zeile je offener Aufgabe aus Phasen und Wartung, Optionen und Bestaetigungstests nur gezaehlt."""
    collapsed = ('Optional', 'Coop')
    for kennung, task in tasks.items():
        if not (task.done or task.confirmation or task.phase in collapsed):
            print(f'  {kennung} {status(kennung, branches, prs, tasks)}'[:80])
    later = sum(not t.done and not t.confirmation and t.phase in collapsed for t in tasks.values())
    print(f'Optional und Coop: {later} Aufgaben (tasks.py list)')
    print(f'Bestätigungstests: {sum(bool(t.confirmation) for t in tasks.values())} (tasks.py tests list)')


def cmd_list(active_only=False, short=False):
    git('fetch', '-q', '--prune', 'origin')
    tasks = todo_tasks()
    branches, prs = remote_branches(), pull_requests()
    if short:
        cmd_list_short(tasks, branches, prs)
        return
    if prs is None:
        print('Hinweis: GitHub CLI fehlt oder ist nicht angemeldet. Abschluss aus TODO.md, '
              'Reservierungen aus Branches; PR-Status unbekannt.')
    section = None
    for kennung, task in tasks.items():
        if task.done or task.confirmation:
            continue
        if active_only and not active_work(kennung, branches, prs):
            continue
        if task.section != section:
            print(f'\n{task.section}')
            section = task.section
        print(f'  {kennung:<20} {status(kennung, branches, prs, tasks):<44} {plain(task.text)[:70]}')
    pending = [t for t in tasks.values() if t.confirmation]
    if pending and not active_only:
        print('\nBestätigungstests ausstehend (nur Info, erledigt, blockieren nichts)')
        for task in pending:
            print(f'  {task.name:<20} {status(task.name, branches, prs, tasks):<44} {plain(task.confirmation)}')
    done = sum(task.done for task in tasks.values())
    print(f'\nErledigt: {done} Aufgaben, siehe {DONE_FILE}')
    extra = sorted({split_branch(name)[0] for name in branches} - set(tasks))
    if extra and not active_only:
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
    elif deferred(task, tasks):
        sys.exit(f'{kennung}: optional, {DEFERRED}. Erst den Run zu zweit spielbar machen; '
                 'vorziehen nur, wenn das Team die Zeile in eine Phase oder nach Wartung verschiebt.')

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


def worktree_verdict(branch, dirty, in_main, branches, prs):
    """(entfernen, Grund) fuer einen lokalen Worktree. Entfernt wird nur, was sicher auf GitHub oder in main liegt."""
    if dirty:
        return False, 'ungesicherte Aenderungen'
    if branch and branch.startswith('task/'):
        name = branch[5:]
        pr = (prs or {}).get(name)
        if name in branches:
            return False, 'Branch auf origin noch vorhanden'
        if pr is None:
            return (True, 'Stand in main') if in_main else (False, 'kein PR bekannt')
        if pr['state'] != 'MERGED':
            return False, f"PR #{pr['number']} {pr['state'].lower()}, nicht gemergt"
        return True, f"PR #{pr['number']} {pr['state'].lower()}, Branch auf origin geloescht"
    return (True, 'Stand in main') if in_main else (False, 'Stand nicht in main')


def local_worktrees(root):
    """(Pfad, Branch oder None) aller Worktrees unter .worktrees/ und .claude/worktrees/."""
    out = git('-C', root, 'worktree', 'list', '--porcelain').stdout
    bases = [os.path.normcase(os.path.join(root, d)) for d in ('.worktrees', os.path.join('.claude', 'worktrees'))]
    found = []
    for block in out.strip().split('\n\n'):
        path = branch = None
        for line in block.splitlines():
            if line.startswith('worktree '):
                path = os.path.normpath(line[9:])
            elif line.startswith('branch refs/heads/'):
                branch = line[18:]
        if path and any(os.path.normcase(path).startswith(base + os.sep) for base in bases):
            found.append((path, branch))
    return found


def cmd_cleanup(apply=False):
    """Verwaiste Worktrees und Branches auflisten; mit --apply nur sichere lokale Worktrees entfernen."""
    git('fetch', '-q', '--prune', 'origin')
    root = repo_root()
    branches, prs, tasks = remote_branches(), pull_requests(), todo_tasks()
    if prs is None:
        sys.exit('GitHub CLI fehlt oder ist nicht angemeldet. Ohne PR-Status wird nichts bewertet.')
    own = os.path.normcase(os.path.normpath(str(worktree_root())))
    print('Lokale Worktrees')
    for path, branch in local_worktrees(root):
        if os.path.normcase(path) == own:
            continue
        dirty = bool(git('-C', path, 'status', '--porcelain', check=False).stdout.strip())
        in_main = git('-C', path, 'merge-base', '--is-ancestor', 'HEAD', 'origin/main', check=False).returncode == 0
        remove, reason = worktree_verdict(branch, dirty, in_main, branches, prs)
        label = f"{os.path.relpath(path, root)} [{branch or 'detached'}]"
        if remove and apply:
            if git('-C', root, 'worktree', 'remove', path, check=False).returncode == 0:
                if branch:
                    git('-C', root, 'branch', '-D', branch, check=False)
                print(f'  entfernt   {label}: {reason}')
            else:
                print(f'  bleibt     {label}: git worktree remove fehlgeschlagen')
        else:
            print(f"  {'verwaist' if remove else 'bleibt':<10} {label}: {reason}")
    print('\nBranches auf origin ohne offenen PR (nur nach Rueckfrage loeschen: git push origin --delete task/<name>)')
    for name in sorted(branches):
        pr = prs.get(name)
        kennung, _ = split_branch(name)
        task = tasks.get(kennung)
        if pr and pr['state'] == 'OPEN':
            continue
        state = f"PR #{pr['number']} {pr['state'].lower()}" if pr else 'kein PR'
        done = ', Aufgabe erledigt' if task and task.done else ''
        print(f'  task/{name}: {state}{done}')
    if not apply:
        print('\nMit "tasks.py cleanup --apply" werden nur die als verwaist markierten lokalen Worktrees entfernt.')


SECTION_ORDER = PHASES + tuple(EXTRA_SECTIONS.values())
PRIO_ORDER = PRIOS


def merge_order(names, merge, unblocks, rank, pair_conflicts, before):
    """Gierige Merge-Reihenfolge: je Schritt der beste PR, der sauber auf den bisherigen Stand passt.

    merge(stand, name) -> (Konfliktdateien, neuer Stand). Vorrang: macht eine offene Aufgabe frei, dann wenige
    Konflikte mit den uebrigen PRs (wer mit vielen kollidiert, kommt spaeter und zieht einmal nach), dann Phase,
    Prio und Nummer. before[name] sind PRs, auf deren Branch dieser aufbaut; sie kommen zuerst.
    Rueckgabe: Reihenfolge und {name: Konfliktdateien} fuer die PRs, die erst nachziehen muessen."""
    state, order, left = 'origin/main', [], list(names)
    while left:
        ready = [n for n in left if not any(b in left for b in before.get(n, ()))]
        results = {n: merge(state, n) for n in ready}
        clean = [n for n in ready if not results[n][0]]
        if not clean:
            break
        best = min(clean, key=lambda n: (not unblocks.get(n), len(pair_conflicts.get(n, set()) & set(left)), rank[n]))
        order.append(best)
        state = results[best][1]
        left.remove(best)
    return order, {n: merge(state, n)[0] for n in left}


def merge_into(state, branch):
    """Merge ohne Arbeitskopie und ohne Ref: (Konfliktdateien, neuer Commit oder None)."""
    result = git('merge-tree', '--write-tree', '--name-only', state, branch, check=False)
    lines = result.stdout.splitlines()
    if result.returncode == 1:
        end = lines.index('') if '' in lines else len(lines)
        return lines[1:end] or ['?'], None
    if result.returncode != 0:
        sys.exit(f'git merge-tree {state} {branch} fehlgeschlagen:\n{result.stderr.strip()}')
    commit = git('commit-tree', lines[0], '-p', state, '-p', branch, '-m', 'merge-plan').stdout.strip()
    return [], commit


def cmd_merge_plan():
    """Empfohlene Reihenfolge fuer offene, fertige PRs von task/-Branches."""
    git('fetch', '-q', '--prune', 'origin')
    tasks, branches, prs = todo_tasks(), remote_branches(), pull_requests()
    if prs is None:
        sys.exit('GitHub CLI fehlt oder ist nicht angemeldet. Ohne PR-Status keine Reihenfolge.')
    names = sorted(n for n, pr in prs.items() if pr['state'] == 'OPEN' and not pr['isDraft'] and n in branches)
    if not names:
        print('Keine offenen PRs, die auf Review warten.')
        return
    ref = {n: f'origin/task/{n}' for n in names}
    cache = {}

    def merge(state, name):
        if (state, name) not in cache:
            cache[(state, name)] = merge_into(state, ref[name])
        return cache[(state, name)]

    open_tasks = [t for t in tasks.values() if not t.done]
    unblocks = {n: [t.name for t in open_tasks if split_branch(n)[0] in t.needs] for n in names}

    def rank(name):
        task = tasks.get(split_branch(name)[0])
        if task is None:
            return (len(SECTION_ORDER), len(PRIO_ORDER), prs[name]['number'])
        prio = PRIO_ORDER.index(task.prio) if task.prio in PRIO_ORDER else len(PRIO_ORDER)
        return (SECTION_ORDER.index(task.phase), prio, prs[name]['number'])

    pair_conflicts = {n: set() for n in names}
    for a in names:
        files, state = merge('origin/main', a)
        if files:
            continue
        for b in names:
            if b != a and merge(state, b)[0]:
                pair_conflicts[a].add(b)
    before = {b: [a for a in names if a != b and
                  git('merge-base', '--is-ancestor', ref[a], ref[b], check=False).returncode == 0 and
                  git('merge-base', '--is-ancestor', ref[a], 'origin/main', check=False).returncode != 0]
              for b in names}
    order, rest = merge_order(names, merge, unblocks, {n: rank(n) for n in names}, pair_conflicts, before)
    print('Merge-Reihenfolge (offene PRs ausser Entwuerfen; nach jedem Merge neu aufrufen)')
    for index, name in enumerate(order, 1):
        notes = []
        if unblocks[name]:
            notes.append('macht frei: ' + ', '.join(unblocks[name]))
        if before[name]:
            notes.append('baut auf ' + ', '.join(f"#{prs[a]['number']}" for a in before[name]))
        print(f"  {index:>2}. #{prs[name]['number']:<4} {name:<30} {'; '.join(notes)}")
    if rest:
        print('\nErst nachziehen (main einmergen, py tools/task_status.py update), Konflikt mit dem Stand davor:')
        for name, files in rest.items():
            print(f"      #{prs[name]['number']:<4} {name:<30} {', '.join(files)}")


def worktree_root():
    return Path(git('rev-parse', '--show-toplevel').stdout.strip())


def describe(task, tasks, state):
    """Kurzbeschreibung einer Aufgabe fuer die Konsole. Links behalten ihr Ziel, damit man gezielt weiterlesen kann."""
    def linked(text):
        return re.sub(r'\[([^\]]*)\]\(([^)]*)\)', r'\1 (\2)', text).replace('**', '')
    if task.done:
        return [f'{task.name}: erledigt (Phase {task.phase})', linked(task.text)]
    needs = ', '.join(f"{n} ({'erledigt' if tasks[n].done else 'offen'})" for n in task.needs) or '–'
    return [f'{task.name}: {task.prio}, {task.section}, {task.mode}, Spielzeit {task.playtime}',
            f'Status: {state}', f'Braucht: {needs}', linked(task.text)]


def cmd_show(kennung):
    git('fetch', '-q', '--prune', 'origin')
    tasks, source = todo_tasks(), 'origin/main'
    if kennung not in tasks:
        # Neu angelegte Aufgaben stehen erst im eigenen Branch.
        tasks, source = parse(read_local(worktree_root())), 'lokaler TODO.md'
    task = tasks.get(kennung)
    if task is None:
        sys.exit(f'{kennung} steht weder auf origin/main noch im lokalen TODO.md.')
    branches, prs = remote_branches(), pull_requests()
    for line in describe(task, tasks, status(kennung, branches, prs, tasks)):
        print(line)
    if source != 'origin/main':
        print(f'(aus {source}, noch nicht auf main)')


def cmd_done(kennung, result):
    root = worktree_root()
    branch = git('rev-parse', '--abbrev-ref', 'HEAD').stdout.strip()
    if branch.split('/')[:2] != ['task', kennung]:
        sys.exit(f'Nur im eigenen Worktree auf task/{kennung}, hier ist {branch}.')
    write_local(root, mark_done(read_local(root), kennung, result))
    stale = regenerate(root)
    if regenerate(root, check=True):
        sys.exit('task_status check schlaegt nach dem Erneuern fehl.')
    print(f'{kennung} steht unter Erledigt. Erneuert: ' + (', '.join(p.name for p in stale) or 'nichts weiter'))
    print('Weiter: committen, PR fertigstellen.')


def cmd_tests(args):
    parser = argparse.ArgumentParser(prog='tasks.py tests', description='Bestaetigungstests ohne Entwicklungsreservierung buendeln.')
    parser.add_argument('action', nargs='?', default='list', choices=('list', 'plan', 'pending', 'update', 'pass', 'reopen'))
    parser.add_argument('names', nargs='*')
    parser.add_argument('--all', action='store_true', help='Alle verfuegbaren ausstehenden Tests, aktive fremde Arbeit auslassen')
    parser.add_argument('--note', help='Letzter Stand bzw. nachgewiesenes Ergebnis, keine automatische Spielabnahme')
    parser.add_argument('--local', action='store_true', help='Bei list/plan den eigenen TODO.md statt origin/main lesen')
    options = parser.parse_args(args)
    reading = options.action in ('list', 'plan')
    if options.all and (options.names or options.action == 'pending'):
        parser.error('--all nicht mit Kennungen oder pending kombinieren')
    if not reading and (not options.note or (not options.names and not options.all)):
        parser.error('Aenderungen brauchen Kennungen oder --all sowie --note')
    if not reading and options.local:
        parser.error('--local ist nur fuer list/plan')
    git('fetch', '-q', '--prune', 'origin')
    branches, prs = remote_branches(), pull_requests()
    if not reading and prs is None:
        sys.exit('PR-Status unbekannt. Aktive fremde Arbeit kann nicht ausgeschlossen werden; nichts geaendert.')
    if reading and prs is None:
        print('Hinweis: PR-Status unbekannt; Verfuegbarkeit vor dem Termin erneut pruefen.')
    root = worktree_root()
    todo = read_local(root) if not reading or options.local else todo_text()
    selected_tasks = parse(todo)
    own_branch = None if reading else git('rev-parse', '--abbrev-ref', 'HEAD').stdout.strip()
    if not reading and not own_branch.startswith('task/'):
        sys.exit('Nur im eigenen task/-Worktree aendern, nie auf main. Testplanung braucht keine Reservierung der getesteten Aufgaben.')
    names = options.names or [t.name for t in selected_tasks.values() if t.confirmation]
    available, excluded = [], []
    for name in names:
        task = selected_tasks.get(name)
        if task is None or (task.done if options.action == 'pending' else not task.confirmation):
            sys.exit(f'{name}: kein passender offener Bestaetigungstest')
        reason = ('aktive fremde Arbeit' if active_work(name, branches, prs, own_branch) else
                  'blockiert, braucht ' + ', '.join(blockers(task, selected_tasks)) if blockers(task, selected_tasks) and
                  (reading or options.action == 'pending') else '')
        if reason:
            if options.names and not reading:
                sys.exit(f'{name}: {reason}. Gesamter Batch unveraendert.')
            excluded.append(f'{name}: {reason}')
        else:
            available.append(name)
    if reading:
        print('Bestätigungstests ausstehend (nur Info, blockieren nichts; kein Spielstart)')
        for name in available:
            task = selected_tasks[name]
            if options.action == 'plan':
                print('\n' + '\n'.join(describe(task, selected_tasks, 'Bestätigungstest ausstehend')))
            else:
                print(f'  {name:<24} Phase {task.phase}: {plain(task.confirmation)}')
        print(f'\n{len(available)} verfuegbar, {len(excluded)} ausgeschlossen.')
    elif available:
        # Erst alle Kennungen pruefen, dann einmal schreiben; nie einen halben Batch bei falschen Kennungen anwenden.
        updated = confirmation_update(todo, available, options.action, options.note)
        write_local(root, updated)
        regenerate(root)
        if regenerate(root, check=True):
            sys.exit('task_status check schlaegt nach dem Erneuern fehl.')
        print(f'{options.action}: ' + ', '.join(available) + '. Nur lokal geaendert; Doku pruefen, committen und PR verwenden.')
    else:
        print('Keine verfuegbaren Bestaetigungstests, nichts geaendert.')
    for reason in excluded:
        print('Ausgeschlossen: ' + reason)


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    args = sys.argv[1:]
    try:
        if args[:1] == ['list'] and len(args) == 1:
            cmd_list()
        elif args == ['list', '--active']:
            cmd_list(active_only=True)
        elif args == ['list', '--short']:
            cmd_list(short=True)
        elif args[:1] == ['tests']:
            cmd_tests(args[1:])
        elif args[:1] == ['show'] and len(args) == 2:
            cmd_show(args[1])
        elif args[:1] == ['done'] and len(args) == 3:
            cmd_done(args[1], args[2])
        elif args[:1] == ['claim'] and len(args) in (3, 4):
            cmd_claim(*args[1:])
        elif args[:1] == ['release'] and len(args) in (2, 3):
            cmd_release(*args[1:])
        elif args == ['merge-plan']:
            cmd_merge_plan()
        elif args in (['cleanup'], ['cleanup', '--apply']):
            cmd_cleanup(apply=len(args) == 2)
        else:
            sys.exit(__doc__)
    except ValueError as error:
        sys.exit(f'TODO.md ungueltig: {error}')


if __name__ == '__main__':
    main()
