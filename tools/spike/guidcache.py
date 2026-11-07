"""Zeigt den NetGUID-Cache einer Instanz mit vollem Pfad: guidcache.py <pid> [<filter>]
Statische GUIDs sind ungerade. Flags: 1 NoLoad, 2 IgnoreWhenMissing, 4 Pending, 8 Broken. Layout in docs/OFFSETS.md."""
import struct
import sys

from ddmem import Game

GOBJECTS = 0x0AA61B00
DRIVER_GUID_CACHE = 0x1C8
ENTRY_SIZE = 0x40


def entries(game):
    cache = game.ptr(game.ptr(game.world() + 0x38) + DRIVER_GUID_CACHE)
    data, num, maximum = struct.unpack('<Qii', game.read(cache + 8, 16))
    slots = min(num, maximum)  # Allocation capacity is not an initialized SparseArray slot range.
    raw = game.read(data, slots * ENTRY_SIZE) or b''
    bits = game.read(game.ptr(cache + 0x28) or (cache + 0x18), (slots + 7) // 8) or b''
    chunks = game.ptr(game.base + getattr(game, 'gobjects', GOBJECTS))
    rows = {}
    for slot in range(slots):
        # Der Cache aendert sich waehrend des Lesens, ein abgeschnittener Block zaehlt als Ende.
        if slot >> 3 >= len(bits) or (slot + 1) * ENTRY_SIZE > len(raw):
            break
        if not (bits[slot >> 3] >> (slot & 7)) & 1:
            continue
        entry = raw[slot * ENTRY_SIZE:(slot + 1) * ENTRY_SIZE]
        key, index, serial, outer, path = struct.unpack_from('<QiiQi', entry)
        live = '-'
        if index > 0:
            item = game.ptr(chunks + 8 * (index >> 16)) + (index & 0xFFFF) * 0x18
            live = 'ok' if game.ptr(item + 8) and game.i32(item + 0x10) == serial else 'stale'
        stamp = struct.unpack_from('<d', entry, 0x28)[0]
        rows[key] = (index, live, outer, game.name_from_index(path) if path else '', entry[0x30], stamp)
    return num, rows


def full_path(rows, key, depth=0):
    row = rows.get(key)
    if not row or depth > 8:
        return '?'
    return (full_path(rows, row[2], depth + 1) + '.' if row[2] else '') + row[3]


if __name__ == '__main__':
    game = Game(int(sys.argv[1]))
    pattern = sys.argv[2] if len(sys.argv) > 2 else ''
    num, rows = entries(game)
    print('Eintraege', len(rows), 'Slots', num, 'statisch', sum(1 for key in rows if key & 1))
    for key, (index, live, outer, name, flags, stamp) in sorted(rows.items()):
        path = full_path(rows, key)
        if pattern in path:
            print(f'{key:#x} {path} obj={live}:{index} flags={flags:#x} stempel={stamp:.0f}')
