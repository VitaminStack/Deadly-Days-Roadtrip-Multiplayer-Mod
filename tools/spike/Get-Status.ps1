# Zustand der laufenden Sitzung auf einen Blick: Prozesse, Netzzustand, letzte Logzeilen, Abstuerze seit dem Start und Mod-Logs.
#   Get-Status.ps1 [-Lines 5] [-Inventory] [-Brief]
# -Brief: eine Zeile je Instanz, Inventar und Abstuerze in je einer Zeile, keine Logs.
param(
    [int]$Lines = 5,
    # Vergleicht zusaetzlich die Inventare, braucht eine Mission oder MetaBase mit beiden Spielern.
    [switch]$Inventory,
    [switch]$Brief
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$Session = Get-Session
$Roles = @(Get-LocalRoles)
if ($Brief) {
    foreach ($Role in $Roles) { Format-NetBrief (Get-RolePid $Role) $Role }
    if ($Inventory -and $Roles.Count -eq 2 -and (Test-GameProcess $Session.Host) -and (Test-GameProcess $Session.Client)) {
        Format-InventoryBrief $Session.Host $Session.Client
    }
    Format-CrashBrief
    return
}
$Alive = @{}
foreach ($Role in $Roles) {
    $ProcessId = Get-RolePid $Role
    $Alive[$Role] = Test-GameProcess $ProcessId
    Write-Output ("{0,-6} {1,6}  {2}" -f $Role, $ProcessId, $(if ($Alive[$Role]) { 'laeuft' } else { 'beendet' }))
}
if ($Roles.Count -eq 1 -and $Alive[$Roles[0]]) {
    # Test auf zwei PCs: nur die eigene Instanz, Inventarvergleich braucht beide.
    & py (Join-Path $PSScriptRoot 'netstate.py') (Get-RolePid $Roles[0])
}
elseif ($Alive.Host -and $Alive.Client) {
    & py (Join-Path $PSScriptRoot 'netstate.py') $Session.Host $Session.Client
    if ($Inventory) {
        & py (Join-Path $PSScriptRoot 'inventory.py') $Session.Host $Session.Client | Select-String '"complete"|"equal"'
    }
}
foreach ($Role in $Roles) {
    $Log = Join-Path $script:CommandDir "$(Get-RolePid $Role).log"
    Write-Output "--- Spike-Log $Role"
    if (Test-Path $Log) { Get-Content $Log -Tail $Lines }
}
if ($Session.PSObject.Properties['Mod'] -and $Session.Mod) {
    $Logs = Join-Path (Split-Path (Get-GameExe)) 'DDCoop\logs'
    foreach ($Role in $Roles) {
        $Log = Join-Path $Logs "ddcoop-$(Get-RolePid $Role).log"
        Write-Output "--- Mod-Log $Role"
        if (Test-Path $Log) { Get-Content $Log -Tail $Lines }
    }
}
Write-Output '--- Abstuerze seit Sitzungsstart'
& py (Join-Path $PSScriptRoot 'crashes.py') --session
