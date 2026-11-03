# Baut eigene Trace-Probe gegen das lokale SDK. Kein Spielstart/Deployment.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\..\tools\spike\Common.ps1')
if (Test-OwnSessionRunning) { throw 'Zuerst eigene Sitzung beenden.' }
$Sdk = ([xml](Get-Content (Find-LocalProps) -Raw)).Project.PropertyGroup.Dumper7SdkRoot
if (-not (Test-Path (Join-Path $Sdk 'SDK\DDSurvivors_classes.hpp'))) { throw 'Mission-SDK fehlt.' }
$Output = Join-Path $script:RepoRoot 'research\local\blueprint-logik'
New-Item -ItemType Directory -Force $Output | Out-Null
$script:SessionDir = $Output
$Sources = @('Basic.cpp','CoreUObject_functions.cpp','Engine_functions.cpp','DDSurvivors_functions.cpp') |
    ForEach-Object { '"' + (Join-Path $Sdk "SDK\$_") + '"' }
Push-Location $Output
try {
    Invoke-InVsEnvironment ("cl /nologo /std:c++20 /EHsc /LD /I`"$Sdk`" /Fe:EventProbe.dll `"$PSScriptRoot\EventProbe.cpp`" " + ($Sources -join ' ') + ' user32.lib')
    if ($LASTEXITCODE) { throw 'EventProbe-Build fehlgeschlagen.' }
}
finally { Pop-Location }
