import importlib.util
from pathlib import Path
import struct
import unittest

SPEC = importlib.util.spec_from_file_location("kismet_audit",
    Path(__file__).parents[1] / "tools/spike/kismet_audit.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class AuditTests(unittest.TestCase):
    def test_valid_final_call(self):
        script = b"\x68" + struct.pack("<Q", 123) + b"\x27\x16\x04\x0b\x53"
        result = MODULE.audit(script, {123: "Example.Call"})
        self.assertTrue(result["complete"])
        self.assertEqual(result["verified_calls"], ["Example.Call"])

    def test_fake_call_inside_constant_is_not_a_call(self):
        script = b"\x1f\x68" + struct.pack("<Q", 0x4141414141414141) + b"\0\x53"
        result = MODULE.audit(script, {0x4141414141414141: "False.Positive"})
        self.assertTrue(result["complete"])
        self.assertEqual(result["verified_calls"], [])

    def test_jump_target_must_be_a_statement_boundary(self):
        for target, valid in ((5, True), (1, False), (99, False)):
            result = MODULE.audit(b"\x06" + struct.pack("<I", target) + b"\x04\x0b\x53", {})
            self.assertEqual(result["complete"], valid)

    def test_conditional_and_computed_jumps_are_distinct(self):
        script = b"\x07" + struct.pack("<I", 6) + b"\x27\x4e\x25\x53"
        result = MODULE.audit(script, {})
        self.assertTrue(result["complete"])
        self.assertEqual(result["conditional_jumps"], 1)
        self.assertEqual(result["computed_jumps"], 1)

    def test_truncation_unknown_token_and_unresolved_call_fail_closed(self):
        for script in (b"\x68\x01", b"\xff\x53", b"\x68" + b"\0" * 8 + b"\x16\x53"):
            self.assertFalse(MODULE.audit(script, {})["complete"])

    def test_trailing_bytes_and_missing_terminator_are_rejected(self):
        for script in (b"\x53\x0b", b"\x0b", b"\x68" + struct.pack("<Q", 123) + b"\x53"):
            self.assertFalse(MODULE.audit(script, {123: "Example.Call"})["complete"])

    def test_dispatch_entry_must_target_callee_statement(self):
        functions = {123: "Example.ExecuteUbergraph_Test"}
        scripts = {functions[123]: b"\x0b\x04\x0b\x53"}
        for entry, valid in ((1, True), (2, False), (99, False)):
            script = b"\x46" + struct.pack("<Q", 123) + b"\x1d" + struct.pack("<i", entry) + b"\x16\x53"
            result = MODULE.audit(script, functions, scripts)
            self.assertTrue(result["complete"])
            self.assertEqual(result["dispatch_entries_valid"], valid)


if __name__ == "__main__":
    unittest.main()
