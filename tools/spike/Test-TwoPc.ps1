# Test auf zwei PCs ueber den Steam-Tunnel (P2P), je PC eine Rolle. Laeuft ohne Eingriff bis in die Mission an Tag 1,
# bleibt dort und schreibt den Tunnel-Bericht nach research\local\session\tunnel-<rolle>.txt.
#   Host-PC:   Test-TwoPc.ps1 -Role Host                      zeigt die SteamID fuer den Client-PC und wartet
#   Client-PC: Test-TwoPc.ps1 -Role Client -Peer <SteamID>     verbindet, folgt dem Host in die Mission
# Vorbereitung und Ablauf in docs\TESTING.md, Abschnitt "Test auf zwei PCs". Die Sitzung laeuft danach weiter,
# beenden mit Stop-Session.ps1 oder gleich mit -Stop.
param(
    [Parameter(Mandatory)][ValidateSet('Host', 'Client')][string]$Role,
    # Nur Client: SteamID64 des Host-Kontos, die der Host-PC beim Start anzeigt.
    [string]$Peer,
    # Fester Run, damit beide Seiten dieselbe Route sehen; Knoten fuer Tag 1 bei diesem Seed.
    [int]$Seed = 20795,
    [int]$NodeX = 62,
    [int]$NodeY = 213,
    # So lange bleiben beide in der Mission und messen.
    [int]$MissionSeconds = 120,
    # So lange wartet jede Seite auf die andere.
    [int]$JoinTimeoutSeconds = 900,
    [switch]$Mod,
    [switch]$Build,
    [switch]$Godmode,
    [switch]$Stop
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
if ($Role -eq 'Client' -and -not $Peer) { throw '-Role Client braucht -Peer mit der SteamID, die der Host-PC anzeigt.' }

$Start = @{ Role = $Role; Tunnel = 'P2p'; JoinTimeoutSeconds = $JoinTimeoutSeconds; Mod = $Mod; Build = $Build; Godmode = $Godmode }
if ($Role -eq 'Client') { $Start.Peer = $Peer }
& (Join-Path $PSScriptRoot 'Start-Session.ps1') @Start

$ProcessId = Get-RolePid $Role
$Steps = [System.Collections.Generic.List[string]]::new()
function Step([string]$Text) { $Steps.Add("$(Get-Date -Format HH:mm:ss) $Text"); Write-Output $Text }
function Assert-Alive {
    if (-not (Test-GameProcess $ProcessId)) {
        & py (Join-Path $PSScriptRoot 'crashes.py') --session --latest 1
        throw "$Role ist abgestuerzt."
    }
}
function Get-World {
    $Line = & py (Join-Path $PSScriptRoot 'netstate.py') $ProcessId | Select-Object -Last 1
    [string]$Line
}

Step 'Verbunden, beide in der MetaBase.'
if ($Role -eq 'Host') {
    Step "Run mit Seed $Seed starten."
    & (Join-Path $PSScriptRoot 'Start-Run.ps1') -Seed $Seed
    Step "Tag 1, Knoten ($NodeX, $NodeY)."
    & (Join-Path $PSScriptRoot 'Travel-Day.ps1') $NodeX $NodeY -Stay
    if ($LASTEXITCODE -eq 2) { throw 'Host ist beim Laden der Mission abgestuerzt.' }
}
else {
    Send-SpikeCommand $ProcessId '!notutorial'
    Step "Warte bis zu $JoinTimeoutSeconds s, bis der Host die Mission startet."
    $Deadline = (Get-Date).AddSeconds($JoinTimeoutSeconds)
    while ((Get-World) -notmatch 'DDDynamic_Map') {
        Assert-Alive
        if ((Get-Date) -gt $Deadline) { throw 'Zeitablauf: Client nicht in der Mission.' }
        Start-Sleep -Seconds 3
    }
}
Step "In der Mission: $(Get-World)"
for ($Waited = 0; $Waited -lt $MissionSeconds; $Waited += 10) {
    Start-Sleep -Seconds 10
    Assert-Alive
}
Step "$MissionSeconds s in der Mission ohne Absturz: $(Get-World)"

$Log = Join-Path $script:CommandDir "$ProcessId.log"
# Ohne Host-Karte erzeugt der Client keinen Boden, die Figuren fallen durch. Verbindung und Ping sagen darueber nichts.
$MapOk = $true
if ($Role -eq 'Client') {
    $MapOk = [bool](Select-String -Path $Log -Pattern 'MapGen: Client uebernimmt' -SimpleMatch -Quiet)
    Step $(if ($MapOk) { 'Host-Karte uebernommen.' } else { 'FEHLER: keine Host-Karte, der Client hat keinen Boden.' })
}
$Report = Join-Path $script:SessionDir "tunnel-$($Role.ToLower()).txt"
$Text = & py (Join-Path $PSScriptRoot 'tunnel_report.py') $Log --role $Role
$Ok = $LASTEXITCODE -eq 0 -and $MapOk
# Ablauf und Bericht ohne SteamIDs oder Adressen, so kann beides in den PR.
@('Ablauf') + ($Steps | ForEach-Object { "  $_" }) + @('') + $Text | Set-Content $Report -Encoding utf8
Get-Content $Report
Write-Output "Bericht: $Report"
if ($Stop) { & (Join-Path $PSScriptRoot 'Stop-Session.ps1') }
if (-not $Ok) { exit 1 }
