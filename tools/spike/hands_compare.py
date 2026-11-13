"""Read-only Zeitreihe der Waffen-Darstellung auf Host und Client: hands_compare.py <host-pid> <client-pid> [--seconds N] [--every MS].

Build 26495302, nicht atomar (opt-schuss-animation). Paart die Figuren beider Instanzen ueber ihre Waffenklassen und liest je Probe
Ort, Tempo und Wackeln der Figur, Drehung und Blickwinkel der Haende und je Waffe die relative Lage ihrer Komponente, den Weltort
des Waffen-Actors und dessen Abstand zur Komponente (Rueckstoss, Stich). Gibt je Figur Spannen beider Seiten und die Abweichung aus.
Schreibt nichts, ruft nichts. Offsets: docs/OFFSETS.md#schuss-animation-opt-schuss-animation.
"""
import argparse
import math
import statistics
import struct
import time

from ddmem import Game
import hands as H

CMC, CMC_VELOCITY, WOBBLE, WOBBLE_PROGRESS = 0x330, 0xD8, 0x780, 0x258


def vec(game, address):
    data = game.read(address, 24) if address else None
    return struct.unpack('<3d', data) if data and len(data) == 24 else None


def quat_yaw(game, component):
    data = game.read(component + H.WORLD_ROTATION, 32) if component else None
    if not data or len(data) != 32:
        return None
    x, y, z, w = struct.unpack('<4d', data)
    return math.degrees(math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)))


def figures(game):
    """Je Figur die festen Zeiger; die Waffen ueber die Kinder des Containers (beim Client ist die Liste der Haende leer)."""
    world = game.world()
    level = game.ptr(world + 0x30) if world else 0
    result = []
    for obj in game.objects():
        if game.i32(obj + H.FLAGS) & 0x30 or game.ptr(obj + H.OUTER) != level or game.name(game.ptr(obj + H.CLASS)) != 'BP_DD_Character_C':
            continue
        hands = game.ptr(obj + H.CHARACTER_HANDS)
        inner = game.ptr(hands + H.HANDS_INNER) if hands else 0
        weapons = []
        for child in (H.array(game, inner + H.ATTACH_CHILDREN) or []) if inner else []:
            if game.name(game.ptr(child + H.CLASS)) != 'ChildActorComponent':
                continue
            actor = game.ptr(child + H.CHILD_ACTOR)
            weapons.append((game.name(game.ptr(actor + H.CLASS)) if actor else '?', child, actor))
        result.append({'figure': obj, 'role': (game.read(obj + H.ROLE, 1) or b'\0')[0], 'hands': hands, 'inner': inner,
                       'weapons': sorted(weapons), 'key': tuple(sorted(name for name, _, _ in weapons))})
    return result


def sample(game, entry):
    figure, hands = entry['figure'], entry['hands']
    cmc, wobble = game.ptr(figure + CMC), game.ptr(figure + WOBBLE)
    velocity = vec(game, cmc + CMC_VELOCITY) if cmc else None
    progress = game.read(wobble + WOBBLE_PROGRESS, 4) if wobble else None
    angle = game.read(hands + H.HANDS_ANGLE, 4) if hands else None
    row = {'location': vec(game, game.ptr(figure + H.ROOT) + H.REL_LOCATION),
           'speed': math.hypot(velocity[0], velocity[1]) if velocity else None,
           'progress': struct.unpack('<f', progress)[0] if progress and len(progress) == 4 else None,
           'hands_yaw': quat_yaw(game, game.ptr(hands + H.ROOT)) if hands else None,
           'angle': struct.unpack('<f', angle)[0] if angle and len(angle) == 4 else None,
           'auto_fire': (game.read(hands + H.HANDS_AUTOFIRE, 1) or b'\0')[0] if hands else None, 'weapons': []}
    for name, child, actor in entry['weapons']:
        comp_world = vec(game, child + H.WORLD_LOCATION)
        actor_root = game.ptr(actor + H.ROOT) if actor else 0
        actor_world = vec(game, actor_root + H.WORLD_LOCATION) if actor_root else None
        rotation = vec(game, child + H.REL_ROTATION)
        row['weapons'].append({'name': name, 'rel': vec(game, child + H.REL_LOCATION), 'rel_yaw': rotation[1] if rotation else None,
                               'world': actor_world, 'yaw': quat_yaw(game, actor_root),
                               'kick': math.dist(comp_world, actor_world) if comp_world and actor_world else None})
    return row


def angle_gap(a, b):
    return abs((a - b + 180) % 360 - 180)


def span(values):
    values = [value for value in values if value is not None]
    return 'keine Werte' if not values else f'{min(values):.1f} bis {max(values):.1f}'


def gap(values):
    values = sorted(value for value in values if value is not None)
    return 'keine Werte' if not values else f'Median {statistics.median(values):.1f}, 90 % {values[int(len(values) * 0.9) - 1]:.1f}, max {values[-1]:.1f}'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host', type=int)
    parser.add_argument('client', type=int)
    parser.add_argument('--seconds', type=float, default=5)
    parser.add_argument('--every', type=int, default=50)
    args = parser.parse_args()
    host, client = Game(args.host), Game(args.client)
    host_figures, client_figures = figures(host), figures(client)
    pairs = []
    for entry in host_figures:
        partner = [other for other in client_figures if other['key'] == entry['key']]
        if len(partner) != 1 or sum(other['key'] == entry['key'] for other in host_figures) != 1:
            raise SystemExit(f"Figuren nicht eindeutig ueber ihre Waffen zu paaren: Host {[e['key'] for e in host_figures]}, Client {[e['key'] for e in client_figures]}")
        pairs.append((entry, partner[0], []))
    end = time.time() + args.seconds
    while time.time() < end:
        for host_entry, client_entry, rows in pairs:
            rows.append((sample(host, host_entry), sample(client, client_entry)))
        time.sleep(args.every / 1000)
    for host_entry, client_entry, rows in pairs:
        who = 'Client-Figur' if client_entry['role'] == 2 else 'Host-Figur'
        print(f"== {who} ({', '.join(host_entry['key'])}), {len(rows)} Proben")
        for label, index in (('Host  ', 0), ('Client', 1)):
            side = [row[index] for row in rows]
            print(f"  {label}: Tempo {span(s['speed'] for s in side)}, Wackeln {span(s['progress'] for s in side)}, "
                  f"Haende-Gier {span(s['hands_yaw'] for s in side)}, Winkel {span(s['angle'] for s in side)}, "
                  f"AutoFire an in {sum(bool(s['auto_fire']) for s in side)} Proben")
        both = [(h, c) for h, c in rows if h['location'] and c['location']]
        print(f"  Abstand der Figur Host/Client: {gap(math.dist(h['location'], c['location']) for h, c in both)}")
        print(f"  Abstand Haende-Gier: {gap(angle_gap(h['hands_yaw'], c['hands_yaw']) for h, c in rows if h['hands_yaw'] is not None and c['hands_yaw'] is not None)}")
        for slot, (name, _, _) in enumerate(host_entry['weapons']):
            host_side = [h['weapons'][slot] for h, _ in rows]
            client_side = [c['weapons'][slot] for _, c in rows]
            print(f"  {name}:")
            for label, side in (('Host  ', host_side), ('Client', client_side)):
                print(f"    {label}: rel X {span(w['rel'][0] for w in side if w['rel'])}, Y {span(w['rel'][1] for w in side if w['rel'])}, "
                      f"Z {span(w['rel'][2] for w in side if w['rel'])}, rel Gier {span(w['rel_yaw'] for w in side)}, "
                      f"Abstand Actor/Komponente {span(w['kick'] for w in side)}")
            print(f"    Abweichung rel Lage: {gap(math.dist(h['rel'], c['rel']) for h, c in zip(host_side, client_side) if h['rel'] and c['rel'])}; "
                  f"Weltort: {gap(math.dist(h['world'], c['world']) for h, c in zip(host_side, client_side) if h['world'] and c['world'])}; "
                  f"Welt-Gier: {gap(angle_gap(h['yaw'], c['yaw']) for h, c in zip(host_side, client_side) if h['yaw'] is not None and c['yaw'] is not None)}")
