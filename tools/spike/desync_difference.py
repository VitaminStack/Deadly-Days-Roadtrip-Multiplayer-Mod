"""Nur eigene isolierte Testsitzung: Broken eines Client-Items kurz toggeln und zuruecksetzen.

Kein allgemeiner Speichereditor, keine fremden PIDs/Adressen als Argumente.
Wird von Test-Desync.ps1 nach der Gleichheitsmessung benutzt.
"""
import argparse
import ctypes
import json
from pathlib import Path
import time

from ddmem import Game, kernel


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hold-seconds', type=int, default=14, choices=range(8, 21))
    args = parser.parse_args()
    session_file = Path(__file__).resolve().parents[2] / 'research/local/session/session.json'
    session = json.loads(session_file.read_text(encoding='utf-8-sig'))
    host, client = Game(session['Host']), Game(session['Client'])
    if host.pid == client.pid:
        raise RuntimeError('Zwei getrennte lokale Testinstanzen erforderlich')
    for game in (host, client):
        if game.name(game.world()) != 'DDDynamic_Map':
            raise RuntimeError('Nur in einer laufenden Testmission erlaubt')
    world = client.world()
    state = client.ptr(world + 0x1B0)
    count = client.i32(state + 0x2C8)
    if count != 2:
        raise RuntimeError('Client braucht genau zwei PlayerStates')
    players = {client.ptr(client.ptr(state + 0x2C0) + 8 * i) for i in range(count)}
    target = 0
    for obj in client.objects():
        if client.ptr(obj + 0x20) in players and client.name(client.ptr(obj + 0x10)) == 'InventoryComponent':
            n = client.i32(obj + 0x128)
            if 0 < n <= 4096:
                target = client.ptr(client.ptr(obj + 0x120))
                if target and client.name(client.ptr(target + 0x10)) == 'InventoryItemInstance':
                    break
                target = 0
    if not target:
        raise RuntimeError('Kein geeignetes Client-Item')
    original = client.read(target + 0x110, 1)
    identity = client.read(target + 8, 24)
    if original not in (b'\x00', b'\x01'):
        raise RuntimeError('Broken ist nicht eindeutig lesbar')
    # Eigener separater Write-Handle; Game bleibt ein nur lesender Helfer.
    handle = kernel.OpenProcess(0x0028, False, client.pid)
    if not handle:
        raise RuntimeError('Kein Schreibzugriff auf eigene Client-Testinstanz')
    kernel.WriteProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                                         ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]

    def write(value):
        written = ctypes.c_size_t()
        data = ctypes.create_string_buffer(value)
        if not kernel.WriteProcessMemory(handle, target + 0x110, data, 1, ctypes.byref(written)) or written.value != 1:
            raise RuntimeError('Schreibzugriff fehlgeschlagen')

    try:
        write(bytes([original[0] ^ 1]))
        print('Client-Inventar: kontrollierte Broken-Abweichung gesetzt', flush=True)
        time.sleep(args.hold_seconds)
    finally:
        try:
            if client.world() != world or client.read(target + 8, 24) != identity:
                raise RuntimeError('Objektlebensdauer geaendert; kein Schreiben in alte Welt')
            write(original)
            print('Client-Inventar: Originalwert wiederhergestellt', flush=True)
        finally:
            kernel.CloseHandle(handle)


if __name__ == '__main__':
    main()
