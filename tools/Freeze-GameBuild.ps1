# Erstellt eine echte, gepruefte Kopie ausserhalb von Steam. Keine Steam-Einstellungen oder Downloads.
param(
    [string]$Destination,
    # Setzt nur PinnedGameDir in der lokalen Konfiguration, GameDir bleibt Update-Quelle.
    [switch]$Activate
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'spike\Common.ps1')
if (Get-Process $script:GameProcess -ErrorAction SilentlyContinue) {
    throw 'Das Spiel laeuft. Erst die eigene Sitzung beenden, fremde Instanzen nicht stoppen.'
}
$PropsPath = Find-LocalProps
$Props = [xml](Get-Content -LiteralPath $PropsPath -Raw)
$Source = $Props.Project.PropertyGroup.GameDir
if (-not $Destination) { $Destination = Join-Path $env:LOCALAPPDATA "DDCoop\builds\$script:SupportedBuild" }
if (Test-Path -LiteralPath $Destination) {
    & py (Join-Path $PSScriptRoot 'game_build.py') verify --directory $Destination --build $script:SupportedBuild
}
else {
    & py (Join-Path $PSScriptRoot 'game_build.py') freeze --source $Source --destination $Destination --build $script:SupportedBuild
}
if ($LASTEXITCODE -ne 0) { throw 'Entwicklungskopie nicht angelegt; Konfiguration bleibt unveraendert.' }
$Destination = (Resolve-Path -LiteralPath $Destination).Path
if ($Activate) {
    $Group = $Props.Project.PropertyGroup
    $Node = $Group.SelectSingleNode('PinnedGameDir')
    if (-not $Node) { $Node = $Props.CreateElement('PinnedGameDir'); $Group.AppendChild($Node) | Out-Null }
    $Node.InnerText = $Destination
    $Props.Save($PropsPath)
    Write-Output 'PinnedGameDir aktiviert. GameDir und Steam-Installation bleiben unveraendert.'
}
else { Write-Output "Zum Aktivieren PinnedGameDir in config\Local.props setzen: $Destination" }
