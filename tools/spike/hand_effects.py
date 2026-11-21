"""Read-only Haende und Rucksackwirkungen je Spieler: hand_effects.py <pid> [<pid> ...] [--items] [--figure] [--json].

Build 26786272, nicht atomar (client-roboterarm). Je PlayerState der aktuellen Welt: ExtraHands und OccupiedHands
(UDDCharacterAttributeSet, Basis und aktueller Wert), die aktiven GameplayEffects seines AbilitySystems (Klassenname der
Definition) und je Figur die Eintraege des UCharacterEffectManager (EffectedSpecifications, EffectedBackpackSpecifications)
mit Item, Wirkungsart und Klasse (nur Eintraege zu Haenden und zum Roboterarm). Mit --items zusaetzlich alle geladenen Item-Daten, deren Wirkungen Haende betreffen
(Klassenname enthaelt Hand), mit --figure die Komponenten und angehaengten Actors jeder Figur. Schreibt nichts, ruft nichts.
Offsets: docs/OFFSETS.md#roboterarm-client-roboterarm.
"""
import argparse
import json
import struct

from ddmem import Game

FLAGS, CLASS, OUTER, SUPER = 0x08, 0x10, 0x20, 0x40
ROLE, ROOT = 0x168, 0x1B8
ATTACH_CHILDREN = 0xE8
PLAYER_ID, PLAYER_PAWN = 0x2AC, 0x320
EXTRA_HANDS, OCCUPIED_HANDS = 0x100, 0x110  # FGameplayAttributeData: BaseValue +0x8, CurrentValue +0xC
ASC_ACTIVE_EFFECTS = 0x8D8 + 0x130          # ActiveGameplayEffects.GameplayEffects_Internal
ACTIVE_EFFECT_SIZE, ACTIVE_EFFECT_DEF, ACTIVE_EFFECT_INHIBITED = 0x328, 0x20, 0x29C
MANAGER_SPECS, MANAGER_BACKPACK_SPECS, MANAGER_CHARACTER = 0xC0, 0xD0, 0xE0
SPEC_SIZE, SPEC_ITEM, SPEC_WRAPPER, SPEC_EFFECTED = 0xF0, 0x00, 0x08, 0xB8
WRAPPER_SIZE, WRAPPER_TYPE, WRAPPER_STANDARD, WRAPPER_EFFECT, WRAPPER_ABILITY, WRAPPER_TAG = 0xB0, 0x00, 0x08, 0x20, 0x48, 0x68
ITEM_DATA, ITEM_IN_BACKPACK = 0xE0, 0x180
DATA_WRAPPERS = 0x118
TYPES = {0: 'Standard', 1: 'GameplayEffect', 2: 'Ability', 3: 'BuffTag'}


def header(game, address, limit):
    data = game.read(address, 16)
    if not data or len(data) != 16:
        return 0, 0
    pointer, count, capacity = struct.unpack('<Qii', data)
    if not 0 <= count <= capacity <= limit or (count and not pointer):
        return 0, 0
    return pointer, count


def pointers(game, address, limit=256):
    pointer, count = header(game, address, limit)
    raw = game.read(pointer, count * 8) if count else b''
    return list(struct.unpack(f'<{count}Q', raw)) if raw and len(raw) == count * 8 else []


def is_a(game, obj, base, cache):
    key = (game.ptr(obj + CLASS), base)
    if key not in cache:
        current, found = key[0], False
        for _ in range(64):
            if not current:
                break
            if game.name(current) == base:
                found = True
                break
            current = game.ptr(current + SUPER)
        cache[key] = found
    return cache[key]


def attribute(game, address):
    data = game.read(address + 8, 8)
    return [round(value, 2) for value in struct.unpack('<ff', data)] if data and len(data) == 8 else None


def wrapper(game, address):
    kind = (game.read(address + WRAPPER_TYPE, 1) or b'\xff')[0]
    names = [game.name(game.ptr(address + offset)) for offset in (WRAPPER_STANDARD, WRAPPER_EFFECT, WRAPPER_ABILITY)]
    tag = game.i32(address + WRAPPER_TAG)
    return {'type': TYPES.get(kind, str(kind)), 'standard': names[0], 'effect': names[1], 'ability': names[2],
            'tag': game.name_from_index(tag) if tag else None}


def specs(game, address):
    pointer, count = header(game, address, 4096)
    rows = []
    for index in range(count):
        at = pointer + index * SPEC_SIZE
        item, effected = game.ptr(at + SPEC_ITEM), game.ptr(at + SPEC_EFFECTED)
        rows.append(dict(wrapper(game, at + SPEC_WRAPPER), item=game.name(game.ptr(item + ITEM_DATA)) if item else None,
                         effected=game.name(game.ptr(effected + ITEM_DATA)) if effected else None))
    return rows


def tree(game, component, depth=0, limit=6):
    if not component or depth > limit:
        return []
    owner = game.ptr(component + OUTER)
    rows = [{'depth': depth, 'name': game.name(component), 'class': game.name(game.ptr(component + CLASS)),
             'owner': game.name(owner), 'owner_class': game.name(game.ptr(owner + CLASS))}]
    for child in pointers(game, component + ATTACH_CHILDREN, 512):
        rows += tree(game, child, depth + 1, limit)
    return rows


def snapshot(pid, items=False, figure=False):
    game = Game(pid)
    world = game.world()
    level = game.ptr(world + 0x30) if world else 0
    if not level:
        raise SystemExit(f'{pid}: keine Welt lesbar')
    cache, states, sets, systems, managers, data = {}, [], {}, {}, [], []
    for obj in game.objects():
        if game.i32(obj + FLAGS) & 0x30 or (game.name(obj) or '').startswith('Default__'):
            continue
        outer = game.ptr(obj + OUTER)
        cls = game.name(game.ptr(obj + CLASS))
        if cls == 'BP_DDPlayerState_C' and outer == level:
            states.append(obj)
        elif cls == 'DDCharacterAttributeSet':
            sets[outer] = obj
        elif cls == 'CharacterEffectManager':
            managers.append(obj)
        elif is_a(game, obj, 'AbilitySystemComponent', cache):
            systems[outer] = obj
        elif items and is_a(game, obj, 'InventoryItemData', cache):
            data.append(obj)
    result = {'world': game.name(world), 'players': [], 'figures': []}
    for state in states:
        pawn = game.ptr(state + PLAYER_PAWN)
        row = {'state': game.name(state), 'player_id': game.i32(state + PLAYER_ID), 'pawn': game.name(pawn) if pawn else None,
               'pawn_role': (game.read(pawn + ROLE, 1) or b'\0')[0] if pawn else None, 'extra_hands': None, 'occupied_hands': None, 'effects': None}
        if state in sets:
            row['extra_hands'] = attribute(game, sets[state] + EXTRA_HANDS)
            row['occupied_hands'] = attribute(game, sets[state] + OCCUPIED_HANDS)
        if state in systems:
            pointer, count = header(game, systems[state] + ASC_ACTIVE_EFFECTS, 4096)
            row['effects'] = []
            for index in range(count):
                at = pointer + index * ACTIVE_EFFECT_SIZE
                name = game.name(game.ptr(game.ptr(at + ACTIVE_EFFECT_DEF) + CLASS))
                row['effects'].append(name + (' (gehemmt)' if (game.read(at + ACTIVE_EFFECT_INHIBITED, 1) or b'\0')[0] else ''))
        result['players'].append(row)
    for manager in managers:
        character = game.ptr(manager + MANAGER_CHARACTER)
        owner = game.ptr(manager + OUTER)
        if game.ptr(owner + OUTER) != level:
            continue
        row = {'character': game.name(character) if character else None, 'owner': game.name(owner),
               'role': (game.read(owner + ROLE, 1) or b'\0')[0],
               'specs': specs(game, manager + MANAGER_SPECS), 'backpack_specs': specs(game, manager + MANAGER_BACKPACK_SPECS)}
        if figure and character:
            row['components'] = tree(game, game.ptr(character + ROOT))
        result['figures'].append(row)
    if items:
        result['items'] = []
        for entry in data:
            pointer, count = header(game, entry + DATA_WRAPPERS, 64)
            found = [wrapper(game, pointer + index * WRAPPER_SIZE) for index in range(count)]
            hand = [row for row in found if any('hand' in (row[key] or '').lower() for key in ('standard', 'effect', 'ability', 'tag'))]
            if hand:
                result['items'].append({'data': game.name(entry), 'class': game.name(game.ptr(entry + CLASS)), 'wrappers': hand})
        result['items'].sort(key=lambda row: row['data'] or '')
    return result


def label(row):
    return f"{row['type']} {' + '.join(name for name in (row['effect'], row['ability'], row['standard'], row['tag']) if name)}"


def brief(report):
    lines = []
    for pid, data in report.items():
        lines.append(f"== {pid} {data['world']}")
        for player in data['players']:
            effects = player['effects']
            hands = [name for name in effects or [] if 'Hand' in name]
            lines.append(f"  {player['state']} (Id {player['player_id']}) Figur {player['pawn']} Rolle {player['pawn_role']}: ExtraHands Basis/aktuell "
                         f"{player['extra_hands']}, OccupiedHands {player['occupied_hands']}, aktive Effekte "
                         f"{'?' if effects is None else len(effects)}, davon Haende {hands}")
        for figure in data['figures']:
            lines.append(f"  {figure['owner']} Rolle {figure['role']}: {len(figure['specs'])} Wirkungen, {len(figure['backpack_specs'])} Rucksackwirkungen")
            for name, rows in (('Wirkung', figure['specs']), ('Rucksack', figure['backpack_specs'])):
                for row in rows:
                    if 'hand' in label(row).lower() or 'Robot' in (row['item'] or ''):
                        lines.append(f"    {name}: {row['item']} -> {label(row)}" + (f" auf {row['effected']}" if row['effected'] else ''))
            for row in figure.get('components') or []:
                lines.append(f"    {'  ' * row['depth']}{row['name']} ({row['class']}) von {row['owner']} ({row['owner_class']})")
        for item in data.get('items') or []:
            lines.append(f"  Item {item['data']} ({item['class']}): {', '.join(label(row) for row in item['wrappers'])}")
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('pids', type=int, nargs='+')
    parser.add_argument('--items', action='store_true', help='geladene Item-Daten mit Wirkungen auf Haende')
    parser.add_argument('--figure', action='store_true', help='Komponentenbaum jeder Figur')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    report = {pid: snapshot(pid, args.items, args.figure) for pid in args.pids}
    print(json.dumps(report, indent=1, ensure_ascii=False) if args.json else brief(report))
