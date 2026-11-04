# Baut DeadlyDaysCoop.sln wie die CI nach bin\<Konfiguration>\. Mit -Test laufen danach die C++- und Python-Tests.
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'spike\Common.ps1')
$Solution = Join-Path $script:RepoRoot 'DeadlyDaysCoop.sln'
Invoke-InVsEnvironment "msbuild `"$Solution`" /m /nologo /v:m /p:Configuration=$Configuration /p:Platform=x64" |
    Where-Object { $_ -match '\b(error|warning)\b|->' }
if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen.' }
if ($Test) {
    & py -B (Join-Path $script:RepoRoot 'tools/task_status.py') check
    if ($LASTEXITCODE -ne 0) { throw 'TODO-Aufgaben oder README-Fortschritt inkonsistent.' }
    & (Join-Path $script:RepoRoot "bin\$Configuration\CoreTests.exe")
    if ($LASTEXITCODE -ne 0) { throw 'C++-Tests fehlgeschlagen.' }
    & (Join-Path $script:RepoRoot "bin\$Configuration\UITests.exe")
    if ($LASTEXITCODE -ne 0) { throw 'UI-Tests fehlgeschlagen.' }
    & (Join-Path $script:RepoRoot "bin\$Configuration\GameTests.exe")
    if ($LASTEXITCODE -ne 0) { throw 'Game-Tests fehlgeschlagen.' }
    & (Join-Path $script:RepoRoot "bin\$Configuration\NetTests.exe")
    if ($LASTEXITCODE -ne 0) { throw 'Net-Tests fehlgeschlagen.' }
    & py -B -m unittest discover -s (Join-Path $script:RepoRoot 'tests')
    if ($LASTEXITCODE -ne 0) { throw 'Python-Tests fehlgeschlagen.' }
}
Join-Path $script:RepoRoot "bin\$Configuration"
