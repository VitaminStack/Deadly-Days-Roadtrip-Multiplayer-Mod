"""SCHREIBENDE Testhilfe, nur fuer Instanzen der eigenen Testsitzung: extra_hands.py <pid> [<pid> ...] --hands N.

Build 26495302 (opt-schuss-animation). Eine Figur hat anfangs eine Waffenhand, das Inventar-Panel lehnt jede weitere Waffe ab.
Das Skript schreibt UDDCharacterAttributeSet.ExtraHands (Basis und aktueller Wert, +0x108 und +0x10C) der Spieler direkt in den
Prozess, damit sich im Test mehrere Waffen ausruesten lassen. Grenzen:
- Der Wert repliziert so nicht: in Host UND Client setzen.
- Er haelt nur bis zum naechsten Kartenwechsel, danach neu setzen. Bis dahin kann der Host mehr Haende belegt haben, als er hat.
- Kein Spielzustand fuer Abnahmen von Haenden, Balance oder Hand-Anzeige; nie gegen ein normal gestartetes Spiel verwenden.
Offsets: docs/OFFSETS.md#schuss-animation-opt-schuss-animation.
"""
import argparse
import ctypes
import struct

from ddmem import Game, kernel

CLASS, OUTER, FLAGS = 0x10, 0x20, 0x08
EXTRA_HANDS_BASE = 0x108  # FGameplayAttributeData ExtraHands +0x100: BaseValue +0x8, CurrentValue +0xC
PROCESS_VM_WRITE_OPERATION = 0x0028

kernel.WriteProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]


def set_extra_hands(pid, hands):
    game = Game(pid)
    handle = kernel.OpenProcess(PROCESS_VM_WRITE_OPERATION, False, pid)
    if not handle:
        raise SystemExit(f'{pid}: kein Schreibzugriff auf den Prozess')
    written = 0
    for obj in game.objects():
        if game.i32(obj + FLAGS) & 0x30 or game.name(game.ptr(obj + CLASS)) != 'DDCharacterAttributeSet':
            continue
        if game.name(game.ptr(game.ptr(obj + OUTER) + CLASS)) != 'BP_DDPlayerState_C' or (game.name(game.ptr(obj + OUTER)) or '').startswith('Default__'):
            continue
        data = struct.pack('<ff', hands, hands)
        if kernel.WriteProcessMemory(handle, obj + EXTRA_HANDS_BASE, data, len(data), None):
            written += 1
    return written


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('pids', type=int, nargs='+')
    parser.add_argument('--hands', type=float, required=True, help='zusaetzliche Waffenhaende, etwa 5')
    args = parser.parse_args()
    for pid in args.pids:
        print(f'{pid}: ExtraHands {args.hands:g} bei {set_extra_hands(pid, args.hands)} Spieler(n) gesetzt')
