"""Ausgerüstete Waffen gegen den Rucksack: weapons.py <pid> [<pid> ...].

Nur lesend, Build 26495302 (waffe-client-auswahl). Je Spieler (PlayerId) die Items im Rucksack (Waffe, wenn es zu ItemData_<X> eine WeaponData Data_<X> gibt) und
die Einträge aus ADDCharacter.EquippedWeapons (+0x7B8) mit WeaponData und zugehörigem Item. EquippedWeapons repliziert
nicht, jede Instanz hat ihren eigenen Stand; maßgeblich für Schaden ist der Host.
Exitcode 0: Auf jeder Instanz ist genau jedes Waffen-Item im Rucksack ausgerüstet. 1: Abweichung. 2: nicht lesbar.
"""
import json
import struct
import sys

from ddmem import Game

GAMESTATE, PLAYER_ARRAY, PLAYER_ID = 0x1B0, 0x2C0, 0x2AC
PAWN_PLAYER_STATE, EQUIPPED_WEAPONS = 0x2C8, 0x7B8
INVENTORY_ITEMS, ITEM_DATA, ITEM_IN_BACKPACK = 0x120, 0xE0, 0x180
WEAPON_ITEM, WEAPON_DATA = 0x38, 0x40
INTERNAL_INDEX, CLASS = 0x0C, 0x10


def snapshot(pid):
    game = Game(pid)
    if not game.base:
        raise ValueError('Prozess nicht mehr lesbar')

    def array(address):
        data = game.read(address, 16)
        if not data or len(data) != 16:
            raise ValueError('Array nicht lesbar')
        pointer, count, capacity = struct.unpack('<Qii', data)
        if not 0 <= count <= capacity <= 4096:
            raise ValueError('Ungueltiger Array-Header; wiederholen')
        raw = game.read(pointer, count * 8) if count else b''
        if count and (not raw or len(raw) != count * 8):
            raise ValueError('Array-Inhalt nicht lesbar')
        return list(struct.unpack(f'<{count}Q', raw)) if count else []

    state = game.ptr(game.world() + GAMESTATE) if game.world() else 0
    if not state:
        raise ValueError('GameState noch nicht vorhanden')
    players = set(array(state + PLAYER_ARRAY))
    inventories, characters, weapon_names = {}, {}, set()
    for obj in game.objects():
        cls = game.name(game.ptr(obj + CLASS)) or ''
        name = game.name(obj) or ''
        if name.startswith('Default__'):
            continue
        if 'WeaponData' in cls and name.startswith('Data_'):
            weapon_names.add(name[len('Data_'):])
        if cls == 'InventoryComponent' and game.ptr(obj + 0x20) in players:
            inventories[game.i32(game.ptr(obj + 0x20) + PLAYER_ID)] = obj
        elif 'Character' in cls and game.ptr(obj + PAWN_PLAYER_STATE) in players:
            characters[game.i32(game.ptr(obj + PAWN_PLAYER_STATE) + PLAYER_ID)] = obj

    result = {}
    for player, inventory in sorted(inventories.items()):
        items = array(inventory + INVENTORY_ITEMS)
        by_index = {game.i32(item + INTERNAL_INDEX): position for position, item in enumerate(items)}
        backpack = []
        for position, item in enumerate(items):
            data = game.ptr(item + ITEM_DATA)
            in_backpack = (game.read(item + ITEM_IN_BACKPACK, 1) or b'\0')[0]
            data_name = game.name(data) or ''
            backpack.append({'index': position, 'data': data_name, 'in_backpack': bool(in_backpack),
                             'weapon': data_name.startswith('ItemData_') and data_name[len('ItemData_'):] in weapon_names})
        equipped = []
        character = characters.get(player)
        for weapon in array(character + EQUIPPED_WEAPONS) if character else []:
            equipped.append({'weapon': game.name(game.ptr(weapon + WEAPON_DATA)),
                             'item': by_index.get(game.i32(weapon + WEAPON_ITEM))})
        weapons_in_backpack = sorted(entry['index'] for entry in backpack
                                     if entry['in_backpack'] and entry['weapon'])
        equipped_items = sorted(entry['item'] for entry in equipped if entry['item'] is not None)
        result[player] = {
            'character': bool(character),
            'items': backpack,
            'equipped': equipped,
            'match': character is not None and weapons_in_backpack == equipped_items and len(equipped) == len(equipped_items),
        }
    if not result:
        raise ValueError('Keine Inventare gefunden')
    return result


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    try:
        report = {pid: snapshot(int(pid)) for pid in sys.argv[1:]}
    except ValueError as error:
        print(f'Waffenvergleich nicht moeglich: {error}', file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if all(player['match'] for value in report.values() for player in value.values()) else 1


if __name__ == '__main__':
    sys.exit(main())
