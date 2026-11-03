# Beendet nur die Testinstanzen dieses Ordners und gibt die Sperre frei. Der Crash-Schutz beendet sich danach von selbst.
# Hat Start-Session.ps1 -Mod die Mod installiert, wird sie danach wieder entfernt.
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
if (Test-Path $File) {
    $Session = Get-Content $File -Raw | ConvertFrom-Json
    if ($Session.PSObject.Properties['Mod'] -and $Session.Mod) {
        try {
            & (Join-Path $PSScriptRoot '..\Deploy-Mod.ps1') -Remove
            $Session.Mod = $false
            $Session | ConvertTo-Json | Set-Content $File -Encoding ascii
        }
        catch { Write-Warning "Mod nicht entfernt: $_ Danach tools\Deploy-Mod.ps1 -Remove ausfuehren." }
    }
}
Exit-SessionLock
