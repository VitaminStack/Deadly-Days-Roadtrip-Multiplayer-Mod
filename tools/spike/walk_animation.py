"""Read-only Laufanimation der Spielerfiguren: walk_animation.py <pid> [<pid> ...] [--seconds N] [--every MS].

Build 26495302, nicht atomar. Liest je BP_DD_Character der aktuellen Welt Rolle, CMC-Tempo, das Wackeln
(UWobbleAnimationComponent, ADDCharacter+0x780) und den Zustand des UPlayerAnimationVelocityProvider (BP_DD_Character_C+0x910) als Rohbytes.
Schreibt nichts, ruft nichts. Ergebnis je Figur: Tempo, Fortschritt des Wackelns (bewegt er sich?), Override-Werte,
und welche Bytes von Provider und Wobble-Zustand sich im Messfenster geaendert haben.
Offsets: docs/GAME_ANALYSIS.md#laufanimation-beim-client-opt-laufanimation-client."""
import argparse
import math
import struct
import time

from ddmem import Game

ROLE = 0x168              # AActor.Role (1 Simulated, 2 Autonomous, 3 Authority)
CMC = 0x330               # ACharacter.CharacterMovement
CMC_VELOCITY = 0xD8       # UMovementComponent.Velocity (FVector, double)
ROOT = 0x1B8
LOCATION = 0x148          # USceneComponent.RelativeLocation
PROVIDER = 0x910          # BP_DD_Character_C.PlayerAnimationVelocityProvider
WOBBLE = 0x780            # ADDCharacter.WobbleAnimation
PROVIDER_STATE = (0xC0, 0x38)  # bool manuell +0xC0, gemessenes Tempo +0xC8 (FVector), letzter Ort +0xE0 (FVector)
WOBBLE_FIELDS = 0x258     # Progress, MaxInputVelocity, VelocityToProgressFactor, MaxSpeed, OverrideVelocity, OverrideFactor
WOBBLE_STATE = (0x290, 0x68)
WOBBLE_CAN_STRETCH = 0x308
WOBBLE_PROVIDER = 0x288


def figures(game):
    world = game.world()
    level = game.ptr(world + 0x30) if world else 0
    result = []
    for obj in game.objects():
        if game.i32(obj + 8) & 0x30 or game.ptr(obj + 0x20) != level:
            continue
        if game.name(game.ptr(obj + 0x10)) == 'BP_DD_Character_C':
            result.append(obj)
    return result


def vec(game, address):
    data = game.read(address, 24)
    return struct.unpack('<ddd', data) if data and len(data) == 24 else (math.nan,) * 3


def sample(game, obj):
    cmc = game.ptr(obj + CMC)
    root = game.ptr(obj + ROOT)
    provider = game.ptr(obj + PROVIDER)
    wobble = game.ptr(obj + WOBBLE)
    fields = game.read(wobble + WOBBLE_FIELDS, 24) if wobble else None
    return {
        'role': (game.read(obj + ROLE, 1) or b'\0')[0],
        'velocity': vec(game, cmc + CMC_VELOCITY) if cmc else (math.nan,) * 3,
        'location': vec(game, root + LOCATION) if root else (math.nan,) * 3,
        'provider': provider,
        'provider_state': game.read(provider + PROVIDER_STATE[0], PROVIDER_STATE[1]) if provider else None,
        'wobble': wobble,
        'wobble_fields': struct.unpack('<6f', fields) if fields and len(fields) == 24 else None,
        'wobble_state': game.read(wobble + WOBBLE_STATE[0], WOBBLE_STATE[1]) if wobble else None,
        'can_stretch': (game.read(wobble + WOBBLE_CAN_STRETCH, 1) or b'\xff')[0] if wobble else None,
        'wobble_provider_same': bool(wobble) and game.ptr(wobble + WOBBLE_PROVIDER) == provider,
    }


def changed(blobs, base):
    """Offsets (relativ zur Klasse) von 4-Byte-Woertern, die sich in den Proben geaendert haben, mit erstem/letztem Wert."""
    blobs = [b for b in blobs if b]
    if len(blobs) < 2:
        return []
    out = []
    for off in range(0, min(len(b) for b in blobs) - 3, 4):
        words = {b[off:off + 4] for b in blobs}
        if len(words) > 1:
            first, last = blobs[0][off:off + 4], blobs[-1][off:off + 4]
            out.append(f'+{base + off:#x}: {struct.unpack("<f", first)[0]:.3g} -> {struct.unpack("<f", last)[0]:.3g} ({len(words)} Werte)')
    return out


def speed(v):
    return math.hypot(v[0], v[1])


def report(pid, samples, elapsed):
    count = max((len(rows) for rows in samples.values()), default=0)
    print(f'Instanz {pid}: {len(samples)} Figuren, je {count} Proben in {elapsed:.1f} s')
    for obj, rows in samples.items():
        role = rows[-1]['role']
        cmc_speeds = [speed(r['velocity']) for r in rows]
        moved = [math.hypot(b['location'][0] - a['location'][0], b['location'][1] - a['location'][1]) for a, b in zip(rows, rows[1:])]
        progress = [r['wobble_fields'][0] for r in rows if r['wobble_fields']]
        last = rows[-1]['wobble_fields']
        print(f'  Figur {obj:#x} Rolle {role}: CMC-Tempo max {max(cmc_speeds):.0f} mittel {sum(cmc_speeds) / len(cmc_speeds):.0f}, '
              f'Weg gesamt {sum(moved):.0f}, netto {math.hypot(rows[-1]["location"][0] - rows[0]["location"][0], rows[-1]["location"][1] - rows[0]["location"][1]):.0f}, '
              f'Schritte ohne Ortsaenderung {sum(1 for m in moved if m == 0)}/{len(moved)}')
        if last:
            print(f'    Wobble: Progress {min(progress):.3f}..{max(progress):.3f} ({len(set(progress))} Werte), MaxInputVelocity {last[1]:.0f}, '
                  f'Factor {last[2]:.3g}, MaxSpeed {last[3]:.0f}, OverrideVelocity {last[4]:.3g}, OverrideFactor {last[5]:.3g}, '
                  f'CanStretch {rows[-1]["can_stretch"]}, Provider gleich {rows[-1]["wobble_provider_same"]}')
        manual = [r['provider_state'][0] for r in rows if r['provider_state']]
        measured = [speed(struct.unpack_from('<ddd', r['provider_state'], 8)) for r in rows if r['provider_state']]
        if manual:
            print(f'    Provider: manuell {min(manual)}..{max(manual)}, gemessenes Tempo max {max(measured):.0f} '
                  f'mittel {sum(measured) / len(measured):.0f}')
        for line in changed([r['wobble_state'] for r in rows], WOBBLE_STATE[0]):
            print(f'    Wobble   {line}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('pids', type=int, nargs='+')
    parser.add_argument('--seconds', type=float, default=5)
    parser.add_argument('--every', type=int, default=100, help='Abstand der Proben in ms')
    args = parser.parse_args()
    games = [Game(pid) for pid in args.pids]
    figs = {game.pid: figures(game) for game in games}
    samples = {game.pid: {obj: [] for obj in figs[game.pid]} for game in games}
    start = time.monotonic()
    while time.monotonic() - start < args.seconds:
        for game in games:
            for obj in figs[game.pid]:
                samples[game.pid][obj].append(sample(game, obj))
        time.sleep(args.every / 1000)
    elapsed = time.monotonic() - start
    for game in games:
        report(game.pid, samples[game.pid], elapsed)


if __name__ == '__main__':
    main()
