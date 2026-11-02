# Baut den separaten Methoden-Spike mit dem lokalen SDK. Startet das Spiel nicht.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\..\..\tools\spike\Common.ps1')
if (Test-OwnSessionRunning) { throw 'Zuerst die eigene Testsitzung beenden.' }
$Sdk = ([xml](Get-Content (Find-LocalProps) -Raw)).Project.PropertyGroup.Dumper7SdkRoot
if (-not (Test-Path (Join-Path $Sdk 'SDK\DDSurvivors_classes.hpp'))) { throw 'Mission-SDK fehlt.' }
$Output = Join-Path $script:RepoRoot 'research\local\pausen'
New-Item -ItemType Directory -Force $Output | Out-Null
$script:SessionDir = $Output
$Sources = @('Basic.cpp', 'CoreUObject_functions.cpp', 'Engine_functions.cpp', 'DDSurvivors_functions.cpp') |
    ForEach-Object { '"' + (Join-Path $Sdk "SDK\$_") + '"' }
Push-Location $Output
try {
    Invoke-InVsEnvironment ("cl /nologo /std:c++20 /EHsc /LD /I`"$Sdk`" /Fe:PauseProbe.dll `"$PSScriptRoot\PauseProbe.cpp`" " + ($Sources -join ' ') + ' user32.lib')
    if ($LASTEXITCODE -ne 0) { throw 'PauseProbe-Build fehlgeschlagen.' }
}
finally { Pop-Location }
