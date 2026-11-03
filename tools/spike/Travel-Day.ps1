# Faehrt einen Tag ueber die echte Route: Knoten auf der Routenkarte anklicken, beide Instanzen pruefen, Tag mit !finish beenden,
# Lager bestaetigen und die naechste Routenkarte als Bild ablegen. Die Knoten liegen je Run anders, also vorher route.png ansehen.
#   Travel-Day.ps1 <x> <y> [-WaitSeconds 35] [-Stay]
# -Stay bleibt in der Mission. Exitcode 2, wenn eine Instanz abgestuerzt ist, dann folgt der neueste Bericht.
param(
    [Parameter(Mandatory)][int]$X,
    [Parameter(Mandatory)][int]$Y,
    [int]$WaitSeconds = 35,
    [switch]$Stay
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$Session = Get-Session
$GameInput = Join-Path $PSScriptRoot 'Game-Input.ps1'

function Assert-Alive {
    foreach ($Role in 'Host', 'Client') {
        if (-not (Test-GameProcess (Get-RolePid $Role))) {
            Write-Output "$Role ist abgestuerzt."
            & py (Join-Path $PSScriptRoot 'crashes.py') --session --latest 1
            exit 2
        }
    }
}

& $GameInput Host click $X $Y
Start-Sleep -Seconds $WaitSeconds
Assert-Alive
& py (Join-Path $PSScriptRoot 'netstate.py') $Session.Host $Session.Client | Select-Object -Last 2
& py (Join-Path $PSScriptRoot 'inventory.py') $Session.Host $Session.Client | Select-String '"complete"|"equal"'
& $GameInput Host shot (Join-Path $script:SessionDir 'day.png') | Out-Null
if ($Stay) { return }

# Rundenuebersicht, dann Fortsetzen und das Lager bestaetigen.
Send-SpikeCommand $Session.Host '!finish'
Start-Sleep -Seconds 6
& $GameInput Host click 410 426
Start-Sleep -Seconds 4
& $GameInput Host click 633 422
Start-Sleep -Seconds 4
Assert-Alive
& $GameInput Host shot (Join-Path $script:SessionDir 'route.png')
