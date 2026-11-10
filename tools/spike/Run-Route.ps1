# Fester Routenlauf fuer Tests ueber mehrere Tage: gleicher Seed, gleiche Knoten, nach jedem Kartenwechsel check_round.py.
# Startet in der MetaBase einer laufenden Sitzung, mit -Start vorher ueber Start-Session.ps1. Nie die Exe von Hand starten.
#   Run-Route.ps1 [-Days 4] [-Seed 20795] [-Start] [-StayLast] [-NetProfile Internet]
# -NetProfile startet mit -Start eine Sitzung mit Mod und Lobby und simulierter Latenz, siehe Start-Session.ps1.
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
    [int]$Settle = 10,
    # Nur mit -Start: Netzprofil fuer Start-Session.ps1, etwa Internet.
    [string]$NetProfile,
    # Nur mit -Start: Sitzung allein mit der Mod, ohne spike.dll (Start-Session.ps1 -NoSpike). Dann nur -Days 1 -StayLast,
    # Zufalls-Seed, und der erste Knoten wird gesucht statt aus -Nodes genommen.
    [switch]$NoSpike,
    # Spike-Godmode fuer beide Spieler, nach jedem Laden erneut an, damit der Lauf nicht durch einen Tod endet.
    [switch]$Godmode,
    # Vergleicht nach jeder Ankunft, kurz vor dem Tagesende und auf der Routenkarte Tag, Uhr, Tank und Missionsziele beider Seiten
    # (day_state.py, braucht den Testkanal der Mod). Jede Abweichung zaehlt wie eine fehlgeschlagene Pruefung.
    [switch]$DayState,
    # Mit -DayState: so lange in jeder Mission bleiben, bevor sie endet, damit Uhr, Nacht und Gefahr laufen.
    [int]$DaySeconds = 0
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$GameInput = Join-Path $PSScriptRoot 'Game-Input.ps1'
$Check = Join-Path $PSScriptRoot 'check_round.py'
if ($Days -gt $Nodes.Count) { throw "Nur $($Nodes.Count) Knoten bekannt, -Nodes ergaenzen." }
# -Restart ersetzt nur eine laufende Sitzung dieses Ordners, fremde bleiben unberuehrt.
if ($NetProfile -and -not $Start) { throw '-NetProfile gilt nur mit -Start. Laufende Sitzung: Profil steht in session.json.' }
if ($NoSpike -and -not $Start) { throw '-NoSpike gilt nur mit -Start. Laufende Sitzung: steht in session.json.' }
if ($Start -and ($NetProfile -or $NoSpike)) {
    $Options = @{ Restart = $true; Mod = $true; Tunnel = 'Lobby'; Brief = $true; NoSpike = [bool]$NoSpike }
    if ($NetProfile) { $Options.NetProfile = $NetProfile }
    if ($Godmode) { $Options.Godmode = $true }
    & (Join-Path $PSScriptRoot 'Start-Session.ps1') @Options | Out-Host
}
elseif ($Start) { & (Join-Path $PSScriptRoot 'Start-Session.ps1') -Restart | Select-Object -Last 1 }
$Session = Get-Session
$WithSpike = Test-SessionSpike
# Ohne Spike gibt es kein !finish, der Lauf endet also in der ersten Mission.
if (-not $WithSpike -and -not ($Days -eq 1 -and $StayLast)) { throw 'Ohne Spike nur -Days 1 -StayLast: Missionsende braucht !finish aus dem Spike.' }
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

# Tag, Uhr, Tank und Missionsziele beider Seiten (nur mit -DayState). Rohdaten je Zeitpunkt unter <Label>-day.json.
function Invoke-DayState([string]$Label) {
    if (-not $DayState) { return }
    $Lines = & py (Join-Path $PSScriptRoot 'day_state.py') $Session.Host $Session.Client --json (Join-Path $RunDir "$Label-day.json")
    $Code = $LASTEXITCODE
    $Lines | Where-Object { $_ -match '^[!~]' } | ForEach-Object { Write-Output "         $_" }
    Write-Output ("{0,-16} {1}" -f $Label, ($Lines | Select-Object -Last 1))
    if ($Code -ne 0) { $script:Failed = $true }
}

function Invoke-Check([string]$Label, [string[]]$Extra, $Process) {
    if (-not $Process) { $Process = Start-Check $Label $Extra }
    $Process.WaitForExit()
    $Code = $Process.ExitCode
    $Result = Get-Content (Join-Path $RunDir "$Label.json") -Raw | ConvertFrom-Json
    $State = if ($Result.ok) { 'sauber' } else { 'ABWEICHUNG' }
    Write-Output ("{0,-8} {1,-10} Laden {2,5} s  Host {3}/{4}  Client {5}/{6}  Tiles {7}/{8}  GUIDs {9}  MB {10}/{11}  UObj {12}/{13}" -f $Label, $State,
        $Result.load_seconds, $Result.host.player_array, $Result.host.characters.Count, $Result.client.player_array,
        $Result.client.characters.Count, $Result.host.map_tiles, $Result.client.map_tiles, $Result.client.guid_entries,
$Result.host.private_mb, $Result.client.private_mb, $Result.host.uobjects, $Result.client.uobjects)
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
# Ohne Spike schaltet die Mod die Tutorial-Hinweise selbst ab (TutorialAdapter), einen festen Seed gibt es dann nicht.
if ($WithSpike) {
    Send-SpikeCommand $Session.Host '!notutorial'
    Send-SpikeCommand $Session.Client '!notutorial'
    & (Join-Path $PSScriptRoot 'Start-Run.ps1') -Seed $Seed | Out-Null
}
else { & (Join-Path $PSScriptRoot 'Start-Run.ps1') | Out-Null }
Copy-Item (Join-Path $script:SessionDir 'route.png') (Join-Path $RunDir 'route-1.png') -ErrorAction SilentlyContinue

for ($Day = 1; $Day -le $Days; $Day++) {
    # Vor dem Klick starten, damit load_seconds ab dem Klick zaehlt; die World wechselt in unter einer Sekunde.
    $Pending = Start-Check "tag$Day" @('--wait', '--settle', $Settle)
    if ($WithSpike) {
        $X, $Y = $Nodes[$Day - 1] -split ','
        & $GameInput Host click $X $Y
    }
    elseif (-not (Test-HostInMission)) {
        # Zufalls-Seed: Die Knoten fuer Tag 1 liegen in der linken Spalte, ihre Hoehe wechselt je Run. Ein Klick daneben tut nichts.
        foreach ($Y in 113, 163, 213, 263, 313, 363) {
            & $GameInput Host click 62 $Y
            Start-Sleep -Seconds 2
            if (-not (Test-RouteMap)) { break }
        }
        if (Test-RouteMap) { throw 'Kein Knoten fuer Tag 1 gefunden, siehe Bild der Routenkarte.' }
    }
    # Bilder waehrend des Ladens, um seltsames Laden spaeter nachvollziehen zu koennen.
    $Elapsed = 0
    foreach ($Second in 3, 8, 15) {
        Start-Sleep -Seconds ($Second - $Elapsed)
        $Elapsed = $Second
        Save-Shots "tag$Day-laden-${Second}s"
    }
    Invoke-Check "tag$Day" @() $Pending
    if ($Godmode -and $WithSpike) { foreach ($Id in $Session.Host, $Session.Client) { Send-SpikeCommand $Id '!god on' } }
    Save-Shots "tag$Day"
    Invoke-DayState "tag$Day-ankunft"
    if ($DayState -and $DaySeconds -gt 0) {
        Start-Sleep -Seconds $DaySeconds
        Invoke-DayState "tag$Day-spaet"
    }
    if ($Day -eq $Days -and $StayLast) { break }

    # Rundenuebersicht, Fortsetzen, Lager bestaetigen bei Host und Client, wie Travel-Day.ps1.
    if (-not (Complete-RoundEnd)) {
        Write-Output "         - Host nach Tag $Day nicht auf der Routenkarte, Lager nicht von beiden bestaetigt?"
        $Failed = $true
    }
    foreach ($Role in 'Host', 'Client') {
        if (-not (Test-GameProcess (Get-RolePid $Role))) {
            Write-Output "$Role ist nach Tag $Day abgestuerzt."
            & py (Join-Path $PSScriptRoot 'crashes.py') --session --latest 1
            exit 2
        }
    }
    & $GameInput Host shot (Join-Path $RunDir "route-$($Day + 1).png") | Out-Null
    Invoke-DayState "tag$Day-route"
}
Write-Output "Ergebnisse in $RunDir"
if ($Failed) { exit 1 }
