"""Liest Runden-/Panelzustand: roundstate.py <pid> [<pid> ...]. Nur Build 26187885.

IsOpen ist der lokale Zustand des AnimationContainers, keine Zusage ueber Sichtbarkeit.
Keine Aufrufe oder Schreibzugriffe. GameInstance-Widgets koennen aeltere Karten ueberleben.
"""
import json
import sys

from ddmem import Game


def state(pid):
    game = Game(pid)
    if not game.base or not game.world():
        raise ValueError('Spielprozess/World nicht mehr lesbar')
    world = game.world()
    instance = game.ptr(world + 0x228)
    game_state = game.ptr(world + 0x1B0)
    if not instance or not game_state:
        raise ValueError('GameInstance/GameState noch nicht vorhanden')
    result = {
        'world': game.name(world),
        'route_missions': game.i32(instance + 0x2B8),
        'route_index': game.i32(instance + 0x2C0),
        'last_round_reason': (game.read(instance + 0x299, 1) or b'\xff')[0],
        'day': game.i32(game_state + 0x390),
        'seconds': game.i32(game_state + 0x38C),
        'panels': [],
    }
    for obj in game.objects():
        if game.i32(obj + 8) & 0x30:
            continue
        cls = game.ptr(obj + 0x10)
        for _ in range(32):
            if not cls:
                break
            if game.name(cls) == 'AbstractPanel':
                # Die Bindung an World oder GameInstance getrennt ausweisen.
                outer = obj
                scope = 'unresolved'
                for _ in range(32):
                    if outer in (world, instance):
                        scope = 'world' if outer == world else 'game_instance'
                        break
                    outer = game.ptr(outer + 0x20) if outer else 0
                    if not outer:
                        break
                container = game.ptr(obj + 0x330)
                raw = game.read(container + 0x328, 1) if container else None
                result['panels'].append({'class': game.name(game.ptr(obj + 0x10)),
                                        'scope': scope, 'open': bool(raw[0]) if raw else None})
                break
            cls = game.ptr(cls + 0x40)
    return result


if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for pid in sys.argv[1:]:
        try:
            print(json.dumps(state(int(pid)), ensure_ascii=False))
        except ValueError as error:
            print(f'Rundenmessung nicht moeglich: {error}', file=sys.stderr)
            sys.exit(2)
