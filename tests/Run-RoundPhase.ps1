$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\tools\spike\Common.ps1')
Initialize-SessionDir
$Source = Join-Path $PSScriptRoot 'round_phase.cpp'
$Exe = Join-Path $script:SessionDir 'round-phase-test.exe'
$Object = Join-Path $script:SessionDir 'round-phase-test.obj'
Invoke-InVsEnvironment "cl /nologo /std:c++20 /W4 /WX /EHsc /Fe:`"$Exe`" /Fo:`"$Object`" `"$Source`""
if ($LASTEXITCODE -ne 0) { throw 'Test-Build fehlgeschlagen' }
& $Exe
if ($LASTEXITCODE -ne 0) { throw 'Phasen-Tests fehlgeschlagen' }
