# Beendet nur die Testinstanzen dieses Ordners und gibt die Sperre frei. Der Crash-Schutz beendet sich danach von selbst.
. (Join-Path $PSScriptRoot 'Common.ps1')
$File = Join-Path $script:SessionDir 'session.json'
if (Test-OwnSessionRunning) {
    $Session = Get-Content $File -Raw | ConvertFrom-Json
    foreach ($ProcessId in $Session.Host, $Session.Client) {
        if (Test-GameProcess $ProcessId) { Stop-Process -Id $ProcessId -Force }
    }
    # Windows gibt spike.dll erst nach ein paar Sekunden frei.
    Start-Sleep -Seconds 6
}
Exit-SessionLock
