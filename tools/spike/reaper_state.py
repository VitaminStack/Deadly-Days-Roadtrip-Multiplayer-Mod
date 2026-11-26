"""Read-only Sensenmann je Instanz: reaper_state.py <pid> [<pid> ...] [--watch <s>] [--interval <s>]. Build 26786272.

Je Instanz der AReaperSpawner der aktuellen Welt (ShouldSpawnReaper +0x528, WasReaperSpawned +0x529) mit den Empfaengern
seines OnReaperSpawned (+0x498, Klasse.Funktion), die Sensenmann-Gegner (BP_ReaperEnemy_C, Rolle, Ort) und
WBP_Stopwatch_C.IsReaperActive (+0x484). Mit --watch eine Zeile je Aenderung mit Zeit seit dem Start, fuer alle Instanzen
gemeinsam (opt-sensenmann-ansage-client: wann der Sensenmann beim Client ankommt). Nicht atomar, keine Schreibzugriffe.
"""
import argparse
import struct
import time

from ddmem import Game

FLAG_CLASS_DEFAULT_OBJECT = 0x10
SUPER = 0x40
OUTER = 0x20
ROOT_COMPONENT = 0x1B8
COMPONENT_LOCATION = 0x148
ROLE = 0x168
SPAWNER_ON_SPAWNED = 0x498
SPAWNER_SHOULD = 0x528
SPAWNER_WAS = 0x529
STOPWATCH_ACTIVE = 0x484
ITEM_SIZE = 0x18


def chain(game, cls, cache):
    if cls not in cache:
        names, current = [], cls
        for _ in range(30):
            if not current:
                break
            names.append(game.name(current))
            current = game.ptr(current + SUPER)
        cache[cls] = names
    return cache[cls]


def object_at(game, index):
    chunks = game.ptr(game.base + game.gobjects)
    chunk = game.ptr(chunks + 8 * (index >> 16)) if chunks and index >= 0 else 0
    return game.ptr(chunk + (index & 0xFFFF) * ITEM_SIZE + 8) if chunk else 0


def in_world(game, obj, world):
    for _ in range(8):
        obj = game.ptr(obj + OUTER)
        if not obj:
            return False
        if obj == world:
            return True
    return False


def location(game, actor):
    component = game.ptr(actor + ROOT_COMPONENT)
    raw = game.read(component + COMPONENT_LOCATION, 24) if component else None
    return tuple(round(v) for v in struct.unpack('<ddd', raw)) if raw and len(raw) == 24 else None


def receivers(game, spawner, cache):
    data, num = game.ptr(spawner + SPAWNER_ON_SPAWNED), game.i32(spawner + SPAWNER_ON_SPAWNED + 8)
    found = []
    for i in range(max(0, min(num, 32))):
        raw = game.read(data + 16 * i, 16)
        if not raw or len(raw) < 16:
            continue
        index, _, function, _ = struct.unpack('<iiii', raw)
        obj = object_at(game, index)
        cls = chain(game, game.ptr(obj + 0x10), cache)[0] if obj else '?'
        found.append(f'{cls}.{game.name_from_index(function)}')
    return found


def snapshot(game):
    cache, world = {}, game.world()
    state = {'world': game.name(world), 'spawner': [], 'reaper': [], 'stopwatch': []}
    for obj in game.objects():
        header = game.read(obj + 8, 16)
        if not header or len(header) < 16 or struct.unpack_from('<I', header)[0] & FLAG_CLASS_DEFAULT_OBJECT:
            continue
        names = chain(game, struct.unpack_from('<Q', header, 8)[0], cache)
        if 'ReaperSpawner' in names and in_world(game, obj, world):
            flags = game.read(obj + SPAWNER_SHOULD, 2) or b'\0\0'
            state['spawner'].append((flags[0], flags[1], tuple(receivers(game, obj, cache))))
        elif 'BP_ReaperEnemy_C' in names and in_world(game, obj, world):
            state['reaper'].append(((game.read(obj + ROLE, 1) or b'\0')[0], location(game, obj)))
        elif 'WBP_Stopwatch_C' in names:
            state['stopwatch'].append((game.read(obj + STOPWATCH_ACTIVE, 1) or b'\0')[0])
    return state


def line(state, coarse):
    spawners = [f'Should {s} Was {w} Empfaenger {list(r)}' for s, w, r in state['spawner']]
    reapers = [f'Rolle {r}' + ('' if coarse else f' {loc}') for r, loc in state['reaper']]
    return f"{state['world']} Spawner {spawners} Sensenmann {reapers} Stoppuhr IsReaperActive {state['stopwatch']}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('pids', type=int, nargs='+')
    parser.add_argument('--watch', type=float, default=0)
    parser.add_argument('--interval', type=float, default=0.5)
    args = parser.parse_args()
    games = [Game(pid) for pid in args.pids]
    if not args.watch:
        for game in games:
            print(game.pid, line(snapshot(game), False))
        return
    start, last = time.monotonic(), {}
    while time.monotonic() - start < args.watch:
        for game in games:
            text = line(snapshot(game), True)
            if last.get(game.pid) != text:
                last[game.pid] = text
                print(f'{time.monotonic() - start:7.1f}s {game.pid} {text}', flush=True)
        time.sleep(args.interval)


main()
