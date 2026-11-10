"""Tag, Uhr, Tank und Missionsziele von Host und Client vergleichen: day_state.py <host-pid> <client-pid> [--all] [--json DATEI].

Fragt beide Instanzen ueber den Testkanal der Mod (Anfrage `day`, nur mit -ddcoop_control) und vergleicht alle skalaren
Reflexionsfelder des GameState, seiner Unterobjekte und aller DDObjective-Instanzen. Schluessel ist Klasse und Feld, nicht der
Objektname, weil Namen je Instanz verschieden sein koennen. Objektverweise (`<Klasse>`) werden nur nach Klasse verglichen.
Ausgabe: eine Zeile je Abweichung, am Ende `gleich` oder `ABWEICHUNG n` und eine Kurzzeile mit Tag, Uhr, Tank und Zielen.
Exitcode 0 gleich, 1 Abweichung, 2 keine Antwort.
"""
import json
import re
import sys

# Felder, die je Seite naturgemaess verschieden sind oder nur lokale Darstellung: Zeitstempel und Netzwerkverwaltung. Dazu Verweise,
# die nur der Server setzt: ThreatLevel kennt GameMode, GameState und Freeze-Manager nur beim Host, der die Gefahr steuert.
LOCAL = re.compile(r'(?i)(LastUpdate|ReplicatedWorldTimeSeconds|ServerWorldTimeSecondsDelta|NetUpdate|Timestamp|LocalPlayer'
                   r'|ThreatLevel\.(GameMode|GameState|FreezeThreatLevelManager)$)')
# Bekannte Abweichungen mit eigener Aufgabe: werden gezeigt, zaehlen aber nicht. Derzeit keine; Schrott wich am 08.11.2026 vor
# lager-handel-client (0x309) ab und soll seitdem gleich sein, darum zaehlt er wieder.
KNOWN = re.compile(r'(?!)')


def ask(pid, line='day'):
    try:
        with open(rf'\\.\pipe\ddcoop-ctl-{pid}', 'r+b', buffering=0) as pipe:
            pipe.write((line + '\n').encode('ascii'))
            data = b''
            while not data.endswith(b'\n'):
                chunk = pipe.read(65536)
                if not chunk:
                    break
                data += chunk
    except OSError as error:
        print(f'{pid}: Testkanal nicht erreichbar ({error})')
        sys.exit(2)
    reply = json.loads(data.decode('utf-8', 'replace'))
    if not reply.get('ok'):
        print(f'{pid}: {reply.get("error")}')
        sys.exit(2)
    return reply


def klass(text):
    match = re.search(r'<([^>]*)>$', text or '')
    return match.group(1) if match else (text or '')


def flatten(reply):
    """Klasse#Nummer.Feld -> Wert. Objektverweise auf die Klasse reduziert."""
    result = {}
    entries = []
    if reply.get('gameState'):
        entries.append(('GameState', reply['gameState']))
    entries += [('child', entry) for entry in reply.get('children', [])]
    entries += [('objective', entry) for entry in reply.get('objectives', [])]
    seen = {}
    for kind, entry in entries:
        name = klass(entry['object'])
        number = seen.get((kind, name), 0)
        seen[(kind, name)] = number + 1
        prefix = f'{kind}:{name}#{number}'
        result[f'{prefix}.outer'] = klass(entry.get('outer'))
        for field, value in entry.get('fields', {}).items():
            if isinstance(value, str) and '<' in value:
                value = klass(value)
            result[f'{prefix}.{field}'] = value
    return result


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if len(args) < 2:
        raise SystemExit(__doc__)
    show_all = '--all' in sys.argv
    host, client = ask(int(args[0])), ask(int(args[1]))
    if '--json' in sys.argv:
        path = sys.argv[sys.argv.index('--json') + 1]
        with open(path, 'w', encoding='utf-8') as out:
            json.dump({'host': host, 'client': client}, out, indent=1)
    left, right = flatten(host), flatten(client)
    differences = 0
    for key in sorted(set(left) | set(right)):
        a, b = left.get(key, '<fehlt>'), right.get(key, '<fehlt>')
        if a == b:
            if show_all:
                print(f'  {key}: {a}')
            continue
        if LOCAL.search(key):
            continue
        if key.endswith('.outer') and a == 'ThreatLevel' and b == '<fehlt>':
            continue  # Freeze-Manager der Gefahrenstufe legt nur der Host an, siehe LOCAL
        if isinstance(a, (int, float)) and isinstance(b, (int, float)) and not isinstance(a, bool) and key.endswith(
                ('SecondsSinceMissionStart', 'CurrentFuel')) and abs(a - b) <= 2:
            continue  # Uhr und Tank laufen weiter, waehrend nacheinander gefragt wird
        if KNOWN.search(key):
            print(f'~ {key}: Host {a} | Client {b} (bekannt, eigene Aufgabe)')
            continue
        differences += 1
        print(f'! {key}: Host {a} | Client {b}')
    def pick(side, suffix):
        return next((value for key, value in side.items() if key.endswith(suffix)), '-')
    summary = ' | '.join(
        f"{name} Tag {pick(side, '.DayNum')} Uhr {pick(side, '.SecondsSinceMissionStart')} "
        f"Tank {pick(side, 'FuelMeter.CurrentFuel')}/{pick(side, 'FuelMeter.MaxRequiredFuel')} "
        f"Nacht {pick(side, 'NightmodeStartedSeconds')} Ziele {len(reply.get('objectives', []))}"
        for name, side, reply in (('Host', left, host), ('Client', right, client)))
    print(('gleich' if not differences else f'ABWEICHUNG {differences}') + '  ' + summary)
    return 1 if differences else 0


if __name__ == '__main__':
    sys.exit(main())
