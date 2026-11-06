"""Installer und Release-Paket ohne Spiel und Steam: nachgebauter Steam-Ordner in einem Temp-Verzeichnis."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).parents[1]
INSTALLER = ROOT / 'tools/installer/Install-DDCoop.ps1'
PACKAGER = ROOT / 'tools/New-ReleasePackage.ps1'
POWERSHELL = shutil.which('powershell') or shutil.which('pwsh')
APP_ID = '3026450'
EXE = 'DDSurvivors/Binaries/Win64/DDSurvivors-Win64-Shipping.exe'
# Kein laufender Prozess heißt so, damit eine laufende Testsitzung auf dem PC die Tests nicht stört.
NO_PROCESS = 'DDCoopInstallerTestNoSuchProcess'


def run_ps(script, *args):
    return subprocess.run([POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script), *args],
                          capture_output=True, text=True, timeout=120)


@unittest.skipUnless(POWERSHELL, 'PowerShell fehlt')
class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.steam = root / 'Steam'
        (self.steam / 'steamapps').mkdir(parents=True)
        # Spiel in einer zweiten Bibliothek, wie bei Installationen auf anderen Laufwerken.
        self.library = root / 'Library Two'
        self.game = self.library / 'steamapps/common/Deadly Days Roadtrip'
        (self.game / EXE).parent.mkdir(parents=True)
        (self.game / EXE).write_bytes(b'exe')
        self.save = self.game / 'DDSurvivors/Saved/SaveGames/slot.sav'
        self.save.parent.mkdir(parents=True)
        self.save.write_bytes(b'save')
        library_vdf = str(self.library).replace('\\', '\\\\')
        (self.steam / 'steamapps/libraryfolders.vdf').write_text(
            f'"libraryfolders"\n{{\n\t"0"\n\t{{\n\t\t"path"\t\t"{library_vdf}"\n\t}}\n}}\n', encoding='utf-8')
        self.write_manifest('26187885')
        self.source = root / 'package'
        (self.source / 'DDCoop').mkdir(parents=True)
        (self.source / 'version.dll').write_bytes(b'MZ loader ModStart v2')
        (self.source / 'DDCoop/DDCoop.dll').write_bytes(b'MZ mod v2')
        self.win64 = (self.game / EXE).parent

    def write_manifest(self, build):
        (self.library / f'steamapps/appmanifest_{APP_ID}.acf').write_text(
            f'"AppState"\n{{\n\t"appid"\t\t"{APP_ID}"\n\t"installdir"\t\t"Deadly Days Roadtrip"\n'
            f'\t"buildid"\t\t"{build}"\n}}\n', encoding='utf-8')

    def install(self, *args):
        return run_ps(INSTALLER, '-SteamPath', str(self.steam), '-Source', str(self.source),
                      '-GameProcess', NO_PROCESS, *args)

    def assert_failed(self, result, text):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(text, result.stderr + result.stdout)

    def test_install_finds_game_in_second_library_and_uninstall_keeps_config(self):
        result = self.install()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.win64 / 'version.dll').read_bytes(), b'MZ loader ModStart v2')
        self.assertEqual((self.win64 / 'DDCoop/DDCoop.dll').read_bytes(), b'MZ mod v2')
        (self.win64 / 'DDCoop/ddcoop.ini').write_text('enabled=false\n')

        result = self.install('-Uninstall')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.win64 / 'version.dll').exists())
        self.assertFalse((self.win64 / 'DDCoop/DDCoop.dll').exists())
        self.assertEqual((self.win64 / 'DDCoop/ddcoop.ini').read_text(), 'enabled=false\n')
        self.assertEqual(self.save.read_bytes(), b'save')

    def test_reinstall_updates_files_and_keeps_config(self):
        self.assertEqual(self.install().returncode, 0)
        (self.win64 / 'DDCoop/ddcoop.ini').write_text('log_level=debug\n')
        (self.source / 'DDCoop/DDCoop.dll').write_bytes(b'MZ mod v3')
        result = self.install()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.win64 / 'DDCoop/DDCoop.dll').read_bytes(), b'MZ mod v3')
        self.assertEqual((self.win64 / 'DDCoop/ddcoop.ini').read_text(), 'log_level=debug\n')

    def test_purge_removes_mod_folder_only(self):
        self.assertEqual(self.install().returncode, 0)
        result = self.install('-Uninstall', '-Purge')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.win64 / 'DDCoop').exists())
        self.assertTrue((self.game / EXE).exists())
        self.assertEqual(self.save.read_bytes(), b'save')

    def test_foreign_version_dll_untouched(self):
        (self.win64 / 'version.dll').write_bytes(b'MZ other mod')
        for args in ((), ('-Uninstall',)):
            self.assert_failed(self.install(*args), 'Fremde version.dll')
            self.assertEqual((self.win64 / 'version.dll').read_bytes(), b'MZ other mod')
        self.assertFalse((self.win64 / 'DDCoop').exists())

    def test_wrong_build_rejected(self):
        self.write_manifest('99999999')
        self.assert_failed(self.install(), 'Spiel-Build 99999999')
        self.assertFalse((self.win64 / 'version.dll').exists())

    def test_explicit_game_dir_without_manifest_rejected(self):
        (self.library / f'steamapps/appmanifest_{APP_ID}.acf').unlink()
        result = run_ps(INSTALLER, '-GameDir', str(self.game), '-Source', str(self.source), '-GameProcess', NO_PROCESS)
        self.assert_failed(result, 'Steam-Manifest nicht gefunden')
        self.assertFalse((self.win64 / 'version.dll').exists())

    def test_running_game_blocks_install(self):
        # Der Test selbst laeuft in PowerShell, also gibt es diesen Prozess sicher.
        result = run_ps(INSTALLER, '-SteamPath', str(self.steam), '-Source', str(self.source),
                        '-GameProcess', Path(POWERSHELL).stem)
        self.assert_failed(result, 'Das Spiel laeuft')
        self.assertFalse((self.win64 / 'version.dll').exists())

    def test_foreign_loader_in_package_rejected(self):
        (self.source / 'version.dll').write_bytes(b'MZ not ours')
        self.assert_failed(self.install(), 'nicht der Loader')
        self.assertFalse((self.win64 / 'version.dll').exists())

    def test_missing_game_reported(self):
        (self.library / f'steamapps/appmanifest_{APP_ID}.acf').unlink()
        self.assert_failed(self.install(), 'in keiner Steam-Bibliothek')


@unittest.skipUnless(POWERSHELL, 'PowerShell fehlt')
class PackageTests(unittest.TestCase):
    def test_package_contains_only_own_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            bin_dir = root / 'bin'
            bin_dir.mkdir()
            (bin_dir / 'version.dll').write_bytes(b'MZ loader ModStart')
            (bin_dir / 'DDCoop.dll').write_bytes(b'MZ mod')
            (bin_dir / 'DDCoop.pdb').write_bytes(b'debug')
            result = run_ps(PACKAGER, '-Version', 'test-1', '-Bin', str(bin_dir), '-OutputDir', str(root / 'dist'))
            self.assertEqual(result.returncode, 0, result.stderr)
            archive = root / 'dist/DDCoop-test-1.zip'
            with zipfile.ZipFile(archive) as package:
                names = sorted(name for name in package.namelist() if not name.endswith('/'))
                package.extractall(root / 'out')
            expected = sorted(f'DDCoop-test-1/{name}' for name in (
                'version.dll', 'DDCoop/DDCoop.dll', 'Install.cmd', 'Uninstall.cmd', 'Install-DDCoop.ps1',
                'LIESMICH.txt', 'LICENSE.txt', 'third_party/minhook-LICENSE.txt', 'SHA256SUMS.txt'))
            self.assertEqual(names, expected)
            sums = (root / 'out/DDCoop-test-1/SHA256SUMS.txt').read_text().splitlines()
            self.assertEqual(len(sums), len(expected) - 1)
            self.assertIn('  DDCoop/DDCoop.dll', '\n'.join(sums))

    def test_invalid_version_rejected(self):
        result = run_ps(PACKAGER, '-Version', '../evil', '-Bin', '.')
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
