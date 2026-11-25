# Beendet nur die Testinstanzen dieses Ordners und gibt die Sperre frei. Der Crash-Schutz beendet sich danach von selbst.
# Hat Start-Session.ps1 -Mod die Mod installiert, wird sie danach wieder entfernt.
. (Join-Path $PSScriptRoot 'Common.ps1')
$File = Join-Path $script:SessionDir 'session.json'
New-Item -ItemType Directory -Force -Path $script:CommandDir | Out-Null
# Die Sperre bleibt bis nach dem Entfernen der Mod belegt, sonst installiert eine wartende Sitzung dazwischen ihre Mod.
$Held = Enter-StopLock
if (Test-OwnSessionRunning) {
    $Session = Get-Content $File -Raw | ConvertFrom-Json
    foreach ($ProcessId in @($Session.Host, $Session.Client) + @($Session.Extra | Where-Object { $_ })) {
        if (Test-GameProcess $ProcessId) { Stop-Process -Id $ProcessId -Force }
    }
    # Windows gibt spike.dll erst nach ein paar Sekunden frei.
    Start-Sleep -Seconds 6
}
if (Test-Path $File) {
    $Session = Get-Content $File -Raw | ConvertFrom-Json
    if ($Session.PSObject.Properties['Mod'] -and $Session.Mod) {
        $SteamInstall = $Session.PSObject.Properties['SteamInstall'] -and $Session.SteamInstall
        if (-not $Held -and $SteamInstall) {
            Write-Warning 'Mod in der Steam-Installation nicht entfernt: Eine andere Testsitzung laeuft. Nach ihrem Ende Stop-Session.ps1 erneut ausfuehren.'
        }
        elseif (-not $Held) {
            # Die andere Sitzung hat beim Start ihre Mod installiert oder eine vorhandene entfernt und raeumt selbst auf.
            Write-Output 'Mod nicht entfernt: Eine andere Testsitzung haelt die Sperre, die Mod im Spielordner gehoert ihr.'
            $Session.Mod = $false
            $Session | ConvertTo-Json | Set-Content $File -Encoding ascii
        }
        else {
            try {
                # Mit -SteamLaunch lag die Mod in der Steam-Installation, nicht in PinnedGameDir.
                if ($SteamInstall) { $env:DDCOOP_STEAM_INSTALL = '1' }
                & (Join-Path $PSScriptRoot '..\Deploy-Mod.ps1') -Remove
                $Session.Mod = $false
                $Session | ConvertTo-Json | Set-Content $File -Encoding ascii
            }
            catch { Write-Warning "Mod nicht entfernt: $_ Danach tools\Deploy-Mod.ps1 -Remove ausfuehren." }
            finally { Remove-Item Env:DDCOOP_STEAM_INSTALL -ErrorAction SilentlyContinue }
        }
    }
}
Exit-SessionLock
