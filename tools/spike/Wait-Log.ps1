# Wartet, bis im Spike-Log einer Instanz eine neue Zeile zum Muster passt, statt fest zu schlafen. Nur Zeilen ab dem Aufruf zaehlen.
#   Wait-Log.ps1 Host 'MapGen step' -TimeoutSeconds 60
# Exitcode 0 mit der Zeile, 1 bei Zeitablauf, 2 wenn die Instanz vorher endet.
param(
    [Parameter(Mandatory)][ValidateSet('Host', 'Client')][string]$Role,
    [Parameter(Mandatory)][string]$Pattern,
    [int]$TimeoutSeconds = 60
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$ProcessId = Get-RolePid $Role
$Log = Join-Path $script:CommandDir "$ProcessId.log"
$Seen = if (Test-Path $Log) { @(Get-Content $Log).Count } else { 0 }
$Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
while ((Get-Date) -lt $Deadline) {
    if (Test-Path $Log) {
        $Lines = @(Get-Content $Log)
        $Match = $Lines | Select-Object -Skip $Seen | Where-Object { $_ -match $Pattern } | Select-Object -First 1
        if ($Match) { Write-Output $Match; exit 0 }
        $Seen = $Lines.Count
    }
    if (-not (Test-GameProcess $ProcessId)) { Write-Output "$Role ($ProcessId) ist beendet, siehe crashes.py."; exit 2 }
    Start-Sleep -Milliseconds 500
}
Write-Output "Zeitablauf nach $TimeoutSeconds s ohne '$Pattern' im Log von $Role."
exit 1
