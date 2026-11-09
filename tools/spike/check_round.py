"""Prueft nach einem Kartenwechsel, ob die Runde bei Host und Client sauber geladen ist.
  check_round.py <host-pid> <client-pid> [--wait] [--timeout 120] [--settle 10] [--label tag2] [--log datei.jsonl]
--wait wartet vorher, bis beide Instanzen eine neue World haben, und laesst sie dann --settle Sekunden laufen.
Gibt eine JSON-Zeile aus und haengt sie an --log an. Exitcode 0 sauber, 1 Abweichung, 2 Instanz beendet.
Nur lesend, Offsets fuer Build 26495302, siehe docs/OFFSETS.md."""
import argparse
import json
import math
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ddmem import Game  # noqa: E402
import guidcache  # noqa: E402

MISSION_WORLD = 'DDDynamic_Map'
# Figuren gelten als am Bus, wenn sie naeher als diese Distanz in Unreal-Einheiten stehen.
BUS_RADIUS = 1500
# Host und Client sollen dieselbe Figur an derselben Stelle sehen. Bewegung zwischen den Messungen ist erlaubt.
POSITION_TOLERANCE = 300
FLAG_CLASS_DEFAULT_OBJECT = 0x10
FLAG_GARBAGE, FLAG_UNREACHABLE = 0x00200000, 0x10000000
COMPONENT_TO_WORLD_TRANSLATION = 0x210
ROOTS = {'DDCharacter', 'DDPlayerState', 'DDPlayerController', 'MapTile', 'Bus', 'WorldSettings'}
WORLD_SETTINGS_PAUSER = 0x4A8
PLAYER_STATE_PAWN = 0x320
PLAYER_CONTROLLER_NET_CONNECTION = 0x520
# Statische NetGUIDs (Klassen, Archetypen, Pakete) dieser Pfade muss der Client behalten, sonst scheitern neue Actors.
GUID_PATTERNS = ('DDPlayerState', 'DDPlayerController', 'DDCharacter')
STATE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'research', 'local', 'session',
                          'check_round_state.json')
LOG_DIR = os.path.join(os.environ.get('TEMP', '.'), 'ddspike')


def process_running(pid):
    """Laeuft der Prozess noch? Unabhaengig von der World, die waehrend des Ladens 0 ist."""
    import ctypes
    kernel = ctypes.windll.kernel32
    handle = kernel.OpenProcess(0x1000, False, pid)  # PROCESS_QUERY_LIMITED_INFORMATION
    if not handle:
        return False
    try:
        code = ctypes.c_ulong()
        return bool(kernel.GetExitCodeProcess(handle, ctypes.byref(code))) and code.value == 259  # STILL_ACTIVE
    finally:
        kernel.CloseHandle(handle)


def open_game(pid):
    """Speicherzugriff auf eine laufende Instanz, auch waehrend des Ladens. None, wenn der Prozess beendet ist."""
    if not process_running(pid):
        return None
    try:
        game = Game(pid)
    except SystemExit:
        return None
    return game if game.base else None


def alive(pid):
    game = open_game(pid)
    return game if game and game.world() else None


def class_root(game, cls, cache):
    if cls not in cache:
        current, found = cls, None
        for _ in range(30):
            if not current:
                break
            name = game.name(current)
            if name in ROOTS:
                found = name
                break
            current = game.ptr(current + 0x40)
        cache[cls] = found
    return cache[cls]


def location(game, actor):
    """Weltposition aus ComponentToWorld der RootComponent, RelativeLocation reicht bei angehaengten Actors nicht."""
    component = game.ptr(actor + 0x1B8)
    raw = game.read(component + COMPONENT_TO_WORLD_TRANSLATION, 24) if component else None
    return tuple(struct.unpack('<ddd', raw)) if raw and len(raw) == 24 else None


def live_objects(game):
    """Wie Game.objects, liefert aber zusaetzlich die internen Flags des GUObjectArray-Elements."""
    chunks = game.ptr(game.base + getattr(game, 'gobjects', guidcache.GOBJECTS))
    count = game.i32(game.base + getattr(game, 'gobjects', guidcache.GOBJECTS) + 8)
    for chunk_index in range((count >> 16) + 1):
        chunk = game.ptr(chunks + 8 * chunk_index)
        raw = game.read(chunk, 0x10000 * 0x18) if chunk else None
        if not raw:
            continue
        for i in range(min(0x10000, count - (chunk_index << 16), len(raw) // 0x18)):
            internal, obj = struct.unpack_from('<IQ', raw, i * 0x18 + 4)
            if obj:
                yield obj, internal


def scan(game):
    """Zaehlt Objekte je Wurzelklasse, getrennt nach aktueller World und Resten frueherer Worlds."""
    world, cache = game.world(), {}
    found = {root: [] for root in ROOTS}
    stale = {root: 0 for root in ROOTS}
    for obj, internal in live_objects(game):
        if internal & (FLAG_GARBAGE | FLAG_UNREACHABLE):
            continue
        header = game.read(obj + 8, 16)
        if not header or len(header) < 16 or struct.unpack_from('<I', header)[0] & FLAG_CLASS_DEFAULT_OBJECT:
            continue
        root = class_root(game, struct.unpack_from('<Q', header, 8)[0], cache)
        if not root:
            continue
        # Actor -> ULevel -> UWorld. Was an einer anderen World haengt, ist ein Rest des letzten Kartenwechsels.
        if game.ptr(game.ptr(obj + 0x20) + 0x20) == world:
            found[root].append(obj)
        else:
            stale[root] += 1
    return found, stale


def private_megabytes(game):
    """Private Bytes des Prozesses in MB, fuer das Wachstum je Tag im Dauerlauf."""
    import ctypes
    from ctypes import wintypes

    class Counters(ctypes.Structure):
        _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD)] + [
            (name, ctypes.c_size_t) for name in (
                'PeakWorkingSetSize', 'WorkingSetSize', 'QuotaPeakPagedPoolUsage', 'QuotaPagedPoolUsage',
                'QuotaPeakNonPagedPoolUsage', 'QuotaNonPagedPoolUsage', 'PagefileUsage', 'PeakPagefileUsage',
                'PrivateUsage')]

    counters = Counters()
    counters.cb = ctypes.sizeof(counters)
    if not ctypes.windll.psapi.GetProcessMemoryInfo(game.handle, ctypes.byref(counters), counters.cb):
        return None
    return round(counters.PrivateUsage / 2**20)


def instance_report(game, log_name):
    world = game.world()
    state = game.ptr(world + 0x1B0)
    driver = game.ptr(world + 0x38)
    found, stale = scan(game)
    states = game.ptr(state + 0x2C0) if state else 0
    count = game.i32(state + 0x2C8) if state else 0
    player_array = [game.ptr(states + 8 * i) for i in range(max(0, min(count, 16)))] if states else []
    # Figur je PlayerState (PawnPrivate), auch wenn sie ausserhalb der aktuellen World haengt oder schon geloescht ist.
    pawns = []
    for player_state in player_array:
        pawn = game.ptr(player_state + PLAYER_STATE_PAWN) if player_state else 0
        position = location(game, pawn) if pawn else None
        pawns.append({'pawn': game.name(pawn) if pawn else None, 'in_world': pawn in found['DDCharacter'],
                      'position': [round(v) for v in position] if position else None})
    characters = []
    for actor in found['DDCharacter']:
        position = location(game, actor)
        role = (game.read(actor + 0x168, 1) or b'\0')[0]
        characters.append({'role': role, 'position': [round(v) for v in position] if position else None})
    buses = [location(game, actor) for actor in found['Bus']]
    buses = [bus for bus in buses if bus]
    for character in characters:
        if buses and character['position']:
            character['bus_distance'] = round(min(math.dist(character['position'], bus) for bus in buses))
    report = {
        'world': game.name(world),
        'world_ptr': hex(world),
        'role': ('client' if game.ptr(driver + 0x100) else 'host') if driver else None,
        'player_array': len(player_array),
        'player_array_valid': all(player_array) and len(set(player_array)) == len(player_array),
        'player_states': len(found['DDPlayerState']),
        'controllers': len(found['DDPlayerController']),
        'characters': characters,
        'pawns': pawns,
        'map_tiles': len(found['MapTile']),
        'buses': [[round(v) for v in bus] for bus in buses],
        # Ein gesetzter PauserPlayerState haelt die Welt an, dann startet die Mission nicht und niemand steigt am Bus aus.
        'paused_by': [game.name(game.ptr(ws + WORLD_SETTINGS_PAUSER)) for ws in found['WorldSettings']
                      if game.ptr(ws + WORLD_SETTINGS_PAUSER)],
        'stale': {root: n for root, n in stale.items() if n},
        'uobjects': game.i32(game.base + getattr(game, 'gobjects', guidcache.GOBJECTS) + 8),
        'private_mb': private_megabytes(game),
    }
    if driver:
        num, rows = guidcache.entries(game)
        report['guid_slots'] = num  # SparseArray high-water mark, not the live entry count.
        report['guid_entries'] = len(rows)
        report['guid_player'] = sorted(
            f'{key:#x}' for key in rows
            if key & 1 and any(p in guidcache.full_path(rows, key) for p in GUID_PATTERNS))
    report['new_actor_failed'] = count_log(log_name, 'NewActor failed')
    return report


def count_log(log_name, pattern):
    path = os.path.join(LOG_DIR, log_name)
    if not os.path.exists(path):
        return None
    with open(path, encoding='utf-8', errors='replace') as handle:
        return sum(1 for line in handle if pattern in line)


def load_state():
    try:
        with open(STATE_FILE, encoding='utf-8') as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return {}


def save_state(state):
    os.makedirs(os.path.dirname(STATE_FILE), exist_ok=True)
    with open(STATE_FILE, 'w', encoding='utf-8') as handle:
        json.dump(state, handle)


def evaluate(host, client, previous, expected_world):
    problems = []
    for name, report in (('host', host), ('client', client)):
        if report['world'] != expected_world:
            problems.append(f'{name}: World {report["world"]}')
        if report['player_array'] != 2 or not report['player_array_valid']:
            problems.append(f'{name}: PlayerArray {report["player_array"]}')
        if report['player_states'] != 2:
            problems.append(f'{name}: {report["player_states"]} PlayerStates in der World')
        if report['paused_by']:
            problems.append(f'{name}: Welt pausiert von {report["paused_by"][0]}')
        if len(report['characters']) != 2:
            problems.append(f'{name}: {len(report["characters"])} Figuren')
    if expected_world == MISSION_WORLD and not host['buses']:
        problems.append('host: kein Bus gefunden')
    for character in host['characters'] if expected_world == MISSION_WORLD else []:
        if character.get('bus_distance', BUS_RADIUS + 1) > BUS_RADIUS:
            problems.append(f'host: Figur {character["position"]} nicht am Bus')
    host_positions = [c['position'] for c in host['characters'] if c['position']]
    for character in client['characters']:
        if character['position'] and host_positions and \
                min(math.dist(character['position'], p) for p in host_positions) > POSITION_TOLERANCE:
            problems.append(f'client: Figur {character["position"]} weicht vom Host ab')
    missing = sorted(set(host.get('guid_player', [])) - set(client.get('guid_player', [])))
    if missing:
        problems.append(f'client: NetGUIDs fehlen {missing}')
    failed = client['new_actor_failed']
    if failed is not None and failed > previous.get('new_actor_failed', 0):
        problems.append(f'client: {failed - previous.get("new_actor_failed", 0)} neue NewActor failed')
    # Shrinking alone is normal cleanup of dynamic/NoLoad records. Required identities and new actor failures above
    # remain acceptance failures; keeping every old actor GUID is neither necessary nor desirable.
    return problems


def players(game, world):
    """Spieler im GameState der World, 0 solange die World noch keinen GameState hat."""
    state = game.ptr(world + 0x1B0) if world else 0
    return game.i32(state + 0x2C8) if state else 0


def wait_new_world(host_pid, client_pid, before, timeout, expected_world, expected_players=2):
    """Wartet, bis beide eine andere World als vorher haben, es die erwartete ist und ihr GameState alle Spieler kennt.
    Liefert die Ladezeit in Sekunden ab dem Aufruf. Waehrend des Ladens ist die World 0 oder eine Zwischenwelt, das zaehlt
    nicht als beendete Instanz. None: Instanz beendet oder Zeit abgelaufen."""
    start = time.time()
    games = {}
    while time.time() - start < timeout:
        worlds = []
        for pid in (host_pid, client_pid):
            game = games.get(pid) or open_game(pid)
            if game is None or not process_running(pid):
                return None
            games[pid] = game
            world = game.world()
            worlds.append((hex(world), game.name(world) if world else None, players(game, world)))
        if all(ptr != before.get(str(i)) and name == expected_world and count >= expected_players
               for i, (ptr, name, count) in enumerate(worlds)):
            return round(time.time() - start, 1)
        time.sleep(0.5)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('host', type=int)
    parser.add_argument('client', type=int)
    parser.add_argument('--wait', action='store_true')
    parser.add_argument('--timeout', type=int, default=120)
    parser.add_argument('--settle', type=int, default=10)
    parser.add_argument('--label', default='')
    parser.add_argument('--log')
    parser.add_argument('--world', default=MISSION_WORLD, help='erwartete World, Standard DDDynamic_Map')
    parser.add_argument('--reset', action='store_true', help='Vergleichsstand der letzten Pruefung verwerfen')
    parser.add_argument('--local-position', action='store_true', help='nur die Position der Host-Figur ausgeben, x y z')
    args = parser.parse_args()
    if args.local_position:
        game = Game(args.host)
        state = game.ptr(game.world() + 0x1B0)
        states = game.ptr(state + 0x2C0)
        for i in range(game.i32(state + 0x2C8)):
            player_state = game.ptr(states + 8 * i)
            controller = game.ptr(player_state + 0x158)
            if controller and not game.ptr(controller + PLAYER_CONTROLLER_NET_CONNECTION):
                position = location(game, game.ptr(player_state + PLAYER_STATE_PAWN))
                print(' '.join(str(round(v)) for v in position) if position else 'none')
                return
        print('none')
        return
    previous = {} if args.reset else load_state()
    if previous.get('pids') != [args.host, args.client]:
        previous = {}
    result = {'label': args.label, 'time': time.strftime('%Y-%m-%d %H:%M:%S')}
    if args.wait:
        result['load_seconds'] = wait_new_world(args.host, args.client, previous.get('worlds', {}), args.timeout, args.world)
        time.sleep(args.settle)
    games = [alive(args.host), alive(args.client)]
    if None in games:
        result['problems'] = ['Instanz beendet: ' + ', '.join(n for n, g in zip(('host', 'client'), games) if g is None)]
        result['ok'] = False
        print(json.dumps(result))
        sys.exit(2)
    host = instance_report(games[0], f'{args.host}.log')
    client = instance_report(games[1], f'{args.client}.log')
    result.update(host=host, client=client)
    result['problems'] = evaluate(host, client, previous, args.world)
    result['ok'] = not result['problems']
    save_state({'pids': [args.host, args.client], 'worlds': {'0': host['world_ptr'], '1': client['world_ptr']},
                'guid_entries': client.get('guid_entries', 0), 'new_actor_failed': client['new_actor_failed'] or 0})
    line = json.dumps(result)
    print(line)
    if args.log:
        with open(args.log, 'a', encoding='utf-8') as handle:
            handle.write(line + '\n')
    sys.exit(0 if result['ok'] else 1)


if __name__ == '__main__':
    main()
