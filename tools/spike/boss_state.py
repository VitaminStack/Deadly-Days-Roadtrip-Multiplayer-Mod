"""Bosskarte und Boss je Instanz: boss_state.py <pid> [<pid> ...]

Zeigt BossMapTileParcel (Arena), BossfightSpawnPoint (SpawnedBoss), BossComponent, lebende Bosse (Rolle, HP, Position)
und die Figuren. Offsets Build 26495302, siehe docs/OFFSETS.md (endboss-spawn).
"""
import struct
import sys

from ddmem import Game

FLAG_CLASS_DEFAULT_OBJECT = 0x10
ROOTS = ('BossMapTileParcel', 'BossfightSpawnPoint', 'BossComponent', 'BossEnemy', 'DDCharacter', 'CommonAttributeSet')
SUPER = 0x40
ROOT_COMPONENT = 0x1B8
COMPONENT_LOCATION = 0x148
ROLE = 0x168
SPAWNED_BOSS = 0x498
PARCEL_SPAWN_POINT = 0x390
PARCEL_TRIGGER_ZONE = 0x370
HEALTH = 0xA0 + 0xC
MAX_HEALTH = 0x30 + 0xC


def chain(game, cls, cache):
    if cls not in cache:
        names, current = [], cls
        for _ in range(30):
            if not current:
                break
            names.append(game.name(current))
            current = game.ptr(current + SUPER)
        cache[cls] = names
    return cache[cls]


def location(game, actor):
    component = game.ptr(actor + ROOT_COMPONENT)
    raw = game.read(component + COMPONENT_LOCATION, 24) if component else None
    return tuple(round(v) for v in struct.unpack('<ddd', raw)) if raw and len(raw) == 24 else None


def component_location(game, component):
    raw = game.read(component + COMPONENT_LOCATION, 24) if component else None
    return tuple(round(v) for v in struct.unpack('<ddd', raw)) if raw and len(raw) == 24 else None


def f32(game, address):
    raw = game.read(address, 4)
    return round(struct.unpack('<f', raw)[0]) if raw and len(raw) == 4 else None


def report(pid):
    game = Game(pid)
    cache, found = {}, {root: [] for root in ROOTS}
    for obj in game.objects():
        header = game.read(obj + 8, 16)
        if not header or len(header) < 16 or struct.unpack_from('<I', header)[0] & FLAG_CLASS_DEFAULT_OBJECT:
            continue
        names = chain(game, struct.unpack_from('<Q', header, 8)[0], cache)
        for root in ROOTS:
            if root in names or 'BP_' + root + '_C' in names:
                found[root].append((obj, names[0]))
    print(pid, game.name(game.world()))
    for obj, cls in found['BossMapTileParcel']:
        print('  Arena', cls, 'SpawnPoint', hex(game.ptr(obj + PARCEL_SPAWN_POINT)), 'Trigger',
              component_location(game, game.ptr(obj + PARCEL_TRIGGER_ZONE)))
    for obj, cls in found['BossfightSpawnPoint']:
        print('  SpawnPoint', cls, hex(obj), 'SpawnedBoss', hex(game.ptr(obj + SPAWNED_BOSS)), location(game, obj))
    print('  BossComponent', len(found['BossComponent']))
    sets = {game.ptr(obj + 0x20): obj for obj, _ in found['CommonAttributeSet']}
    for obj, cls in found['BossEnemy']:
        attributes = sets.get(obj)
        hp = (f32(game, attributes + HEALTH), f32(game, attributes + MAX_HEALTH)) if attributes else None
        print('  Boss', cls, hex(obj), 'Rolle', (game.read(obj + ROLE, 1) or b'\0')[0], 'HP', hp, location(game, obj))
    print('  Figuren', [(cls, (game.read(obj + ROLE, 1) or b'\0')[0], location(game, obj)) for obj, cls in found['DDCharacter']])


for pid in sys.argv[1:]:
    report(int(pid))
