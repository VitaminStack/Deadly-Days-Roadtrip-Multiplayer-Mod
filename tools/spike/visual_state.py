"""Lesender Host-/Client-Vergleich visueller Objekte, nur Build 26187885.

Keine Actors erzeugen, keine Effekte oder XP ausloesen. Rohberichte gehoeren
unter research/local, nicht ins oeffentliche Repository.
"""
import argparse
import collections
import json
import math
import struct

ROOTS = {'Projectile', 'ExpEmissionHandler', 'Orb', 'DamageNumberActor',
         'EnemyChaosDestructionProxyActor', 'ProjectileImpactActor', 'BP_Airdrop_Lootable_C'}


def array_header(data):
    """Ein defekter/unlesbarer Header ist unbekannt, nicht ein leeres Array."""
    if data is None or len(data) != 16:
        return None
    pointer, count, capacity = struct.unpack('<Qii', data)
    if not 0 <= count <= capacity <= 100000 or (count and not pointer):
        return None
    return count


def actor_flags(data):
    if data is None or len(data) != 6:
        return None
    return {'replicate_movement': bool(data[0] & 0x10),
            'hidden': bool(data[0] & 0x80),
            'destroying': bool(data[5] & 0x02)}


def vector(game, address):
    data = game.read(address, 24)
    if data is None or len(data) != 24:
        return None
    values = list(struct.unpack('<ddd', data))
    return values if all(math.isfinite(value) for value in values) else None


def snapshot(pid):
    from ddmem import Game

    game = Game(pid)
    world = game.world()
    if not world:
        raise RuntimeError('Keine aktuelle Welt lesbar; keine leere Szene behaupten.')
    level = game.ptr(world + 0x30)
    if not level:
        raise RuntimeError('Kein PersistentLevel lesbar; keine leere Szene behaupten.')
    classes, counts, actors = {}, collections.Counter(), []
    for obj in game.objects():
        if game.i32(obj + 8) & 0x30 or game.ptr(obj + 0x20) != level:
            continue
        cls = game.ptr(obj + 0x10)
        if cls not in classes:
            current, root = cls, None
            for _ in range(64):
                if not current:
                    break
                name = game.name(current)
                if name in ROOTS:
                    root = name
                    break
                current = game.ptr(current + 0x40)
            classes[cls] = root
        root = classes[cls]
        if not root:
            continue
        counts[root] += 1
        component = game.ptr(obj + 0x1b8)
        role = game.read(obj + 0x168, 1)
        row = {'name': game.name(obj), 'class': game.name(cls), 'root': root,
               'role': role[0] if role and len(role) == 1 else None,
               'flags': actor_flags(game.read(obj + 0x58, 6)),
               'position': vector(game, component + 0x148) if component else None,
               'scale': vector(game, component + 0x178) if component else None}
        if root == 'Projectile':
            data = game.read(obj + 0x2fc, 4)
            row['bullet_size'] = struct.unpack('<f', data)[0] if data and len(data) == 4 else None
            row['visual_component'] = game.name(game.ptr(obj + 0x400))
        if root == 'ExpEmissionHandler':
            row['emitter'] = game.name(game.ptr(obj + 0x2f8))
            row['array_counts'] = {name: array_header(game.read(obj + offset, 16)) for name, offset in
                             [('exp', 0x378), ('pending', 0x388), ('spawn', 0x398),
                              ('end', 0x3a8), ('values', 0x3b8), ('colors', 0x3c8)]}
        actors.append(row)
    return {'world': game.name(world), 'counts_including_destroying': dict(counts), 'actors': actors,
            'limitations': ['Snapshots are sequential, not frame-synchronous.',
                            'Emitter array sizes are retained CPU data, not live GPU particle counts.',
                            'Actor presence does not prove visible rendering or equal lifecycle.',
                            'Only actors directly in the current persistent level are selected.',
                            'Fixed offsets: supported build 26187885 only.']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pids', type=int, nargs='+')
    args = parser.parse_args()
    print(json.dumps([snapshot(pid) for pid in args.pids], ensure_ascii=True, indent=2))
