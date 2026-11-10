# Startet aus der MetaBase einen Run bis zur Routenkarte: Host laeuft in den Startring links neben dem Bus, dann den Startdialog bestaetigen.
# Gilt fuer ein UserDir ohne Tutorial-Dialoge, siehe docs/TESTING.md. Optional vorher einen festen Seed setzen.
#   Start-Run.ps1 [-Seed 20795] [-Brief]
# Der Ring loest nur beim Hineinlaufen aus. Steht die Figur schon darin, etwa nach einem Tod, geht sie erst hinaus.
param(
    [int]$Seed,
    # Mitte des Startrings in der MetaBase, Build 26495302. A laeuft auf dem Bildschirm nach links, also zu kleinerem Y.
    [int]$RingY = -684,
    [int]$RingRadius = 250,
    # Nur eine Zeile: Routenkarte offen oder nicht.
    [switch]$Brief
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$GameInput = Join-Path $PSScriptRoot 'Game-Input.ps1'
$HostId = Get-RolePid 'Host'
function Get-HostY {
    $Position = & py (Join-Path $PSScriptRoot 'check_round.py') $HostId 0 --local-position
    if ($Position -eq 'none') { throw 'Host-Figur nicht gefunden, MetaBase noch nicht geladen?' }
    [int]($Position -split ' ')[1]
}
if ($Seed) { Send-SpikeCommand $HostId "DD_SetCurrentRunSeed $Seed" }

# Auch ein direkter Missionsstart zaehlt: Ein frischer Spielstand faehrt nach "So wird gespielt" ohne Routenkarte los.
function Wait-RouteMap([int]$Seconds) {
    for ($Wait = 0; $Wait -lt $Seconds -and -not (Test-RouteMap) -and -not (Test-HostInMission); $Wait++) { Start-Sleep -Seconds 1 }
    (Test-RouteMap) -or (Test-HostInMission)
}

# Kurz nach dem Laden der MetaBase reagiert der Ring manchmal nicht, darum bis zu drei Anlaeufe.
for ($Attempt = 1; $Attempt -le 3; $Attempt++) {
    if ([Math]::Abs((Get-HostY) - $RingY) -lt $RingRadius) {
        & $GameInput Host key D 0.8
        Start-Sleep -Milliseconds 500
    }
    # Ein durchgehender Druck, kurze Tipper loesen den Ring nicht aus. Die Figur laeuft etwa 560 Einheiten pro Sekunde.
    $Distance = (Get-HostY) - $RingY
    if ($Distance -gt 0) { & $GameInput Host key A ([Math]::Round($Distance / 560 + 0.1, 2)) }
    if (Wait-RouteMap 8) { break }
    # Bei einem frischen Spielstand kommt vorher ein Startdialog, nach einem beendeten Run nicht. Erst klicken,
    # wenn die Karte ausbleibt, sonst traefe der Klick "Zurueck" auf der Routenkarte.
    & $GameInput Host click 398 417
    if (Wait-RouteMap 6) { break }
}
if (Test-HostInMission) {
    'Mission direkt gestartet (frischer Spielstand, keine Routenkarte)'
    return
}
$Shot = & $GameInput Host shot (Join-Path $script:SessionDir 'route.png')
if (-not (Test-RouteMap)) { throw 'Routenkarte nicht offen, siehe research\local\session\route.png.' }
if ($Brief) { 'Routenkarte offen, Bild: research\local\session\route.png' } else { $Shot }