# Prueft das Ende einer Sitzung ohne Handarbeit, wie in src\Net\README.md unter "Fehlerbehandlung": Host und Client
# verlassen, Client und Host hart beendet, Neubeitritt. Misst die Zeit bis zur Meldung im Overlay der Gegenseite.
#   Test-NetEnd.ps1 [-Start] [-Cases a,b,d,c]
# -Start beginnt mit Start-Session.ps1 -Mod -Tunnel Lobby -Restart, sonst muss eine solche Sitzung laufen.
# Der Fall c (Host beendet) kommt zuletzt, danach endet die Sitzung mit Stop-Session.ps1.
# Bericht als Tabelle und in research\local\net-end-<Zeit>.md. Exitcode 1, wenn ein Fall scheitert.
param(
    [switch]$Start,
    [ValidateSet('a', 'b', 'c', 'd')][string[]]$Cases = @('a', 'b', 'd', 'c'),
    [int]$TimeoutSeconds = 40
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
if ($Start) { & (Join-Path $PSScriptRoot 'Start-Session.ps1') -Mod -Tunnel Lobby -Restart | Out-Host }
$Session = Get-Session
if ($Session.Role -ne 'Both' -or -not $Session.PSObject.Properties['Tunnel'] -or $Session.Tunnel -ne 'Lobby') {
    throw 'Braucht eine Sitzung mit Start-Session.ps1 -Mod -Tunnel Lobby auf diesem PC, oder -Start.'
}
$Results = New-Object System.Collections.Generic.List[object]
function Add-Result([string]$Case, [string]$What, $Seconds, [string]$Seen, [bool]$Ok) {
    $Results.Add([pscustomobject]@{ Fall = $Case; Pruefung = $What; Sekunden = if ($null -ne $Seconds) { '{0:n1}' -f $Seconds } else { '-' }; Gesehen = $Seen; Ok = $Ok })
}
# Zeit, bis die Bedingung gilt, oder $null. $script:Seen haelt, was zuletzt gelesen wurde.
function Measure-Until([scriptblock]$Condition, [int]$Timeout = $TimeoutSeconds) {
    $Timer = [Diagnostics.Stopwatch]::StartNew()
    while ($Timer.Elapsed.TotalSeconds -lt $Timeout) {
        try { if (& $Condition) { return $Timer.Elapsed.TotalSeconds } } catch { $script:Seen = "$_" }
        Start-Sleep -Milliseconds 250
    }
    $null
}
function Test-Overlay([int]$ProcessId, [string]$Pattern) {
    $State = Get-OverlayState $ProcessId
    $script:Seen = "$($State.State) | $($State.Message)"
    $State.Message -match $Pattern
}
function Test-World([int]$ProcessId, [string]$World, [int]$Players = 0) {
    $State = Get-NetState $ProcessId
    $script:Seen = "$($State.world), $($State.players) Spieler"
    $State.world -eq $World -and (-not $Players -or $State.players -eq $Players)
}
function Measure-Rejoin([string]$Case) {
    $HostId = Get-RolePid 'Host'; $ClientId = Get-RolePid 'Client'
    $Timer = [Diagnostics.Stopwatch]::StartNew()
    try {
        Join-HostLobby $HostId $ClientId | Out-Null
        $World = (Get-NetState $HostId).world
        Wait-Condition { Test-World $ClientId $World 2 } 'Client wieder in der Welt des Hosts' 60 @($HostId, $ClientId) 500
        Add-Result $Case 'Neubeitritt per Lobby-ID' $Timer.Elapsed.TotalSeconds 'Im Spiel' $true
    }
    catch { Add-Result $Case 'Neubeitritt per Lobby-ID' $null "$_" $false }
}
function Get-CrashLine { (& py (Join-Path $PSScriptRoot 'crashes.py') --session | Select-Object -First 1) -join '' }
$CrashesBefore = Get-CrashLine

foreach ($Case in $Cases) {
    $HostId = Get-RolePid 'Host'; $ClientId = Get-RolePid 'Client'
    $script:Seen = ''
    switch ($Case) {
        'a' {
            Invoke-OverlayAction $HostId Leave
            $T = Measure-Until { Test-Overlay $ClientId 'Der Host hat die Sitzung verlassen' }
            Add-Result 'a' 'Host verlaesst: Meldung beim Client' $T $script:Seen ($null -ne $T)
            $T = Measure-Until { Test-World $ClientId 'MainMenu' }
            Add-Result 'a' 'Client im Hauptmenue' $T $script:Seen ($null -ne $T)
            Measure-Rejoin 'e'
        }
        'b' {
            Invoke-OverlayAction $ClientId Leave
            $T = Measure-Until { Test-Overlay $HostId 'Der Mitspieler hat die Sitzung verlassen' }
            Add-Result 'b' 'Client verlaesst: Meldung beim Host' $T $script:Seen ($null -ne $T)
            $T = Measure-Until { Test-World $HostId (Get-NetState $HostId).world 1 }
            Add-Result 'b' 'P2 beim Host entfernt' $T $script:Seen ($null -ne $T)
            Measure-Rejoin 'e'
        }
        'd' {
            Stop-Process -Id $ClientId -Force
            $T = Measure-Until { Test-Overlay $HostId 'Verbindung zum Mitspieler verloren' }
            Add-Result 'd' 'Client beendet: Meldung beim Host' $T $script:Seen ($null -ne $T)
            try {
                & (Join-Path $PSScriptRoot 'Restart-Instance.ps1') | Out-Null
                Add-Result 'd' 'Client neu gestartet und verbunden' $null 'Restart-Instance.ps1' $true
            }
            catch { Add-Result 'd' 'Client neu gestartet und verbunden' $null "$_" $false }
        }
        'c' {
            Stop-Process -Id $HostId -Force
            $T = Measure-Until { Test-Overlay $ClientId 'Verbindung zum Host verloren' }
            Add-Result 'c' 'Host beendet: Meldung beim Client' $T $script:Seen ($null -ne $T)
            $T = Measure-Until { Test-World $ClientId 'MainMenu' }
            Add-Result 'c' 'Client im Hauptmenue' $T $script:Seen ($null -ne $T)
        }
    }
}
$CrashesAfter = Get-CrashLine
Add-Result '-' 'Abstuerze' $null $(if ($CrashesAfter -eq $CrashesBefore) { 'keine neuen' } else { $CrashesAfter }) ($CrashesAfter -eq $CrashesBefore)
if ($Cases -contains 'c') { & (Join-Path $PSScriptRoot 'Stop-Session.ps1') | Out-Null }

$Results | Format-Table -AutoSize | Out-String -Width 220 | Write-Output
$Report = Join-Path $script:RepoRoot ("research\local\net-end-{0}.md" -f (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path (Split-Path $Report) | Out-Null
$Lines = @('| Fall | Pruefung | Sekunden | Gesehen | Ok |', '|---|---|---|---|---|')
$Lines += $Results | ForEach-Object { "| $($_.Fall) | $($_.Pruefung) | $($_.Sekunden) | $($_.Gesehen -replace '\|', '/') | $(if ($_.Ok) { 'ja' } else { 'nein' }) |" }
Set-Content -Path $Report -Value $Lines -Encoding utf8
Write-Output "Bericht: $Report"
if (@($Results | Where-Object { -not $_.Ok }).Count) { exit 1 }
