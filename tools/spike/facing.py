"""Read-only Blickrichtung der Spielerfiguren: facing.py <pid> [<pid> ...] [--watch <s>] [--interval <s>]. Build 26495302.

Je BP_DD_Character_C: Rolle, Tempo der Bewegungskomponente, DirectionalCharacterMesh (Modus, UseHandsDirection, Hands,
Fallback-Ziel, Rohbytes des internen Zustands), Drehung von Mesh, RotationProxy und Haenden. Schreibt nichts, ruft nichts.
Offsets: docs/OFFSETS.md und SDK (Abschnitt opt-blickrichtung-client in docs/GAME_ANALYSIS.md)."""
import argparse
import math
import struct
import time

from ddmem import Game

ACTOR_ROLE, ACTOR_ROOT = 0x168, 0x1B8
CHAR_MOVEMENT, MOVE_VELOCITY = 0x330, 0xD8
CHAR_HANDS, CHAR_MESH = 0x7B0, 0x7E0  # ADDCharacter.Hands, .DirectionalMeshComponent (nativer Zeiger)
BP_MESH = 0x8F0                       # BP_DD_Character_C.DirectionalCharacterMesh (Komponente des Blueprints)
MESH_FALLBACK_FLAG, MESH_FLAGS, MESH_ANGLE = 0x6E8, 0x700, 0x720
MESH_ACTOR, MESH_HANDS, MESH_ROTPROXY, MESH_SCALEPROXY, MESH_VELPROV = 0x748, 0x750, 0x758, 0x760, 0x768
MESH_STATE, MESH_STATE_SIZE, MESH_FALLBACK = 0x770, 0x90, 0x800
HANDS_INNER, HANDS_PARENT, HANDS_TARGETS = 0x2C8, 0x320, 0x3D8
COMP_REL_ROT, COMP_WORLD_QUAT, COMP_REL_SCALE = 0x160, 0x1F0, 0x178
ROLES = {0: 'None', 1: 'Sim', 2: 'Auto', 3: 'Auth'}


def vec(game, address, count=3):
    data = game.read(address, 8 * count)
    return struct.unpack(f'<{count}d', data) if data and len(data) == 8 * count else None


def yaw_of_quat(q):
    if not q:
        return None
    x, y, z, w = q
    return math.degrees(math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)))


def fmt(values, digits=0):
    return '-' if values is None else '(' + ','.join(f'{v:.{digits}f}' for v in values) + ')'


def characters(game):
    return [o for o in game.objects() if game.name(game.ptr(o + 0x10)) == 'BP_DD_Character_C'
            and not (game.name(o) or '').startswith('Default__')]


def report(game, previous):
    lines = []
    for char in characters(game):
        role = (game.read(char + ACTOR_ROLE, 1) or b'\0')[0]
        movement = game.ptr(char + CHAR_MOVEMENT)
        velocity = vec(game, movement + MOVE_VELOCITY) if movement else None
        native = game.ptr(char + CHAR_MESH)
        mesh = native or game.ptr(char + BP_MESH)
        hands = game.ptr(char + CHAR_HANDS)
        parent = game.ptr(hands + HANDS_PARENT) if hands else 0
        line = (f'  {game.name(char)} {ROLES.get(role, role)} v={fmt(velocity)} nativeMesh={"ja" if native else "LEER"}'
                f' handsParent={"=" if parent == char else ("LEER" if not parent else hex(parent))}')
        if mesh:
            flags = game.read(mesh + MESH_FLAGS, 4) or b'\0\0\0\0'
            fallback_flag = (game.read(mesh + MESH_FALLBACK_FLAG, 1) or b'\0')[0]
            angle = struct.unpack('<f', game.read(mesh + MESH_ANGLE, 4) or b'\0\0\0\0')[0]
            rotproxy = game.ptr(mesh + MESH_ROTPROXY)
            line += (f' mode={flags[3]} useHands={flags[0]} rotp={flags[1]} scalep={flags[2]} fb0={fallback_flag} init={angle:.0f}'
                     f' meshHands={"=" if game.ptr(mesh + MESH_HANDS) == hands else hex(game.ptr(mesh + MESH_HANDS))}'
                     f' fbTarget={game.name(game.ptr(mesh + MESH_FALLBACK))}'
                     f' velProv={"ja" if game.ptr(mesh + MESH_VELPROV) else "nein"}'
                     f' meshRel={fmt(vec(game, mesh + COMP_REL_ROT))} meshYaw={yaw_of_quat(vec(game, mesh + COMP_WORLD_QUAT, 4)) or 0:.0f}'
                     f' meshScale={fmt(vec(game, mesh + COMP_REL_SCALE), 2)}')
            if rotproxy:
                line += f' proxyRel={fmt(vec(game, rotproxy + COMP_REL_ROT))}'
            scaleproxy = game.ptr(mesh + MESH_SCALEPROXY)
            if scaleproxy:
                line += f' scaleProxy={fmt(vec(game, scaleproxy + COMP_REL_SCALE), 2)}'
            state = game.read(mesh + MESH_STATE, MESH_STATE_SIZE) or b''
            key = (game.pid, char)
            if key in previous and previous[key] != state and len(state) == MESH_STATE_SIZE:
                changed = [i for i in range(0, MESH_STATE_SIZE, 4) if state[i:i + 4] != previous[key][i:i + 4]]
                line += ' state~' + ','.join(f'{MESH_STATE + i:#x}' for i in changed[:12])
            previous[key] = state
            line += ' st=' + state.hex()
        if hands:
            root = game.ptr(hands + ACTOR_ROOT)
            inner = game.ptr(hands + HANDS_INNER)
            targets = struct.unpack('<Qii', game.read(hands + HANDS_TARGETS, 16) or bytes(16))[1]
            line += (f' handsYaw={yaw_of_quat(vec(game, root + COMP_WORLD_QUAT, 4)) if root else None}'
                     f' innerRel={fmt(vec(game, inner + COMP_REL_ROT)) if inner else "-"}'
                     f' innerYaw={yaw_of_quat(vec(game, inner + COMP_WORLD_QUAT, 4)) if inner else None}'
                     f' targets={targets}')
        lines.append(line)
    return lines


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('pids', type=int, nargs='+')
    parser.add_argument('--watch', type=float, default=0)
    parser.add_argument('--interval', type=float, default=0.5)
    parser.add_argument('--raw', action='store_true', help='Rohbytes des Mesh-Zustands ausgeben')
    args = parser.parse_args()
    games = [Game(pid) for pid in args.pids]
    previous = {}
    end = time.time() + args.watch
    while True:
        stamp = time.strftime('%H:%M:%S')
        for game in games:
            print(f'== {stamp} {game.pid}')
            for line in report(game, previous):
                print(line if args.raw else line.split(' st=')[0])
        if time.time() >= end:
            break
        time.sleep(args.interval)


if __name__ == '__main__':
    main()
