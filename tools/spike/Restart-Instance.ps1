# Startet nur den Client der laufenden Sitzung neu und verbindet ihn wieder, der Host laeuft weiter. Spart den vollen Neustart,
# etwa nach einem Absturz oder einem hart beendeten Client. Den Host neu starten geht nur mit Start-Session.ps1 -Restart.
#   Restart-Instance.ps1 [-Role Client] [-NoConnect]
# Mit -Tunnel Lobby tritt der Client der Lobby des Hosts per Overlay bei. Ist sie weg (ein Konto auf einem PC: Steam loest sie
# nach dem Verlassen auf), erstellt der Host ueber sein Overlay eine neue.
param(
    [ValidateSet('Client')][string]$Role = 'Client',
    # Nur starten und den Spike laden, nicht verbinden.
    [switch]$NoConnect,
    [int]$TimeoutSeconds = 120,
    # Nur Zeit je Schritt und eine Zeile je Instanz.
    [switch]$Brief
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$Timer = [Diagnostics.Stopwatch]::StartNew()
function Write-Step([string]$Text) { Write-Host ('[{0,4:n0} s] {1}' -f $Timer.Elapsed.TotalSeconds, $Text) }
$File = Join-Path $script:SessionDir 'session.json'
$Session = Get-Session
if ($Session.Role -ne 'Both') { throw 'Nur fuer Sitzungen mit Host und Client auf diesem PC.' }
$Lock = Get-Content $script:LockFile -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json
if (-not $Lock -or $Lock.Owner -ne $script:RepoRoot) { throw 'Die Sitzung gehoert nicht zu diesem Ordner.' }
$HostId = [int]$Session.Host
if (-not (Test-GameProcess $HostId)) { throw 'Der Host laeuft nicht. Start-Session.ps1 -Restart verwenden.' }
$Tunnel = if ($Session.PSObject.Properties['Tunnel']) { [string]$Session.Tunnel } else { 'None' }
if ($Tunnel -notin 'None', 'Lobby', 'Mod') { throw "Einzelneustart mit -Tunnel $Tunnel nicht umgesetzt, Start-Session.ps1 -Restart verwenden." }

$Old = [int]$Session.Client
if (Test-GameProcess $Old) {
    Stop-Process -Id $Old -Force
    Wait-Condition { -not (Test-GameProcess $Old) } 'alter Client beendet' 20 @() 250
    Write-Step "alter Client $Old beendet"
}

$env:SteamAppId = '3026450'
$env:SteamGameId = '3026450'
$Exe = Get-GameExe
$UserDir = Join-Path $script:SessionDir 'ud_client'
$Arguments = @('-windowed', '-ResX=800', '-ResY=450', '-WinX=820', '-WinY=0', '-nosplash', "-UserDir=`"$UserDir`"")
if ($Tunnel -eq 'Mod') { $Arguments += '-ddcoop_transport=loopback' }
# Gleiches Netzprofil wie beim Start der Sitzung.
if ($Session.PSObject.Properties['NetSim'] -and $Session.NetSim) { $Arguments += "-ddcoop_netsim=$($Session.NetSim)" }
$Client = Start-Process $Exe -PassThru -ArgumentList $Arguments
$Session.Client = $Client.Id
$Session | ConvertTo-Json | Set-Content $File -Encoding ascii
Write-SessionLock $HostId $Client.Id | Set-Content $script:LockFile -Encoding ascii

Wait-MainMenu $Client.Id
Start-Sleep -Seconds 2
$Running = Get-Process -Id $Client.Id -ErrorAction Stop
if (-not $Running.Path -or $Running.Path -ne $Exe) { throw 'Unerwarteter Spielpfad. Kein Spike geladen.' }
$Injected = & py (Join-Path $PSScriptRoot 'inject.py') $Client.Id (Join-Path $script:SessionDir 'spike.dll')
if ($LASTEXITCODE) { throw "Spike nicht geladen: $Injected" }
if (-not $Brief) { $Injected }
Start-Sleep -Seconds 3
Write-Step "Client $($Client.Id) mit Spike im Hauptmenue"
if ($NoConnect) { return }

if ($Tunnel -eq 'Lobby') {
    Write-Output "Lobby: $(Join-HostLobby $HostId $Client.Id)"
}
elseif ($Tunnel -eq 'Mod') {
    $Log = Join-Path (Split-Path $Exe) "DDCoop\logs\ddcoop-$($Client.Id).log"
    Wait-Condition { (Test-Path $Log) -and (Select-String -Path $Log -Pattern 'Transport: verbunden' -Quiet) } 'Tunnel der Mod' 150 @($Client.Id)
    Send-SpikeCommand $Client.Id 'open 127.0.0.1:7778'
}
else {
    Send-SpikeCommand $Client.Id 'open 127.0.0.1:7777'
}
$World = (Get-NetState $HostId).world
Wait-Condition {
    $State = Get-NetState $Client.Id
    $State.world -eq $World -and $State.players -ge 2 -and (Get-NetState $HostId).clients -ge 1
} "Client in $World" $TimeoutSeconds @($HostId, $Client.Id)
Write-Step "Client in $World verbunden"
# Dieselben Spike-Hilfen wie in Start-Session.ps1 fuer den Client.
Start-Sleep -Seconds 3
foreach ($Command in '!cam', '!trace', '!flow') {
    Send-SpikeCommand $Client.Id $Command
    Start-Sleep -Milliseconds 800
}
if ($Brief) { Format-NetBrief $HostId 'Host'; Format-NetBrief $Client.Id 'Client' }
else { & py (Join-Path $PSScriptRoot 'netstate.py') $HostId $Client.Id }
