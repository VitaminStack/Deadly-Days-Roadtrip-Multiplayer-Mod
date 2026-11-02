# Startet Host und Client auf einem PC, verbindet sie und reist in die angegebene Karte.
# Beide laufen mit eigenem -UserDir, der echte Spielstand bleibt unberuehrt. -unattended niemals verwenden.
param(
    [string]$Map = '/Game/Maps/MetaBase',
    [int]$MenuDelaySeconds = 20,
    [int]$TravelDelaySeconds = 40
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Initialize-SessionDir
Assert-SupportedBuild
& (Join-Path $PSScriptRoot 'Stop-Session.ps1')

$Dll = Join-Path $script:SessionDir 'spike.dll'
if (-not (Test-Path $Dll)) { & (Join-Path $PSScriptRoot 'Build-Spike.ps1') | Out-Null }

$Guard = Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$(Join-Path $PSScriptRoot 'CrashGuard.ps1')`""
Start-Sleep -Seconds 1
if ($Guard.HasExited) { throw 'Crash-Schutz laeuft nicht, Abbruch.' }

$env:SteamAppId = '3026450'
$env:SteamGameId = '3026450'
$Exe = Get-GameExe
function Start-Instance([string]$Name, [int]$X) {
    $UserDir = Join-Path $script:SessionDir "ud_$Name"
    Start-Process $Exe -PassThru -ArgumentList '-windowed', '-ResX=800', '-ResY=450', "-WinX=$X", '-WinY=0', '-nosplash', "-UserDir=`"$UserDir`""
}
$GameHost = Start-Instance 'host' 0
$Client = Start-Instance 'client' 820
@{ Host = $GameHost.Id; Client = $Client.Id; Guard = $Guard.Id } | ConvertTo-Json |
    Set-Content (Join-Path $script:SessionDir 'session.json') -Encoding ascii

Start-Sleep -Seconds $MenuDelaySeconds
foreach ($Instance in $GameHost, $Client) { & py (Join-Path $PSScriptRoot 'inject.py') $Instance.Id $Dll }
Start-Sleep -Seconds 3
Send-SpikeCommand $GameHost.Id 'open /Game/Maps/MainMenu?listen'
Start-Sleep -Seconds 12
Send-SpikeCommand $Client.Id 'open 127.0.0.1:7777'
Start-Sleep -Seconds 10
Send-SpikeCommand $GameHost.Id "servertravel $Map`?listen"
Start-Sleep -Seconds $TravelDelaySeconds

if ($GameHost.HasExited -or $Client.HasExited) { throw 'Eine Instanz ist abgestuerzt, siehe tools\spike\crashes.py.' }
Send-SpikeCommand $Client.Id '!cam'
Start-Sleep -Milliseconds 800
Send-SpikeCommand $Client.Id '!trace'
Send-SpikeCommand $GameHost.Id '!trace'
Start-Sleep -Milliseconds 800
Send-SpikeCommand $GameHost.Id '!authmove'
Start-Sleep -Milliseconds 800
Send-SpikeCommand $GameHost.Id '!flow'
Send-SpikeCommand $Client.Id '!flow'
Start-Sleep -Milliseconds 800
Send-SpikeCommand $GameHost.Id '!arm'
Start-Sleep -Seconds 1
& py (Join-Path $PSScriptRoot 'netstate.py') $GameHost.Id $Client.Id
Write-Output "Logs: $($script:CommandDir)\$($GameHost.Id).log und $($Client.Id).log"
