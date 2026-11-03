"""Prueft die Modulgrenzen aus docs/ARCHITECTURE.md an den Includes in src/."""
import re
import unittest
from pathlib import Path

SRC = Path(__file__).parents[1] / "src"
ALLOWED = {
    "Core": set(),
    "Loader": set(),
    "Net": {"Core"},
    "Game": {"Core"},
    "Sync": {"Core", "Net", "Game"},
    "UI": {"Core", "Net"},
    "Mod": {"Core", "Net", "Game", "Sync", "UI"},
}
INCLUDE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]', re.M)
SDK = re.compile(r'(^|/)(SDK|CppSDK)(/|\.hpp$)|^Basic\.hpp$|^Engine_', re.I)


def sources():
    for path in SRC.rglob("*"):
        if path.suffix in {".h", ".hpp", ".cpp", ".c", ".inl"}:
            yield path.relative_to(SRC).parts[0], path, INCLUDE.findall(path.read_text(encoding="utf-8"))


class ModuleBoundaryTests(unittest.TestCase):
    def test_every_module_is_known(self):
        modules = {path.name for path in SRC.iterdir() if path.is_dir()}
        self.assertEqual(modules, set(ALLOWED))

    def test_includes_follow_the_module_graph(self):
        for module, path, includes in sources():
            for include in includes:
                target = include.split("/")[0]
                if "/" in include and target in ALLOWED and target != module:
                    self.assertIn(target, ALLOWED[module], f"{path}: {include}")

    def test_sdk_only_in_game(self):
        for module, path, includes in sources():
            if module == "Game":
                continue
            for include in includes:
                self.assertIsNone(SDK.search(include), f"{path}: {include}")


if __name__ == "__main__":
    unittest.main()
