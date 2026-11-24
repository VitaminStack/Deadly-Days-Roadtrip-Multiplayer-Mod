$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\tools\spike\Common.ps1')
Initialize-SessionDir
$Source = Join-Path $PSScriptRoot 'round_phase_runtime.cpp'
$Exe = Join-Path $script:SessionDir 'round-phase-runtime-test.exe'
$Object = Join-Path $script:SessionDir 'round-phase-runtime-test.obj'
Invoke-InVsEnvironment "cl /nologo /std:c++20 /W4 /WX /EHsc /Fe:`"$Exe`" /Fo:`"$Object`" `"$Source`""
if ($LASTEXITCODE -ne 0) { throw 'Runtime-Test-Build fehlgeschlagen' }
& $Exe (Join-Path $script:SessionDir 'round-phase-wire-test.bin')
if ($LASTEXITCODE -ne 0) { throw 'Runtime-/Transport-Tests fehlgeschlagen' }
