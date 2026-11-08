# Die beiden MSBuild-Pfade lassen sich ohne MSBuild, Steam oder eine Spiel-EXE pruefen.
$ErrorActionPreference = 'Stop'
$Helper = Join-Path $PSScriptRoot '..\tools\Ci-CompilerArgs.ps1'
$Normal = @(& $Helper -Configuration Release)
if ($Normal -notcontains '/p:Configuration=Release;Platform=x64') { throw 'Release-Konfiguration fehlt' }
if (@($Normal | Where-Object { $_ -like '*CLToolPath*' }).Count) { throw 'Fallback darf CLToolPath nicht setzen' }
if (@($Normal | Where-Object { $_ -like '*UseMultiToolTask*' }).Count) { throw 'Fallback darf MultiToolTask nicht erzwingen' }

$Folder = Join-Path ([IO.Path]::GetTempPath()) ("ddcoop-cache-test-" + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $Folder -Force | Out-Null
    New-Item -ItemType File -Path (Join-Path $Folder 'cl.exe') -Force | Out-Null
    $Cached = @(& $Helper -Configuration Debug -CacheBin $Folder -CacheReady)
    if ($Cached -notcontains '/p:Configuration=Debug;Platform=x64') { throw 'Debug-Konfiguration fehlt' }
    if (@($Cached | Where-Object { $_ -like '/p:DdcoopCompilerCache=true;CLToolPath=*;CLToolExe=cl.exe' }).Count -ne 1) {
        throw 'Aktiver Cache wird nicht als Compiler-Wrapper verwendet'
    }
    if ($Cached -notcontains '/p:UseMultiToolTask=true') { throw 'MultiToolTask fuer Cache fehlt' }

    Remove-Item -LiteralPath (Join-Path $Folder 'cl.exe') -Force
    $FailedAsExpected = $false
    try { $null = & $Helper -Configuration Debug -CacheBin $Folder -CacheReady } catch { $FailedAsExpected = $true }
    if (-not $FailedAsExpected) { throw 'Fehlende cl.exe darf nicht als gueltiger Cache gelten' }
}
finally {
    Remove-Item -LiteralPath $Folder -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Output 'CI Compiler-Cache-Fallback: Parameterpruefungen bestanden.'
