# Startet Host und Client auf einem PC, verbindet sie und reist in die angegebene Karte.
# Beide laufen mit eigenem -UserDir, der echte Spielstand bleibt unberuehrt. -unattended niemals verwenden.
# Test auf zwei PCs: -Role Host auf dem einen, -Role Client -Peer <SteamID des Hosts> auf dem anderen, beide mit -Tunnel P2p.
# Ablauf und Vorbereitung in docs\TESTING.md, Abschnitt "Test auf zwei PCs".
# Das Skript wartet auf Zustaende (Hauptmenue, verbunden, Karte geladen) statt fest zu schlafen und nennt die Zeit je Schritt.
param(
    [string]$Map = '/Game/Maps/MetaBase',
    # Hoechstens so lange auf das Hauptmenue warten, bevor der Spike geladen wird.
    [int]$MenuTimeoutSeconds = 90,
    # Hoechstens so lange auf die Reise in -Map warten.
    [int]$TravelTimeoutSeconds = 180,
    # Ersetzt eine laufende Sitzung aus diesem Ordner. Fremde Sitzungen bleiben unberuehrt.
    [switch]$Restart,
    # Laeuft schon eine fremde Sitzung, so viele Sekunden auf ihr Ende warten statt abzubrechen.
    [int]$Wait = 0,
    # Baut spike.dll neu, vorher endet die eigene Sitzung wie bei -Restart.
    [switch]$Build,
    # Baut und installiert zusaetzlich die Mod aus src\. Stop-Session.ps1 entfernt sie wieder.
    [switch]$Mod,
    # Testhilfe fuer beide Rollen; bleibt im Spike ueber Kartenwechsel aktiv, nicht im Spielstand.
    [switch]$Godmode,
    # Client verbindet ueber den Steam-Tunnel aus Tunnel.cpp statt direkt: P2p zur eigenen SteamID oder Ip ueber 127.0.0.1.
    # Mit -Role Client geht P2p zur SteamID aus -Peer. Mod: Tunnel der Mod aus src\Net statt des Spikes, braucht -Mod.
    # Lobby: Steam-Lobby der Mod. Nach dem Laden des Spikes erstellt der Host sie ueber das Overlay, der Client tritt
    # ebenso mit der Lobby-ID bei (-Peer bei -Role Client).
    [ValidateSet('None', 'Ip', 'P2p', 'Mod', 'Lobby')]
    [string]$Tunnel = 'None',
    # Both: Host und Client auf diesem PC. Host oder Client: nur diese Instanz, die Gegenseite laeuft auf einem anderen PC.
    [ValidateSet('Both', 'Host', 'Client')]
    [string]$Role = 'Both',
    # Nur mit -Role Client: SteamID64 des Hosts bei -Tunnel P2p, sonst Adresse:Port fuer eine direkte Verbindung.
    [string]$Peer,
    # Nur mit -Role Host oder Client: so lange auf die Gegenseite warten.
    [int]$JoinTimeoutSeconds = 900,
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    # Kurze Ausgabe: Zeit je Schritt und eine Zeile je Instanz, ohne Build-, Injektions- und Zustandsdetails.
    [switch]$Brief,
    # Simulierte Latenz im Tunnel der Mod: Internet (120 ms +-30, 2 % Verlust), Schlecht oder "Ping,Schwankung,Verlust".
    # Braucht -Mod mit -Tunnel Lobby oder Mod. Profile in Common.ps1, Messwerte im Mod-Log ("Netzsimulation:").
    [string]$NetProfile,
    # Wie im Release: nur die Mod, ohne spike.dll. Braucht -Mod und -Tunnel Lobby, Ziel bleibt die MetaBase.
    # Ohne Spike gibt es keine Konsolenbefehle, Karte und PCG kommen nicht vom Host, siehe src\Game\README.md.
    [switch]$NoSpike,
    # Nur -Role Client -Mod -Tunnel Lobby -NoSpike, ohne -Peer: Steam startet das Spiel selbst, etwa mit +connect_lobby nach einer
    # angenommenen Einladung bei geschlossenem Spiel. Die Mod kommt dafuer in die Steam-Installation (nicht in PinnedGameDir), ohne
    # eigenes -UserDir; Stop-Session.ps1 entfernt sie wieder. Das Skript wartet auf den Prozess, den Steam startet.
    [switch]$SteamLaunch,
    # Nur auf einem anderen Build als 26187885: Patch-Listen einzeln setzen (DDCOOP_PARTIAL_PATCHES), fuer die Portierung.
    [switch]$PartialPatches
)
$ErrorActionPreference = 'Stop'
# Start-Process vererbt die Umgebung an die Instanzen.
if ($PartialPatches) { $env:DDCOOP_PARTIAL_PATCHES = '1' } else { Remove-Item Env:DDCOOP_PARTIAL_PATCHES -ErrorAction SilentlyContinue }
. (Join-Path $PSScriptRoot 'Common.ps1')
if ($SteamLaunch) {
    if ($Role -ne 'Client' -or -not $Mod -or $Tunnel -ne 'Lobby' -or -not $NoSpike) { throw '-SteamLaunch braucht -Role Client -Mod -Tunnel Lobby -NoSpike.' }
    if ($Peer) { throw '-SteamLaunch ohne -Peer: Die Lobby kommt von Steam (+connect_lobby).' }
    if ($NetProfile) { throw '-NetProfile geht nicht mit -SteamLaunch, Steam setzt die Kommandozeile.' }
    $env:DDCOOP_STEAM_INSTALL = '1'
}
else { Remove-Item Env:DDCOOP_STEAM_INSTALL -ErrorAction SilentlyContinue }
if ($Role -eq 'Client' -and -not $Peer -and -not $SteamLaunch) { throw '-Role Client braucht -Peer: SteamID64 des Hosts (mit -Tunnel P2p) oder Adresse:Port.' }
if ($Role -ne 'Client' -and $Peer) { throw '-Peer gilt nur mit -Role Client.' }
if ($Role -ne 'Both' -and $Tunnel -eq 'Ip') { throw '-Tunnel Ip verbindet nur auf einem PC. Fuer zwei PCs -Tunnel P2p verwenden.' }
if ($Tunnel -in 'Mod', 'Lobby' -and -not $Mod) { throw "-Tunnel $Tunnel braucht -Mod." }
# Mit der Mod kommt die Host-Karte ueber ihren Kontrollkanal (missionsstart), nicht ueber die Datei des Spikes.
if ($Mod -and $Tunnel -notin 'Mod', 'Lobby') { Write-Warning "Ohne Tunnel der Mod bekommt der Client keine Host-Karte und erzeugt nach 60 s eine eigene. Fuer die Mission -Tunnel Lobby verwenden." }
if ($Tunnel -eq 'Lobby' -and $Role -eq 'Client' -and -not $SteamLaunch -and $Peer -notmatch '^\d{1,20}$') { throw '-Tunnel Lobby mit -Role Client braucht -Peer mit der Lobby-ID aus der Ausgabe des Hosts.' }
if ($Tunnel -eq 'Mod' -and $Role -eq 'Client' -and $Peer -notmatch '^\d{17}$') { throw '-Tunnel Mod mit -Role Client braucht -Peer mit der SteamID64 des Hosts.' }
$NetSim = Resolve-NetProfile $NetProfile
if ($NetSim -and $Tunnel -notin 'Mod', 'Lobby') { throw '-NetProfile wirkt nur im Tunnel der Mod: -Mod mit -Tunnel Lobby oder Mod.' }
if ($NoSpike) {
    # Ohne Spike gibt es weder open noch servertravel. Die Lobby der Mod macht den Host selbst zum Listen-Server in der MetaBase.
    # -Role Client -Tunnel Mod geht nur bis zum Tunnel, etwa um die Ablehnung eines Kontos ausserhalb der Lobby zu pruefen (lobby-auth).
    $ModClient = $Tunnel -eq 'Mod' -and $Role -eq 'Client'
    if (-not $Mod -or ($Tunnel -ne 'Lobby' -and -not $ModClient)) { throw '-NoSpike braucht -Mod -Tunnel Lobby, als Client auch -Tunnel Mod.' }
    if ($Godmode) { throw '-Godmode ist eine Testhilfe des Spikes, nicht mit -NoSpike.' }
    if (($Map -split '/')[-1] -ne 'MetaBase') { throw '-NoSpike startet immer in der MetaBase, -Map gilt nicht.' }
}
$Timer = [Diagnostics.Stopwatch]::StartNew()
function Write-Step([string]$Text) { Write-Host ('[{0,4:n0} s] {1}' -f $Timer.Elapsed.TotalSeconds, $Text) }
Initialize-SessionDir
Assert-SupportedBuild
# Der Spike enthaelt feste Adressen des Referenz-Builds und wird nicht umgerechnet (update-basis).
if ($script:ActiveBuild -ne $script:SupportedBuild -and -not $NoSpike) {
    throw "Build $($script:ActiveBuild): der Spike gilt nur fuer $($script:SupportedBuild). Mit -Mod -Tunnel Lobby -NoSpike starten."
}
if ($Restart -or $Build) {
    $Stopped = & (Join-Path $PSScriptRoot 'Stop-Session.ps1')
    if (-not $Brief) { $Stopped }
}
Enter-SessionLock $Wait
if (Get-Process $script:GameProcess -ErrorAction SilentlyContinue) {
    Exit-SessionLock
    throw 'Das Spiel laeuft ausserhalb einer Testsitzung. Bitte zuerst selbst beenden.'
}

$Dll = Join-Path $script:SessionDir 'spike.dll'
try {
    if (-not $NoSpike -and ($Build -or -not (Test-Path $Dll))) { & (Join-Path $PSScriptRoot 'Build-Spike.ps1') | Out-Null }
    if ($Mod) {
        & (Join-Path $PSScriptRoot '..\Build-Mod.ps1') -Configuration $Configuration | Out-Null
        $Deployed = & (Join-Path $PSScriptRoot '..\Deploy-Mod.ps1') -Configuration $Configuration
        if (-not $Brief) { $Deployed }
    }
    elseif (Test-Path (Join-Path (Split-Path (Get-GameExe)) 'DDCoop\DDCoop.dll')) {
        # Eine fest installierte Mod laedt sonst mit. Mit ihren Patches laesst der Spike die Karte aus, ohne Tunnel
        # der Mod kaeme beim Client keine an und die Figuren fielen durch den Boden (missionsstart).
        & (Join-Path $PSScriptRoot '..\Deploy-Mod.ps1') -Remove | Out-Null
        Write-Warning 'Installierte Mod fuer diese Sitzung ohne -Mod entfernt. Danach bei Bedarf tools\Deploy-Mod.ps1 ausfuehren.'
    }
}
catch {
    Exit-SessionLock
    throw
}
Write-Step $(if ($NetSim) { "Build fertig, Netzprofil $NetSim (Ping, Schwankung, Verlust)" } else { 'Build fertig' })

$Guard = Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$(Join-Path $PSScriptRoot 'CrashGuard.ps1')`"", '-StartupGraceSeconds', $(if ($SteamLaunch) { $JoinTimeoutSeconds + 60 } else { 60 })
Start-Sleep -Seconds 1
if ($Guard.HasExited) {
    if ($Mod) { & (Join-Path $PSScriptRoot '..\Deploy-Mod.ps1') -Remove }
    Exit-SessionLock
    throw 'Crash-Schutz laeuft nicht, Abbruch.'
}

$env:SteamAppId = '3026450'
$env:SteamGameId = '3026450'
$Exe = Get-GameExe
# Testschalter fuer den Tunnel der Mod, siehe src\Net\README.md. Die Lobby laeuft ueber das Overlay, ohne Schalter.
function Get-TransportArgument([string]$Name) {
    if ($Tunnel -ne 'Mod') { return @() }
    if ($Name -eq 'host') { return @('-ddcoop_transport=host') }
    if ($Role -eq 'Both') { return @('-ddcoop_transport=loopback') }
    @("-ddcoop_transport=$Peer")
}
function Start-Instance([string]$Name, [int]$X) {
    $UserDir = Join-Path $script:SessionDir "ud_$Name"
    $Arguments = @('-windowed', '-ResX=800', '-ResY=450', "-WinX=$X", '-WinY=0', '-nosplash', "-UserDir=`"$UserDir`"") + (Get-TransportArgument $Name)
    if ($NetSim) { $Arguments += "-ddcoop_netsim=$NetSim" }
    # Beide Instanzen laufen unter einem Konto, der Client kommt nur ueber 127.0.0.1. Das erlaubt der Host nur im Testmodus.
    if ($Tunnel -eq 'Lobby' -and $Role -eq 'Both' -and $Name -eq 'host') { $Arguments += '-ddcoop_localtest' }
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
    $Seen = Wait-ModLog $ProcessId 'Transport: verbunden|keine Verbindung zum Host|Start fehlgeschlagen|Steam nicht verf|abgelehnt' 150
    if ("$Seen" -notmatch 'Transport: verbunden') { throw "Tunnel der Mod nicht aufgebaut: $($Seen -replace '^.*Transport: ', '')" }
    Write-Output "Tunnel der Mod: $($Seen -replace '^.*Transport: ', '')"
    # Ohne Spike oeffnet niemand das Spiel zum Tunnel, die Sitzung endet hier.
    if ($NoSpike) { return }
    Send-SpikeCommand $ProcessId 'open 127.0.0.1:7778'
}
# Die Fixes fuer den Beitritt (Host 0x5901CE8, Client 0x56B16D9) setzt die Mod beim Spielstart, ein Spike ist dafuer nicht noetig.
function Join-Lobby([int]$ProcessId, [string]$LobbyId) {
    $State = Join-OverlayLobby $ProcessId $LobbyId
    if ($State.State -ne 'Im Spiel') { throw "Beitritt zur Lobby gescheitert: $($State.Message)" }
    Write-Output 'Lobby beigetreten, Spiel verbindet sich ueber den Tunnel der Mod.'
}
# Wartet auf das Spiel, das Steam aus seiner Installation startet. Andere Pfade zaehlen nicht.
function Wait-SteamLaunch([int]$TimeoutSeconds) {
    # Write-Host, weil die Rueckgabe der Prozess ist.
    Write-Host "Mod liegt in der Steam-Installation. Jetzt die Einladung in Steam annehmen, Spiel nicht selbst starten (bis zu $TimeoutSeconds s) ..."
    $Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $Deadline) {
        $Found = Get-Process $script:GameProcess -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Select-Object -First 1
        if ($Found) { Write-Step 'Steam hat das Spiel gestartet'; return $Found }
        Start-Sleep -Seconds 1
    }
    throw "Zeitablauf nach $TimeoutSeconds s: Steam hat das Spiel nicht gestartet."
}
# Nur ob +connect_lobby dabei war, ohne die Lobby-ID.
function Get-LaunchSummary([int]$ProcessId) {
    $Line = (Get-CimInstance Win32_Process -Filter "ProcessId=$ProcessId" -ErrorAction SilentlyContinue).CommandLine
    if ("$Line" -match '\+connect_lobby\s+\d+') { '+connect_lobby <Lobby-ID>' } else { 'ohne +connect_lobby' }
}
function Add-Spike($Instance) {
    # Falls Steam einen Neustart in seiner Installation erzwingt, dort niemals den Spike injizieren.
    $Running = Get-Process -Id $Instance.Id -ErrorAction Stop
    if (-not $Running.Path -or $Running.Path -ne $Exe) {
        throw 'Unerwarteter Spielpfad. Kein Spike geladen; eigene Sitzung mit Stop-Session.ps1 beenden.'
    }
    $Injected = & py (Join-Path $PSScriptRoot 'inject.py') $Instance.Id $Dll
    if ($LASTEXITCODE) { throw "Spike nicht geladen in $($Instance.Id): $Injected" }
    if (-not $Brief) { $Injected }
}
$GameHost = if ($Role -ne 'Client') { Start-Instance 'host' 0 }
if ($SteamLaunch) {
    # Vorab eintragen, damit Stop-Session.ps1 die Mod auch nach einem Abbruch waehrend des Wartens aus der Steam-Installation nimmt.
    @{ Host = 0; Client = 0; Role = $Role; Guard = $Guard.Id; Mod = $true; SteamInstall = $true; Tunnel = $Tunnel; NoSpike = $true; Map = $Map } |
        ConvertTo-Json | Set-Content (Join-Path $script:SessionDir 'session.json') -Encoding ascii
    try { $Client = Wait-SteamLaunch $JoinTimeoutSeconds }
    catch {
        & (Join-Path $PSScriptRoot 'Stop-Session.ps1') | Out-Null
        throw
    }
}
$Client = if ($SteamLaunch) { $Client }
elseif ($Role -ne 'Host') { Start-Instance 'client' $(if ($Role -eq 'Both') { 820 } else { 0 }) }
$Local = @($GameHost, $Client) | Where-Object { $_ }
$LocalIds = @($Local | ForEach-Object { $_.Id })
# Die Rolle, die auf einem anderen PC laeuft, steht mit Prozess-ID 0 in der Sitzung.
$HostId = if ($GameHost) { $GameHost.Id } else { 0 }
$ClientId = if ($Client) { $Client.Id } else { 0 }
@{ Host = $HostId; Client = $ClientId; Role = $Role; Guard = $Guard.Id; Mod = [bool]$Mod; SteamInstall = [bool]$SteamLaunch; Tunnel = $Tunnel; NetSim = $NetSim; NoSpike = [bool]$NoSpike; Map = $Map; Started = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() } | ConvertTo-Json |
    Set-Content (Join-Path $script:SessionDir 'session.json') -Encoding ascii
Write-SessionLock $HostId $ClientId | Set-Content $script:LockFile -Encoding ascii

# Beim Start ueber Steam tritt die Mod schon im Hauptmenue bei, das Hauptmenue ist dann oft nicht mehr zu sehen.
if (-not $SteamLaunch) {
    foreach ($Instance in $Local) { Wait-MainMenu $Instance.Id $MenuTimeoutSeconds }
    Write-Step 'Hauptmenue geladen'
}
else { Wait-Condition { (Get-NetState $Client.Id).world } 'Welt des von Steam gestarteten Spiels' $MenuTimeoutSeconds @($Client.Id) }
Start-Sleep -Seconds 2
if ($NoSpike) {
    foreach ($Instance in $Local) { Assert-NoSpikeLoaded $Instance.Id }
    Write-Step 'ohne Spike, nur DDCoop.dll'
}
else {
    foreach ($Instance in $Local) { Add-Spike $Instance }
    Start-Sleep -Seconds 3
    Write-Step 'Spike geladen'
}
if ($Godmode) {
    foreach ($Instance in $Local) { Send-SpikeCommand $Instance.Id '!god on' }
    Start-Sleep -Seconds 1
}

$HostHasClient = { (Get-NetState $GameHost.Id).clients -ge 1 }

if ($GameHost -and $Tunnel -notin 'Lobby') {
    Send-SpikeCommand $GameHost.Id 'open /Game/Maps/MainMenu?listen'
    Wait-Condition { (Get-NetState $GameHost.Id).role -eq 'host' } 'Host lauscht' 60 $LocalIds
    Start-Sleep -Seconds 2
}
$LobbyId = $null
if ($GameHost -and $Tunnel -eq 'Lobby') {
    $LobbyId = New-OverlayLobby $GameHost.Id
    Write-Output "Lobby erstellt: $LobbyId"
}
if ($Role -eq 'Both') {
    if ($Tunnel -eq 'None') {
        Send-SpikeCommand $Client.Id 'open 127.0.0.1:7777'
    }
    elseif ($Tunnel -eq 'Mod') {
        Connect-ModTransport $Client.Id | Out-Null
    }
    elseif ($Tunnel -eq 'Lobby') {
        Join-Lobby $Client.Id $LobbyId
    }
    else {
        Send-SpikeCommand $GameHost.Id '!tunnel host'
        Start-Sleep -Seconds 2
        Send-SpikeCommand $Client.Id $(if ($Tunnel -eq 'Ip') { '!tunnel join ip' } else { '!tunnel join self' })
        Start-Sleep -Seconds 8
        Send-SpikeCommand $Client.Id 'open 127.0.0.1:7778'
    }
    Wait-Condition $HostHasClient 'Client beim Host angekommen' 120 $LocalIds
    Wait-Condition { (Get-NetState $Client.Id).players -ge 2 } 'Client sieht beide Spieler' 60 $LocalIds
    Write-Step 'verbunden'
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
    elseif ($Tunnel -eq 'Lobby') {
        Write-Output "Host bereit. Client-PC: -Tunnel Lobby -Peer $LobbyId oder Einladung ueber das Overlay der Mod."
    }
    else {
        Write-Output 'Host bereit, wartet auf direkte Verbindung auf Port 7777.'
    }
    Write-Output "Warte bis zu $JoinTimeoutSeconds s auf den Client ..."
    Wait-Condition $HostHasClient 'kein Client verbunden' $JoinTimeoutSeconds $LocalIds 2000
    Write-Output 'Client verbunden.'
    Start-Sleep -Seconds 5
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
        if ($NoSpike) {
            Write-Step 'Tunnel steht, ohne Spike verbindet das Spiel nicht. Sitzung mit Stop-Session.ps1 beenden.'
            return
        }
    }
    elseif ($SteamLaunch) {
        Write-Output "Startparameter von Steam: $(Get-LaunchSummary $Client.Id)"
        $Seen = Wait-ModLog $Client.Id 'Start .ber Steam-Einladung' 120
        if (-not $Seen) { throw 'Mod meldet keinen Start ueber eine Steam-Einladung (+connect_lobby).' }
        Write-Output "Mod: $($Seen -replace '^.*Lobby: ', '')"
    }
    elseif ($Tunnel -eq 'Lobby') {
        Join-Lobby $Client.Id $Peer
    }
    else {
        Send-SpikeCommand $Client.Id "open $Peer"
    }
    $Target = ($Map -split '/')[-1]
    Write-Output "Warte bis zu $JoinTimeoutSeconds s, bis der Host nach $Target reist ..."
    Wait-Condition { $State = Get-NetState $Client.Id; ($State.world -eq $Target -or $State.package -like "*$Target") -and $State.players -ge 2 } "Client nicht in $Target" $JoinTimeoutSeconds $LocalIds 2000
}

# Reise nur, wenn der Host nicht schon dort ist. Die Lobby macht ihn selbst zum Listen-Server in der MetaBase.
$Target = ($Map -split '/')[-1]
if ($GameHost -and (Get-NetState $GameHost.Id).world -ne $Target) {
    Send-SpikeCommand $GameHost.Id "servertravel $Map`?listen"
    Wait-Condition {
        $HostState = Get-NetState $GameHost.Id
        if ($HostState.world -ne $Target -or $HostState.players -lt 2) { return $false }
        if (-not $Client) { return $true }
        $ClientState = Get-NetState $Client.Id
        $ClientState.world -eq $Target -and $ClientState.players -ge 2
    } "Reise nach $Target" $TravelTimeoutSeconds $LocalIds
    Write-Step "in $Target"
}
# Kurz Zeit fuer Controller-Tausch und Spawn, frueher 40 s fest.
Start-Sleep -Seconds 5

if (@($Local | Where-Object { $_.HasExited }).Count) { throw 'Eine Instanz ist abgestuerzt, siehe tools\spike\crashes.py.' }
if ($NoSpike) {
    # Kamera, Client-Position und Reise uebernimmt die Mod selbst, Trace und Flow sind Messung des Spikes.
    foreach ($Instance in $Local) { Assert-NoSpikeLoaded $Instance.Id }
    if ($Brief) {
        if ($GameHost) { Format-NetBrief $GameHost.Id 'Host' }
        if ($Client) { Format-NetBrief $Client.Id 'Client' }
    }
    else { & py (Join-Path $PSScriptRoot 'netstate.py') $LocalIds }
    Write-Step 'Sitzung bereit, ohne Spike'
    if (-not $Brief) { Write-Output "Logs: $(($Local | ForEach-Object { Join-Path (Split-Path $Exe) "DDCoop\logs\ddcoop-$($_.Id).log" }) -join ' und ')" }
    return
}
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
if ($Brief) {
    if ($GameHost) { Format-NetBrief $GameHost.Id 'Host' }
    if ($Client) { Format-NetBrief $Client.Id 'Client' }
    Write-Step 'Sitzung bereit'
    return
}
& py (Join-Path $PSScriptRoot 'netstate.py') $LocalIds
Write-Step 'Sitzung bereit'
Write-Output "Logs: $(($Local | ForEach-Object { Join-Path $script:CommandDir "$($_.Id).log" }) -join ' und ')"
