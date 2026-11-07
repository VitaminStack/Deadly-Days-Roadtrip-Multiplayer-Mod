"""Read-only remote-P2 snapshot, build 26187885. No writes or native calls; not an atomic snapshot."""
import argparse
import json
import struct


def snapshot(game):
    def exact(address, size):
        raw = game.read(address, size)
        if raw is None or len(raw) != size:
            raise ValueError(f'incomplete read at {address:#x}')
        return raw

    def is_a(obj, name):
        cls = game.ptr(obj + 0x10) if obj else 0
        for _ in range(64):
            if not cls:
                return False
            if game.name(cls) == name:
                return True
            cls = game.ptr(cls + 0x40)
        return False

    def weak(index, serial):
        if index <= 0 or index >= game.i32(game.base + getattr(game, 'gobjects', 0xAA61B00) + 8):
            return 0
        chunks = game.ptr(game.base + getattr(game, 'gobjects', 0xAA61B00))
        chunk = game.ptr(chunks + 8 * (index >> 16))
        item = chunk + (index & 0xFFFF) * 0x18 if chunk else 0
        return game.ptr(item + 8) if item and serial > 0 and game.i32(item + 0x10) == serial else 0

    world = game.world()
    instance = game.ptr(world + 0x228)
    driver = game.ptr(world + 0x38)
    state = game.ptr(world + 0x1B0)
    if not world or not instance:
        raise ValueError('world/GameInstance missing')
    local_count = game.i32(instance + 0x40)
    states, count = game.ptr(state + 0x2C0), game.i32(state + 0x2C8)
    if not 0 <= count <= 16 or not 0 <= local_count <= 16:
        raise ValueError('unexpected player count')
    controllers = []
    for i in range(count):
        ps = game.ptr(states + i * 8)
        pc = game.ptr(ps + 0x158)
        player = game.ptr(pc + 0x348)
        controllers.append({'address': hex(pc), 'name': game.name(pc), 'player_state': game.name(ps),
                            'player_class': game.name(game.ptr(player + 0x10)),
                            'local_player': is_a(player, 'LocalPlayer'),
                            'net_connection': bool(game.ptr(pc + 0x520))})
    objects = list(game.objects())
    coop = next((o for o in objects if is_a(o, 'LocalCoopSubsystem') and game.ptr(o + 0x20) == instance
                 and not game.i32(o + 8) & 0x10), 0)
    result = {'world': game.name(world), 'local_players': local_count, 'player_states': count,
              'role': 'client' if driver and game.ptr(driver + 0x100) else 'host' if driver else 'standalone',
              'controllers': controllers, 'coop': None, 'router': None}
    if coop:
        raw = exact(coop + 0x100, 0x50)
        data, num, capacity = struct.unpack_from('<Qii', raw)
        secondary, bits_count, max_bits, first_free, free = struct.unpack_from('<Qiiii', raw, 0x20)
        if not (0 <= num <= capacity <= 4096 and bits_count == num <= max_bits and 0 <= free <= num):
            raise ValueError('unexpected ActivePlayers layout')
        if not secondary and num > 128:
            raise ValueError('missing ActivePlayers bits')
        bits = exact(secondary or coop + 0x110, ((num + 31) // 32) * 4) if num else b''
        hash_pointer, hash_size = struct.unpack_from('<Qi', raw, 0x40)
        if not (0 <= hash_size <= 32 and (not hash_size or not hash_size & (hash_size - 1))):
            raise ValueError('unexpected ActivePlayers hash size')
        if not hash_pointer and hash_size > 2:
            raise ValueError('missing ActivePlayers hash')
        buckets = struct.unpack(f'<{hash_size}i', exact(hash_pointer or coop + 0x138, hash_size * 4)) if hash_size else []
        entries = []
        slots = {}
        for i in range(num):
            if not bits[i // 8] & (1 << (i % 8)):
                continue
            key, index, serial, link, bucket = struct.unpack('<iiiii', exact(data + i * 0x14, 0x14))
            pc = weak(index, serial)
            slots[i] = (key, link, bucket)
            entries.append({'slot': i, 'key': key, 'index': index, 'serial': serial, 'controller': hex(pc),
                            'name': game.name(pc), 'hash_next': link, 'bucket': bucket})
        if len(entries) != num - free or len({e['key'] for e in entries}) != len(entries):
            raise ValueError('ActivePlayers allocation or duplicate key')
        linked = set()
        for bucket, slot in enumerate(buckets):
            while slot != -1:
                if slot not in slots or slot in linked:
                    raise ValueError('ActivePlayers hash link missing or cyclic')
                key, following, stored_bucket = slots[slot]
                if key not in (0, 1) or stored_bucket != bucket or key & (hash_size - 1) != bucket:
                    raise ValueError('ActivePlayers hash/key mismatch')
                linked.add(slot)
                slot = following
        if linked != set(slots):
            raise ValueError('ActivePlayers unlinked allocation')
        flags = exact(coop + 0x180, 0x10)
        result['coop'] = {'address': hex(coop), 'enabled': bool(flags[0]),
                          'max_local_players': struct.unpack_from('<i', flags, 4)[0],
                          'flags_188_18f': flags[8:].hex(), 'reconnect_required': bool(flags[9]),
                          'recreate_p2': bool(flags[10]), 'active_players': entries, 'hash_valid': True}
    router = next((o for o in objects if is_a(o, 'CoopInputRouterSubsystem') and game.ptr(o + 0x20) == instance
                   and not game.i32(o + 8) & 0x10), 0)
    if router:
        data, num = game.ptr(router + 0xA8), game.i32(router + 0xB0)
        if not 0 <= num <= 64:
            raise ValueError('unexpected router stack')
        sessions = []
        for i in range(num):
            raw = exact(data + 0x1C * i, 0x1C)
            sessions.append({'id': struct.unpack_from('<i', raw)[0],
                             'authority': struct.unpack_from('<i', raw, 0x10)[0],
                             'blocks_gameplay': bool(raw[0x1A])})
        result['router'] = {'address': hex(router), 'sessions': sessions}
    return result


if __name__ == '__main__':
    from ddmem import Game
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pids', nargs='+', type=int)
    args = parser.parse_args()
    for pid in args.pids:
        try:
            print(json.dumps({'pid': pid, **snapshot(Game(pid))}))
        except ValueError as error:
            raise SystemExit(f'Remote-P2 snapshot failed: {error}')
