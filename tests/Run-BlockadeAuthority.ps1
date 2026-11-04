$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\tools\spike\Common.ps1')
Initialize-SessionDir
$Source = Join-Path $PSScriptRoot 'blockade_authority.cpp'
$Exe = Join-Path $script:SessionDir 'blockade-authority-test.exe'
$Object = Join-Path $script:SessionDir 'blockade-authority-test.obj'
Invoke-InVsEnvironment "cl /nologo /std:c++20 /W4 /WX /EHsc /Fe:`"$Exe`" /Fo:`"$Object`" `"$Source`""
if ($LASTEXITCODE -ne 0) { throw 'Blockade-Test-Build fehlgeschlagen' }
& $Exe
if ($LASTEXITCODE -ne 0) { throw 'Blockade-Tests fehlgeschlagen' }
