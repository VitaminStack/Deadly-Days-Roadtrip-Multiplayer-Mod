"""Read-only Zeitreihe der gemeinsamen Kamera und der Figuren: camera_jitter.py <pid> [<pid> ...] [--seconds N] [--csv DATEI].

Build 26786272, nicht atomar, nicht an Frames gebunden. Liest so schnell wie moeglich je Instanz den Blickort des lokalen
PlayerCameraManager (ViewTarget.POV.Location, das gezeigte Bild), ViewTarget, Arm-Laenge und je BP_DD_Character_C Rolle, Ort,
CMC-Tempo und Klasse der Unterlage (MovementBase). Gleiche aufeinanderfolgende Proben fallen weg. Meldet je Figur Rueckspruenge (Schritt gegen die letzte Richtung, ab
--back Einheiten) und Einzelschritte ueber --step Einheiten, je Blickort Bilder, groesste Luecke, groessten Schritt und Umkehr.
Der Blickort steht mitten in der Kamera-Berechnung kurz auf Zwischenwerten (0, 0, 0 oder ein wiederkehrender Wert); Nullen
fallen weg, Umkehr nur aus einem einzigen Bild darum mit Vorsicht lesen.
Schreibt nichts, ruft nichts. Offsets: docs/OFFSETS.md (Kamera, Bewegung). Befund: docs/faelle/opt-kamera-ruck.md."""
import argparse
import csv
import math
import struct
import time

from ddmem import Game

LEVEL = 0x30                  # UWorld.PersistentLevel
ROLE = 0x168                  # AActor.Role
ROOT = 0x1B8                  # AActor.RootComponent
LOCATION = 0x148              # USceneComponent.RelativeLocation
CMC = 0x330                   # ACharacter.CharacterMovement
CMC_VELOCITY = 0xD8           # UMovementComponent.Velocity
MOVEMENT_BASE = 0x350         # ACharacter.BasedMovement.MovementBase
OUTER = 0x20                  # UObject.Outer
SPRING_ARM = 0x310            # AAllPlayerFramingCamera.CameraSpringArm
ARM_LENGTH = 0x250            # USpringArmComponent.TargetArmLength
CONTROLLER_PLAYER = 0x348     # APlayerController.Player
CAMERA_MANAGER = 0x360        # APlayerController.PlayerCameraManager
VIEW_TARGET = 0x340           # APlayerCameraManager.ViewTarget.Target
POV_LOCATION = 0x350          # APlayerCameraManager.ViewTarget.POV.Location
NAN3 = (math.nan,) * 3


def vec(game, address):
    data = game.read(address, 24) if address else None
    return struct.unpack('<ddd', data) if data and len(data) == 24 else NAN3


def level_actors(game):
    world = game.world()
    level = game.ptr(world + LEVEL) if world else 0
    found = {'camera': 0, 'controller': 0, 'figures': []}
    for obj in game.objects():
        if game.i32(obj + 8) & 0x30 or game.ptr(obj + 0x20) != level:
            continue
        cls = game.name(game.ptr(obj + 0x10))
        if cls == 'BP_PlayerCameraActor_C' and not found['camera']:
            found['camera'] = obj
        elif cls == 'BP_DDPlayerController_C' and game.name(game.ptr(game.ptr(obj + CONTROLLER_PLAYER) + 0x10)) == 'LocalPlayer':
            found['controller'] = obj
        elif cls == 'BP_DD_Character_C':
            found['figures'].append(obj)
    return found


def base_name(game, obj, names):
    base = game.ptr(obj + MOVEMENT_BASE)
    if base not in names:
        owner = game.ptr(base + OUTER) if base else 0
        names[base] = game.name(game.ptr(owner + 0x10)) if owner else '-'
    return names[base]


def sample(game, actors, names):
    manager = game.ptr(actors['controller'] + CAMERA_MANAGER) if actors['controller'] else 0
    arm = game.ptr(actors['camera'] + SPRING_ARM) if actors['camera'] else 0
    arm_length = game.read(arm + ARM_LENGTH, 4) if arm else None
    row = {
        'pov': vec(game, manager + POV_LOCATION) if manager else NAN3,
        'view_target': game.ptr(manager + VIEW_TARGET) if manager else 0,
        'arm': struct.unpack('<f', arm_length)[0] if arm_length and len(arm_length) == 4 else math.nan,
        'figures': [],
    }
    for obj in actors['figures']:
        root = game.ptr(obj + ROOT)
        cmc = game.ptr(obj + CMC)
        row['figures'].append({
            'role': (game.read(obj + ROLE, 1) or b'\0')[0],
            'location': vec(game, root + LOCATION) if root else NAN3,
            'velocity': vec(game, cmc + CMC_VELOCITY) if cmc else NAN3,
            'base': base_name(game, obj, names),
        })
    return row


def key(row):
    return (row['pov'], tuple((f['location'], f['base']) for f in row['figures']))


def distinct(times, points):
    """Nur Proben, in denen sich der Punkt aendert (je Bild hoechstens eine)."""
    out = []
    for t, p in zip(times, points):
        if not out or out[-1][1] != p:
            out.append((t, p))
    return out


def steps(track):
    return [(t1, (b[0] - a[0], b[1] - a[1]), t1 - t0) for (t0, a), (t1, b) in zip(track, track[1:])]


def back_jumps(track, limit):
    """Schritte ab limit Einheiten, die gegen die Richtung des letzten Schritts laufen."""
    out, last = [], None
    for t, step, _ in steps(track):
        size = math.hypot(*step)
        if size < 1:
            continue
        if last and size >= limit and (step[0] * last[0] + step[1] * last[1]) < -0.5 * size * math.hypot(*last):
            out.append((t, size))
        last = step
    return out


def report(pid, rows, elapsed, back, big):
    if not rows:
        print(f'Instanz {pid}: keine Proben')
        return
    times = [t for t, _ in rows]
    arms = [r['arm'] for _, r in rows]
    print(f'Instanz {pid}: {len(rows)} Proben in {elapsed:.1f} s, ViewTargets {len({r["view_target"] for _, r in rows})}, '
          f'Arm {min(arms):.0f}..{max(arms):.0f}')
    view = distinct(times, [r['pov'] for _, r in rows])
    view_steps = steps(view)
    if view_steps:
        gap = max(view_steps, key=lambda s: s[2])
        size = sorted(math.hypot(*s[1]) for s in view_steps)
        print(f'  Blick: {len(view)} Bilder, groesste Luecke {gap[2] * 1000:.0f} ms bei {gap[0]:.3f} s, '
              f'Schritt median {size[len(size) // 2]:.1f} max {size[-1]:.1f}, Umkehr ab 3: '
              + (', '.join(f'{t:.3f} s ({s:.0f})' for t, s in back_jumps(view, 3)[:10]) or 'keine'))
    count = min(len(r['figures']) for _, r in rows)
    for i in range(count):
        track = distinct(times, [(r['figures'][i]['location'][0], r['figures'][i]['location'][1]) for _, r in rows])
        way = sum(math.hypot(*s[1]) for s in steps(track))
        jumps = back_jumps(track, back)
        large = [(t, math.hypot(*s), dt) for t, s, dt in steps(track) if math.hypot(*s) > big]
        bases = sorted({r['figures'][i]['base'] for _, r in rows})
        print(f'  Figur {i} Unterlagen: {", ".join(bases)}')
        print(f'  Figur {i} Rolle {rows[-1][1]["figures"][i]["role"]}: Weg {way:.0f}, Ruecksprung ab {back:.0f}: '
              + (', '.join(f'{t:.3f} s ({s:.0f})' for t, s in jumps) or 'keiner')
              + f'; Schritte ueber {big:.0f}: ' + (', '.join(f'{t:.3f} s ({s:.0f} in {dt * 1000:.0f} ms)' for t, s, dt in large) or 'keine'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('pids', type=int, nargs='+')
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--csv', help='Rohdaten als CSV')
    parser.add_argument('--back', type=float, default=30, help='Ruecksprung einer Figur ab so vielen Einheiten')
    parser.add_argument('--step', type=float, default=150, help='Einzelschritt einer Figur ab so vielen Einheiten melden')
    args = parser.parse_args()
    games = [Game(pid) for pid in args.pids]
    actors = {g.pid: level_actors(g) for g in games}
    for g in games:
        a = actors[g.pid]
        print(f'Instanz {g.pid}: Kamera {a["camera"]:#x}, Controller {a["controller"]:#x}, {len(a["figures"])} Figuren')
    rows = {g.pid: [] for g in games}
    last = {g.pid: None for g in games}
    names = {g.pid: {} for g in games}
    start = time.perf_counter()
    while time.perf_counter() - start < args.seconds:
        for g in games:
            row = sample(g, actors[g.pid], names[g.pid])
            # Waehrend der Kamera-Berechnung steht der Blickort kurz auf (0, 0, 0); solche Proben zaehlen nicht.
            if row['pov'] == (0.0, 0.0, 0.0):
                continue
            if key(row) != last[g.pid]:
                last[g.pid] = key(row)
                rows[g.pid].append((time.perf_counter() - start, row))
    elapsed = time.perf_counter() - start
    for g in games:
        report(g.pid, rows[g.pid], elapsed, args.back, args.step)
    if args.csv:
        with open(args.csv, 'w', newline='', encoding='utf-8') as handle:
            out = csv.writer(handle)
            out.writerow(['pid', 't', 'pov_x', 'pov_y', 'pov_z', 'arm', 'view_target', 'figure', 'role', 'x', 'y', 'z', 'vx', 'vy', 'base'])
            for pid, series in rows.items():
                for t, r in series:
                    for i, f in enumerate(r['figures']):
                        out.writerow([pid, f'{t:.4f}', *(f'{v:.1f}' for v in r['pov']), f'{r["arm"]:.0f}', hex(r['view_target']), i,
                                      f['role'], *(f'{v:.1f}' for v in f['location']), f'{f["velocity"][0]:.0f}', f'{f["velocity"][1]:.0f}', f['base']])


if __name__ == '__main__':
    main()
