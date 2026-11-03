import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("trace_summary",
    Path(__file__).parents[1] / "tools/spike/blueprint_trace_summary.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class TraceSummaryTests(unittest.TestCase):
    def test_installed_is_not_executed(self):
        result = MODULE.summarize(["1 hook Example.Event", "2 hooks=1"])
        self.assertTrue(result["installation_count_matches"])
        self.assertEqual(result["observed_enter_lines"], {})

    def test_incomplete_installation_is_not_success(self):
        result = MODULE.summarize(["1 hook Example.Event", "2 hooks=2"])
        self.assertFalse(result["installation_count_matches"])

    def test_capped_output_and_snapshot_are_separate(self):
        result = MODULE.summarize(["1 enter Example.Event class=Example depth=0",
                                  "2 count Example.Event=400"])
        self.assertEqual(result["observed_enter_lines"], {"Example.Event": 1})
        self.assertEqual(result["last_counter_snapshot"], {"Example.Event": 400})

    def test_failed_stimulus_does_not_prove_event(self):
        result = MODULE.summarize(["1 action=1", "2 ApplyDamage return=0.000000"])
        self.assertEqual(result["actions"], [1])
        self.assertEqual(result["observed_enter_lines"], {})

    def test_authority_requires_actual_result(self):
        result = MODULE.summarize(["1 hook Actor.HasAuthority",
                                  "2 authority-result=false", "3 authority-result=true"])
        self.assertEqual(result["authority_results"], [False, True])

    def test_noise_and_prose_do_not_create_counts(self):
        result = MODULE.summarize(["hook Example.Event", "1 count not-a-count",
                                  "2 this mentions enter Example.Event"])
        self.assertEqual(result["installed"], [])
        self.assertEqual(result["observed_enter_lines"], {})


if __name__ == "__main__":
    unittest.main()
