$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\tools\spike\Common.ps1')
Initialize-SessionDir
$Source = Join-Path $PSScriptRoot 'remote_p2.cpp'
$Exe = Join-Path $script:SessionDir 'remote-p2-test.exe'
$Object = Join-Path $script:SessionDir 'remote-p2-test.obj'
Invoke-InVsEnvironment "cl /nologo /std:c++20 /W4 /WX /EHsc /Fe:`"$Exe`" /Fo:`"$Object`" `"$Source`""
if ($LASTEXITCODE -ne 0) { throw 'Remote-P2-Test-Build fehlgeschlagen' }
& $Exe
if ($LASTEXITCODE -ne 0) { throw 'Remote-P2-Tests fehlgeschlagen' }
