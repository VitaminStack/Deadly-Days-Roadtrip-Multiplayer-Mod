"""Tank lesen oder fast fuellen: fuel.py <pid> [--fill]. Nur Build 26495302, fuer Testlaeufe zum Rundenende.

Liest jeden UFuelMeter (MaxRequiredFuel +0xF4, CurrentFuel +0xF8, beide float, siehe docs/OFFSETS.md).
--fill schreibt CurrentFuel = MaxRequiredFuel - 2, nur beim Host sinnvoll: Der Wert ist repliziert,
der letzte Kanister im Spiel loest danach das echte Rundenende ueber den Bus aus.
"""
import ctypes
import json
import struct
import sys

from ddmem import Game, kernel


def meters(game):
    for obj in game.objects():
        if game.name(game.ptr(obj + 0x10)) != 'FuelMeter':
            continue
        name = game.name(obj) or ''
        if name.startswith('Default__') or name.endswith('_GEN_VARIABLE'):  # Vorlagen, nicht der Laufzeitwert
            continue
        data = game.read(obj + 0xF4, 8)
        if data and len(data) == 8:
            yield obj, struct.unpack('<ff', data)


def main():
    if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != '--fill'):
        raise SystemExit(__doc__)
    game = Game(int(sys.argv[1]))
    found = list(meters(game))
    result = [{'object': hex(obj), 'name': game.name(obj), 'max': maximum, 'current': current}
              for obj, (maximum, current) in found]
    if len(sys.argv) == 3:
        handle = kernel.OpenProcess(0x0438, False, game.pid)  # VM_OPERATION | VM_READ | VM_WRITE | QUERY_INFORMATION
        if not handle:
            raise SystemExit('Kein Schreibzugriff auf den Prozess')
        for entry, (obj, (maximum, _)) in zip(result, found):
            if not 0 < maximum < 100000:
                entry['skipped'] = 'Maximum unplausibel'
                continue
            value = struct.pack('<f', maximum - 2)
            written = ctypes.c_size_t()
            ok = kernel.WriteProcessMemory(handle, ctypes.c_void_p(obj + 0xF8), value, 4, ctypes.byref(written))
            entry['filled_to'] = maximum - 2 if ok and written.value == 4 else None
    print(json.dumps(result, indent=2))
    return 0 if result else 1


if __name__ == '__main__':
    sys.exit(main())
