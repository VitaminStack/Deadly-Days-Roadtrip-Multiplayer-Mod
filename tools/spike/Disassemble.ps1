# Zeigt einen Adressbereich der Spiel-Exe, etwa: Disassemble.ps1 0x5A45C20 0x5A45C90
# Adressen sind RVAs, wie sie in Absturz-Stacks und docs\OFFSETS.md stehen.
param(
    [Parameter(Mandatory)][string]$Start,
    [Parameter(Mandatory)][string]$End
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Initialize-SessionDir
$ImageBase = 0x140000000
$From = '0x{0:X}' -f ($ImageBase + [Convert]::ToInt64($Start, 16))
$To = '0x{0:X}' -f ($ImageBase + [Convert]::ToInt64($End, 16))
Invoke-InVsEnvironment "dumpbin /nologo /disasm:nobytes /range:$From,$To `"$(Get-GameExe)`"" |
    Where-Object { $_ -match '^\s+0*14' }
