# Baut nur die eigene Probe, kein Spielstart und keine Injection.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\..\tools\spike\Common.ps1')
$Output = Join-Path $script:RepoRoot 'research\local\client-visuell'
New-Item -ItemType Directory -Force $Output | Out-Null
$script:SessionDir = $Output
Push-Location $Output
try {
    Invoke-InVsEnvironment "cl /nologo /std:c++20 /EHsc /LD /Fe:VisualProbe.dll `"$PSScriptRoot\VisualProbe.cpp`" `"$PSScriptRoot\..\a-connect\Core.cpp`" user32.lib"
    if ($LASTEXITCODE) { throw 'VisualProbe-Build fehlgeschlagen.' }
}
finally { Pop-Location }
