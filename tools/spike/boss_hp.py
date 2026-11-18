"""Lebenspunkte je Boss und Attributsatz: boss_hp.py <pid> [<pid> ...]

Listet je lebendem BP_BossEnemy_C alle Attributsaetze (Outer = Boss) mit Klasse, Health und MaxHealth, dazu die Boss-Objectives
(DDBossObjective.MaxHealth, CurrentProgress). Offsets siehe docs/OFFSETS.md (opt-endboss-leiste).
"""
import struct
import sys

from ddmem import Game

FLAGS_NON_INSTANCE = 0x30
SUPER = 0x40
OUTER = 0x20
ROLE = 0x168
ACTOR_FLAGS = 0x5D
HEALTH = 0xA0 + 0xC
MAX_HEALTH = 0x30 + 0xC
OBJECTIVE_MAX = 0xD0


def chain(game, cls, cache):
    if cls not in cache:
        names, current = [], cls
        for _ in range(40):
            if not current:
                break
            names.append(game.name(current))
            current = game.ptr(current + SUPER)
        cache[cls] = names
    return cache[cls]


def f32(game, address):
    raw = game.read(address, 4)
    return round(struct.unpack('<f', raw)[0]) if raw and len(raw) == 4 else None


def report(pid):
    game = Game(pid)
    cache, bosses, sets, objectives = {}, [], [], []
    for obj in game.objects():
        header = game.read(obj + 8, 16)
        if not header or len(header) < 16 or struct.unpack_from('<I', header)[0] & FLAGS_NON_INSTANCE:
            continue
        names = chain(game, struct.unpack_from('<Q', header, 8)[0], cache)
        if 'BP_BossEnemy_C' in names:
            bosses.append((obj, names[0]))
        elif 'CommonAttributeSet' in names:
            sets.append((obj, names[0], game.ptr(obj + OUTER)))
        elif 'DDBossObjective' in names:
            objectives.append((obj, names[0]))
    print(pid, game.name(game.world()))
    for boss, cls in bosses:
        destroyed = (game.read(boss + ACTOR_FLAGS, 1) or b'\0')[0] & 0x02
        print('  Boss', cls, hex(boss), 'Rolle', (game.read(boss + ROLE, 1) or b'\0')[0], 'zerstoert' if destroyed else '')
        for obj, set_cls, outer in sets:
            if outer == boss:
                print('    Satz', set_cls, hex(obj), 'HP', f32(game, obj + HEALTH), '/', f32(game, obj + MAX_HEALTH))
    for obj, cls in objectives:
        raw = game.read(obj + OBJECTIVE_MAX, 8)
        print('  Objective', cls, hex(obj), 'MaxHealth', struct.unpack('<q', raw)[0] if raw and len(raw) == 8 else None)


for pid in sys.argv[1:]:
    report(int(pid))
