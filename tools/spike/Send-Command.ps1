# Schickt einen Konsolen- oder Spike-Befehl an eine Instanz, etwa: Send-Command.ps1 Host '!finish'
param(
    [Parameter(Mandatory)][ValidateSet('Host', 'Client')][string]$Role,
    [Parameter(Mandatory)][string]$Command
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$ProcessId = Get-RolePid $Role
Send-SpikeCommand $ProcessId $Command
Start-Sleep -Seconds 1
Get-Content (Join-Path $script:CommandDir "$ProcessId.log") -Tail 5
