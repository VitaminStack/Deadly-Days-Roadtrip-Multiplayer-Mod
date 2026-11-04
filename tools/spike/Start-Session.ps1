# Startet Host und Client auf einem PC, verbindet sie und reist in die angegebene Karte.
# Beide laufen mit eigenem -UserDir, der echte Spielstand bleibt unberuehrt. -unattended niemals verwenden.
# Test auf zwei PCs: -Role Host auf dem einen, -Role Client -Peer <SteamID des Hosts> auf dem anderen, beide mit -Tunnel P2p.
# Ablauf und Vorbereitung in docs\TESTING.md, Abschnitt "Test auf zwei PCs".
param(
    [string]$Map = '/Game/Maps/MetaBase',
    [int]$MenuDelaySeconds = 20,
    [int]$TravelDelaySeconds = 40,
    # Ersetzt eine laufende Sitzung aus diesem Ordner. Fremde Sitzungen bleiben unberuehrt.
    [switch]$Restart,
    # Baut spike.dll neu, vorher endet die eigene Sitzung wie bei -Restart.
    [switch]$Build,
    # Baut und installiert zusaetzlich die Mod aus src\. Stop-Session.ps1 entfernt sie wieder.
    [switch]$Mod,
    # Testhilfe fuer beide Rollen; bleibt im Spike ueber Kartenwechsel aktiv, nicht im Spielstand.
    [switch]$Godmode,
    # Client verbindet ueber den Steam-Tunnel aus Tunnel.cpp statt direkt: P2p zur eigenen SteamID oder Ip ueber 127.0.0.1.
    # Mit -Role Client geht P2p zur SteamID aus -Peer. Mod: Tunnel der Mod aus src\Net statt des Spikes, braucht -Mod.
    [ValidateSet('None', 'Ip', 'P2p', 'Mod')]
    [string]$Tunnel = 'None',
    # Both: Host und Client auf diesem PC. Host oder Client: nur diese Instanz, die Gegenseite laeuft auf einem anderen PC.
    [ValidateSet('Both', 'Host', 'Client')]
    [string]$Role = 'Both',
    # Nur mit -Role Client: SteamID64 des Hosts bei -Tunnel P2p, sonst Adresse:Port fuer eine direkte Verbindung.
    [string]$Peer,
    # Nur mit -Role Host oder Client: so lange auf die Gegenseite warten.
    [int]$JoinTimeoutSeconds = 900,
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
if ($Role -eq 'Client' -and -not $Peer) { throw '-Role Client braucht -Peer: SteamID64 des Hosts (mit -Tunnel P2p) oder Adresse:Port.' }
if ($Role -ne 'Client' -and $Peer) { throw '-Peer gilt nur mit -Role Client.' }
if ($Role -ne 'Both' -and $Tunnel -eq 'Ip') { throw '-Tunnel Ip verbindet nur auf einem PC. Fuer zwei PCs -Tunnel P2p verwenden.' }
if ($Tunnel -eq 'Mod' -and -not $Mod) { throw '-Tunnel Mod braucht -Mod.' }
if ($Tunnel -eq 'Mod' -and $Role -eq 'Client' -and $Peer -notmatch '^\d{17}$') { throw '-Tunnel Mod mit -Role Client braucht -Peer mit der SteamID64 des Hosts.' }
Initialize-SessionDir
Assert-SupportedBuild
if ($Restart -or $Build) { & (Join-Path $PSScriptRoot 'Stop-Session.ps1') }
Enter-SessionLock
if (Get-Process $script:GameProcess -ErrorAction SilentlyContinue) {
    Exit-SessionLock
    throw 'Das Spiel laeuft ausserhalb einer Testsitzung. Bitte zuerst selbst beenden.'
}

$Dll = Join-Path $script:SessionDir 'spike.dll'
try {
    if ($Build -or -not (Test-Path $Dll)) { & (Join-Path $PSScriptRoot 'Build-Spike.ps1') | Out-Null }
    if ($Mod) {
        & (Join-Path $PSScriptRoot '..\Build-Mod.ps1') -Configuration $Configuration | Out-Null
        & (Join-Path $PSScriptRoot '..\Deploy-Mod.ps1') -Configuration $Configuration
    }
}
catch {
    Exit-SessionLock
    throw
}

$Guard = Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$(Join-Path $PSScriptRoot 'CrashGuard.ps1')`""
Start-Sleep -Seconds 1
if ($Guard.HasExited) {
    if ($Mod) { & (Join-Path $PSScriptRoot '..\Deploy-Mod.ps1') -Remove }
    Exit-SessionLock
    throw 'Crash-Schutz laeuft nicht, Abbruch.'
}

$env:SteamAppId = '3026450'
$env:SteamGameId = '3026450'
$Exe = Get-GameExe
# Testschalter fuer den Tunnel der Mod, siehe src\Net\README.md.
function Get-TransportArgument([string]$Name) {
    if ($Tunnel -ne 'Mod') { return @() }
    if ($Name -eq 'host') { return @('-ddcoop_transport=host') }
    if ($Role -eq 'Both') { return @('-ddcoop_transport=loopback') }
    @("-ddcoop_transport=$Peer")
}
function Start-Instance([string]$Name, [int]$X) {
    $UserDir = Join-Path $script:SessionDir "ud_$Name"
    $Arguments = @('-windowed', '-ResX=800', '-ResY=450', "-WinX=$X", '-WinY=0', '-nosplash', "-UserDir=`"$UserDir`"") + (Get-TransportArgument $Name)
    Start-Process $Exe -PassThru -ArgumentList $Arguments
}
# Wartet auf eine Zeile im Log der Mod, liefert sie oder $null.
function Wait-ModLog([int]$ProcessId, [string]$Pattern, [int]$TimeoutSeconds) {
    $Log = Join-Path (Split-Path $Exe) "DDCoop\logs\ddcoop-$ProcessId.log"
    $Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $Deadline) {
        if (Test-Path $Log) {
            $Hit = Select-String -Path $Log -Pattern $Pattern | Select-Object -Last 1
            if ($Hit) { return $Hit.Line }
        }
        Start-Sleep -Seconds 1
    }
    $null
}
function Connect-ModTransport([int]$ProcessId) {
    $Seen = Wait-ModLog $ProcessId 'Transport: verbunden|keine Verbindung zum Host|Start fehlgeschlagen|Steam nicht verf' 150
    if ("$Seen" -notmatch 'Transport: verbunden') { throw "Tunnel der Mod nicht aufgebaut: $Seen" }
    Write-Output "Tunnel der Mod: $($Seen -replace '^.*Transport: ', '')"
    Send-SpikeCommand $ProcessId 'open 127.0.0.1:7778'
}
$GameHost = if ($Role -ne 'Client') { Start-Instance 'host' 0 }
$Client = if ($Role -ne 'Host') { Start-Instance 'client' $(if ($Role -eq 'Both') { 820 } else { 0 }) }
$Local = @($GameHost, $Client) | Where-Object { $_ }
# Die Rolle, die auf einem anderen PC laeuft, steht mit Prozess-ID 0 in der Sitzung.
$HostId = if ($GameHost) { $GameHost.Id } else { 0 }
$ClientId = if ($Client) { $Client.Id } else { 0 }
@{ Host = $HostId; Client = $ClientId; Role = $Role; Guard = $Guard.Id; Mod = [bool]$Mod; Started = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() } | ConvertTo-Json |
    Set-Content (Join-Path $script:SessionDir 'session.json') -Encoding ascii
Write-SessionLock $HostId $ClientId | Set-Content $script:LockFile -Encoding ascii

Start-Sleep -Seconds $MenuDelaySeconds
foreach ($Instance in $Local) {
    # Falls Steam einen Neustart in seiner Installation erzwingt, dort niemals den Spike injizieren.
    $Running = Get-Process -Id $Instance.Id -ErrorAction Stop
    if (-not $Running.Path -or $Running.Path -ne $Exe) {
        throw 'Unerwarteter Spielpfad. Kein Spike geladen; eigene Sitzung mit Stop-Session.ps1 beenden.'
    }
    & py (Join-Path $PSScriptRoot 'inject.py') $Instance.Id $Dll
}
Start-Sleep -Seconds 3
if ($Godmode) {
    foreach ($Instance in $Local) { Send-SpikeCommand $Instance.Id '!god on' }
    Start-Sleep -Seconds 1
}

# Netzzustand einer Instanz aus netstate.py, etwa @{ package = 'MetaBase'; clients = 1 }.
function Get-NetState([int]$ProcessId) {
    $Line = & py (Join-Path $PSScriptRoot 'netstate.py') $ProcessId | Select-Object -Last 1
    $State = @{ line = $Line }
    foreach ($Match in [regex]::Matches([string]$Line, "'(\w+)': (?:'([^']*)'|(-?\d+))")) {
        $State[$Match.Groups[1].Value] = if ($Match.Groups[2].Success) { $Match.Groups[2].Value } else { [int]$Match.Groups[3].Value }
    }
    $State
}

function Wait-Until([scriptblock]$Condition, [string]$What) {
    $Deadline = (Get-Date).AddSeconds($JoinTimeoutSeconds)
    while ((Get-Date) -lt $Deadline) {
        foreach ($Instance in $Local) {
            if ($Instance.HasExited) { throw 'Eine Instanz ist abgestuerzt, siehe tools\spike\crashes.py.' }
        }
        if (& $Condition) { return }
        Start-Sleep -Seconds 2
    }
    throw "Zeitablauf nach $JoinTimeoutSeconds s: $What"
}

if ($GameHost) {
    Send-SpikeCommand $GameHost.Id 'open /Game/Maps/MainMenu?listen'
    Start-Sleep -Seconds 12
}
if ($Role -eq 'Both') {
    if ($Tunnel -eq 'None') {
        Send-SpikeCommand $Client.Id 'open 127.0.0.1:7777'
    }
    elseif ($Tunnel -eq 'Mod') {
        Connect-ModTransport $Client.Id
    }
    else {
        Send-SpikeCommand $GameHost.Id '!tunnel host'
        Start-Sleep -Seconds 2
        Send-SpikeCommand $Client.Id $(if ($Tunnel -eq 'Ip') { '!tunnel join ip' } else { '!tunnel join self' })
        Start-Sleep -Seconds 8
        Send-SpikeCommand $Client.Id 'open 127.0.0.1:7778'
    }
    Start-Sleep -Seconds 10
}
elseif ($Role -eq 'Host') {
    if ($Tunnel -eq 'P2p') {
        Send-SpikeCommand $GameHost.Id '!tunnel host'
        & (Join-Path $PSScriptRoot 'Wait-Log.ps1') Host 'Tunnel: eigene SteamID' -TimeoutSeconds 30 | Out-Null
        $Own = Select-String -Path (Join-Path $script:CommandDir "$($GameHost.Id).log") -Pattern 'Tunnel: eigene SteamID (\d+)' |
            Select-Object -Last 1
        # Nur lokal anzeigen, nie ins Repo: Die Gegenseite braucht sie fuer -Peer.
        Write-Output "Host bereit. SteamID fuer -Peer auf dem Client-PC: $($Own.Matches[0].Groups[1].Value)"
    }
    elseif ($Tunnel -eq 'Mod') {
        # Die Mod loggt keine SteamIDs. Die SteamID64 steht im eigenen Steam-Profil.
        Write-Output 'Host bereit, Tunnel der Mod wartet. Client-PC: -Tunnel Mod -Peer <SteamID64 dieses Kontos>.'
    }
    else {
        Write-Output 'Host bereit, wartet auf direkte Verbindung auf Port 7777.'
    }
    Write-Output "Warte bis zu $JoinTimeoutSeconds s auf den Client ..."
    Wait-Until { (Get-NetState $GameHost.Id).clients -ge 1 } 'kein Client verbunden'
    Write-Output 'Client verbunden.'
    Start-Sleep -Seconds 10
}
else {
    if ($Tunnel -eq 'P2p') {
        Send-SpikeCommand $Client.Id "!tunnel join $Peer"
        $Seen = & (Join-Path $PSScriptRoot 'Wait-Log.ps1') Client 'Tunnel: Verbindung \d+ verbunden|Tunnel: Verbindung beendet|abgelehnt' -TimeoutSeconds 60
        if ("$Seen" -match 'abgelehnt|beendet') {
            throw "Tunnel zum Host nicht aufgebaut: $Seen Stimmt die SteamID, laeuft der Host schon mit -Tunnel P2p?"
        }
        Send-SpikeCommand $Client.Id 'open 127.0.0.1:7778'
    }
    elseif ($Tunnel -eq 'Mod') {
        Connect-ModTransport $Client.Id
    }
    else {
        Send-SpikeCommand $Client.Id "open $Peer"
    }
    $Target = ($Map -split '/')[-1]
    Write-Output "Warte bis zu $JoinTimeoutSeconds s, bis der Host nach $Target reist ..."
    Wait-Until { $State = Get-NetState $Client.Id; $State.world -eq $Target -or $State.package -like "*$Target" } "Client nicht in $Target"
}
if ($GameHost) {
    Send-SpikeCommand $GameHost.Id "servertravel $Map`?listen"
}
Start-Sleep -Seconds $TravelDelaySeconds

if (@($Local | Where-Object { $_.HasExited }).Count) { throw 'Eine Instanz ist abgestuerzt, siehe tools\spike\crashes.py.' }
if ($Client) {
    Send-SpikeCommand $Client.Id '!cam'
    Start-Sleep -Milliseconds 800
    Send-SpikeCommand $Client.Id '!trace'
}
if ($GameHost) { Send-SpikeCommand $GameHost.Id '!trace' }
Start-Sleep -Milliseconds 800
if ($GameHost) {
    Send-SpikeCommand $GameHost.Id '!authmove'
    Start-Sleep -Milliseconds 800
    Send-SpikeCommand $GameHost.Id '!flow'
}
if ($Client) { Send-SpikeCommand $Client.Id '!flow' }
Start-Sleep -Milliseconds 800
if ($GameHost) {
    Send-SpikeCommand $GameHost.Id '!arm'
    Start-Sleep -Seconds 1
}
& py (Join-Path $PSScriptRoot 'netstate.py') @($Local | ForEach-Object { $_.Id })
Write-Output "Logs: $(($Local | ForEach-Object { Join-Path $script:CommandDir "$($_.Id).log" }) -join ' und ')"
