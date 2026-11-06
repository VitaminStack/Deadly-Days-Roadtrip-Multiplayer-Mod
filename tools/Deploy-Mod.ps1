# Kopiert die gebaute Mod neben die Spiel-Exe oder entfernt sie wieder. Der Spielordner kommt aus config\Local.props.
# Solange die Mod dort liegt, laedt auch jeder normale Spielstart sie. Nach dem Testen deshalb -Remove ausfuehren.
# Kopieren und Pruefen macht derselbe Installer wie im Release-Paket (tools\installer\Install-DDCoop.ps1).
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'spike\Common.ps1')

$Installer = Join-Path $PSScriptRoot 'installer\Install-DDCoop.ps1'
$GameDir = Get-GameDir

if ($Remove) {
    & $Installer -GameDir $GameDir -Uninstall
    return
}

# Eingefrorene Entwicklungskopien haben kein Steam-Manifest, darum prueft hier Common.ps1 den Build.
Assert-SupportedBuild
$Bin = Join-Path $script:RepoRoot "bin\$Configuration"
foreach ($File in (Join-Path $Bin 'version.dll'), (Join-Path $Bin 'DDCoop.dll')) {
    if (-not (Test-Path $File)) { throw "$File fehlt. Zuerst DeadlyDaysCoop.sln mit $Configuration|x64 bauen." }
}
& $Installer -GameDir $GameDir -Source $Bin -SkipBuildCheck | Out-Null
Write-Output "Mod ($Configuration) liegt in $(Split-Path (Get-GameExe)). Nach dem Testen mit -Remove entfernen."
