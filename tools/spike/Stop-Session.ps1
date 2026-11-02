# Beendet die Testinstanzen. Der Crash-Schutz beendet sich danach von selbst.
. (Join-Path $PSScriptRoot 'Common.ps1')
$Running = Get-Process DDSurvivors-Win64-Shipping -ErrorAction SilentlyContinue
if ($Running) {
    $Running | Stop-Process -Force
    # Windows gibt spike.dll erst nach ein paar Sekunden frei.
    Start-Sleep -Seconds 6
}
