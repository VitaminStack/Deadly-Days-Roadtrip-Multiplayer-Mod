# Isolierter Test, kein Deployment und kein Spielstart.
param([switch]$WithEntryProbe)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\..\tools\spike\Common.ps1')
$Output = Join-Path $script:RepoRoot 'research\local\andockpunkte\full'
New-Item -ItemType Directory -Force $Output | Out-Null
$script:SessionDir = $Output
Push-Location $Output
try {
    Invoke-InVsEnvironment "ml64 /nologo /c /FoVersionForwarders.obj `"$PSScriptRoot\VersionForwarders.asm`""
    if ($LASTEXITCODE -ne 0) { throw 'Assembler-Build fehlgeschlagen' }
    $Extra = if ($WithEntryProbe) { "/DENTRY_PROBE `"$PSScriptRoot\EntryProbe.cpp`" `"$PSScriptRoot\..\a-connect\Core.cpp`" user32.lib" } else { '' }
    Invoke-InVsEnvironment "cl /nologo /std:c++20 /O2 /W4 /WX /EHsc /LD /Fe:version.dll `"$PSScriptRoot\FullVersionProxy.cpp`" VersionForwarders.obj $Extra /link /DEF:`"$PSScriptRoot\FullVersionProxy.def`" /IMPLIB:proxy-import.lib"
    if ($LASTEXITCODE -ne 0) { throw 'Full-Proxy-Build fehlgeschlagen' }
    Invoke-InVsEnvironment "cl /nologo /std:c++20 /W4 /WX /EHsc /Fe:ProxySmoke.exe `"$PSScriptRoot\ProxySmoke.cpp`" version.lib"
    if ($LASTEXITCODE -ne 0) { throw 'Smoke-Build fehlgeschlagen' }
    & .\ProxySmoke.exe
    if ($LASTEXITCODE -ne 0) { throw "Smoke-Test fehlgeschlagen: $LASTEXITCODE" }
}
finally { Pop-Location }
