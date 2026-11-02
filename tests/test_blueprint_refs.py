"""Nur synthetische Daten, keine Kismet-Bytes aus dem Spiel."""
import importlib.util
import struct
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("blueprint_refs", Path(__file__).parents[1] / "tools/spike/blueprint_refs.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ReferenceTests(unittest.TestCase):
    def test_known_function_after_call(self):
        data = b"\x68" + struct.pack("<Q", 0x12345678)
        self.assertEqual(module.candidate_calls(data, {0x12345678: "Example.Call"}), ["Example.Call"])

    def test_unknown_pointer(self):
        self.assertEqual(module.candidate_calls(b"\x1c" + struct.pack("<Q", 1), {}), [])

    def test_object_constant_is_not_call(self):
        self.assertEqual(module.candidate_calls(b"\x20" + struct.pack("<Q", 1), {1: "Example.Call"}), [])

    def test_short_input(self):
        self.assertEqual(module.candidate_calls(b"\x68\x01", {}), [])

    def test_deduplicate(self):
        data = (b"\x46" + struct.pack("<Q", 1)) * 2
        self.assertEqual(module.candidate_calls(data, {1: "Example.Call"}), ["Example.Call"])


if __name__ == "__main__":
    unittest.main()
