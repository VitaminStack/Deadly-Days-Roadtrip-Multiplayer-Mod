"""Zeigt Welt, GameMode, GameState und Netzwerkrolle je Prozess: netstate.py <pid> [<pid> ...]"""
import sys

from ddmem import Game


def state(pid):
    game = Game(pid)
    world = game.world()
    if not world:
        return f'{pid}: keine World'
    game_mode, game_state, driver = game.ptr(world + 0x1A8), game.ptr(world + 0x1B0), game.ptr(world + 0x38)
    result = {
        'world': game.name(world),
        'package': game.name(game.ptr(world + 0x20)),
        'gamemode': game.name(game.ptr(game_mode + 0x10)) if game_mode else None,
        'gamestate': game.name(game.ptr(game_state + 0x10)) if game_state else None,
        'players': game.i32(game_state + 0x2C8) if game_state else None,
    }
    if driver:
        result['role'] = 'client' if game.ptr(driver + 0x100) else 'host'
        result['clients'] = game.i32(driver + 0x110)
    return f'{pid}: {result}'


for pid in sys.argv[1:]:
    print(state(int(pid)))
