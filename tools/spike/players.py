"""Zeigt SpawnedPlayers des GameMode und die PlayerArray des GameState: players.py <host-pid>"""
import sys

from ddmem import Game

game = Game(int(sys.argv[1]))
world = game.world()
game_mode, game_state = game.ptr(world + 0x1A8), game.ptr(world + 0x1B0)
if not game_mode:
    raise SystemExit('Kein GameMode, das geht nur beim Host.')

entries, count = game.ptr(game_mode + 0x388), game.i32(game_mode + 0x390)
print('SpawnedPlayers:', count)
for i in range(count):
    controller = game.ptr(entries + i * 0x10)
    if controller >> 47:
        print(f'  {i}: frei')
    elif controller:
        print(f'  {i}: {game.name(controller)} Figur {game.name(game.ptr(controller + 0x2E8))}')
    else:
        print(f'  {i}: null')

states, state_count = game.ptr(game_state + 0x2C0), game.i32(game_state + 0x2C8)
print('PlayerArray:', state_count)
for i in range(state_count):
    state = game.ptr(states + 8 * i)
    print(f'  {game.name(state)} Figur {game.name(game.ptr(state + 0x320))} Besitzer {game.name(game.ptr(state + 0x158))}')
