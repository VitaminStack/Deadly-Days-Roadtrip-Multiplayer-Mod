# Erzeugt MSBuild-Argumente fuer die CI. Ohne gueltigen ccache wird der normale MSVC-Compiler verwendet.
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration,
    [string]$CacheBin,
    [switch]$CacheReady
)
$ErrorActionPreference = 'Stop'
$Arguments = @('/m', '/nologo', '/v:m', "/p:Configuration=$Configuration;Platform=x64")
if ($CacheReady) {
    if (-not $CacheBin -or -not (Test-Path -LiteralPath (Join-Path $CacheBin 'cl.exe'))) {
        throw 'ccache ist als bereit markiert, aber cl.exe fehlt'
    }
    $Arguments += "/p:DdcoopCompilerCache=true;CLToolPath=$CacheBin;CLToolExe=cl.exe"
    $Arguments += '/p:UseMultiToolTask=true'
    $Arguments += '/p:EnforceProcessCountAcrossBuilds=true'
    $Arguments += '/p:TrackFileAccess=false'
}
$Arguments
