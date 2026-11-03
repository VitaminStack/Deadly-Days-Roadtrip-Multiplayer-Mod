"""Synthetische Speicherbilder, kein Spiel/SDK und keine Windows-Abhaengigkeit."""
import importlib.util
from pathlib import Path
import struct
import unittest

SPEC = importlib.util.spec_from_file_location(
    "pause_state", Path(__file__).parents[1] / "tools/spike/pause_state.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class Memory:
    def __init__(self):
        self.rows, self.pointers, self.ints, self.names, self.bytes = [], {}, {}, {}, {}

    def add(self, address, cls, name, flags=0):
        class_address = address + 0x10000
        self.rows.append(address)
        self.pointers[address + 0x10] = class_address
        self.names.update({class_address: cls, address: name})
        self.ints[address + 8] = flags

    def objects(self):
        return iter(self.rows)

    def ptr(self, address):
        return self.pointers.get(address, 0)

    def i32(self, address):
        return self.ints.get(address, 0)

    def name(self, address):
        return self.names.get(address)

    def world(self):
        return 0

    def read(self, address, size):
        return self.bytes.get(address)

    def value(self, address, fmt, *values):
        self.bytes[address] = struct.pack(fmt, *values)


class SnapshotTests(unittest.TestCase):
    def snapshot(self, memory):
        return MODULE.snapshot(123, lambda _: memory)

    def test_local_request_does_not_imply_world_pause(self):
        memory = Memory()
        memory.add(0x1000, "TimescaleManager", "TimescaleManager")
        memory.add(0x2000, "WorldSettings", "WorldSettings")
        memory.pointers[0x1040] = 0x3000
        memory.names[0x3000] = "PauseScreenManager"
        memory.value(0x23E0, "<f", 1.0)
        result = self.snapshot(memory)
        self.assertEqual(result["timescale"][0]["active_requester"], "PauseScreenManager")
        self.assertEqual(result["world_settings"][0]["time_dilation"], 1.0)
        self.assertEqual(result["pid"], 123)

    def test_replicated_world_pause_needs_no_local_session(self):
        memory = Memory()
        memory.add(0x1000, "WorldSettings", "WorldSettings")
        memory.value(0x13E0, "<f", 0.0001)
        result = self.snapshot(memory)
        self.assertEqual(result["ui_sessions"], [])
        self.assertAlmostEqual(result["world_settings"][0]["time_dilation"], 0.0001)

    def test_default_and_archetype_are_not_live_animation_containers(self):
        memory = Memory()
        for address, flags in ((0x1000, 0x10), (0x2000, 0x20), (0x3000, 0)):
            memory.add(address, "PanelAnimationContainer", "Container", flags)
            memory.value(address + 0x328, "<?", True)
        result = self.snapshot(memory)
        self.assertEqual(len(result["animation_containers"]), 1)
        self.assertTrue(result["animation_containers"][0]["open"])

    def test_nested_sessions_preserve_parent_and_blocking_state(self):
        memory = Memory()
        memory.add(0x1000, "CoopInputRouterSubsystem", "Router")
        memory.pointers[0x10A8] = 0x5000
        memory.ints[0x10B0] = 2
        for address, session, parent in ((0x5000, 7, -1), (0x501C, 8, 7)):
            memory.ints[address] = session
            memory.ints[address + 4] = parent
            memory.value(address + 0x1A, "<?", True)
        result = self.snapshot(memory)
        self.assertEqual([row["parent"] for row in result["ui_sessions"]], [-1, 7])
        self.assertTrue(all(row["blocks_gameplay"] for row in result["ui_sessions"]))


if __name__ == "__main__":
    unittest.main()
