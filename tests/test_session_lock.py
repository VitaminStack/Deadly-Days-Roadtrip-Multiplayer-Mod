"""Stop-Session.ps1 und die Sitzungssperre ohne Spiel: Kopie der Skripte in einem Temp-Ordner, Deploy-Mod.ps1 als Attrappe."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]
POWERSHELL = shutil.which('powershell') or shutil.which('pwsh')

# Die Attrappe haelt fest, wie die Sperre beim Entfernen der Mod aussah.
DEPLOY_STUB = r'''param([switch]$Remove)
. (Join-Path $PSScriptRoot 'spike\Common.ps1')
$Lock = Get-Content $script:LockFile -Raw | ConvertFrom-Json
@{ Own = ($Lock.Owner -eq $script:RepoRoot); Active = [bool](Test-LockActive $Lock); Host = [int]$Lock.Host } |
    ConvertTo-Json | Set-Content (Join-Path $script:RepoRoot 'removed.json') -Encoding ascii
'''
# Sperre so schreiben, wie Start-Session.ps1 es nach dem Start der Instanzen tut.
LOCK_HELPER = r'''param([int]$HostId)
. (Join-Path $PSScriptRoot 'Common.ps1')
New-Item -ItemType Directory -Force -Path $script:CommandDir | Out-Null
Write-SessionLock $HostId 0 | Set-Content $script:LockFile -Encoding ascii
'''


@unittest.skipUnless(POWERSHELL, 'PowerShell fehlt')
class StopSessionLockTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'repo'
        spike = self.root / 'tools/spike'
        spike.mkdir(parents=True)
        for name in 'Common.ps1', 'Stop-Session.ps1':
            shutil.copy(ROOT / 'tools/spike' / name, spike / name)
        (self.root / 'tools/Deploy-Mod.ps1').write_text(DEPLOY_STUB, encoding='ascii')
        (spike / 'Write-TestLock.ps1').write_text(LOCK_HELPER, encoding='ascii')
        self.session = self.root / 'research/local/session/session.json'
        self.session.parent.mkdir(parents=True)
        temp = Path(self.temp.name) / 'temp'
        temp.mkdir()
        self.lock = temp / 'ddspike/session.lock'
        self.removed = self.root / 'removed.json'
        # Der Platzhalter fuer das Spiel heisst wie PowerShell selbst; nichts auf dem PC heisst dabei wie das echte Spiel.
        self.env = dict(os.environ, TEMP=str(temp), TMP=str(temp), DDCOOP_TEST_GAME_PROCESS=Path(POWERSHELL).stem)

    def run_ps(self, name, *args):
        script = self.root / 'tools/spike' / name
        return subprocess.run([POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script), *args],
                              capture_output=True, text=True, timeout=120, env=self.env)

    def write_session(self, host):
        self.session.write_text(json.dumps({'Host': host, 'Client': 0, 'Mod': True}), encoding='ascii')

    def session_mod(self):
        return json.loads(self.session.read_text(encoding='utf-8-sig'))['Mod']

    def test_lock_stays_held_until_mod_is_removed(self):
        game = subprocess.Popen([POWERSHELL, '-NoProfile', '-Command', 'Start-Sleep -Seconds 120'], env=self.env)
        self.addCleanup(game.kill)
        self.write_session(game.pid)
        self.assertEqual(self.run_ps('Write-TestLock.ps1', '-HostId', str(game.pid)).returncode, 0)
        result = self.run_ps('Stop-Session.ps1')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNotNone(game.wait(timeout=10))
        # Beim Entfernen gehoerte die Sperre noch dieser Sitzung und galt als belegt, obwohl die Instanz schon beendet war.
        self.assertEqual(json.loads(self.removed.read_text(encoding='utf-8-sig')), {'Own': True, 'Active': True, 'Host': 0})
        self.assertFalse(self.lock.exists())
        self.assertFalse(self.session_mod())

    def test_foreign_session_keeps_its_mod(self):
        self.write_session(0)
        self.lock.parent.mkdir(parents=True)
        # Fremde Sitzung im Start: beide Rollen 0, ihr Startprozess lebt.
        foreign = json.dumps({'Owner': 'X:\\other', 'Starter': os.getpid(), 'Host': 0, 'Client': 0})
        self.lock.write_text(foreign, encoding='ascii')
        result = self.run_ps('Stop-Session.ps1')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.removed.exists(), result.stdout)
        self.assertEqual(self.lock.read_text(encoding='ascii'), foreign)
        self.assertFalse(self.session_mod())

    def test_dead_session_without_lock_removes_mod(self):
        self.write_session(0)
        result = self.run_ps('Stop-Session.ps1')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(self.removed.read_text(encoding='utf-8-sig')), {'Own': True, 'Active': True, 'Host': 0})
        self.assertFalse(self.lock.exists())
        self.assertFalse(self.session_mod())


if __name__ == '__main__':
    unittest.main()
