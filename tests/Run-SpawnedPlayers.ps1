$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\tools\spike\Common.ps1')
Initialize-SessionDir
$Source = Join-Path $PSScriptRoot 'spawnedplayers_set.cpp'
$Exe = Join-Path $script:SessionDir 'spawnedplayers-test.exe'
$Object = Join-Path $script:SessionDir 'spawnedplayers-test.obj'
Invoke-InVsEnvironment "cl /nologo /std:c++20 /W4 /WX /EHsc /Fe:`"$Exe`" /Fo:`"$Object`" `"$Source`""
if ($LASTEXITCODE -ne 0) { throw 'Test-Build fehlgeschlagen' }
& $Exe
if ($LASTEXITCODE -ne 0) { throw 'SpawnedPlayers-Tests fehlgeschlagen' }
& py (Join-Path $PSScriptRoot 'test_players_audit.py')
if ($LASTEXITCODE -ne 0) { throw 'Read-only-Audit-Tests fehlgeschlagen' }
