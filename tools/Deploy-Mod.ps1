# Kopiert die gebaute Mod neben die Spiel-Exe oder entfernt sie wieder. Der Spielordner kommt aus config\Local.props.
# Solange die Mod dort liegt, laedt auch jeder normale Spielstart sie. Nach dem Testen deshalb -Remove ausfuehren.
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'spike\Common.ps1')

$Win64 = Split-Path (Get-GameExe)
$Proxy = Join-Path $Win64 'version.dll'
$ModDir = Join-Path $Win64 'DDCoop'

# Eine laufende Instanz sperrt die Dateien. Beenden darf sie nur, wem sie gehoert.
if (Get-Process $script:GameProcess -ErrorAction SilentlyContinue) {
    throw 'Das Spiel laeuft. Erst die Instanzen beenden, Testsitzungen mit Stop-Session.ps1.'
}

# Andere Mods nutzen denselben Dateinamen. Nur der eigene Loader enthaelt diesen Exportnamen.
if ((Test-Path $Proxy) -and -not [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($Proxy)).Contains('ModStart')) {
    throw "Fremde version.dll gefunden, sie bleibt unberuehrt: $Proxy"
}

if ($Remove) {
    Remove-Item $Proxy, (Join-Path $ModDir 'DDCoop.dll') -Force -ErrorAction SilentlyContinue
    Write-Output "Mod entfernt. Logs und ddcoop.ini bleiben in $ModDir."
    return
}

Assert-SupportedBuild
$Bin = Join-Path $script:RepoRoot "bin\$Configuration"
$Files = (Join-Path $Bin 'version.dll'), (Join-Path $Bin 'DDCoop.dll')
foreach ($File in $Files) {
    if (-not (Test-Path $File)) { throw "$File fehlt. Zuerst DeadlyDaysCoop.sln mit $Configuration|x64 bauen." }
}

New-Item -ItemType Directory -Force -Path $ModDir | Out-Null
Copy-Item $Files[0] $Proxy -Force
Copy-Item $Files[1] $ModDir -Force
Write-Output "Mod ($Configuration) liegt in $Win64. Nach dem Testen mit -Remove entfernen."
