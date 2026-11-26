"""Run-Statistik (DDGameStatsSubsystem) von Host und Client vergleichen: round_stats.py <host-pid> [<client-pid>] [--json DATEI].

Liest nur Speicher. Gesamtwerte liegen direkt hinter CurrentGameData (+0x198, TMultiMap<FMissionTimeIdentifier, FGameStat>),
die Werte je Spieler dahinter (+0x1E8, TMap<int32, gleicher Container>), Layout in docs/OFFSETS.md (opt-ergebnis-werte).
Ausgabe: je Tag und Typ eine Zeile mit den Werten beider Seiten, am Ende `gleich` oder `ABWEICHUNG n`.
Exitcode 0 gleich, 1 Abweichung, 2 kein Subsystem gefunden.
"""
import json
import struct
import sys

from ddmem import Game

TYPES = ['MaxThreatLevel', 'Duration', 'Kills', 'DamageDealt', 'DPS', 'LevelUps', 'ItemsLooted', 'HealthPercentage',
         'NightSeconds', 'HitCount', 'DamagePerHit']
GLOBAL_OFFSET, PLAYER_OFFSET, TIME_OFFSET = 0x198, 0x1E8, 0xD0  # TIME: CurrentGameData.TimeIdentifier
FLAG_CLASS_DEFAULT_OBJECT = 0x10


def is_stats_class(game, cls, cache):
    """Die Instanz gehoert zur Blueprint-Klasse BP_DDGameStatsSubsystem_C, darum ueber die Oberklassen (+0x40) suchen."""
    if cls not in cache:
        current, found = cls, False
        for _ in range(8):
            if not current:
                break
            if game.name(current) == 'DDGameStatsSubsystem':
                found = True
                break
            current = game.ptr(current + 0x40)
        cache[cls] = found
    return cache[cls]


def find_subsystem(game):
    cache = {}
    for obj in game.objects():
        if not is_stats_class(game, game.ptr(obj + 0x10), cache):
            continue
        flags = struct.unpack('<I', game.read(obj + 8, 4) or b'\x10\0\0\0')[0]
        if not flags & FLAG_CLASS_DEFAULT_OBJECT:
            return obj
    return 0


def elements(game, header, stride):
    """Belegte Elemente eines TSet/TMap aus seinem Kopf (TSparseArray, Belegungsbits inline bis 128 Plaetze)."""
    if not header or len(header) < 0x30:
        return []
    data, num = struct.unpack_from('<Qi', header, 0)
    secondary = struct.unpack_from('<Q', header, 0x20)[0]
    if num <= 0 or num > 65536 or not data:
        return []
    bits = game.read(secondary, (num + 31) // 32 * 4) if secondary else header[0x10:0x20]
    raw = game.read(data, num * stride) or b''
    out = []
    for i in range(min(num, len(raw) // stride)):
        word = struct.unpack_from('<I', bits, (i // 32) * 4)[0] if bits and len(bits) >= (i // 32 + 1) * 4 else 0
        if word & (1 << (i % 32)):
            out.append(raw[i * stride:(i + 1) * stride])
    return out


def container(game, header):
    values = {}
    for element in elements(game, header, 0x20):
        day, sub, kind, value = struct.unpack_from('<iiB7xq', element, 0)
        values[(day, sub, kind)] = value
    return values


def read_stats(pid):
    game = Game(pid)
    subsystem = find_subsystem(game)
    if not subsystem:
        return None
    time = struct.unpack('<ii', game.read(subsystem + TIME_OFFSET, 8) or b'\0' * 8)
    stats = {'global': container(game, game.read(subsystem + GLOBAL_OFFSET, 0x50)), 'players': {}, 'time': time}
    for element in elements(game, game.read(subsystem + PLAYER_OFFSET, 0x50), 0x60):
        # Schluessel int32, der Container des Spielers liegt im Element bei +8.
        stats['players'][struct.unpack_from('<i', element, 0)[0]] = container(game, element[8:0x58])
    return stats

def label(key):
    day, sub, kind = key
    return f'Tag {day}.{sub} {TYPES[kind] if kind < len(TYPES) else kind}'


def main(argv):
    out = None
    if '--json' in argv:
        at = argv.index('--json')
        out = argv[at + 1]
        argv = argv[:at] + argv[at + 2:]
    pids = [int(a) for a in argv]
    if not pids:
        print(__doc__)
        return 2
    sides = [read_stats(pid) for pid in pids]
    if any(side is None for side in sides):
        print('kein DDGameStatsSubsystem gefunden')
        return 2
    differences = 0
    print('Tageskennung ' + ' | '.join(f'{s["time"][0]}.{s["time"][1]}' for s in sides))
    differences += len({s['time'] for s in sides}) > 1
    scopes = [('gesamt', [side['global'] for side in sides])]
    players = sorted(set().union(*[side['players'].keys() for side in sides]))
    scopes += [(f'P{p}', [side['players'].get(p, {}) for side in sides]) for p in players]
    for name, maps in scopes:
        for key in sorted(set().union(*[m.keys() for m in maps])):
            values = [m.get(key) for m in maps]
            same = len(set(values)) == 1
            differences += not same
            print(f'{name:7} {label(key):28} ' + ' | '.join('-' if v is None else str(v) for v in values) + ('' if same else '  ABWEICHUNG'))
    if out:
        with open(out, 'w', encoding='utf-8') as handle:
            json.dump([{'global': {label(k): v for k, v in s['global'].items()},
                        'players': {str(p): {label(k): v for k, v in m.items()} for p, m in s['players'].items()}} for s in sides],
                      handle, indent=1)
    if len(sides) > 1:
        print('gleich' if not differences else f'ABWEICHUNG {differences}')
    return 1 if differences and len(sides) > 1 else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
