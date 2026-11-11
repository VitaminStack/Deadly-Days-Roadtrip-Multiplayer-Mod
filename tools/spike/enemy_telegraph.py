"""Lesender Vergleich der Warnanzeigen von Gegnerangriffen zwischen Host und Client, nur Build 26495302.

    py tools/spike/enemy_telegraph.py <sekunden> <host_pid> [<client_pid>]

Sucht alle zwei Sekunden im Objekt-Array Niagara-Komponenten, `TackleAbilityExecution`, Projektile, Gegner und
Instanzen von Gegner-Angriffsfaehigkeiten (`EnemyAttackAbility`). Dazwischen fragt es je 50 ms nur die gemerkten Objekte ab
und zaehlt Einschaltungen (aus nach an): aktive Niagara-Komponenten je System-Asset, gesetzte und aktive `LineTelegraph`
und gesetzte `OptionalProjectileTelegraph`. Eine Anzeige, die zwischen zwei Suchen entsteht und vergeht, fehlt auf beiden
Seiten gleich. Erzeugt und veraendert nichts. Rohberichte nicht ins Repo.
"""
import argparse
import collections
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ddmem import Game  # noqa: E402

CLASS, SUPER = 0x10, 0x40
SKIP = 0x10 | 0x30  # ClassDefaultObject, Garbage/Unreachable
NIAGARA_ASSET = 0x580  # UNiagaraComponent::Asset
ACTIVE_BYTE, ACTIVE_BIT = 0x9A, 0x10  # UActorComponent::bIsActive
LINE_TELEGRAPH = 0x238  # UTackleAbilityExecution::LineTelegraph
PROJECTILE_TELEGRAPH = 0x378  # AProjectile::OptionalProjectileTelegraph
ROOTS = ('NiagaraComponent', 'EnemyBase', 'EnemyAttackAbility', 'TackleAbilityExecution', 'Projectile')


class Side:
    def __init__(self, pid):
        self.game = Game(pid)
        self.roots, self.state = {}, {}
        self.lists = collections.defaultdict(list)
        self.edges = collections.defaultdict(collections.Counter)
        self.peak = collections.defaultdict(collections.Counter)
        self.scan_seconds = 0.0

    def root(self, cls):
        if cls not in self.roots:
            current, found = cls, None
            for _ in range(64):
                if not current:
                    break
                name = self.game.name(current)
                if name in ROOTS:
                    found = name
                    break
                current = self.game.ptr(current + SUPER)
            self.roots[cls] = found
        return self.roots[cls]

    def scan(self):
        start, game = time.time(), self.game
        lists, counts = collections.defaultdict(list), collections.defaultdict(collections.Counter)
        for obj in game.objects():
            if game.i32(obj + 8) & SKIP:
                continue
            cls = game.ptr(obj + CLASS)
            kind = self.root(cls)
            if kind:
                lists[kind].append(obj)
                if kind in ('EnemyBase', 'EnemyAttackAbility', 'Projectile'):
                    counts[kind][game.name(cls)] += 1
        self.lists = lists
        for group, counter in counts.items():
            for key, value in counter.items():
                self.peak[group][key] = max(self.peak[group][key], value)
        self.scan_seconds = max(self.scan_seconds, time.time() - start)

    def active(self, component):
        return bool((self.game.read(component + ACTIVE_BYTE, 1) or b'\0')[0] & ACTIVE_BIT)

    def edge(self, key, on, group, label):
        if on and not self.state.get(key):
            self.edges[group][label] += 1
        self.state[key] = on

    def poll(self):
        game = self.game
        for obj in self.lists['NiagaraComponent']:
            asset = game.ptr(obj + NIAGARA_ASSET)
            on = bool(asset) and self.active(obj)
            if on or self.state.get(('n', obj)):
                self.edge(('n', obj), on, 'niagara an', str(game.name(asset)))
        for obj in self.lists['TackleAbilityExecution']:
            line = game.ptr(obj + LINE_TELEGRAPH)
            self.edge(('ts', obj), bool(line), 'tackle', 'LineTelegraph gesetzt')
            self.edge(('ta', obj), bool(line) and self.active(line), 'tackle',
                      'LineTelegraph aktiv ' + str(game.name(game.ptr(line + NIAGARA_ASSET)) if line else ''))
        for obj in self.lists['Projectile']:
            telegraph = game.ptr(obj + PROJECTILE_TELEGRAPH)
            self.edge(('p', obj), bool(telegraph), 'projektil', 'OptionalProjectileTelegraph ' + game.name(game.ptr(obj + CLASS)))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('seconds', type=float)
    parser.add_argument('pids', type=int, nargs='+')
    args = parser.parse_args()
    sides = [Side(pid) for pid in args.pids]
    labels = ['Host', 'Client'][:len(sides)]
    start, next_scan, polls = time.time(), 0.0, 0
    while time.time() - start < args.seconds:
        if time.time() >= next_scan:
            for side in sides:
                side.scan()
            next_scan = time.time() + 2.0
        for side in sides:
            side.poll()
        polls += 1
        time.sleep(0.05)
    print(f'{polls} Abfragen, laengste Suche ' + ', '.join(f'{label} {side.scan_seconds:.1f} s' for label, side in zip(labels, sides)))
    for name, source in (('Gegner (max gleichzeitig)', 'peak:EnemyBase'), ('Angriffsfaehigkeiten', 'peak:EnemyAttackAbility'),
                         ('Projektile', 'peak:Projectile'), ('Tackle (Einschaltungen)', 'tackle'),
                         ('Projektil-Telegraph (Einschaltungen)', 'projektil'), ('Niagara (Einschaltungen)', 'niagara an')):
        kind, _, group = source.partition(':')
        tables = [side.peak[group] if group else side.edges[kind] for side in sides]
        keys = sorted(set().union(*(table.keys() for table in tables)))
        print(f'[{name}]')
        for key in keys:
            print(f'  {key}: ' + ', '.join(f'{label} {table[key]}' for label, table in zip(labels, tables)))


if __name__ == '__main__':
    sys.exit(main())
