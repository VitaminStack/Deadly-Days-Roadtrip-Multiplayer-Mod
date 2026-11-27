"""Read-only Power-ups je Spielerfigur: powerups.py <pid> [<pid> ...] [--defs]. Build 26786272; nicht atomar.

Zeigt je Figur aus dem PlayerArray Leben, Ausdauer und Loot-Radius (Attributsatz am PlayerState), die Slots ihrer ActivePowerupInventory und die lebenden
Instanzen der Power-up-Fähigkeiten (GA_PowerUp*, GA_Powerup*), dazu alle Power-up-Objekte in der Welt mit Rolle.
`--defs` listet zusätzlich jede geladene PowerupDefinition mit Fähigkeit, Effekt und Ausführungsrichtlinie der Fähigkeit.
Schreibt nichts, ruft nichts. Offsets: SDK des Builds, belegt in docs/OFFSETS.md (opt-powerups-client)."""
import struct
import sys

from ddmem import Game

OBJ_FLAGS, OBJ_CLASS, OBJ_OUTER = 0x08, 0x10, 0x20
CLASS_SUPER = 0x40
CLASS_DEFAULT_OBJECT = 0x118
RF_CLASS_DEFAULT = 0x10
STATE_PLAYER_ARRAY, PLAYER_PAWN = 0x2C0, 0x320
PAWN_COMMON_ATTRIBUTES, COMMON_HEALTH = 0x700, 0xAC        # FGameplayAttributeData.CurrentValue
STAMINA, MAX_STAMINA, STAMINA_REGEN, LOOT_RADIUS = 0x5C, 0x3C, 0x7C, 0x8C  # UDDCharacterAttributeSet, CurrentValue
ATTRACTOR_BUFF = 0xC0                                         # ULootAttractor.RadiusBuffFactor
ACTOR_HIDDEN = 0x58                                           # AActor.bHidden, Bit 7
LEVEL_FIELDS = 0xF0                                           # ULevelSystemComponent, zwei int32 vor ExpSubsystem (Stufe, Exp)
INVENTORY_OWNER, INVENTORY_SLOTS = 0x170, 0x138               # UActivePowerupInventory
SLOT_DEFINITION, SLOT_CONSUMABLE = 0x48, 0x68                 # UPowerupSlot
DEF_EFFECT, DEF_ABILITY, DEF_AUTO = 0x48, 0x50, 0x59          # UPowerupDefinition
ABILITY_POLICY = 0x198                                        # UGameplayAbility.NetExecutionPolicy
LOOT_ROLE, LOOT_DEFINITION, LOOT_IMMEDIATE = 0x168, 0x2E0, 0x2E8  # AActor.Role, APowerUpLoot
ACTOR_ROOT, COMPONENT_WORLD_LOCATION = 0x1B8, 0x210              # AActor.RootComponent, ComponentToWorld.Translation
POLICIES = {0: 'LocalPredicted', 1: 'LocalOnly', 2: 'ServerInitiated', 3: 'ServerOnly'}
ROLES = {0: 'None', 1: 'SimulatedProxy', 2: 'AutonomousProxy', 3: 'Authority'}


def f32(game, address):
    raw = game.read(address, 4)
    return round(struct.unpack('<f', raw)[0], 1) if raw and len(raw) == 4 else None


def u8(game, address):
    raw = game.read(address, 1)
    return raw[0] if raw else None


def where(game, actor):
    root = game.ptr(actor + ACTOR_ROOT) if actor else 0
    raw = game.read(root + COMPONENT_WORLD_LOCATION, 24) if root else None
    return '(%.0f %.0f %.0f)' % struct.unpack('<3d', raw) if raw and len(raw) == 24 else '(?)'


def chain(game, obj):
    names = []
    cls = game.ptr(obj + OBJ_CLASS)
    while cls and len(names) < 32:
        names.append(game.name(cls))
        cls = game.ptr(cls + CLASS_SUPER)
    return names


def snapshot(pid, defs):
    game = Game(pid)
    world = game.world()
    state = game.ptr(world + 0x1B0)
    data = game.ptr(state + STATE_PLAYER_ARRAY)
    count = game.i32(state + STATE_PLAYER_ARRAY + 8)
    states = [game.ptr(data + i * 8) for i in range(count if data and 0 < count <= 16 else 0)]
    pawns = [game.ptr(state + PLAYER_PAWN) for state in states]
    character_sets, inventories, attractors, levels, abilities, loot, definitions = {}, {}, {}, [], [], [], []
    for obj in game.objects():
        cls = game.ptr(obj + OBJ_CLASS)
        name = game.name(cls) or ''
        flags = game.i32(obj + OBJ_FLAGS)
        if name == 'DDCharacterAttributeSet':
            character_sets[game.ptr(obj + OBJ_OUTER)] = obj
        elif name == 'LootAttractor' and not flags & RF_CLASS_DEFAULT:
            attractors[game.ptr(obj + OBJ_OUTER)] = obj
        elif name == 'LevelSystemComponent' and not flags & RF_CLASS_DEFAULT:
            levels.append(obj)
        elif name == 'ActivePowerupInventory' and not flags & RF_CLASS_DEFAULT:
            inventories[game.ptr(obj + INVENTORY_OWNER)] = obj
        elif name.lower().startswith('ga_powerup') and not flags & RF_CLASS_DEFAULT:
            abilities.append((obj, name))
        elif name == 'PowerupDefinition' and defs:
            definitions.append(obj)
        elif 'ower' in name and not flags & RF_CLASS_DEFAULT and 'PowerUpLoot' in chain(game, obj)[1:]:
            loot.append((obj, name))
    print(f'[{pid}] Welt {game.name(world)}, {len(pawns)} Figuren')
    for index, pawn in enumerate(pawns):
        common = game.ptr(pawn + PAWN_COMMON_ATTRIBUTES) if pawn else 0
        sets = character_sets.get(states[index], 0)  # Attributsatz und Inventar hängen am PlayerState
        line = (f'  Spieler {index}: {game.name(pawn)} Rolle {ROLES.get(u8(game, pawn + LOOT_ROLE)) if pawn else None} Leben {f32(game, common + COMMON_HEALTH) if common else None}'
                f' Ausdauer {f32(game, sets + STAMINA) if sets else None}/{f32(game, sets + MAX_STAMINA) if sets else None}'
                f' Regeneration {f32(game, sets + STAMINA_REGEN) if sets else None}'
                f' Radiusfaktor {f32(game, attractors[pawn] + ATTRACTOR_BUFF) if pawn in attractors else None}'
                f' LootRadius {f32(game, sets + LOOT_RADIUS) if sets else None} {where(game, pawn)}')
        print(line)
        inventory = inventories.get(pawn, 0)
        slots = game.ptr(inventory + INVENTORY_SLOTS) if inventory else 0
        slot_count = game.i32(inventory + INVENTORY_SLOTS + 8) if inventory else 0
        filled = []
        for i in range(slot_count if 0 <= slot_count <= 16 else 0):
            slot = game.ptr(slots + i * 8)
            definition = game.ptr(slot + SLOT_DEFINITION) if slot else 0
            filled.append(game.name(definition) or '-')
        print(f'    Slots ({"Inventar fehlt" if not inventory else slot_count}): {", ".join(filled) or "-"}')
    for obj in levels:
        raw = game.read(obj + LEVEL_FIELDS, 8)
        values = struct.unpack('<2i', raw) if raw and len(raw) == 8 else None
        print(f'  Stufensystem in {game.name(game.ptr(obj + OBJ_OUTER))}: {values}')
    for obj, name in abilities:
        outer = game.ptr(obj + OBJ_OUTER)
        print(f'  Faehigkeit {name} in {game.name(outer)}')
    for obj, name in loot:
        definition = game.ptr(obj + LOOT_DEFINITION)
        print(f'  Loot {game.name(obj)} Rolle {ROLES.get(u8(game, obj + LOOT_ROLE))} Definition {game.name(definition)}'
              f' sofort {u8(game, obj + LOOT_IMMEDIATE)} versteckt {(u8(game, obj + ACTOR_HIDDEN) or 0) >> 7} {where(game, obj)}')
    for definition in definitions:
        ability = game.ptr(definition + DEF_ABILITY)
        default = game.ptr(ability + CLASS_DEFAULT_OBJECT) if ability else 0
        policy = POLICIES.get(u8(game, default + ABILITY_POLICY)) if default else None
        print(f'  Definition {game.name(definition)}: Faehigkeit {game.name(ability)} ({policy}),'
              f' Effekt {game.name(game.ptr(definition + DEF_EFFECT))}, automatisch {u8(game, definition + DEF_AUTO)}')


if __name__ == '__main__':
    arguments = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not arguments:
        raise SystemExit(__doc__)
    for pid in arguments:
        snapshot(int(pid), '--defs' in sys.argv)
