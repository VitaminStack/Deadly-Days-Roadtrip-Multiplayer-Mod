"""Read-only Lauftempo je Spielerfigur: movement_speed.py <pid> [<pid> ...] [--watch <s>] [--interval <s>]. Build 26786272.

Liest je BP_DD_Character_C der aktuellen Welt Rolle, das replizierte Attribut UCommonAttributeSet.MovementSpeed (Basis und
aktuell) und die Werte der Bewegungskomponente, die das Spiel daraus setzt: MaxWalkSpeed und MaxAcceleration
(OnMovementSpeedChanged_Server, Beschleunigung = Tempo x 3,5), dazu das gemessene Tempo. Schreibt nichts, ruft nichts,
nicht atomar. Mit --watch wird je Intervall nur bei einer Änderung eine Zeile gedruckt.
Offsets: docs/OFFSETS.md, Abschnitt Bewegung."""
import argparse
import math
import struct
import time

from ddmem import Game

ROLE = 0x168                 # AActor.Role (1 Simulated, 2 Autonomous, 3 Authority)
CMC = 0x330                  # ACharacter.CharacterMovement
CMC_VELOCITY = 0xD8          # UMovementComponent.Velocity (FVector, double)
MAX_WALK_SPEED = 0x288       # UCharacterMovementComponent.MaxWalkSpeed
MAX_ACCELERATION = 0x29C     # UCharacterMovementComponent.MaxAcceleration
ATTRIBUTES = 0x700           # ADDCharacterBase.CommonAttributeSet
MOVEMENT_SPEED = 0xE8        # UCommonAttributeSet.MovementSpeed, FGameplayAttributeData: Basis +0x08, aktuell +0x0C
ROLES = {1: 'fremd', 2: 'eigen (Client)', 3: 'Host'}


def figures(game):
    world = game.world()
    level = game.ptr(world + 0x30) if world else 0
    return [obj for obj in game.objects()
            if not game.i32(obj + 8) & 0x30 and game.ptr(obj + 0x20) == level
            and game.name(game.ptr(obj + 0x10)) == 'BP_DD_Character_C']


def floats(game, address, count):
    data = game.read(address, 4 * count) if address else None
    return struct.unpack(f'<{count}f', data) if data and len(data) == 4 * count else (math.nan,) * count


def sample(game, obj):
    cmc = game.ptr(obj + CMC)
    attributes = game.ptr(obj + ATTRIBUTES)
    velocity = game.read(cmc + CMC_VELOCITY, 24) if cmc else None
    vx, vy, _ = struct.unpack('<ddd', velocity) if velocity and len(velocity) == 24 else (math.nan,) * 3
    base, current = floats(game, attributes + MOVEMENT_SPEED + 8 if attributes else 0, 2)
    return {
        'role': (game.read(obj + ROLE, 1) or b'\0')[0],
        'base': base,
        'current': current,
        'walk': floats(game, cmc + MAX_WALK_SPEED if cmc else 0, 1)[0],
        'accel': floats(game, cmc + MAX_ACCELERATION if cmc else 0, 1)[0],
        'speed': math.hypot(vx, vy),
    }


def line(pid, obj, row):
    expected = row['current'] * 3.5
    ok = abs(row['walk'] - row['current']) < 0.5 and abs(row['accel'] - expected) < 0.5
    return (f'{pid} Figur {obj:#x} {ROLES.get(row["role"], row["role"])}: Attribut {row["current"]:.1f} (Basis {row["base"]:.1f}), '
            f'MaxWalkSpeed {row["walk"]:.1f}, MaxAcceleration {row["accel"]:.1f} (erwartet {expected:.1f}), '
            f'Tempo {row["speed"]:.0f}, {"passt" if ok else "ABWEICHUNG"}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('pids', type=int, nargs='+')
    parser.add_argument('--watch', type=float, default=0, help='Sekunden beobachten, Zeile nur bei Änderung')
    parser.add_argument('--interval', type=float, default=0.25)
    args = parser.parse_args()
    games = [Game(pid) for pid in args.pids]
    figs = {game.pid: figures(game) for game in games}
    last = {}
    start = time.monotonic()
    while True:
        for game in games:
            for obj in figs[game.pid]:
                row = sample(game, obj)
                key = tuple(round(row[k], 1) for k in ('role', 'base', 'current', 'walk', 'accel'))
                if last.get((game.pid, obj)) != key:
                    last[(game.pid, obj)] = key
                    stamp = f'[{time.monotonic() - start:6.2f}s] ' if args.watch else ''
                    print(stamp + line(game.pid, obj, row), flush=True)
        if time.monotonic() - start >= args.watch:
            break
        time.sleep(args.interval)


if __name__ == '__main__':
    main()
