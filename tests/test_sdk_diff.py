"""SDK-Vergleich mit selbst geschriebenen SDK-Ausschnitten im Stil von Dumper-7, ohne echtes SDK."""
import importlib.util
import io
from contextlib import redirect_stdout
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location('sdk_diff', TOOLS / 'sdk_diff.py')
sdk_diff = importlib.util.module_from_spec(SPEC)
sys.modules['sdk_diff'] = sdk_diff  # dataclasses braucht das Modul in sys.modules
SPEC.loader.exec_module(sdk_diff)

OLD_CLASSES = """
// Class CoreUObject.Object
// 0x0028 (0x0028 - 0x0000)
class alignas(0x08) UObject
{
public:
\tstatic inline class TUObjectArrayWrapper      GObjects;                 // 0x0000(0x0008)(NOT AUTO-GENERATED PROPERTY)
\tEObjectFlags                                  Flags;                    // 0x0008(0x0004)(NOT AUTO-GENERATED PROPERTY)
\tclass UObject*                                Outer;                    // 0x0020(0x0008)(NOT AUTO-GENERATED PROPERTY)

public:
\tstatic class UClass* StaticClass()
\t{
\t\treturn nullptr;
\t}
};

// Class Game.InventoryPanel
// 0x0400 (0x0800 - 0x0400)
class UInventoryPanel final : public UObject
{
public:
\tuint8                                         Pad_398[0x8];             // 0x0398(0x0008)(Fixing Size After Last Property [ Dumper-7 ])
\tbool                                          IsDragging;               // 0x0790(0x0001)(ZeroConstructor)
\tuint8                                         bIsActive : 1;            // 0x0388(0x0001)(BitIndex: 0x00, PropSize: 0x0001 (NoDestructor))
\tint32                                         OldField;                 // 0x0398(0x0004)(ZeroConstructor)

public:
\tvoid SetupInventory(class UInventoryComponent* Inventory);
\tbool HandleConfirmButton() const;
};

// Class Game.GA_Dash_C
// 0x0010 (0x03B0 - 0x03A0)
class UGA_Dash_C final : public UObject
{
public:
\tvoid K2_ActivateAbility();
};
"""

NEW_CLASSES = OLD_CLASSES.replace(
    "IsDragging;               // 0x0790", "IsDragging;               // 0x0788"
).replace(
    "\tint32                                         OldField;                 // 0x0398(0x0004)(ZeroConstructor)\n", ""
).replace(
    "bool HandleConfirmButton() const;", "bool HandleConfirmButton(int32 Slot) const;"
).replace("// 0x0010 (0x03B0 - 0x03A0)", "// 0x0018 (0x03B8 - 0x03A0)")

SOURCE = """
// Liest UInventoryPanel.IsDragging und UInventoryPanel.bIsActive, dazu UInventoryPanel.OldField.
constexpr auto Panel = "InventoryPanel";
auto Fn = FindObject("SetupInventory", "InventoryPanel");
auto Confirm = FindObject("HandleConfirmButton", "InventoryPanel");
auto Dash = "GA_Dash_C";
// Memory.Read ist kein SDK-Name und erscheint nicht.
"""

SDK_CHECKS = "static_assert(UObject::Flags == offsetof(SDK::UObject, Flags));\nstatic_assert(X == offsetof(SDK::UObject, Missing));\n"


def write_sdk(root, text):
    sdk = root / 'CppSDK' / 'SDK'
    sdk.mkdir(parents=True)
    (sdk / 'Game_classes.hpp').write_text(text, encoding='utf-8')
    return root


class SdkDiffTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.old = sdk_diff.load_sdk(write_sdk(root / 'old', OLD_CLASSES))
        self.new = sdk_diff.load_sdk(write_sdk(root / 'new', NEW_CLASSES))
        self.repo = root / 'repo'
        (self.repo / 'src' / 'Game').mkdir(parents=True)
        (self.repo / 'src' / 'Game' / 'Panel.cpp').write_text(SOURCE, encoding='utf-8')
        (self.repo / 'src' / 'Game' / 'SdkChecks.cpp').write_text(SDK_CHECKS, encoding='utf-8')
        (self.repo / 'docs').mkdir()
        (self.repo / 'docs' / 'OFFSETS.md').write_text('| `UGA_Dash_C.K2_ActivateAbility` | Hook |\n', encoding='utf-8')
        self.rows = {r['name']: r for r in sdk_diff.compare(self.old, self.new, sdk_diff.collect_uses(self.repo, [self.old, self.new]))}

    def tearDown(self):
        self.tmp.cleanup()

    def test_parser_reads_members_bits_functions_and_size(self):
        panel = self.old.classes['UInventoryPanel']
        self.assertEqual(panel.size, 0x800)
        self.assertEqual(panel.bases, ['UObject'])
        self.assertEqual(panel.members['IsDragging'].offset, 0x790)
        self.assertEqual(panel.members['bIsActive'].bit, 0)
        self.assertNotIn('Pad_398', panel.members)
        self.assertIn('SetupInventory', panel.functions)
        self.assertNotIn('GObjects', self.old.classes['UObject'].members)
        self.assertNotIn('StaticClass', self.old.classes['UObject'].functions)

    def test_statuses(self):
        self.assertEqual(self.rows['UInventoryPanel.IsDragging']['status'], 'verschoben')
        self.assertEqual(self.rows['UInventoryPanel.IsDragging']['new'], '0x788(0x1)')
        self.assertEqual(self.rows['UInventoryPanel.bIsActive']['status'], 'unverändert')
        self.assertEqual(self.rows['UInventoryPanel.OldField']['status'], 'verschwunden')
        self.assertEqual(self.rows['UInventoryPanel.SetupInventory']['status'], 'unverändert')
        self.assertEqual(self.rows['UInventoryPanel.HandleConfirmButton']['status'], 'Signatur geändert')
        self.assertEqual(self.rows['UGA_Dash_C']['status'], 'Größe geändert')
        self.assertEqual(self.rows['UObject.Flags']['status'], 'unverändert')
        self.assertEqual(self.rows['UObject.Missing']['status'], 'unbekannt')

    def test_uses_are_resolved_and_deduplicated(self):
        self.assertIn('UInventoryPanel', self.rows)  # aus "InventoryPanel" ohne Präfix
        self.assertEqual(sum(1 for name in self.rows if name == 'UGA_Dash_C.K2_ActivateAbility'), 1)
        self.assertFalse(any(name.startswith('Memory') for name in self.rows))
        self.assertTrue(self.rows['UInventoryPanel.SetupInventory']['where'].startswith('src/Game/Panel.cpp:'))

    def test_inherited_member_is_found(self):
        cls = self.old.resolve('InventoryPanel')
        self.assertEqual(self.old.find(cls, 'Outer', 'field').offset, 0x20)

    def test_main_exit_code_and_report(self):
        root = Path(self.tmp.name)
        out = io.StringIO()
        with redirect_stdout(out):
            code = sdk_diff.main(['--old', str(root / 'old'), '--new', str(root / 'new'), '--repo', str(self.repo)])
        self.assertEqual(code, 1)
        text = out.getvalue()
        self.assertIn('verschwunden (1)', text)
        self.assertIn('0x790(0x1) -> 0x788(0x1)', text)
        with redirect_stdout(io.StringIO()):
            same = sdk_diff.main(['--old', str(root / 'old'), '--new', str(root / 'old'), '--repo', str(self.repo)])
        self.assertEqual(same, 0)


if __name__ == '__main__':
    unittest.main()
