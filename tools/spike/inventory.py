"""Vergleicht aktive Inventare: inventory.py <host-pid> <client-pid>.

Nur lesend, Build 26495302. Im ruhenden Spielzustand aufrufen: kein atomarer Snapshot.
Prueft replizierte Werte, nicht Effekte, Raritaet, UI oder Inventar-RPCs.
"""
import json
import struct
import sys

from ddmem import Game


def snapshot(pid):
    game = Game(pid)
    if not game.base:
        raise ValueError('Prozess nicht mehr lesbar')

    def read(address, size):
        data = game.read(address, size)
        if data is None or len(data) != size:
            raise ValueError('Unvollstaendiger Speicherzugriff; Snapshot wiederholen')
        return data

    def array(address):
        data, count, capacity = struct.unpack('<Qii', read(address, 16))
        if not 0 <= count <= capacity <= 4096:
            raise ValueError('Ungueltiger Array-Header; Snapshot wiederholen')
        return list(struct.unpack(f'<{count}Q', read(data, count * 8))) if count else []

    world = game.world()
    state = game.ptr(world + 0x1B0) if world else 0
    if not state:
        raise ValueError('GameState noch nicht vorhanden')
    players = set(array(state + 0x2C0))
    if not players or 0 in players:
        raise ValueError('PlayerArray noch nicht vollstaendig')

    def item(pointer):
        if not pointer:
            return None
        return {
            'data': game.name(game.ptr(pointer + 0xE0)),
            'turns': struct.unpack('<i', read(pointer + 0xD8, 4))[0],
            'pivot': struct.unpack('<ii', read(pointer + 0x28, 8)),
            'broken': read(pointer + 0x110, 1)[0],
            'in_backpack': read(pointer + 0x180, 1)[0],
            'reward': read(pointer + 0x182, 1)[0],
            'trash': read(pointer + 0x184, 1)[0],
        }

    result = {}
    for obj in game.objects():
        owner = game.ptr(obj + 0x20)
        if owner not in players or game.name(game.ptr(obj + 0x10)) != 'InventoryComponent':
            continue
        player_id = struct.unpack('<i', read(owner + 0x2AC, 4))[0]
        if player_id in result:
            raise ValueError('Mehrdeutige Inventarzuordnung')
        slots, items = array(obj + 0x100), array(obj + 0x120)
        def slot(pointer):
            if not pointer:
                return None
            occupier = struct.unpack('<Q', read(pointer + 0x30, 8))[0]
            if occupier and occupier not in items:
                raise ValueError('Slot verweist auf ein Item ausserhalb von MyItems')
            return {
                'coordinates': struct.unpack('<ii', read(pointer + 0x28, 8)),
                'occupier_index': items.index(occupier) if occupier else None,
            }
        result[player_id] = {
            'items': [item(pointer) for pointer in items],
            'slots': [slot(pointer) for pointer in slots],
        }
    if len(result) != len(players):
        raise ValueError('Nicht fuer jeden aktuellen PlayerState ein Inventar gefunden')
    return result


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    try:
        host, client = (snapshot(int(pid)) for pid in sys.argv[1:])
        complete = all(None not in value['slots'] and None not in value['items']
                       for value in client.values())
        equal = host == client
        print(json.dumps({'host': host, 'client': client, 'complete': complete, 'equal': equal}, indent=2))
        return 0 if complete and equal else 1
    except ValueError as error:
        print(f'Inventarvergleich nicht moeglich: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
