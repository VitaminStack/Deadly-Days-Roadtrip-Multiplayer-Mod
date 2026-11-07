"""Build einfrieren ohne Spiel/Steam: Kopierisolation und fail-closed Dateipruefung."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('game_build', Path(__file__).parents[1] / 'tools/game_build.py')
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)


class FrozenBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'steamapps/common/game'
        self.source.mkdir(parents=True)
        self.destination = self.root / 'frozen'
        self.exe = 'DDSurvivors/Binaries/Win64/DDSurvivors-Win64-Shipping.exe'
        self.pak = 'DDSurvivors/Content/Paks/game.pak'
        for name, content in ((self.exe, b'known-exe'), (self.pak, b'known-pak'), ('Engine/engine.dll', b'known-dll')):
            file = self.source / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(content)
        reference = {name: hashlib.sha256((self.source / name).read_bytes()).hexdigest() for name in (self.exe, self.pak)}
        self.reference = patch.object(build, 'REFERENCES', {build.BUILD: reference, '1': {self.exe: '0' * 64}})
        self.reference.start()
        self.addCleanup(self.reference.stop)

    def freeze(self):
        return build.freeze(self.source, self.destination)

    def test_full_inventory_and_no_personal_source_path(self):
        receipt = self.freeze()
        self.assertEqual(len(receipt['files']), 3)
        self.assertNotIn(str(self.source), json.dumps(receipt))
        self.assertEqual(build.verify(self.destination), receipt)

    def test_steam_update_does_not_touch_frozen_copy(self):
        self.freeze()
        (self.source / self.exe).write_bytes(b'new-steam-build')
        (self.source / self.pak).unlink()
        build.verify(self.destination)
        self.assertEqual((self.destination / self.exe).read_bytes(), b'known-exe')
        self.assertEqual((self.destination / self.exe).stat().st_nlink, 1)

    def test_modified_exe_rejected_even_with_original_receipt(self):
        self.freeze()
        (self.destination / self.exe).write_bytes(b'new-build')
        with self.assertRaises(ValueError):
            build.verify(self.destination)

    def test_modified_non_reference_dll_rejected(self):
        self.freeze()
        (self.destination / 'Engine/engine.dll').write_bytes(b'changed')
        with self.assertRaises(ValueError):
            build.verify(self.destination)

    def test_added_pak_rejected(self):
        self.freeze()
        (self.destination / 'DDSurvivors/Content/Paks/extra.pak').write_bytes(b'addon')
        with self.assertRaises(ValueError):
            build.verify(self.destination)

    def test_added_source_pak_is_not_accepted_as_part_of_the_build(self):
        (self.source / 'DDSurvivors/Content/Paks/extra.pak').write_bytes(b'addon')
        with self.assertRaises(ValueError):
            self.freeze()
        self.assertFalse(self.destination.exists())

    def test_missing_file_rejected(self):
        self.freeze()
        (self.destination / 'Engine/engine.dll').unlink()
        with self.assertRaises(ValueError):
            build.verify(self.destination)

    def test_unsupported_source_leaves_no_destination(self):
        (self.source / self.exe).write_bytes(b'unknown-build')
        with self.assertRaises(ValueError):
            self.freeze()
        self.assertFalse(self.destination.exists())

    def test_existing_destination_never_overwritten(self):
        self.destination.mkdir()
        marker = self.destination / 'user-file'
        marker.write_bytes(b'keep')
        with self.assertRaises(ValueError):
            self.freeze()
        self.assertEqual(marker.read_bytes(), b'keep')

    def test_steamapps_destination_rejected(self):
        with self.assertRaises(ValueError):
            build.freeze(self.source, self.root / 'SteamApps/frozen')

    def test_nested_destination_rejected(self):
        outside_source = self.root / 'source'
        outside_source.mkdir()
        with self.assertRaises(ValueError):
            build.freeze(outside_source, outside_source / 'frozen')

    def test_unknown_build_rejected(self):
        with self.assertRaises(ValueError):
            build.freeze(self.source, self.destination, '999')

    def test_saved_and_own_mod_not_copied_but_allowed_at_runtime(self):
        for name in ('DDSurvivors/Saved/log.txt', build.MOD_PROXY, 'DDSurvivors/Binaries/Win64/DDCoop/DDCoop.dll'):
            file = self.source / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b'local')
        self.freeze()
        self.assertFalse((self.destination / build.MOD_PROXY).exists())
        for name in ('DDSurvivors/Saved/log.txt', build.MOD_PROXY, 'DDSurvivors/Binaries/Win64/DDCoop/DDCoop.dll'):
            file = self.destination / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b'local')
        build.verify(self.destination)

    def test_forged_reference_hash_rejected(self):
        receipt = self.freeze()
        receipt['files'][self.exe]['sha256'] = '0' * 64
        (self.destination / build.RECEIPT).write_text(json.dumps(receipt), encoding='utf-8')
        with self.assertRaises(ValueError):
            build.verify(self.destination)

    def test_empty_receipt_rejected(self):
        self.freeze()
        (self.destination / build.RECEIPT).write_text('{}', encoding='utf-8')
        with self.assertRaises(ValueError):
            build.verify(self.destination)

    def test_hardlinked_source_rejected(self):
        os.link(self.source / self.exe, self.source / 'linked.exe')
        with self.assertRaises(ValueError):
            self.freeze()

    def test_hardlinked_snapshot_rejected(self):
        self.freeze()
        os.link(self.destination / self.exe, self.root / 'outside.exe')
        with self.assertRaises(ValueError):
            build.verify(self.destination)

    def test_receipt_path_traversal_rejected(self):
        receipt = self.freeze()
        receipt['files']['../outside.dll'] = {'size': 1, 'sha256': '0' * 64}
        (self.destination / build.RECEIPT).write_text(json.dumps(receipt), encoding='utf-8')
        with self.assertRaises(ValueError):
            build.verify(self.destination)

    def test_source_change_during_copy_does_not_publish_partial_copy(self):
        copyfile = build.shutil.copyfile
        def copy_and_mutate(source, target):
            copyfile(source, target)
            if str(source).endswith('engine.dll'):
                Path(source).write_bytes(b'update')
        with patch.object(build.shutil, 'copyfile', side_effect=copy_and_mutate):
            with self.assertRaises(ValueError):
                self.freeze()
        self.assertFalse(self.destination.exists())
        self.assertEqual(list(self.root.glob('frozen.partial-*')), [])

    def test_unreadable_subtree_aborts_instead_of_creating_partial_receipt(self):
        def denied_walk(root, **options):
            options['onerror'](PermissionError('unreadable subtree'))
        with patch.object(build.os, 'walk', side_effect=denied_walk):
            with self.assertRaises(PermissionError):
                self.freeze()
        self.assertFalse(self.destination.exists())

    @unittest.skipUnless(shutil.which('powershell'), 'Windows PowerShell erforderlich')
    def test_pinned_path_has_priority_and_never_falls_back(self):
        props = self.root / 'Local.props'
        # Die Steam-Quelle waere fuer die alte ACF-Pruefung gueltig.
        (self.root / 'steamapps/appmanifest_3026450.acf').write_text('"buildid" "26187885"', encoding='utf-8')
        common = Path(__file__).parents[1] / 'tools/spike/Common.ps1'
        for pinned_exists in (False, True):
            if pinned_exists:
                self.destination.mkdir()
            props.write_text(f'<Project><PropertyGroup><GameDir>{self.source}</GameDir>'
                             f'<PinnedGameDir>{self.destination}</PinnedGameDir></PropertyGroup></Project>', encoding='utf-8')
            quote = lambda path: str(path).replace("'", "''")
            code = (f". '{quote(common)}'; function Find-LocalProps {{ '{quote(props)}' }}; "
                    "try { Assert-SupportedBuild; exit 0 } catch { Write-Output 'BLOCKED'; exit 7 }")
            result = subprocess.run(['powershell', '-NoProfile', '-Command', code], capture_output=True)
            self.assertEqual(result.returncode, 7, result.stdout)
            self.assertIn(b'BLOCKED', result.stdout)


if __name__ == '__main__':
    unittest.main()
