# Startet aus der MetaBase einen Run bis zur Routenkarte: Host haelt A, bis die Figur im Ring steht, dann den Startdialog bestaetigen.
# Gilt fuer ein UserDir ohne Tutorial-Dialoge, siehe docs/TESTING.md. Optional vorher einen festen Seed setzen.
#   Start-Run.ps1 [-Seed 20795]
param(
    [int]$Seed,
    [double]$HoldSeconds = 1.6
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$GameInput = Join-Path $PSScriptRoot 'Game-Input.ps1'
if ($Seed) { Send-SpikeCommand (Get-RolePid 'Host') "DD_SetCurrentRunSeed $Seed"; Start-Sleep -Seconds 1 }
& $GameInput Host key A $HoldSeconds
Start-Sleep -Seconds 3
& $GameInput Host click 398 417
Start-Sleep -Seconds 6
& $GameInput Host shot (Join-Path $script:SessionDir 'route.png')
