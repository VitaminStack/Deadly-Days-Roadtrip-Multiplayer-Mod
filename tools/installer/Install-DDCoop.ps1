# Installs the co-op mod for Deadly Days: Roadtrip or removes it again. Runs without the repo, with Windows PowerShell only.
# Copies exactly two files next to the game exe: version.dll (loader) and DDCoop\DDCoop.dll (mod).
# Savegames, other mods and game files stay untouched. See docs/INSTALL.md.
param(
    # Game folder containing DDSurvivors\. If omitted, it is searched in the Steam libraries.
    [string]$GameDir,
    [switch]$Uninstall,
    # With -Uninstall also delete logs and ddcoop.ini in the DDCoop folder.
    [switch]$Purge,
    # Folder with version.dll and DDCoop\DDCoop.dll (or DDCoop.dll). Defaults to this script's folder.
    [string]$Source = $PSScriptRoot,
    # Steam folder instead of the registry entry, for tests and unusual installations.
    [string]$SteamPath,
    # Only for developer copies without a Steam manifest; the caller then does the build check.
    [switch]$SkipBuildCheck,
    # Name of the game process that must not be running. Only tests change it.
    [string]$GameProcess = 'DDSurvivors-Win64-Shipping'
)
$ErrorActionPreference = 'Stop'

# All game builds the mod has addresses and patches for. Must match SupportedBuilds in src/Core/Version.cpp and
# tools/spike/Common.ps1; tests/test_installer.py checks that.
$SupportedBuilds = @('26495302')
$AppId = '3026450'
$ExeRelative = 'DDSurvivors\Binaries\Win64\DDSurvivors-Win64-Shipping.exe'
# Only our own loader exports this name. That identifies a foreign version.dll.
$LoaderMarker = 'ModStart'

function Get-SteamLibraries([string]$Root) {
    $Libraries = New-Object System.Collections.Generic.List[string]
    $Libraries.Add($Root)
    $Vdf = Join-Path $Root 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $Vdf) {
        foreach ($Match in [regex]::Matches((Get-Content -LiteralPath $Vdf -Raw), '"path"\s+"([^"]+)"')) {
            $Path = $Match.Groups[1].Value -replace '\\\\', '\'
            if (-not $Libraries.Contains($Path)) { $Libraries.Add($Path) }
        }
    }
    $Libraries
}

function Find-GameDir {
    $Root = $SteamPath
    if (-not $Root) {
        $Key = Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -Name SteamPath -ErrorAction SilentlyContinue
        if ($Key) { $Root = $Key.SteamPath -replace '/', '\' }
    }
    if (-not $Root -or -not (Test-Path -LiteralPath $Root)) {
        throw 'Steam not found. Specify the game folder with -GameDir "<folder>".'
    }
    foreach ($Library in Get-SteamLibraries $Root) {
        $Manifest = Join-Path $Library "steamapps\appmanifest_$AppId.acf"
        if (-not (Test-Path -LiteralPath $Manifest)) { continue }
        $InstallDir = [regex]::Match((Get-Content -LiteralPath $Manifest -Raw), '"installdir"\s+"([^"]+)"').Groups[1].Value
        if ($InstallDir) {
            $Candidate = Join-Path $Library "steamapps\common\$InstallDir"
            if (Test-Path -LiteralPath $Candidate) { return $Candidate }
        }
    }
    throw 'Deadly Days: Roadtrip not found in any Steam library. Specify the game folder with -GameDir "<folder>".'
}

function Assert-Build([string]$Dir) {
    $Manifest = Join-Path $Dir "..\..\appmanifest_$AppId.acf"
    if (-not (Test-Path -LiteralPath $Manifest)) {
        throw "Steam manifest not found ($Manifest). Only games in a Steam library are supported."
    }
    $Build = [regex]::Match((Get-Content -LiteralPath $Manifest -Raw), '"buildid"\s+"(\d+)"').Groups[1].Value
    if ($SupportedBuilds -notcontains $Build) {
        $Known = $SupportedBuilds -join ' or '
        throw "Game build $Build instead of $Known. This mod version only works with build $Known, see docs/INSTALL.md."
    }
}

function Test-OwnLoader([string]$Path) {
    [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($Path)).Contains($LoaderMarker)
}

if (-not $GameDir) { $GameDir = Find-GameDir }
$Exe = Join-Path $GameDir $ExeRelative
if (-not (Test-Path -LiteralPath $Exe)) { throw "Game exe not found: $Exe" }
$Win64 = Split-Path $Exe
$Proxy = Join-Path $Win64 'version.dll'
$ModDir = Join-Path $Win64 'DDCoop'
$ModDll = Join-Path $ModDir 'DDCoop.dll'

# A running instance locks the files.
if (Get-Process $GameProcess -ErrorAction SilentlyContinue) {
    throw 'The game is running. Close it first, then run again.'
}

# Other mods use the same file name. The foreign file stays as it is.
if ((Test-Path -LiteralPath $Proxy) -and -not (Test-OwnLoader $Proxy)) {
    throw "Foreign version.dll found, left untouched: $Proxy. Remove the other mod first."
}

if ($Uninstall) {
    Remove-Item -LiteralPath $Proxy, $ModDll -Force -ErrorAction SilentlyContinue
    if ($Purge) {
        Remove-Item -LiteralPath $ModDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-Output "Mod removed from $Win64."
    }
    else {
        Write-Output "Mod removed from $Win64. Logs and ddcoop.ini stay in $ModDir."
    }
    return
}

if (-not $SkipBuildCheck) { Assert-Build $GameDir }

$SourceProxy = Join-Path $Source 'version.dll'
$SourceMod = Join-Path $Source 'DDCoop\DDCoop.dll'
if (-not (Test-Path -LiteralPath $SourceMod)) { $SourceMod = Join-Path $Source 'DDCoop.dll' }
foreach ($File in $SourceProxy, $SourceMod) {
    if (-not (Test-Path -LiteralPath $File)) { throw "$File is missing. Extract the package completely." }
}
if (-not (Test-OwnLoader $SourceProxy)) { throw "$SourceProxy is not the loader of this mod." }

# Mod first, then the loader: if copying aborts, the game simply starts normally without the loader.
New-Item -ItemType Directory -Force -Path $ModDir | Out-Null
Copy-Item -LiteralPath $SourceMod -Destination $ModDll -Force
Copy-Item -LiteralPath $SourceProxy -Destination $Proxy -Force
# Files downloaded from the internet carry a mark Windows may object to when loading.
Unblock-File -LiteralPath $ModDll, $Proxy -ErrorAction SilentlyContinue
Write-Output "Mod installed in $Win64. Remove with Uninstall.cmd."
