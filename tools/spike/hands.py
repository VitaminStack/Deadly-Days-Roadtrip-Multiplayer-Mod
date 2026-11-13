"""Read-only Waffen-Darstellung an den Haenden: hands.py <pid> [<pid> ...] [--json].

Build 26495302, nicht atomar (opt-schuss-animation). Je BP_DD_Character_C der aktuellen Welt: Rolle, Ort, EquippedWeapons,
die Haende (AHands: ParentCharacter, AutoFire, Blickwinkel, ChildActorComponents, CurrentRanged/MeleeWeapons) und je
Waffen-Darstellung Klasse, Anhaengepunkt, relative Lage und Weltort. Dazu alle AWeapon-Actors der Welt, auch solche, die keine
Hand mehr fuehrt (verwaiste Darstellungen). Beim Host dazu je Figur: EquippedWeapons gegen die Listen der Haende und gegen
InventoryComponent.WeaponLookup; Exitcode 1 bei einer Geisterwaffe oder einer ausgeruesteten Waffe ohne Darstellung.
Schreibt nichts, ruft nichts.
Offsets: docs/OFFSETS.md#schuss-animation-opt-schuss-animation.
"""
import argparse
import json
import math
import struct

from ddmem import Game

FLAGS, CLASS, OUTER, SUPER = 0x08, 0x10, 0x20, 0x40
ROLE, OWNER, ROOT, ACTOR_FLAGS = 0x168, 0x158, 0x1B8, 0x58
ATTACH_PARENT, ATTACH_SOCKET, ATTACH_CHILDREN = 0xD0, 0xD8, 0xE8
REL_LOCATION, REL_ROTATION, REL_SCALE, VISIBILITY = 0x148, 0x160, 0x178, 0x1A8
WORLD_ROTATION, WORLD_LOCATION = 0x1F0, 0x210
CHARACTER_HANDS, EQUIPPED_WEAPONS = 0x7B0, 0x7B8
HANDS_INNER, HANDS_ANGLE, HANDS_PARENT, HANDS_AUTOFIRE = 0x2C8, 0x31C, 0x320, 0x378
HANDS_CHILDREN, HANDS_RANGED, HANDS_MELEE = 0x3A0, 0x3B0, 0x3C0
CHILD_CLASS, CHILD_ACTOR = 0x250, 0x258
INSTANCE_ITEM, INSTANCE_DATA, INSTANCE_COMPONENT, INSTANCE_ACTOR = 0x38, 0x40, 0x50, 0x58
INVENTORY_ITEMS, INVENTORY_CHARACTER, WEAPON_LOOKUP = 0x120, 0x220, 0x238
ITEM_DATA, ITEM_IN_BACKPACK = 0xE0, 0x180


def array(game, address, limit=64):
    """None bei unlesbarem Header: unbekannt ist nicht leer."""
    data = game.read(address, 16)
    if not data or len(data) != 16:
        return None
    pointer, count, capacity = struct.unpack('<Qii', data)
    if not 0 <= count <= capacity <= 4096 or (count and not pointer):
        return None
    count = min(count, limit)
    raw = game.read(pointer, count * 8) if count else b''
    if count and (not raw or len(raw) != count * 8):
        return None
    return list(struct.unpack(f'<{count}Q', raw)) if count else []


def doubles(game, address, count):
    data = game.read(address, 8 * count) if address else None
    if not data or len(data) != 8 * count:
        return None
    values = struct.unpack(f'<{count}d', data)
    return [round(value, 1) for value in values] if all(math.isfinite(value) for value in values) else None


def yaw(game, component):
    quat = doubles(game, component + WORLD_ROTATION, 4) if component else None
    if not quat:
        return None
    data = struct.unpack('<4d', game.read(component + WORLD_ROTATION, 32))
    x, y, z, w = data
    return round(math.degrees(math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z))), 1)


def is_a(game, obj, base, cache):
    cls = (game.ptr(obj + CLASS), base)
    if cls not in cache:
        current, found = cls[0], False
        for _ in range(64):
            if not current:
                break
            if game.name(current) == base:
                found = True
                break
            current = game.ptr(current + SUPER)
        cache[cls] = found
    return cache[cls]


def component(game, comp):
    if not comp:
        return None
    parent = game.ptr(comp + ATTACH_PARENT)
    bits = game.read(comp + VISIBILITY, 2) or b'\0\0'
    return {'name': game.name(comp), 'parent': game.name(parent) if parent else None,
            'parent_owner': game.name(game.ptr(parent + OUTER)) if parent else None,
            'socket': game.name_from_index(game.i32(comp + ATTACH_SOCKET)) if game.i32(comp + ATTACH_SOCKET) else None,
            'rel_location': doubles(game, comp + REL_LOCATION, 3), 'rel_rotation': doubles(game, comp + REL_ROTATION, 3),
            'rel_scale': doubles(game, comp + REL_SCALE, 3), 'world_location': doubles(game, comp + WORLD_LOCATION, 3),
            'world_yaw': yaw(game, comp), 'visible': bool(bits[0] & 0x20), 'hidden_in_game': bool(bits[1] & 0x08)}


def actor(game, obj):
    if not obj:
        return None
    flags = game.read(obj + ACTOR_FLAGS, 6) or b''
    flags = flags if len(flags) == 6 else bytes(6)
    role = game.read(obj + ROLE, 1) or b'\0'
    return {'name': game.name(obj), 'class': game.name(game.ptr(obj + CLASS)), 'role': role[0],
            'hidden': bool(flags[0] & 0x80),
            # Zerstoert, aber noch nicht eingesammelt: RF_MirroredGarbage am Objekt oder bActorIsBeingDestroyed.
            'destroyed': bool(game.i32(obj + FLAGS) & 0x40000000 or flags[5] & 0x02), 'owner': game.name(game.ptr(obj + OWNER)) if game.ptr(obj + OWNER) else None,
            'root': component(game, game.ptr(obj + ROOT))}


def weak(game, address):
    """TWeakObjectPtr: Index im Objekt-Array; die Seriennummer wird nicht geprueft."""
    index = game.i32(address)
    if index <= 0:
        return 0
    chunk = game.ptr(game.ptr(game.base + game.gobjects) + 8 * (index >> 16))
    return game.ptr(chunk + (index & 0xFFFF) * 0x18 + 8)


def instance(game, obj):
    item = weak(game, obj + INSTANCE_ITEM)
    return {'data': game.name(game.ptr(obj + INSTANCE_DATA)), 'item': item,
            'item_in_backpack': bool((game.read(item + ITEM_IN_BACKPACK, 1) or b'\0')[0]) if item else None, 'component': game.name(game.ptr(obj + INSTANCE_COMPONENT)) if game.ptr(obj + INSTANCE_COMPONENT) else None,
            'actor': game.name(game.ptr(obj + INSTANCE_ACTOR)) if game.ptr(obj + INSTANCE_ACTOR) else None}


def snapshot(pid):
    game = Game(pid)
    world = game.world()
    level = game.ptr(world + 0x30) if world else 0
    if not level:
        raise SystemExit(f'{pid}: keine Welt lesbar')
    level_class = game.ptr(level + CLASS)
    cache, figures, weapons, all_hands = {}, [], [], []
    for obj in game.objects():
        if game.i32(obj + FLAGS) & 0x30:
            continue
        outer = game.ptr(obj + OUTER)
        cls = game.name(game.ptr(obj + CLASS))
        if cls == 'BP_DD_Character_C' and outer == level:
            figures.append(obj)
        elif game.ptr(outer + CLASS) != level_class or (game.name(obj) or '').startswith('Default__'):
            continue  # Vorlagen (…_CAT an der Komponente) und Default-Objekte sind keine Actors einer Welt
        elif is_a(game, obj, 'Weapon', cache):
            weapons.append((obj, outer == level))
        elif is_a(game, obj, 'Hands', cache) and outer == level:
            all_hands.append(obj)
    led = set()
    inventories = {}
    for obj in game.objects():
        if game.name(game.ptr(obj + CLASS)) == 'InventoryComponent' and not game.i32(obj + FLAGS) & 0x30:
            inventories[game.ptr(obj + INVENTORY_CHARACTER)] = obj
    result = {'world': game.name(world), 'figures': [], 'hands_total': len(all_hands)}
    for figure in figures:
        root = game.ptr(figure + ROOT)
        hands = game.ptr(figure + CHARACTER_HANDS)
        equipped = array(game, figure + EQUIPPED_WEAPONS)
        row = {'name': game.name(figure), 'role': (game.read(figure + ROLE, 1) or b'\0')[0],
               'location': doubles(game, root + REL_LOCATION, 3) if root else None,
               'equipped': None if equipped is None else [instance(game, entry) for entry in equipped], 'hands': None}
        if hands:
            children = array(game, hands + HANDS_CHILDREN)
            ranged = array(game, hands + HANDS_RANGED)
            melee = array(game, hands + HANDS_MELEE)
            angle = game.read(hands + HANDS_ANGLE, 4)
            inner = game.ptr(hands + HANDS_INNER)
            reps = []
            attached = array(game, inner + ATTACH_CHILDREN) if inner else None
            listed = list(children or [])
            # Beim Client ist die Liste der Haende leer; die replizierten Komponenten haengen trotzdem am Container.
            listed += [entry for entry in attached or [] if entry not in listed and game.name(game.ptr(entry + CLASS)) == 'ChildActorComponent']
            for child in listed:
                child_actor = game.ptr(child + CHILD_ACTOR)
                led.add(child_actor)
                reps.append({'listed': child in (children or []), 'component': component(game, child), 'child_class': game.name(game.ptr(child + CHILD_CLASS)),
                             'actor': actor(game, child_actor)})
            hands_root = game.ptr(hands + ROOT)
            row['hands'] = {'name': game.name(hands), 'role': (game.read(hands + ROLE, 1) or b'\0')[0],
                            'parent_character': game.name(game.ptr(hands + HANDS_PARENT)) if game.ptr(hands + HANDS_PARENT) else None,
                            'owner': game.name(game.ptr(hands + OWNER)) if game.ptr(hands + OWNER) else None,
                            'auto_fire': (game.read(hands + HANDS_AUTOFIRE, 1) or b'\0')[0],
                            'angle': round(struct.unpack('<f', angle)[0], 1) if angle and len(angle) == 4 else None,
                            'root': component(game, hands_root), 'inner': component(game, inner),
                            'inner_children': None if attached is None else [game.name(entry) for entry in attached],
                            'root_children': [game.name(entry) for entry in (array(game, hands_root + ATTACH_CHILDREN) or [])] if hands_root else None,
                            'ranged': None if ranged is None else [instance(game, entry) for entry in ranged],
                            'melee': None if melee is None else [instance(game, entry) for entry in melee],
                            'listed': None if children is None else len(children), 'representations': reps}
        # Nur beim Host aussagekraeftig: EquippedWeapons und WeaponLookup replizieren nicht. Eine Waffe an den Haenden, deren Item
        # nicht (mehr) im Rucksack liegt, oder ein Lookup-Eintrag ohne Item im Inventar ist eine Geisterwaffe.
        inventory = inventories.get(figure)
        items = (array(game, inventory + INVENTORY_ITEMS, 512) or []) if inventory else []
        lookup = []
        if inventory:
            data, count = game.ptr(inventory + WEAPON_LOOKUP), game.i32(inventory + WEAPON_LOOKUP + 8)
            for index in range(max(0, min(count, 64))):
                key = game.ptr(data + index * 0x18)
                if key:
                    lookup.append({'item': game.name(game.ptr(key + ITEM_DATA)), 'in_inventory': key in items})
        row['lookup'] = lookup
        held = ((row['hands'] or {}).get('ranged') or []) + ((row['hands'] or {}).get('melee') or [])
        row['ghosts'] = ([entry['data'] for entry in held if entry['item'] not in items or not entry['item_in_backpack']] +
                         [entry['item'] for entry in lookup if not entry['in_inventory']]) if row['role'] == 3 and inventory else []
        row['unrepresented'] = [entry['data'] for entry in (row['equipped'] or []) if not entry['actor']] if row['role'] == 3 else []
        result['figures'].append(row)
    result['weapon_actors'] = [dict(actor(game, obj), led=obj in led, current_level=current) for obj, current in weapons]
    return result


def brief(report):
    lines = []
    for pid, data in report.items():
        gone = sum(weapon['destroyed'] for weapon in data['weapon_actors'])
        lines.append(f"== {pid} {data['world']}: {len(data['figures'])} Figuren, {data['hands_total']} Haende, "
                     f"{len(data['weapon_actors']) - gone} Waffen-Actors (dazu {gone} zerstoerte vor dem Einsammeln)")
        for figure in data['figures']:
            hands = figure['hands'] or {}
            count = lambda value: '?' if value is None else len(value)
            lines.append(f"  {figure['name']} Rolle {figure['role']} Ort {figure['location']} equipped {count(figure['equipped'])} "
                         f"| Haende {hands.get('name')} Rolle {hands.get('role')} Parent {hands.get('parent_character')} Owner {hands.get('owner')} "
                         f"AutoFire {hands.get('auto_fire')} Winkel {hands.get('angle')} ranged {count(hands.get('ranged'))} "
                         f"melee {count(hands.get('melee'))} Darstellungen {hands.get('listed')} gelistet, {count(hands.get('representations'))} am Container")
            if hands:
                lines.append(f"    Root {hands['root'] and (hands['root']['parent_owner'], hands['root']['rel_location'], hands['root']['world_location'], hands['root']['world_yaw'])}"
                             f" Inner {hands['inner'] and (hands['inner']['rel_location'], hands['inner']['rel_rotation'])}"
                             f" Kinder Root {hands['root_children']} Inner {hands['inner_children']}")
            if figure['role'] == 3:
                lines.append(f"    Host-Zustand: ausgeruestet {[entry['data'] for entry in figure['equipped'] or []]}, Lookup {len(figure['lookup'])}, "
                             f"Geisterwaffen {figure['ghosts'] or 'keine'}, ausgeruestet ohne Darstellung {figure['unrepresented'] or 'keine'}")
            for rep in hands.get('representations') or []:
                comp, act = rep['component'] or {}, rep['actor'] or {}
                lines.append(f"    {comp.get('name')} an {comp.get('parent')} rel {comp.get('rel_location')} rot {comp.get('rel_rotation')} "
                             f"skal {comp.get('rel_scale')} welt {comp.get('world_location')} sichtbar {comp.get('visible')} -> {act.get('name')} ({rep['child_class']}) "
                             f"Rolle {act.get('role')} versteckt {act.get('hidden')} "
                             f"welt {(act.get('root') or {}).get('world_location')}")
        for weapon in data['weapon_actors']:
            if not weapon['led'] and not weapon['destroyed']:
                root = weapon['root'] or {}
                lines.append(f"  OHNE HAND{'' if weapon['current_level'] else ' (andere Welt)'} {weapon['name']} ({weapon['class']}) Rolle {weapon['role']} Owner {weapon['owner']} versteckt {weapon['hidden']} "
                             f"an {root.get('parent_owner')}/{root.get('parent')} rel {root.get('rel_location')} welt {root.get('world_location')}")
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pids', type=int, nargs='+')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    report = {pid: snapshot(pid) for pid in args.pids}
    print(json.dumps(report, indent=1, ensure_ascii=False) if args.json else brief(report))
    raise SystemExit(1 if any(figure['ghosts'] or figure['unrepresented'] for data in report.values() for figure in data['figures']) else 0)
