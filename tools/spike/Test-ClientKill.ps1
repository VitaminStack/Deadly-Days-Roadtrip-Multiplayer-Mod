# Prueft, ob der Host weiterlaeuft, wenn der Client-Prozess ohne Abmeldung endet (opt-host-absturz-trennung).
# Beendet nur den Client der eigenen Sitzung hart, misst die Zeit bis zum Timeout des Hosts und bis er den Remote-Spieler
# entfernt, und beobachtet den Host danach -HoldSeconds lang. Ohne Maus, Tastatur und Fokus.
#   Test-ClientKill.ps1 [-HoldSeconds 45] [-Label <Text>] [-AnyWorld]
# Braucht eine Sitzung mit Start-Session.ps1 -Mod -Tunnel Lobby (oder Mod) auf diesem PC. Ohne -AnyWorld muss der Host mit
# zwei Spielern in DDDynamic_Map sein. Den Client danach mit Restart-Instance.ps1 neu starten.
# Bericht als Zeilen und in research\local\client-kill-<Zeit>.md. Exitcode 0: Host laeuft, 2: Host beendet, 1: Ablauf unklar.
param(
    [int]$HoldSeconds = 45,
    [string]$Label = '',
    [switch]$AnyWorld,
    [int]$TimeoutSeconds = 40
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$Session = Get-Session
if ($Session.Role -ne 'Both') { throw 'Braucht Host und Client auf diesem PC.' }
$Lock = Get-Content $script:LockFile -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json
if (-not $Lock -or $Lock.Owner -ne $script:RepoRoot) { throw 'Die Sitzung gehoert nicht zu diesem Ordner.' }
$HostId = Get-RolePid 'Host'; $ClientId = Get-RolePid 'Client'
if (-not (Test-GameProcess $HostId) -or -not (Test-GameProcess $ClientId)) { throw 'Host oder Client laeuft nicht.' }
$Before = Get-NetState $HostId
if (-not $AnyWorld -and ($Before.world -ne 'DDDynamic_Map' -or $Before.players -ne 2)) {
    throw "Host ist nicht mit zwei Spielern in der Mission: $($Before.world), $($Before.players) Spieler."
}
$Log = Join-Path (Split-Path (Get-GameExe)) "DDCoop\logs\ddcoop-$HostId.log"
$LinesBefore = @(Get-Content $Log -Encoding utf8).Count
$Crashes = Join-Path $PSScriptRoot 'crashes.py'
$CrashesBefore = (& py $Crashes --session | Select-Object -First 1) -join ''

function Get-NewLines { @(Get-Content $Log -Encoding utf8 | Select-Object -Skip $LinesBefore) }
function Find-Seconds([string]$Pattern, [Diagnostics.Stopwatch]$Timer, [int]$Timeout) {
    while ($Timer.Elapsed.TotalSeconds -lt $Timeout) {
        if (-not (Test-GameProcess $HostId)) { return $null }
        if (Get-NewLines | Where-Object { $_ -match $Pattern }) { return $Timer.Elapsed.TotalSeconds }
        Start-Sleep -Milliseconds 200
    }
    $null
}

$Timer = [Diagnostics.Stopwatch]::StartNew()
Stop-Process -Id $ClientId -Force
$End = Find-Seconds 'Transport: Verbindung beendet' $Timer $TimeoutSeconds
$Unbound = Find-Seconds 'P2-Bindung entfernt|, Host, 1 Spieler' $Timer $TimeoutSeconds
$HoldTimer = [Diagnostics.Stopwatch]::StartNew()
$Died = $null
while ($HoldTimer.Elapsed.TotalSeconds -lt $HoldSeconds) {
    if (-not (Test-GameProcess $HostId)) { $Died = $Timer.Elapsed.TotalSeconds; break }
    Start-Sleep -Milliseconds 250
}
$Alive = Test-GameProcess $HostId
$After = if ($Alive) { Get-NetState $HostId } else { @{} }
$New = Get-NewLines
$Reason = ($New | Where-Object { $_ -match 'Transport: Verbindung beendet' } | Select-Object -First 1) -replace '^.*Transport: ', ''
$CrashesAfter = (& py $Crashes --session | Select-Object -First 1) -join ''
$Fmt = { param($Value) if ($null -ne $Value) { '{0:n1} s' -f $Value } else { '-' } }

$Lines = @(
    "Fall: $Label",
    "Vorher: $($Before.world), $($Before.players) Spieler",
    "Timeout des Hosts nach: $(& $Fmt $End) ($Reason)",
    "Remote-Spieler entfernt nach: $(& $Fmt $Unbound)",
    "Host nach $HoldSeconds s Beobachtung: $(if ($Alive) { "laeuft, $($After.world), $($After.players) Spieler" } else { "BEENDET $(& $Fmt $Died) nach dem Kill" })",
    "Abstuerze: $(if ($CrashesAfter -eq $CrashesBefore) { 'keine neuen' } else { $CrashesAfter })",
    "Neue Fehlerzeilen im Host-Log: $(@($New | Where-Object { $_ -match '\[ERROR\]' }).Count)"
)
$Lines | Write-Output
$Report = Join-Path $script:RepoRoot ("research\local\client-kill-{0}.md" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path (Split-Path $Report) | Out-Null
Set-Content -Path $Report -Value ($Lines + '' + '```' + $New + '```') -Encoding utf8
Write-Output "Bericht: $Report"
if (-not $Alive) { exit 2 }
if ($null -eq $End -or $null -eq $Unbound -or $CrashesAfter -ne $CrashesBefore) { exit 1 }
