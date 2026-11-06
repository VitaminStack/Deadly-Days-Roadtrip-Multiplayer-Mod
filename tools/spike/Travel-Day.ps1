# Faehrt einen Tag ueber die echte Route: Knoten auf der Routenkarte anklicken, beide Instanzen pruefen, Tag mit !finish beenden,
# Lager bestaetigen und die naechste Routenkarte als Bild ablegen. Die Knoten liegen je Run anders, also vorher route.png ansehen.
#   Travel-Day.ps1 <x> <y> [-WaitSeconds 60] [-Stay] [-Brief]
# Wartet nach dem Klick, bis beide Instanzen eine neue Karte mit 2 Spielern haben, hoechstens -WaitSeconds.
# -Brief: eine Zeile je Instanz und eine fuer das Inventar.
# -Stay bleibt in der Mission. Exitcode 2, wenn eine Instanz abgestuerzt ist, dann folgt der neueste Bericht.
param(
    [Parameter(Mandatory)][int]$X,
    [Parameter(Mandatory)][int]$Y,
    [int]$WaitSeconds = 60,
    [switch]$Stay,
    [switch]$Brief
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$Session = Get-Session
$GameInput = Join-Path $PSScriptRoot 'Game-Input.ps1'

function Assert-Alive {
    foreach ($Role in Get-LocalRoles) {
        if (-not (Test-GameProcess (Get-RolePid $Role))) {
            Write-Output "$Role ist abgestuerzt."
            & py (Join-Path $PSScriptRoot 'crashes.py') --session --latest 1
            exit 2
        }
    }
}

$Local = @(Get-LocalRoles | ForEach-Object { Get-RolePid $_ })
$Before = (Get-NetState $Local[0]).world
& $GameInput Host click $X $Y
# Statt fest zu schlafen: warten, bis alle lokalen Instanzen eine andere Karte mit 2 Spielern zeigen.
$Deadline = (Get-Date).AddSeconds($WaitSeconds)
while ((Get-Date) -lt $Deadline) {
    Assert-Alive
    $Arrived = @($Local | Where-Object { $State = Get-NetState $_; $State.world -and $State.world -ne $Before -and $State.players -ge 2 }).Count
    if ($Arrived -eq $Local.Count) { break }
    Start-Sleep -Seconds 1
}
# Zeit fuer Spawn und Inventar-Replikation.
Start-Sleep -Seconds 5
Assert-Alive
if ($Brief) {
    foreach ($Role in Get-LocalRoles) { Format-NetBrief (Get-RolePid $Role) $Role }
    if ($Local.Count -eq 2) { Format-InventoryBrief $Session.Host $Session.Client }
}
else {
    & py (Join-Path $PSScriptRoot 'netstate.py') $Local | Select-Object -Last 2
    # Beim Test auf zwei PCs fehlt die Gegenseite, dann kein Inventarvergleich.
    if ($Local.Count -eq 2) {
        & py (Join-Path $PSScriptRoot 'inventory.py') $Session.Host $Session.Client | Select-String '"complete"|"equal"'
    }
}
& $GameInput Host shot (Join-Path $script:SessionDir 'day.png') | Out-Null
if ($Stay) { return }

# Rundenuebersicht, dann Fortsetzen und das Lager bestaetigen, bei jeder lokalen Instanz (der Host wartet auf den Client).
# Beim Test auf zwei PCs bestaetigt der Client am anderen PC, dann hier laenger auf die Routenkarte warten.
$OnRoute = Complete-RoundEnd -TimeoutSeconds $(if ($Local.Count -eq 2) { 30 } else { 120 })
Assert-Alive
if (-not $OnRoute) { Write-Output 'Host nicht auf der Routenkarte: Lager noch nicht von beiden bestaetigt?' }
$Shot = & $GameInput Host shot (Join-Path $script:SessionDir 'route.png')
if ($Brief) { 'Tag beendet, Routenkarte: research\local\session\route.png' } else { $Shot }
