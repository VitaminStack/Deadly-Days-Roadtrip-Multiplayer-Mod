"""Negativtests fuer Patchlisten, fehlende Sites und Signaturen ohne Spieldateien."""
import unittest

from tools.check_patch_registry import check_patch, check_repository, check_sources

VALID = """
constexpr uintptr_t NewHook = 0x05967900;
bool CheckSold() { return true; }
bool ApplySold(int&) { return true; }
constexpr uintptr_t SoldSites[] = {NewHook};
const Patch List[] = {
    {"Verkaufen", CheckSold, ApplySold, SoldSites},
};
std::span<const Patch> RewardPatches() { return List; }
"""
SIGS = '{0x05967900, 0, "48 89", 0, "NewHook"}, // RewardPatches.cpp\n'


class PatchRegistryTests(unittest.TestCase):
    def test_real_repository(self):
        self.assertEqual(check_repository(), [])

    def test_valid_fixture(self):
        self.assertEqual(check_sources({"RewardPatches.cpp": VALID}, SIGS), [])

    def test_duplicate_registry_catches_previous_compiler_error(self):
        broken = VALID + '\nconst Patch List[] = {{"Nochmal", CheckSold, ApplySold, SoldSites}};\n'
        self.assertTrue(any("mehrfach definiert" in e
                            for e in check_sources({"RewardPatches.cpp": broken}, SIGS)))

    def test_missing_site_array(self):
        broken = VALID.replace("constexpr uintptr_t SoldSites[] = {NewHook};", "")
        self.assertTrue(any("Site-Array fehlt" in e
                            for e in check_sources({"RewardPatches.cpp": broken}, SIGS)))

    def test_new_hook_without_signature(self):
        self.assertTrue(any("ohne Signatur" in e
                            for e in check_sources({"RewardPatches.cpp": VALID},
                                                   '{0x05969750, 0, "48", 0, "Other"},\n')))

    def test_missing_callback(self):
        broken = VALID.replace("bool CheckSold() { return true; }", "")
        self.assertTrue(any("Callback fehlt" in e
                            for e in check_sources({"RewardPatches.cpp": broken}, SIGS)))

    def test_duplicate_signature_rows(self):
        errors = check_sources({"RewardPatches.cpp": VALID}, SIGS + SIGS)
        self.assertTrue(any("doppelte" in e for e in errors))

    def test_missing_export_registry(self):
        self.assertTrue(any("ohne Patch List" in e
                            for e in check_patch("RewardPatches.cpp",
                                                 "std::span<const Patch> RewardPatches() { return {}; }",
                                                 {0x05967900})))


if __name__ == "__main__":
    unittest.main()
