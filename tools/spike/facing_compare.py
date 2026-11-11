"""Read-only Vergleich der Blickrichtung Host gegen Client: facing_compare.py <host-pid> <client-pid> [--seconds N] [--interval S].

Build 26495302. Liest je Probe in beiden Instanzen jede BP_DD_Character_C: Ort, Rolle, Blickwinkel der Haende (AHands+0x31C),
Welt-Gierwinkel des Haende-Actors und die Ansicht des Richtungs-Meshes (FacingRight/FacingDown, Mesh+0x7F0). Figuren werden ueber
den naechsten Ort gepaart, nicht ueber die Reihenfolge. Am Ende je Figur: Anteil gleicher Ansicht, Winkelabstand, wie oft die
Ansicht beim Client wechselte. Schreibt nichts, ruft nichts."""
import argparse
import math
import struct
import time

from ddmem import Game
import facing

HANDS_ANGLE = 0x31C
MESH_FACING = 0x7F0
COMP_WORLD_LOCATION = 0x210


def snapshot(game, chars):
    rows = []
    for char in chars:
        root = game.ptr(char + facing.ACTOR_ROOT)
        hands = game.ptr(char + facing.CHAR_HANDS)
        mesh = game.ptr(char + facing.BP_MESH)
        location = facing.vec(game, root + COMP_WORLD_LOCATION) if root else None
        hands_root = game.ptr(hands + facing.ACTOR_ROOT) if hands else 0
        angle = struct.unpack('<f', game.read(hands + HANDS_ANGLE, 4) or b'\0\0\0\0')[0] if hands else None
        yaw = facing.yaw_of_quat(facing.vec(game, hands_root + facing.COMP_WORLD_QUAT, 4)) if hands_root else None
        face = tuple(game.read(mesh + MESH_FACING, 2) or b'\xff\xff') if mesh else None
        rows.append({'role': (game.read(char + facing.ACTOR_ROLE, 1) or b'\0')[0], 'location': location,
                     'angle': angle, 'yaw': yaw, 'face': face})
    return rows


def wrap(value):
    return (value + 180.0) % 360.0 - 180.0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('host', type=int)
    parser.add_argument('client', type=int)
    parser.add_argument('--seconds', type=float, default=20)
    parser.add_argument('--interval', type=float, default=0.05)
    args = parser.parse_args()
    host, client = Game(args.host), Game(args.client)
    stats = {}
    # Figurenliste nur einmal suchen (Suche ueber alle Objekte dauert ueber eine Sekunde), dann beide Seiten dicht hintereinander.
    host_chars, client_chars = facing.characters(host), facing.characters(client)
    end = time.time() + args.seconds
    while time.time() < end:
        host_rows, client_rows = snapshot(host, host_chars), snapshot(client, client_chars)
        for row in client_rows:
            if not row['location'] or not host_rows:
                continue
            partner = min((h for h in host_rows if h['location']),
                          key=lambda h: math.dist(h['location'][:2], row['location'][:2]), default=None)
            if not partner:
                continue
            key = 'Client-Figur' if row['role'] == 2 else 'Host-Figur' if row['role'] == 1 else f'Rolle {row["role"]}'
            s = stats.setdefault(key, {'n': 0, 'same': 0, 'diff': [], 'client_faces': set(), 'host_faces': set(),
                                       'switches': 0, 'last': None, 'client_angle0': 0, 'distance': []})
            s['n'] += 1
            s['same'] += row['face'] == partner['face']
            s['client_faces'].add(row['face'])
            s['host_faces'].add(partner['face'])
            s['distance'].append(math.dist(partner['location'][:2], row['location'][:2]))
            if row['angle'] == 0.0:
                s['client_angle0'] += 1
            if row['angle'] is not None and partner['angle'] is not None:
                s['diff'].append(abs(wrap(row['angle'] - partner['angle'])))
            if s['last'] is not None and s['last'] != row['face']:
                s['switches'] += 1
            s['last'] = row['face']
        time.sleep(args.interval)
    for key, s in stats.items():
        diffs = sorted(s['diff'])
        median = diffs[len(diffs) // 2] if diffs else float('nan')
        p90 = diffs[int(len(diffs) * 0.9)] if diffs else float('nan')
        print(f'{key}: Proben {s["n"]}, gleiche Ansicht {100 * s["same"] / max(1, s["n"]):.0f} %, '
              f'Winkelabstand Median {median:.1f} / 90 % {p90:.1f} Grad, Ansichtswechsel beim Client {s["switches"]}, '
              f'Ansichten Client {sorted(s["client_faces"])}, Host {sorted(s["host_faces"])}, '
              f'Winkel 0 beim Client {s["client_angle0"]}, Ortsabstand Median {sorted(s["distance"])[len(s["distance"]) // 2]:.0f}')


if __name__ == '__main__':
    main()
