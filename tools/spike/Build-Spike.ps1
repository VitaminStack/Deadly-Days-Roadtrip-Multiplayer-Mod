# Baut research\spikes\a-connect\*.cpp nach research\local\session\spike.dll. Das SDK wird dafuer nicht gebraucht.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
Initialize-SessionDir
if (Test-OwnSessionRunning) {
    throw 'Die Sitzung dieses Ordners sperrt spike.dll. Zuerst tools\spike\Stop-Session.ps1 ausfuehren.'
}
$Sources = (Get-ChildItem (Join-Path $script:RepoRoot 'research\spikes\a-connect\*.cpp') | ForEach-Object { "`"$($_.FullName)`"" }) -join ' '
Push-Location $script:SessionDir
try {
    Invoke-InVsEnvironment "cl /nologo /std:c++20 /O2 /EHsc /LD /Fe:spike.dll $Sources user32.lib kernel32.lib" |
        Where-Object { $_ -match '\berror\b' }
    if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen.' }
}
finally {
    Pop-Location
}
Join-Path $script:SessionDir 'spike.dll'
