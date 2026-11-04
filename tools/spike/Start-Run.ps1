# Startet aus der MetaBase einen Run bis zur Routenkarte: Host laeuft in den Startring links neben dem Bus, dann den Startdialog bestaetigen.
# Gilt fuer ein UserDir ohne Tutorial-Dialoge, siehe docs/TESTING.md. Optional vorher einen festen Seed setzen.
#   Start-Run.ps1 [-Seed 20795]
# Der Ring loest nur beim Hineinlaufen aus. Steht die Figur schon darin, etwa nach einem Tod, geht sie erst hinaus.
param(
    [int]$Seed,
    # Mitte des Startrings in der MetaBase, Build 26187885. A laeuft auf dem Bildschirm nach links, also zu kleinerem Y.
    [int]$RingY = -684,
    [int]$RingRadius = 250
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

# Die Routenkarte erkennt man am gruenen Banner "Felder" oben rechts.
function Test-RouteMap {
    $Color = & $GameInput Host pixel 700 13
    $R, $G, $B = [Convert]::ToInt32($Color.Substring(0, 2), 16), [Convert]::ToInt32($Color.Substring(2, 2), 16), [Convert]::ToInt32($Color.Substring(4, 2), 16)
    [Math]::Abs($R - 0x3C) -lt 16 -and [Math]::Abs($G - 0x5F) -lt 16 -and [Math]::Abs($B - 0x3C) -lt 16
}

function Wait-RouteMap([int]$Seconds) {
    for ($Wait = 0; $Wait -lt $Seconds -and -not (Test-RouteMap); $Wait++) { Start-Sleep -Seconds 1 }
    Test-RouteMap
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
& $GameInput Host shot (Join-Path $script:SessionDir 'route.png')
if (-not (Test-RouteMap)) { throw 'Routenkarte nicht offen, siehe research\local\session\route.png.' }