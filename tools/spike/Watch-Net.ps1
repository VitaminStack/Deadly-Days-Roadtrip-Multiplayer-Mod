# Beobachtet Host und Client der Sitzung: Overlay-Zustand, Prozess und neue Zeilen der Mod-Logs zu Sitzung, Transport,
# Lobby und Welt, jeweils mit Zeit ab dem Aufruf. Nur Aenderungen werden ausgegeben.
#   Watch-Net.ps1 [-Seconds 30] [-Until 'verlassen|verloren']
# Mit -Until endet die Beobachtung, sobald eine Overlay-Meldung oder Logzeile passt. Exitcode 1, wenn sie nicht kam.
param(
    [int]$Seconds = 30,
    [string]$Until,
    [int]$PollMilliseconds = 500
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$Logs = Join-Path (Split-Path (Get-GameExe)) 'DDCoop\logs'
$Roles = @(Get-LocalRoles)
$Pids = @{}; $Seen = @{}; $Last = @{}
foreach ($Role in $Roles) {
    $Pids[$Role] = Get-RolePid $Role
    $Log = Join-Path $Logs "ddcoop-$($Pids[$Role]).log"
    $Seen[$Role] = if (Test-Path $Log) { @(Get-Content $Log).Count } else { 0 }
}
$Timer = [Diagnostics.Stopwatch]::StartNew()
function Out-Event([string]$Role, [string]$Text) { '{0,6:n1} s  {1,-6} {2}' -f $Timer.Elapsed.TotalSeconds, $Role, $Text }
while ($Timer.Elapsed.TotalSeconds -lt $Seconds) {
    foreach ($Role in $Roles) {
        $ProcessId = $Pids[$Role]
        $Now = if (-not (Test-GameProcess $ProcessId)) { 'Prozess beendet' }
        else { try { $State = Get-OverlayState $ProcessId; "Overlay: $($State.State) | $($State.Message)" } catch { 'Overlay: nicht lesbar' } }
        if ($Now -ne $Last[$Role]) {
            Out-Event $Role $Now
            $Last[$Role] = $Now
            if ($Until -and $Now -match $Until) { exit 0 }
        }
        $Log = Join-Path $Logs "ddcoop-$ProcessId.log"
        if (Test-Path $Log) {
            $Lines = @(Get-Content $Log)
            foreach ($Line in ($Lines | Select-Object -Skip $Seen[$Role])) {
                if ($Line -match '\] (Sitzung|Transport|Lobby|Welt|Protokoll): (.*)$') {
                    Out-Event $Role "Log: $($Matches[1]): $($Matches[2])"
                    if ($Until -and $Line -match $Until) { exit 0 }
                }
            }
            $Seen[$Role] = $Lines.Count
        }
    }
    Start-Sleep -Milliseconds $PollMilliseconds
}
if ($Until) { Write-Output "Nach $Seconds s kein '$Until'."; exit 1 }
