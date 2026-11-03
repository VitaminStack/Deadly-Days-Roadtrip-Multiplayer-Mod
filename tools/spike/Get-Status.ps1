# Zustand der laufenden Sitzung auf einen Blick: Prozesse, Netzzustand, letzte Logzeilen, Abstuerze seit dem Start und Mod-Logs.
#   Get-Status.ps1 [-Lines 5] [-Inventory]
param(
    [int]$Lines = 5,
    # Vergleicht zusaetzlich die Inventare, braucht eine Mission oder MetaBase mit beiden Spielern.
    [switch]$Inventory
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$Session = Get-Session
$Alive = @{}
foreach ($Role in 'Host', 'Client') {
    $ProcessId = Get-RolePid $Role
    $Alive[$Role] = Test-GameProcess $ProcessId
    Write-Output ("{0,-6} {1,6}  {2}" -f $Role, $ProcessId, $(if ($Alive[$Role]) { 'laeuft' } else { 'beendet' }))
}
if ($Alive.Host -and $Alive.Client) {
    & py (Join-Path $PSScriptRoot 'netstate.py') $Session.Host $Session.Client
    if ($Inventory) {
        & py (Join-Path $PSScriptRoot 'inventory.py') $Session.Host $Session.Client | Select-String '"complete"|"equal"'
    }
}
foreach ($Role in 'Host', 'Client') {
    $Log = Join-Path $script:CommandDir "$(Get-RolePid $Role).log"
    Write-Output "--- Spike-Log $Role"
    if (Test-Path $Log) { Get-Content $Log -Tail $Lines }
}
if ($Session.PSObject.Properties['Mod'] -and $Session.Mod) {
    $Logs = Join-Path (Split-Path (Get-GameExe)) 'DDCoop\logs'
    foreach ($Role in 'Host', 'Client') {
        $Log = Join-Path $Logs "ddcoop-$(Get-RolePid $Role).log"
        Write-Output "--- Mod-Log $Role"
        if (Test-Path $Log) { Get-Content $Log -Tail $Lines }
    }
}
Write-Output '--- Abstuerze seit Sitzungsstart'
& py (Join-Path $PSScriptRoot 'crashes.py') --session
