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

    def test_requests_list_live_entries_skip_free_slots_and_keep_orphans(self):
        memory = Memory()
        memory.add(0x1000, "TimescaleManager", "TimescaleManager")
        memory.add(0x2000, "GS_DDMissionState_C", "GameState")
        memory.ints[0x200C] = 7                      # Objektindex des lebenden Anfragenden
        memory.pointers[0x1048] = 0x5000             # TimeRequests.Data
        memory.ints[0x1050] = 3                      # drei Plaetze
        memory.ints[0x107C] = 1                      # einer frei
        memory.value(0x1058, "<I", 0b101)            # Platz 1 ist frei
        for slot, index, serial, values in ((0, 7, 3, (1.0, 20, True)), (1, 9, 9, (0.5, 99, True)),
                                            (2, 4242, 5, (0.0, 10, False))):
            row = 0x5000 + slot * 0x1C
            memory.ints[row] = index
            memory.ints[row + 4] = serial
            memory.value(row + 8, "<fi?", *values)
        entry = self.snapshot(memory)["timescale"][0]
        self.assertEqual((entry["request_slots"], entry["request_free"]), (3, 1))
        live, orphan = entry["requests"]
        self.assertEqual((live["requester"], live["class"], live["priority"], live["pause"]),
                         ("GameState", "GS_DDMissionState_C", 20, True))
        self.assertEqual((orphan["index"], orphan["serial"], orphan["requester"], orphan["timescale"]),
                         (4242, 5, None, 0.0))

    def test_requests_empty_without_data(self):
        memory = Memory()
        memory.add(0x1000, "TimescaleManager", "TimescaleManager")
        self.assertEqual(self.snapshot(memory)["timescale"][0]["requests"], [])


if __name__ == "__main__":
    unittest.main()
