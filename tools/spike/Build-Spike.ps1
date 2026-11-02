# Baut research\spikes\a-connect\spike.cpp nach research\local\session\spike.dll. Das SDK wird dafuer nicht gebraucht.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Initialize-SessionDir
if (Get-Process DDSurvivors-Win64-Shipping -ErrorAction SilentlyContinue) {
    throw 'Das Spiel laeuft und sperrt spike.dll. Zuerst tools\spike\Stop-Session.ps1 ausfuehren.'
}
$Source = Join-Path $script:RepoRoot 'research\spikes\a-connect\spike.cpp'
Push-Location $script:SessionDir
try {
    Invoke-InVsEnvironment "cl /nologo /std:c++20 /O2 /EHsc /LD /Fe:spike.dll `"$Source`" user32.lib kernel32.lib" |
        Where-Object { $_ -match 'error|Creating|spike\.cpp' }
    if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen.' }
}
finally {
    Pop-Location
}
Join-Path $script:SessionDir 'spike.dll'
