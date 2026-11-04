"""Read-only current-roster health snapshot for Test-Godmode.ps1; never repairs missing players."""
import json
import struct
import sys

from ddmem import Game


def snapshot(pid):
    game = Game(pid)
    world = game.world()
    state = game.ptr(world + 0x1B0)
    data = game.ptr(state + 0x2C0)
    count = game.i32(state + 0x2C8)
    if count != 2 or not data:
        raise RuntimeError(f'{pid}: expected two current PlayerStates, got {count}')
    players = []
    for index in range(count):
        pawn = game.ptr(game.ptr(data + index * 8) + 0x320)
        attributes = game.ptr(pawn + 0x700) if pawn else 0
        raw = game.read(attributes + 0xAC, 4) if attributes else None
        if not raw or len(raw) != 4:
            raise RuntimeError(f'{pid}: missing current pawn/health at roster index {index}')
        players.append({'pawn': hex(pawn), 'health': struct.unpack('<f', raw)[0]})
    return {'pid': pid, 'world': hex(world), 'players': players}


if __name__ == '__main__':
    print(json.dumps([snapshot(int(pid)) for pid in sys.argv[1:]]))
