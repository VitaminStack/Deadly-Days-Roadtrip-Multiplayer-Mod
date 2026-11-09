# Installiert die Coop-Mod fuer Deadly Days: Roadtrip oder entfernt sie wieder. Laeuft ohne Repo, nur mit Windows-PowerShell.
# Kopiert genau zwei Dateien neben die Spiel-Exe: version.dll (Loader) und DDCoop\DDCoop.dll (Mod).
# Spielstaende, andere Mods und Spieldateien bleiben unberuehrt. Siehe docs/INSTALL.md.
param(
    # Spielordner mit DDSurvivors\ darin. Ohne Angabe wird er in den Steam-Bibliotheken gesucht.
    [string]$GameDir,
    [switch]$Uninstall,
    # Mit -Uninstall auch Logs und ddcoop.ini im Ordner DDCoop loeschen.
    [switch]$Purge,
    # Ordner mit version.dll und DDCoop\DDCoop.dll (oder DDCoop.dll). Standard ist der Ordner dieses Skripts.
    [string]$Source = $PSScriptRoot,
    # Steam-Ordner statt des Registry-Eintrags, fuer Tests und ungewoehnliche Installationen.
    [string]$SteamPath,
    # Nur fuer Entwickler-Kopien ohne Steam-Manifest, die Build-Pruefung macht dann der Aufrufer.
    [switch]$SkipBuildCheck,
    # Name des Spielprozesses, der nicht laufen darf. Nur Tests aendern ihn.
    [string]$GameProcess = 'DDSurvivors-Win64-Shipping'
)
$ErrorActionPreference = 'Stop'

# Alle Spiel-Builds, fuer die die Mod Adressen und Patches hat. Muss zu SupportedBuilds in src/Core/Version.cpp und
# tools/spike/Common.ps1 passen, das prueft tests/test_installer.py.
$SupportedBuilds = @('26495302')
$AppId = '3026450'
$ExeRelative = 'DDSurvivors\Binaries\Win64\DDSurvivors-Win64-Shipping.exe'
# Nur der eigene Loader exportiert diesen Namen. Daran ist eine fremde version.dll erkennbar.
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
        throw 'Steam nicht gefunden. Den Spielordner mit -GameDir "<Ordner>" angeben.'
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
    throw 'Deadly Days: Roadtrip in keiner Steam-Bibliothek gefunden. Den Spielordner mit -GameDir "<Ordner>" angeben.'
}

function Assert-Build([string]$Dir) {
    $Manifest = Join-Path $Dir "..\..\appmanifest_$AppId.acf"
    if (-not (Test-Path -LiteralPath $Manifest)) {
        throw "Steam-Manifest nicht gefunden ($Manifest). Liegt das Spiel nicht in einer Steam-Bibliothek, wird nicht installiert."
    }
    $Build = [regex]::Match((Get-Content -LiteralPath $Manifest -Raw), '"buildid"\s+"(\d+)"').Groups[1].Value
    if ($SupportedBuilds -notcontains $Build) {
        $Known = $SupportedBuilds -join ' oder '
        throw "Spiel-Build $Build statt $Known. Diese Mod-Version laeuft nur mit Build $Known, siehe docs/INSTALL.md."
    }
}

function Test-OwnLoader([string]$Path) {
    [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($Path)).Contains($LoaderMarker)
}

if (-not $GameDir) { $GameDir = Find-GameDir }
$Exe = Join-Path $GameDir $ExeRelative
if (-not (Test-Path -LiteralPath $Exe)) { throw "Spiel-Exe nicht gefunden: $Exe" }
$Win64 = Split-Path $Exe
$Proxy = Join-Path $Win64 'version.dll'
$ModDir = Join-Path $Win64 'DDCoop'
$ModDll = Join-Path $ModDir 'DDCoop.dll'

# Eine laufende Instanz sperrt die Dateien.
if (Get-Process $GameProcess -ErrorAction SilentlyContinue) {
    throw 'Das Spiel laeuft. Erst beenden, dann erneut ausfuehren.'
}

# Andere Mods nutzen denselben Dateinamen. Die fremde Datei bleibt, wie sie ist.
if ((Test-Path -LiteralPath $Proxy) -and -not (Test-OwnLoader $Proxy)) {
    throw "Fremde version.dll gefunden, sie bleibt unberuehrt: $Proxy. Erst die andere Mod entfernen."
}

if ($Uninstall) {
    Remove-Item -LiteralPath $Proxy, $ModDll -Force -ErrorAction SilentlyContinue
    if ($Purge) {
        Remove-Item -LiteralPath $ModDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-Output "Mod entfernt aus $Win64."
    }
    else {
        Write-Output "Mod entfernt aus $Win64. Logs und ddcoop.ini bleiben in $ModDir."
    }
    return
}

if (-not $SkipBuildCheck) { Assert-Build $GameDir }

$SourceProxy = Join-Path $Source 'version.dll'
$SourceMod = Join-Path $Source 'DDCoop\DDCoop.dll'
if (-not (Test-Path -LiteralPath $SourceMod)) { $SourceMod = Join-Path $Source 'DDCoop.dll' }
foreach ($File in $SourceProxy, $SourceMod) {
    if (-not (Test-Path -LiteralPath $File)) { throw "$File fehlt. Das Paket vollstaendig entpacken." }
}
if (-not (Test-OwnLoader $SourceProxy)) { throw "$SourceProxy ist nicht der Loader dieser Mod." }

# Erst die Mod, dann der Loader: Bricht das Kopieren ab, laedt das Spiel ohne Loader einfach normal.
New-Item -ItemType Directory -Force -Path $ModDir | Out-Null
Copy-Item -LiteralPath $SourceMod -Destination $ModDll -Force
Copy-Item -LiteralPath $SourceProxy -Destination $Proxy -Force
# Aus dem Internet geladene Dateien tragen eine Markierung, die Windows beim Laden beanstanden kann.
Unblock-File -LiteralPath $ModDll, $Proxy -ErrorAction SilentlyContinue
Write-Output "Mod installiert in $Win64. Entfernen mit Uninstall.cmd."
