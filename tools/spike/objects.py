"""Zaehlt wichtige Objekte und zeigt Figuren mit Rolle und Position: objects.py <pid> [<pid> ...]"""
import collections
import struct
import sys

from ddmem import Game

ROOTS = {'DDCharacter', 'EnemyBase', 'Projectile', 'LootBase', 'Weapon', 'Hands', 'DDPlayerController', 'DDPlayerState',
         'ChaosDestructibleActor', 'MapTile', 'MapGenerator', 'EnemySpawner', 'ActorChannel', 'DDMissionGameState'}
FLAG_CLASS_DEFAULT_OBJECT = 0x10


def root_class(game, cls, cache):
    if cls not in cache:
        current, found = cls, None
        for _ in range(30):
            if not current:
                break
            name = game.name(current)
            if name in ROOTS:
                found = name
                break
            current = game.ptr(current + 0x40)
        cache[cls] = found
    return cache[cls]


def report(pid):
    game = Game(pid)
    counts, roles, characters, cache = collections.Counter(), collections.Counter(), [], {}
    for obj in game.objects():
        header = game.read(obj + 8, 16)
        if not header or len(header) < 16 or struct.unpack_from('<I', header)[0] & FLAG_CLASS_DEFAULT_OBJECT:
            continue
        root = root_class(game, struct.unpack_from('<Q', header, 8)[0], cache)
        if not root:
            continue
        counts[root] += 1
        if root in ('DDCharacter', 'EnemyBase'):
            role = (game.read(obj + 0x168, 1) or b'\0')[0]
            roles[(root, role)] += 1
            if root == 'DDCharacter':
                component = game.ptr(obj + 0x1B8)
                raw = game.read(component + 0x148, 24) if component else None
                position = tuple(round(v) for v in struct.unpack('<ddd', raw)) if raw and len(raw) == 24 else None
                characters.append((role, position))
    print(pid, game.name(game.world()), dict(counts))
    print('  Rollen (1 Simulated, 2 Autonomous, 3 Authority):', dict(roles))
    print('  Figuren (Rolle, Position):', characters)


for pid in sys.argv[1:]:
    report(int(pid))
