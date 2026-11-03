"""Read-only-Pausensnapshot: pause_state.py <pid> [<pid> ...]. Nur Build 26187885.

Keine atomare Aufnahme; Vorher/Nachher vergleichen. Felder aus dem Mission-SDK,
keine Blueprint-Implementierungen oder SDK-Dateien im Repository.
"""
import json
import struct
import sys
import time

def snapshot(pid, game_factory=None):
    if game_factory is None:
        from ddmem import Game
        game_factory = Game
    game = game_factory(pid)
    result = {"pid": pid, "captured_at": time.time(),
              "world": game.name(game.world()), "timescale": [], "pause_screens": [],
              "ui_sessions": [], "world_settings": [], "panel_defaults": [], "panels": [],
              "animation_containers": []}
    classes = {}

    def ancestors(cls):
        if cls not in classes:
            names, current = [], cls
            for _ in range(40):
                if not current:
                    break
                names.append(game.name(current))
                current = game.ptr(current + 0x40)
            classes[cls] = names
        return classes[cls]

    def fields(address, fmt):
        size = struct.calcsize(fmt)
        data = game.read(address, size)
        if not data or len(data) != size:
            return None
        values = struct.unpack(fmt, data)
        return values[0] if len(values) == 1 else values

    for obj in game.objects():
        flags = game.i32(obj + 8)
        chain = ancestors(game.ptr(obj + 0x10))
        if flags & 0x10:
            if "AbstractPanel" in chain and any(
                    name in chain for name in ("InventoryPanel", "AbstractUpgradePanel")):
                result["panel_defaults"].append({"class": chain[0],
                    "use_coop_session": fields(obj + 0x348, "<?"),
                    "blocks_gameplay": fields(obj + 0x349, "<?"),
                    "pause_while_open": fields(obj + 0x34B, "<?")})
            continue
        if flags & 0x20:  # Archetypen sind keine laufenden Komponenten.
            continue
        if "PanelAnimationContainer" in chain:
            result["animation_containers"].append({"name": game.name(obj),
                "outer": game.name(game.ptr(obj + 0x20)),
                "open": fields(obj + 0x328, "<?")})
        if "AbstractPanel" in chain:
            animation = game.ptr(obj + 0x330)
            result["panels"].append({"name": game.name(obj),
                "open": fields(animation + 0x328, "<?") if animation else None,
                "blocks_gameplay": fields(obj + 0x349, "<?"),
                "pause_while_open": fields(obj + 0x34B, "<?")})
        if "TimescaleManager" in chain:
            result["timescale"].append({"active_requester": game.name(game.ptr(obj + 0x40)),
                "request_slots": game.i32(obj + 0x50), "request_free": game.i32(obj + 0x7C)})
        elif "PauseScreenManager" in chain:
            result["pause_screens"].append({"screen": game.name(game.ptr(obj + 0xD8)),
                "outer": game.name(game.ptr(obj + 0x20)),
                "time_request": fields(obj + 0xE4, "<fi?"),
                "blockers": game.i32(obj + 0x108), "open_requests": game.i32(obj + 0x118)})
        elif "CoopInputRouterSubsystem" in chain:
            data, count = game.ptr(obj + 0xA8), game.i32(obj + 0xB0)
            if 0 <= count <= 64:
                for i in range(count):
                    row = data + i * 0x1C
                    result["ui_sessions"].append({"id": game.i32(row),
                        "parent": game.i32(row + 4), "authority_player": game.i32(row + 0x10),
                        "blocks_gameplay": fields(row + 0x1A, "<?")})
        elif "WorldSettings" in chain:
            result["world_settings"].append({"name": game.name(obj),
                "outer": game.name(game.ptr(obj + 0x20)),
                "time_dilation": fields(obj + 0x3E0, "<f"),
                "cinematic_time_dilation": fields(obj + 0x3E4, "<f"),
                "pauser": game.name(game.ptr(obj + 0x4A8))})
    return result


if __name__ == "__main__":
    for process_id in sys.argv[1:]:
        print(json.dumps(snapshot(int(process_id)), ensure_ascii=True))
