# Baut und testet nur das isolierte Beispiel, startet niemals das Spiel.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\..\tools\spike\Common.ps1')
$Output = Join-Path $script:RepoRoot 'research\local\andockpunkte'
$OldTemp, $OldTmp = $env:TEMP, $env:TMP
New-Item -ItemType Directory -Force $Output, (Join-Path $Output 'temp') | Out-Null
$env:TEMP = Join-Path $Output 'temp'
$env:TMP = $env:TEMP
$script:SessionDir = $Output
Push-Location $Output
try {
    Invoke-InVsEnvironment "cl /nologo /std:c++20 /W4 /WX /EHsc /LD /Fe:version.dll `"$PSScriptRoot\VersionProxy.cpp`" /link /DEF:`"$PSScriptRoot\VersionProxy.def`" /IMPLIB:proxy-import.lib"
    if ($LASTEXITCODE -ne 0) { throw 'Proxy-Build fehlgeschlagen' }
    Invoke-InVsEnvironment "cl /nologo /std:c++20 /W4 /WX /EHsc /Fe:ProxySmoke.exe `"$PSScriptRoot\ProxySmoke.cpp`" version.lib"
    if ($LASTEXITCODE -ne 0) { throw 'Test-Build fehlgeschlagen' }
    & .\ProxySmoke.exe
    if ($LASTEXITCODE -ne 0) { throw "Proxy-Test fehlgeschlagen: $LASTEXITCODE" }
}
finally {
    Pop-Location
    $env:TEMP, $env:TMP = $OldTemp, $OldTmp
}
