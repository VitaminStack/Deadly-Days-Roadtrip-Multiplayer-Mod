# Prueft den Wiederbeitritt ohne Neustart (wiedereintritt-karte): Der Client verlaesst die laufende Mission ueber den Testkanal,
# tritt im selben Prozess der Lobby des Hosts wieder bei und muss die Karte des Hosts uebernehmen. Ohne echte Eingaben.
#   Test-Rejoin.ps1 [-Rounds 1] [-TimeoutSeconds 90] [-SettleSeconds 5]
# Braucht eine laufende Sitzung mit Start-Session.ps1 -Mod -Tunnel Lobby -Map /Game/Maps/DDDynamic_Map. Vor und nach jedem
# Wiederbeitritt stehen Inventarvergleich (inventory.py) und Get-Status.ps1 -Brief in der Ausgabe. Exitcode 1, wenn eine Runde scheitert.
param(
    [int]$Rounds = 1,
    [int]$TimeoutSeconds = 90,
    # Wartezeit nach dem Wiederbeitritt vor dem Vergleich. Das Inventar gibt der Host erst nach rund 13 s zurueck (wiedereintritt-inventar).
    [int]$SettleSeconds = 5
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$Session = Get-Session
if ($Session.Role -ne 'Both' -or -not $Session.PSObject.Properties['Tunnel'] -or $Session.Tunnel -ne 'Lobby') {
    throw 'Braucht eine Sitzung mit Start-Session.ps1 -Mod -Tunnel Lobby auf diesem PC.'
}
$HostId = Get-RolePid 'Host'; $ClientId = Get-RolePid 'Client'
$ClientLog = Join-Path (Split-Path (Get-GameExe)) "DDCoop\logs\ddcoop-$ClientId.log"

function Show-State([string]$When) {
    Write-Output "--- $When"
    # Bild des Client-Fensters (PrintWindow), unten links stehen Level und XP-Leiste.
    New-Item -ItemType Directory -Force (Join-Path $script:SessionDir 'shots') | Out-Null
    $Shot = Join-Path $script:SessionDir ("shots\rejoin-{0}-{1}.png" -f (Get-Date -Format 'HHmmss'), ($When -replace '\W', '-'))
    & (Join-Path $PSScriptRoot 'Game-Input.ps1') Client shot $Shot | Out-Null
    Write-Output "Bild: $Shot"
    & (Join-Path $PSScriptRoot 'Get-Status.ps1') -Brief | Write-Output
    $Json = (& py (Join-Path $PSScriptRoot 'inventory.py') $HostId $ClientId 2>$null) -join "`n"
    try { $Inventory = $Json | ConvertFrom-Json } catch { Write-Output "Inventar: nicht lesbar"; return }
    $Parts = foreach ($Side in 'host', 'client') {
        $Players = foreach ($Player in $Inventory.$Side.PSObject.Properties) {
            "$($Player.Name)=$(@($Player.Value.items).Count) Items/$(@($Player.Value.slots).Count) Felder"
        }
        "$Side $($Players -join ', ')"
    }
    Write-Output "Inventar: $($Parts -join ' | '), gleich=$($Inventory.equal)"
}
function Get-LogCount { if (Test-Path $ClientLog) { @(Get-Content -Encoding UTF8 $ClientLog).Count } else { 0 } }
# Erste neue Kartenzeile des Clients ab Zeile $From: Uebernahme der Host-Werte oder eigene Karte.
function Find-MapLine([int]$From) {
    if (-not (Test-Path $ClientLog)) { return $null }
    @(Get-Content -Encoding UTF8 $ClientLog) | Select-Object -Skip $From |
        Where-Object { $_ -match 'Karte: Client .bernimmt \d|Client erzeugt eine eigene Karte' } | Select-Object -First 1
}

$Failed = 0
for ($Round = 1; $Round -le $Rounds; ++$Round) {
    Show-State "Runde $Round vor der Trennung"
    $World = (Get-NetState $HostId).world
    Invoke-OverlayAction $ClientId Leave
    Wait-Condition { (Get-NetState $ClientId).world -eq 'MainMenu' } 'Client im Hauptmenue' 60 @($HostId, $ClientId) 500
    Write-Output "Client im Hauptmenue"
    $From = Get-LogCount
    Join-HostLobby $HostId $ClientId | Out-Null
    $Timer = [Diagnostics.Stopwatch]::StartNew()
    $Line = $null
    while (-not $Line -and $Timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        foreach ($ProcessId in @($HostId, $ClientId)) { if (-not (Test-GameProcess $ProcessId)) { throw "Instanz $ProcessId ist beendet, siehe crashes.py." } }
        $Line = Find-MapLine $From
        if (-not $Line) { Start-Sleep -Milliseconds 500 }
    }
    $Ok = $Line -match 'Client .bernimmt'
    Write-Output ("Runde {0}: {1} nach {2:n1} s: {3}" -f $Round, $(if ($Ok) { 'OK' } else { 'FEHLER' }), $Timer.Elapsed.TotalSeconds, $(if ($Line) { $Line } else { 'keine Kartenzeile' }))
    if (-not $Ok) { ++$Failed; continue }
    Wait-Condition { $S = Get-NetState $ClientId; $S.world -eq $World -and $S.players -eq 2 } 'Client wieder in der Welt des Hosts' 60 @($HostId, $ClientId) 500
    Start-Sleep -Seconds $SettleSeconds # Inventar und Fortschritt kommen nach dem Beitritt per Replikation und Snapshot
    Show-State "Runde $Round nach dem Wiederbeitritt"
}
if ($Failed) { exit 1 }
