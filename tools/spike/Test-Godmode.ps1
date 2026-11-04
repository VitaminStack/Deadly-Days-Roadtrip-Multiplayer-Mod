# Controlled 5-damage positive/negative probe for both authoritative players and their client replicas.
# Run only in a quiet situation without combat or route automation/travel. Leaves Godmode ON even on failure.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
if (-not (Test-OwnSessionRunning)) { throw 'Keine eigene aktive Sitzung.' }
$Session = Get-Session
function Snapshot {
    $Json = & py (Join-Path $PSScriptRoot 'godmode_state.py') $Session.Host $Session.Client
    if ($LASTEXITCODE -ne 0) { throw 'Spielerzustand unvollstaendig; kein Schadenstest.' }
    return ($Json | ConvertFrom-Json)
}
function God([bool]$Enabled) {
    $Command = if ($Enabled) { '!god on' } else { '!god off' }
    Send-SpikeCommand $Session.Host $Command
    Send-SpikeCommand $Session.Client $Command
    Start-Sleep -Seconds 2
}
function Damage {
    foreach ($Index in 0, 1) {
        Send-SpikeCommand $Session.Host "DD_DamagePlayerAtIndex 5 $Index"
        Start-Sleep -Seconds 1
    }
    Start-Sleep -Seconds 1
}
function Assert-SameActors($Before, $After) {
    foreach ($Role in 0, 1) {
        if ($Before[$Role].world -ne $After[$Role].world) { throw 'Kartenwechsel waehrend Schadensprobe.' }
        foreach ($Index in 0, 1) {
            if ($Before[$Role].players[$Index].pawn -ne $After[$Role].players[$Index].pawn) {
                throw 'Spielerfigur waehrend Schadensprobe gewechselt.'
            }
        }
    }
}
$Before = Snapshot
foreach ($Role in $Before) {
    foreach ($Player in $Role.players) {
        if ($Player.health -lt 50) { throw 'Mindestens 50 HP fuer sichere Gegenprobe erforderlich.' }
    }
}
try {
    Send-SpikeCommand $Session.Host '!cheats'
    Start-Sleep -Seconds 1
    God $true
    Damage
    $Protected = Snapshot
    God $false
    Damage
    $Unprotected = Snapshot
    God $true
    Damage
    $Reprotected = Snapshot
    Assert-SameActors $Before $Protected
    Assert-SameActors $Before $Unprotected
    Assert-SameActors $Before $Reprotected
    foreach ($Role in 0, 1) {
        foreach ($Index in 0, 1) {
            $A = $Before[$Role].players[$Index].health
            $B = $Protected[$Role].players[$Index].health
            $C = $Unprotected[$Role].players[$Index].health
            $D = $Reprotected[$Role].players[$Index].health
            if ([math]::Abs($A - $B) -gt 0.01 -or $C -ge $B -or [math]::Abs($C - $D) -gt 0.01) {
                throw "Godmode nicht bestaetigt: Rolle $Role Spieler $Index HP $A -> $B -> $C -> $D"
            }
        }
    }
    $Result = @{ Before = $Before; Protected = $Protected; Unprotected = $Unprotected; Reprotected = $Reprotected }
    $Path = Join-Path $script:SessionDir ("godmode-probe-{0}.json" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
    $Result | ConvertTo-Json -Depth 8 | Set-Content $Path -Encoding utf8
    Write-Output "Godmode fuer beide Spieler samt Client-Replikaten bestaetigt: $Path"
}
finally { God $true }
