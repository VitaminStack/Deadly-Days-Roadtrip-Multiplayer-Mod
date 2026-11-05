# Baut nur die eigene Probe, kein Spielstart.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\..\tools\spike\Common.ps1')
$Output = Join-Path $script:RepoRoot 'research\local\itemsack-absturz'
New-Item -ItemType Directory -Force $Output | Out-Null
$script:SessionDir = $Output
Push-Location $Output
try {
    Invoke-InVsEnvironment "cl /nologo /std:c++20 /EHsc /LD /Fe:ItemLootProbe.dll `"$PSScriptRoot\ItemLootProbe.cpp`" `"$PSScriptRoot\..\a-connect\Core.cpp`" user32.lib"
    if ($LASTEXITCODE) { throw 'ItemLootProbe-Build fehlgeschlagen.' }
}
finally { Pop-Location }
