# Prueft die lokale Einrichtung aus docs\SETUP.md und nennt, was fehlt. Aendert nichts.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'spike\Common.ps1')
$script:Failed = 0

function Report([string]$Name, [bool]$Ok, [string]$Hint, [switch]$Optional) {
    if ($Ok) { Write-Output "OK      $Name"; return }
    if ($Optional) { Write-Output "HINWEIS $Name - $Hint"; return }
    Write-Output "FEHLT   $Name - $Hint"
    $script:Failed++
}

Report 'Git' ([bool](Get-Command git -ErrorAction SilentlyContinue)) 'Git installieren'
Report 'Python-Launcher py' ([bool](Get-Command py -ErrorAction SilentlyContinue)) 'Python 3 mit Launcher installieren'

$Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$Vs = if (Test-Path $Vswhere) { & $Vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath }
Report 'Visual Studio mit C++' ([bool]$Vs) 'Visual Studio 2022 oder neuer mit "Desktopentwicklung mit C++" installieren'

try { $Props = Find-LocalProps } catch { $Props = $null }
Report 'config\Local.props' ([bool]$Props) 'config\Local.props.example nach config\Local.props kopieren'
if ($Props) {
    $Values = ([xml](Get-Content $Props -Raw)).Project.PropertyGroup
    try { $GameOk = Test-Path (Get-GameExe) } catch { $GameOk = $false }
    Report 'Aktiver Spielordner' $GameOk 'GameDir bzw. PinnedGameDir in config\Local.props pruefen (kein automatischer Rueckfall)'
    if ($GameOk) {
        try { Assert-SupportedBuild; $BuildOk = $true; $BuildHint = '' } catch { $BuildOk = $false; $BuildHint = $_.Exception.Message }
        Report "Spiel-Build $script:SupportedBuild" $BuildOk $BuildHint
        Report 'Keine fremde version.dll' (-not (Test-Path (Join-Path (Split-Path (Get-GameExe)) 'version.dll')) -or
            [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes((Join-Path (Split-Path (Get-GameExe)) 'version.dll'))).Contains('ModStart')) 'Eine andere Mod nutzt version.dll, Deploy-Mod.ps1 bricht dann ab'
    }
    Report 'Dumper-7-SDK' ([bool]$Values.Dumper7SdkRoot -and (Test-Path (Join-Path $Values.Dumper7SdkRoot 'SDK'))) 'erst fuer src\Game noetig, siehe docs\SETUP.md#sdk' -Optional
    Report 'Steamworks SDK' ([bool]$Values.SteamworksSdkRoot -and (Test-Path $Values.SteamworksSdkRoot)) 'erst fuer Steam-Funktionen noetig' -Optional
}

if ($script:Failed) {
    Write-Output "$script:Failed Punkt(e) fehlen, siehe docs\SETUP.md."
    exit 1
}
Write-Output 'Einrichtung vollstaendig.'
