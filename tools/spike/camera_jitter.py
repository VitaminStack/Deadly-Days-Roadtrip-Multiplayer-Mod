"""Read-only Zeitreihe der gemeinsamen Kamera und der Figuren: camera_jitter.py <pid> [<pid> ...] [--seconds N] [--csv DATEI].

Build 26786272, nicht atomar, nicht an Frames gebunden. Liest so schnell wie moeglich je Instanz: Blickort des lokalen
PlayerCameraManager (ViewTarget.POV.Location, das gezeigte Bild), ViewTarget, Ort des BP_PlayerCameraActor_C, Arm-Laenge und
je BP_DD_Character_C Rolle, Ort und CMC-Tempo. Gleiche aufeinanderfolgende Proben fallen weg. Meldet Spruenge: Schritte weit
ueber dem laufenden Tempo und Richtungsumkehr des Blickorts, dazu Spruenge der Figuren im selben Zeitfenster.
Schreibt nichts, ruft nichts. Offsets: docs/OFFSETS.md (Kamera, Bewegung)."""
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


def sample(game, actors):
    manager = game.ptr(actors['controller'] + CAMERA_MANAGER) if actors['controller'] else 0
    arm = game.ptr(actors['camera'] + SPRING_ARM) if actors['camera'] else 0
    arm_length = game.read(arm + ARM_LENGTH, 4) if arm else None
    row = {
        'pov': vec(game, manager + POV_LOCATION) if manager else NAN3,
        'view_target': game.ptr(manager + VIEW_TARGET) if manager else 0,
        'camera': vec(game, game.ptr(actors['camera'] + ROOT)) if actors['camera'] else NAN3,
        'arm': struct.unpack('<f', arm_length)[0] if arm_length and len(arm_length) == 4 else math.nan,
        'figures': [],
    }
    for obj in actors['figures']:
        root = game.ptr(obj + ROOT)
        row['figures'].append({
            'role': (game.read(obj + ROLE, 1) or b'\0')[0],
            'location': vec(game, root + LOCATION) if root else NAN3,
            'velocity': vec(game, game.ptr(obj + CMC) + CMC_VELOCITY) if game.ptr(obj + CMC) else NAN3,
        })
    return row


def key(row):
    return (row['pov'], row['camera'], tuple(f['location'] for f in row['figures']))


def planar(a, b):
    return (b[0] - a[0], b[1] - a[1])


def length(v):
    return math.hypot(v[0], v[1])


def jumps(times, points, factor, floor):
    """Schritte, deren Tempo das Mittel der 200 ms davor um factor uebersteigt (mindestens floor), und Richtungsumkehr."""
    events = []
    steps = [(times[i], planar(points[i - 1], points[i]), times[i] - times[i - 1]) for i in range(1, len(points))]
    for i, (t, step, dt) in enumerate(steps):
        if dt <= 0 or any(math.isnan(x) for x in step):
            continue
        speed = length(step) / dt
        window = [length(s) / d for (tt, s, d) in steps[max(0, i - 200):i] if t - tt <= 0.2 and d > 0]
        usual = sum(window) / len(window) if window else 0
        if speed > floor and speed > factor * max(usual, 1):
            events.append((t, f'Sprung {length(step):.0f} in {dt * 1000:.0f} ms ({speed:.0f}/s, vorher {usual:.0f}/s)'))
        if i:
            prev = steps[i - 1][1]
            if length(prev) > 2 and length(step) > 2 and prev[0] * step[0] + prev[1] * step[1] < -0.5 * length(prev) * length(step):
                events.append((t, f'Umkehr {length(prev):.0f} dann {length(step):.0f}'))
    return events


def report(pid, rows, elapsed, factor, floor):
    if not rows:
        print(f'Instanz {pid}: keine Proben')
        return
    times = [t for t, _ in rows]
    print(f'Instanz {pid}: {len(rows)} verschiedene Proben in {elapsed:.1f} s, '
          f'ViewTargets {len({r["view_target"] for _, r in rows})}, Arm {min(r["arm"] for _, r in rows):.0f}..{max(r["arm"] for _, r in rows):.0f}')
    tracks = {'Blick': [r['pov'] for _, r in rows], 'Kamera-Actor': [r['camera'] for _, r in rows]}
    count = min(len(r['figures']) for _, r in rows)
    for i in range(count):
        tracks[f'Figur {i} Rolle {rows[-1][1]["figures"][i]["role"]}'] = [r['figures'][i]['location'] for _, r in rows]
    found = {name: jumps(times, points, factor, floor) for name, points in tracks.items()}
    for name, events in found.items():
        print(f'  {name}: {len(events)} Ereignisse')
    for t, text in found['Blick']:
        near = [f'{name} {e}' for name, events in found.items() if name.startswith('Figur') for tt, e in events if abs(tt - t) <= 0.1]
        print(f'    {t:7.3f} s Blick {text}' + (f'; Figuren: {"; ".join(near[:3])}' if near else '; keine Figur springt'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('pids', type=int, nargs='+')
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--csv', help='Rohdaten als CSV')
    parser.add_argument('--factor', type=float, default=3, help='Sprung, wenn Schritt-Tempo das Mittel davor so oft uebersteigt')
    parser.add_argument('--floor', type=float, default=1500, help='Sprung erst ab diesem Schritt-Tempo (Einheiten/s)')
    args = parser.parse_args()
    games = [Game(pid) for pid in args.pids]
    actors = {g.pid: level_actors(g) for g in games}
    for g in games:
        a = actors[g.pid]
        print(f'Instanz {g.pid}: Kamera {a["camera"]:#x}, Controller {a["controller"]:#x}, {len(a["figures"])} Figuren')
    rows = {g.pid: [] for g in games}
    last = {g.pid: None for g in games}
    start = time.perf_counter()
    while time.perf_counter() - start < args.seconds:
        for g in games:
            row = sample(g, actors[g.pid])
            # Waehrend der Kamera-Berechnung steht der Blickort kurz auf (0, 0, 0); solche Proben zaehlen nicht.
            if row['pov'] == (0.0, 0.0, 0.0):
                continue
            if key(row) != last[g.pid]:
                last[g.pid] = key(row)
                rows[g.pid].append((time.perf_counter() - start, row))
    elapsed = time.perf_counter() - start
    for g in games:
        report(g.pid, rows[g.pid], elapsed, args.factor, args.floor)
    if args.csv:
        with open(args.csv, 'w', newline='', encoding='utf-8') as handle:
            out = csv.writer(handle)
            out.writerow(['pid', 't', 'pov_x', 'pov_y', 'pov_z', 'cam_x', 'cam_y', 'arm', 'view_target', 'figure', 'role', 'x', 'y', 'z', 'vx', 'vy'])
            for pid, series in rows.items():
                for t, r in series:
                    for i, f in enumerate(r['figures']):
                        out.writerow([pid, f'{t:.4f}', *(f'{v:.1f}' for v in r['pov']), f'{r["camera"][0]:.1f}', f'{r["camera"][1]:.1f}',
                                      f'{r["arm"]:.0f}', hex(r['view_target']), i, f['role'], *(f'{v:.1f}' for v in f['location']),
                                      f'{f["velocity"][0]:.0f}', f'{f["velocity"][1]:.0f}'])


if __name__ == '__main__':
    main()
