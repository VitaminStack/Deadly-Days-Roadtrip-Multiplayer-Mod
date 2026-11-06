# Baut die Mod als Release und packt das Paket fuer Spieler nach dist\DDCoop-<Version>.zip.
# Enthaelt nur eigene Dateien: Loader, Mod, Installer, Anleitung und Lizenzen. Nie Spieldateien oder PDBs.
param(
    # Platzhalter bis zur Versionierung in der Aufgabe release.
    [string]$Version,
    # Fertige DLLs aus diesem Ordner nehmen statt neu zu bauen, etwa in Tests.
    [string]$Bin,
    [string]$OutputDir
)
$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $OutputDir) { $OutputDir = Join-Path $RepoRoot 'dist' }
if (-not $Version) { $Version = 'dev-' + (& git -C $RepoRoot rev-parse --short HEAD) }
if ($Version -notmatch '^[A-Za-z0-9._-]+$') { throw "Ungueltige Version: $Version" }

if (-not $Bin) { $Bin = & (Join-Path $PSScriptRoot 'Build-Mod.ps1') -Configuration Release | Select-Object -Last 1 }

$Name = "DDCoop-$Version"
$Stage = Join-Path ([IO.Path]::GetTempPath()) ("$Name-" + [guid]::NewGuid().ToString('N'))
$Package = Join-Path $Stage $Name
$Installer = Join-Path $PSScriptRoot 'installer'
# Ziel im Paket = Quelle. Die Lizenz von MinHook muss beiliegen, weil es in DDCoop.dll steckt (BSD-2-Clause).
$Files = [ordered]@{
    'version.dll'                     = Join-Path $Bin 'version.dll'
    'DDCoop\DDCoop.dll'               = Join-Path $Bin 'DDCoop.dll'
    'Install.cmd'                     = Join-Path $Installer 'Install.cmd'
    'Uninstall.cmd'                   = Join-Path $Installer 'Uninstall.cmd'
    'Install-DDCoop.ps1'              = Join-Path $Installer 'Install-DDCoop.ps1'
    'LIESMICH.txt'                    = Join-Path $Installer 'LIESMICH.txt'
    'LICENSE.txt'                     = Join-Path $RepoRoot 'LICENSE'
    'third_party\minhook-LICENSE.txt' = Join-Path $RepoRoot 'third_party\minhook\LICENSE.txt'
}
try {
    foreach ($Target in $Files.Keys) {
        $From = $Files[$Target]
        if (-not (Test-Path -LiteralPath $From)) { throw "$From fehlt." }
        $To = Join-Path $Package $Target
        New-Item -ItemType Directory -Force -Path (Split-Path $To) | Out-Null
        Copy-Item -LiteralPath $From -Destination $To
    }
    # Pruefsummen zum Vergleich mit der Veroeffentlichung und fuer Meldungen an Virenscanner-Hersteller.
    # SHA256 direkt ueber .NET: Get-FileHash fehlt, wenn PowerShell 5.1 einen fremden Modulpfad erbt (so in der CI).
    $Sha = [Security.Cryptography.SHA256]::Create()
    $Sums = foreach ($Target in $Files.Keys) {
        $Bytes = $Sha.ComputeHash([IO.File]::ReadAllBytes((Join-Path $Package $Target)))
        '{0}  {1}' -f (-join ($Bytes | ForEach-Object { $_.ToString('x2') })), ($Target -replace '\\', '/')
    }
    $Sha.Dispose()
    [IO.File]::WriteAllLines((Join-Path $Package 'SHA256SUMS.txt'), [string[]]$Sums)

    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
    $Zip = Join-Path $OutputDir "$Name.zip"
    Remove-Item -LiteralPath $Zip -Force -ErrorAction SilentlyContinue
    # Compress-Archive aus Windows PowerShell 5.1 schreibt Backslashes in die Eintragsnamen, ZipFile nicht.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($Package, $Zip, [IO.Compression.CompressionLevel]::Optimal, $true)
    $Zip
}
finally {
    Remove-Item -LiteralPath $Stage -Recurse -Force -ErrorAction SilentlyContinue
}
