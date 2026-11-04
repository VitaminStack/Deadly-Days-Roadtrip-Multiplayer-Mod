# Fester Routenlauf fuer Tests ueber mehrere Tage: gleicher Seed, gleiche Knoten, nach jedem Kartenwechsel check_round.py.
# Startet in der MetaBase einer laufenden Sitzung, mit -Start vorher ueber Start-Session.ps1. Nie die Exe von Hand starten.
#   Run-Route.ps1 [-Days 4] [-Seed 20795] [-Start] [-StayLast]
# Ergebnis unter research\local\session\runs\<zeit>\: checks.jsonl, Bilder beider Fenster waehrend und nach dem Laden.
# Exitcode 0 alle Pruefungen sauber, 1 Abweichung, 2 Instanz abgestuerzt.
param(
    [int]$Days = 4,
    [int]$Seed = 20795,
    # Knoten der Routenkarte fuer Seed 20795, Tag 1 bis 5, in Fensterkoordinaten (800 x 450).
    [string[]]$Nodes = @('62,213', '160,213', '255,213', '352,112', '448,230'),
    [switch]$Start,
    # Bleibt am letzten Tag in der Mission, statt ihn zu beenden.
    [switch]$StayLast,
    [int]$Settle = 10
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$GameInput = Join-Path $PSScriptRoot 'Game-Input.ps1'
$Check = Join-Path $PSScriptRoot 'check_round.py'
if ($Days -gt $Nodes.Count) { throw "Nur $($Nodes.Count) Knoten bekannt, -Nodes ergaenzen." }
# -Restart ersetzt nur eine laufende Sitzung dieses Ordners, fremde bleiben unberuehrt.
if ($Start) { & (Join-Path $PSScriptRoot 'Start-Session.ps1') -Restart | Select-Object -Last 1 }
$Session = Get-Session
$RunDir = Join-Path $script:SessionDir ("runs\{0:yyyyMMdd-HHmmss}" -f (Get-Date))
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
$Log = Join-Path $RunDir 'checks.jsonl'
$Failed = $false

function Save-Shots([string]$Name) {
    foreach ($Role in 'Host', 'Client') {
        if (Test-GameProcess (Get-RolePid $Role)) {
            & $GameInput $Role shot (Join-Path $RunDir "$Name-$($Role.ToLower()).png") | Out-Null
        }
    }
}

# Startet die Pruefung im Hintergrund, damit sie die Ladezeit ab dem Klick misst, waehrend hier Bilder entstehen.
function Start-Check([string]$Label, [string[]]$Extra) {
    $Out = Join-Path $RunDir "$Label.json"
    $Arguments = @("`"$Check`"", $Session.Host, $Session.Client, '--label', $Label, '--log', "`"$Log`"") + $Extra
    $Process = Start-Process py -ArgumentList $Arguments -RedirectStandardOutput $Out -NoNewWindow -PassThru
    $null = $Process.Handle # Ohne fruehen Zugriff auf das Handle liefert ExitCode spaeter nichts.
    $Process
}

function Invoke-Check([string]$Label, [string[]]$Extra, $Process) {
    if (-not $Process) { $Process = Start-Check $Label $Extra }
    $Process.WaitForExit()
    $Code = $Process.ExitCode
    $Result = Get-Content (Join-Path $RunDir "$Label.json") -Raw | ConvertFrom-Json
    $State = if ($Result.ok) { 'sauber' } else { 'ABWEICHUNG' }
    Write-Output ("{0,-8} {1,-10} Laden {2,5} s  Host {3}/{4}  Client {5}/{6}  Tiles {7}/{8}  GUIDs {9}" -f $Label, $State,
        $Result.load_seconds, $Result.host.player_array, $Result.host.characters.Count, $Result.client.player_array,
        $Result.client.characters.Count, $Result.host.map_tiles, $Result.client.map_tiles, $Result.client.guid_entries)
    foreach ($Problem in $Result.problems) { Write-Output "         - $Problem" }
    if ($Code -eq 2) {
        & py (Join-Path $PSScriptRoot 'crashes.py') --session --latest 1
        Write-Output "Lauf abgebrochen, Ergebnisse in $RunDir"
        exit 2
    }
    if ($Code -ne 0) { $script:Failed = $true }
    if ($Result.ok -and $Label -ne 'metabase') {
        $InventoryPath = Join-Path $RunDir "$Label-inventory.json"
        & py (Join-Path $PSScriptRoot 'inventory.py') $Session.Host $Session.Client |
            Set-Content $InventoryPath -Encoding utf8
        if ($LASTEXITCODE -ne 0) {
            Write-Output "         - Inventar nicht vollstaendig/gleich, siehe $InventoryPath"
            $script:Failed = $true
        }
        else { Write-Output '         Inventar complete=true equal=true' }
    }
}

# Ausgangslage: beide in der MetaBase, Vergleichsstand zuruecksetzen.
Invoke-Check 'metabase' @('--world', 'MetaBase', '--reset')
Send-SpikeCommand $Session.Host '!notutorial'
Send-SpikeCommand $Session.Client '!notutorial'
& (Join-Path $PSScriptRoot 'Start-Run.ps1') -Seed $Seed | Out-Null
Copy-Item (Join-Path $script:SessionDir 'route.png') (Join-Path $RunDir 'route-1.png')

for ($Day = 1; $Day -le $Days; $Day++) {
    $X, $Y = $Nodes[$Day - 1] -split ','
    & $GameInput Host click $X $Y
    $Pending = Start-Check "tag$Day" @('--wait', '--settle', $Settle)
    # Bilder waehrend des Ladens, um seltsames Laden spaeter nachvollziehen zu koennen.
    $Elapsed = 0
    foreach ($Second in 3, 8, 15) {
        Start-Sleep -Seconds ($Second - $Elapsed)
        $Elapsed = $Second
        Save-Shots "tag$Day-laden-${Second}s"
    }
    Invoke-Check "tag$Day" @() $Pending
    Save-Shots "tag$Day"
    if ($Day -eq $Days -and $StayLast) { break }

    # Rundenuebersicht, Fortsetzen, Lager bestaetigen, wie Travel-Day.ps1.
    Send-SpikeCommand $Session.Host '!finish'
    Start-Sleep -Seconds 6
    & $GameInput Host click 410 426
    Start-Sleep -Seconds 4
    & $GameInput Host click 633 422
    Start-Sleep -Seconds 4
    foreach ($Role in 'Host', 'Client') {
        if (-not (Test-GameProcess (Get-RolePid $Role))) {
            Write-Output "$Role ist nach Tag $Day abgestuerzt."
            & py (Join-Path $PSScriptRoot 'crashes.py') --session --latest 1
            exit 2
        }
    }
    & $GameInput Host shot (Join-Path $RunDir "route-$($Day + 1).png") | Out-Null
}
Write-Output "Ergebnisse in $RunDir"
if ($Failed) { exit 1 }
