"""Lesender Abgleich von Spawn und Tod der Gegner zwischen Host und Client, nur Build 26187885.

    py tools/spike/enemy_sync.py <sekunden> <host_pid> <client_pid>

Liest je Viertelsekunde alle EnemyBase-Actors des PersistentLevel beider Prozesse, merkt sich je Objekt den ersten
Zeitpunkt lebend und den ersten mit bActorIsBeingDestroyed und ordnet Tode beider Seiten ueber die letzte bekannte
Position zu. Erzeugt und veraendert nichts. Die Proben beider Seiten sind nacheinander, nicht im selben Frame; ein
Gegner, der zwischen zwei Neuaufnahmen der Objektliste entsteht und stirbt, fehlt. Rohberichte nicht ins Repo.
"""
import argparse
import math
import statistics
import struct
import sys
import time
from pathlib import Path

BEING_DESTROYED = 0x02  # AActor+0x5D
HIDDEN = 0x80  # AActor+0x58


def match_events(host, client, max_delay=3.0, max_distance=300.0):
    """Ordnet jedem Host-Ereignis (Spawn oder Tod) hoechstens eines des Clients zu: (Zeit, Position) je Eintrag,
    Ergebnis die Verzoegerungen.

    Ein Client-Ereignis zaehlt, wenn es hoechstens eine Sekunde vor und max_delay Sekunden nach dem des Hosts liegt und
    die letzte bekannte Position in der Ebene naeher als max_distance ist. Bei mehreren Kandidaten gewinnt die
    kleinste Zeitdifferenz. Eintraege ohne Position werden nie zugeordnet.
    """
    used, delays = set(), []
    for host_time, host_pos in sorted(host, key=lambda entry: entry[0]):
        best = None
        for index, (client_time, client_pos) in enumerate(client):
            if index in used or host_pos is None or client_pos is None:
                continue
            delay = client_time - host_time
            if -1.0 <= delay <= max_delay and math.dist(host_pos[:2], client_pos[:2]) < max_distance:
                if best is None or abs(delay) < abs(best[1]):
                    best = (index, delay)
        if best:
            used.add(best[0])
            delays.append(best[1])
    return delays


def summary(label, host_count, client_count, delays):
    """Eine Zeile je Ereignisart, ohne Prozessadressen."""
    line = f'{label}: Host {host_count}, Client {client_count}, zugeordnet {len(delays)}'
    if delays:
        line += f', beim Client spaeter um Median {statistics.median(delays):.2f} s, hoechstens {max(delays):.2f} s'
    return line


class Side:
    def __init__(self, pid):
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from ddmem import Game

        self.game = Game(pid)
        self.level = self.game.ptr(self.game.world() + 0x30)
        if not self.level:
            raise RuntimeError(f'{pid}: kein PersistentLevel lesbar')
        self.enemy_class, self.seen, self.objects = {}, {}, []
        self.rescan()

    def is_enemy(self, cls):
        if cls not in self.enemy_class:
            current, found = cls, False
            for _ in range(64):
                if not current:
                    break
                if self.game.name(current) == 'EnemyBase':
                    found = True
                    break
                current = self.game.ptr(current + 0x40)
            self.enemy_class[cls] = found
        return self.enemy_class[cls]

    def rescan(self):
        game = self.game
        self.objects = [obj for obj in game.objects()
                        if not game.i32(obj + 8) & 0x30 and game.ptr(obj + 0x20) == self.level
                        and self.is_enemy(game.ptr(obj + 0x10))]

    def sample(self, now):
        alive = 0
        for obj in self.objects:
            flags = self.game.read(obj + 0x58, 6)
            if not flags or len(flags) < 6:
                continue
            dying = bool(flags[5] & BEING_DESTROYED)
            entry = self.seen.get(obj)
            if entry is None:
                if dying:
                    continue
                entry = self.seen[obj] = {'spawn': now, 'death': None, 'pos': None, 'first': None}
            if dying:
                entry['death'] = entry['death'] if entry['death'] is not None else now
                continue
            if not flags[0] & HIDDEN:
                alive += 1
            component = self.game.ptr(obj + 0x1B8)
            raw = self.game.read(component + 0x148, 24) if component else None
            if raw and len(raw) == 24:
                entry['pos'] = struct.unpack('<ddd', raw)
                entry['first'] = entry['first'] or entry['pos']
        return alive

    def deaths(self):
        return [(entry['death'], entry['pos']) for entry in self.seen.values() if entry['death'] is not None]

    def spawns(self):
        # Gegner aus der ersten Sekunde gab es schon vor der Messung.
        return [(entry['spawn'], entry['first']) for entry in self.seen.values() if entry['spawn'] > 1.0]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('seconds', type=float)
    parser.add_argument('host', type=int)
    parser.add_argument('client', type=int)
    args = parser.parse_args()
    host, client = Side(args.host), Side(args.client)
    start, samples, equal = time.time(), 0, 0
    while time.time() - start < args.seconds:
        if samples % 4 == 0:
            host.rescan()
            client.rescan()
        now = time.time() - start
        equal += host.sample(now) == client.sample(now)
        samples += 1
        time.sleep(0.25)
    print(summary('Spawns', len(host.spawns()), len(client.spawns()), match_events(host.spawns(), client.spawns(), 10.0)))
    print(summary('Tode', len(host.deaths()), len(client.deaths()), match_events(host.deaths(), client.deaths())))
    print(f'Lebende Gegner gleich in {equal} von {samples} Proben')


if __name__ == '__main__':
    sys.exit(main())
