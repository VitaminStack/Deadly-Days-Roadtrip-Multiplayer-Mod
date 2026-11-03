"""Read-only roster/hash audit: players.py <host-pid>. Build 26187885; non-atomic snapshot."""
import json
import struct
import sys
import argparse


def pointer_hash(value):
    mask = (1 << 64) - 1
    value = ((value ^ (value >> 33)) * 0xFF51AFD7ED558CCD) & mask
    value = ((value ^ (value >> 33)) * 0xC4CEB9FE1A85EC53) & mask
    value ^= value >> 33
    return (value + 23 * (value >> 32)) & 0xFFFFFFFF


def audit(game):
    if not game.base:
        raise ValueError('Spielprozess nicht mehr lesbar')
    world = game.world()
    if not world:
        raise ValueError('World noch nicht vorhanden')
    mode, state = game.ptr(world + 0x1A8), game.ptr(world + 0x1B0)
    if not mode or not state:
        raise ValueError('Host-GameMode/GameState fehlt')
    cls = game.ptr(mode + 0x10)
    for _ in range(64):
        if not cls or game.name(cls) == 'DDMissionGameMode':
            break
        cls = game.ptr(cls + 0x40)
    if not cls or game.name(cls) != 'DDMissionGameMode':
        raise ValueError('GameMode besitzt kein DDMissionGameMode-Controller-Set')
    address = mode + 0x388
    raw = game.read(address, 0x50)
    if raw is None or len(raw) != 0x50:
        raise ValueError('TSet nicht lesbar')
    data, count, capacity = struct.unpack_from('<Qii', raw)
    secondary_bits, num_bits, max_bits, first_free, num_free = struct.unpack_from('<Qiiii', raw, 0x20)
    secondary_hash, hash_size = struct.unpack_from('<Qi', raw, 0x40)
    if not (0 <= count <= capacity <= 65536 and num_bits == count and num_bits <= max_bits and
            0 <= num_free <= count and 0 <= hash_size <= 8192 and
            (not hash_size or not hash_size & (hash_size - 1)) and
            (secondary_bits or num_bits <= 128) and (secondary_hash or hash_size <= 2)):
        raise ValueError('Unbekanntes oder inkonsistentes TSet-Layout')

    def read_exact(pointer, length):
        result = game.read(pointer, length) if length else b''
        if result is None or len(result) != length:
            raise ValueError('Snapshot waehrend Lesen nicht vollstaendig')
        return result

    bits = read_exact(secondary_bits or address + 0x10, ((num_bits + 31) // 32) * 4)
    entries = read_exact(data, count * 16)
    buckets = struct.unpack(f'<{hash_size}i', read_exact(secondary_hash or address + 0x38, hash_size * 4))
    allocated = {i for i in range(count) if bits[i // 8] & (1 << (i % 8))}
    rows = {i: struct.unpack_from('<Qii', entries, i * 16) for i in allocated}
    errors, linked = [], set()
    for bucket, index in enumerate(buckets):
        while index != -1:
            if index not in allocated or index in linked:
                errors.append('invalid_or_repeated_hash_link')
                break
            linked.add(index)
            controller, index_next, stored_bucket = rows[index]
            if stored_bucket != bucket:
                errors.append('stored_bucket_mismatch')
            if controller and pointer_hash(controller) & (hash_size - 1) != bucket:
                errors.append('key_hash_mismatch')
            index = index_next
    if linked != allocated or len(allocated) != count - num_free:
        errors.append('allocation_hash_mismatch')
    states, state_count = game.ptr(state + 0x2C0), game.i32(state + 0x2C8)
    if not states or not 0 < state_count <= 16:
        raise ValueError('PlayerArray fehlt oder ist unvollstaendig')
    current = [game.ptr(game.ptr(states + 8 * i) + 0x158) for i in range(state_count)]
    actual = [row[0] for row in rows.values()]
    if not all(current) or len(current) != len(set(current)):
        errors.append('incomplete_current_roster')
    if len(actual) != len(set(actual)):
        errors.append('duplicate_controller')
    return {'world': game.name(world), 'allocated': len(allocated), 'free': num_free,
            'hash_size': hash_size, 'hash_valid': not errors, 'errors': errors,
            'roster_equal': sorted(current) == sorted(actual),
            'controllers': [game.name(controller) for controller in actual]}


if __name__ == '__main__':
    from ddmem import Game
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int)
    parser.add_argument('--expected-players', type=int, choices=range(1, 17), default=2)
    args = parser.parse_args()
    try:
        result = audit(Game(args.pid))
        result['expected_players'] = args.expected_players
        result['player_count_equal'] = result['allocated'] == args.expected_players
        print(json.dumps(result))
        sys.exit(0 if result['hash_valid'] and result['roster_equal'] and result['player_count_equal'] else 1)
    except ValueError as error:
        raise SystemExit(f'Rostermessung nicht moeglich: {error}')
