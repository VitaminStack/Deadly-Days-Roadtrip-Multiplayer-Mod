"""Read-only Charakterwahl einer Spielinstanz: characters.py <pid> [<pid> ...]. Build 26495302; nicht atomar.

Zeigt je Instanz die Figuren des CharacterSelectionManager (CharacterDefinitions), seine Wahl je Spielerindex (SelectedCharacters)
und je BP_DD_Character die angewendete Definition der Komponente CharacterDefinitionApplier. Schreibt nichts, ruft nichts.
Offsets: docs/GAME_ANALYSIS.md#charakterwahl-in-der-metabase-charakter-wahl (SDK, Build 26495302)."""
import struct
import sys

from ddmem import Game

MANAGER_DEFINITIONS = 0x58   # TArray<UPlayerCharacterDefinition*>
MANAGER_SELECTED = 0x68      # TMap<int32, UPlayerCharacterDefinition*>
MANAGER_ID = 0xB8            # FName SelectedCharacterID
APPLIER_APPLIED = 0xE8       # TSoftObjectPtr<UPlayerCharacterDefinition>: Weak 8 Byte, dann Paketname und Assetname als FName
ELEMENT = 24                 # Schluessel 4 + Fuellung 4 + Wert 8 + HashNext 4 + HashIndex 4
OUTER = 0x20


def fname(game, address):
    data = game.read(address, 8)
    if not data or len(data) != 8:
        return '?'
    index, number = struct.unpack('<ii', data)
    text = game.name_from_index(index)
    return text if number == 0 else f'{text}_{number - 1}'


def definitions(game, manager):
    data, count, _ = struct.unpack_from('<Qii', game.read(manager + MANAGER_DEFINITIONS, 16))
    if not data or not 0 <= count <= 256:
        return []
    return [game.name(game.ptr(data + 8 * i)) for i in range(count)]


def selection(game, manager):
    """Zuordnung Spielerindex -> Name der gewaehlten Definition, nur belegte Plaetze der TSparseArray."""
    raw = game.read(manager + MANAGER_SELECTED, 0x50)
    if not raw or len(raw) != 0x50:
        return None
    data, count, capacity = struct.unpack_from('<Qii', raw)
    bits_pointer, num_bits = struct.unpack_from('<Qi', raw, 0x20)
    if count == 0:
        return {}
    if not data or not 0 <= count <= capacity <= 64 or not 0 <= num_bits <= 64:
        return None
    bits = struct.unpack_from('<4I', raw, 0x10)
    if bits_pointer:
        words = game.read(bits_pointer, 4 * ((num_bits + 31) // 32))
        bits = struct.unpack(f'<{len(words) // 4}I', words) if words else bits
    result = {}
    for slot in range(num_bits):
        if not bits[slot // 32] >> (slot % 32) & 1:
            continue
        element = game.read(data + slot * ELEMENT, ELEMENT)
        if not element:
            continue
        key, value = struct.unpack_from('<iQ', element, 0)[0], struct.unpack_from('<Q', element, 8)[0]
        result[key] = game.name(value) if value else None
    return result


def appliers(game):
    """Je Figur die angewendete Definition. Besitzer ist das Outer der Komponente."""
    found = []
    for obj in game.objects():
        if game.name(game.ptr(obj + 0x10)) != 'CharacterDefinitionApplier':
            continue
        owner = game.ptr(obj + OUTER)
        found.append((game.name(owner), hex(owner), fname(game, obj + APPLIER_APPLIED + 0x10)))
    return found


def report(pid):
    game = Game(pid)
    print(f'== {pid}')
    managers = [o for o in game.objects()
                if game.name(game.ptr(o + 0x10)) in ('BP_CharacterSelectionManager_C', 'CharacterSelectionManager')
                and not game.name(o).startswith('Default__')]
    if not managers:
        print('  kein CharacterSelectionManager (noch nicht geladen?)')
    for manager in managers:
        print(f'  Manager {game.name(manager)} {hex(manager)}: SelectedCharacterID={fname(game, manager + MANAGER_ID)}')
        print(f'    Definitionen: {", ".join(definitions(game, manager))}')
        print(f'    Wahl je Spielerindex: {selection(game, manager)}')
    for name, address, applied in appliers(game):
        print(f'  Applier an {name} {address}: angewendet {applied}')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for argument in sys.argv[1:]:
        report(int(argument))
