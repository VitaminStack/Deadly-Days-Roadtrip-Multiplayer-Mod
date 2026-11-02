# Gemeinsame Helfer der Spike-Skripte. Pfade kommen aus config\Local.props.
$script:SupportedBuild = '26187885'
$script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:SessionDir = Join-Path $script:RepoRoot 'research\local\session'
$script:CommandDir = Join-Path $env:TEMP 'ddspike'

function Get-GameDir {
    $Props = Join-Path $script:RepoRoot 'config\Local.props'
    if (-not (Test-Path $Props)) {
        throw 'config\Local.props fehlt. config\Local.props.example kopieren und GameDir eintragen.'
    }
    $GameDir = ([xml](Get-Content $Props -Raw)).Project.PropertyGroup.GameDir
    if (-not (Test-Path $GameDir)) { throw "GameDir aus config\Local.props gibt es nicht: $GameDir" }
    $GameDir
}

function Get-GameExe {
    Join-Path (Get-GameDir) 'DDSurvivors\Binaries\Win64\DDSurvivors-Win64-Shipping.exe'
}

# Offsets und Patches gelten nur fuer einen Build. Auf einem anderen laufen sie ins Leere oder stuerzen ab.
function Assert-SupportedBuild {
    $Manifest = Join-Path (Get-GameDir) '..\..\appmanifest_3026450.acf'
    if (-not (Test-Path $Manifest)) { throw "Steam-Manifest nicht gefunden: $Manifest" }
    $Match = [regex]::Match((Get-Content $Manifest -Raw), '"buildid"\s+"(\d+)"')
    if ($Match.Groups[1].Value -ne $script:SupportedBuild) {
        throw "Spiel-Build $($Match.Groups[1].Value) statt $($script:SupportedBuild). Siehe CONTRIBUTING.md, Abschnitt Spiel-Updates."
    }
}

function Initialize-SessionDir {
    New-Item -ItemType Directory -Force -Path $script:SessionDir, $script:CommandDir | Out-Null
}

function Get-Session {
    $File = Join-Path $script:SessionDir 'session.json'
    if (-not (Test-Path $File)) { throw 'Keine Sitzung gefunden. Zuerst tools\spike\Start-Session.ps1 ausfuehren.' }
    Get-Content $File -Raw | ConvertFrom-Json
}

function Get-RolePid([string]$Role) {
    $Session = Get-Session
    if ($Role -eq 'Host') { [int]$Session.Host } else { [int]$Session.Client }
}

function Send-SpikeCommand([int]$ProcessId, [string]$Command) {
    $Temp = Join-Path $script:CommandDir "$ProcessId.tmp"
    Set-Content -Path $Temp -Value $Command -NoNewline -Encoding ascii
    Move-Item $Temp (Join-Path $script:CommandDir "$ProcessId.cmd") -Force
}

# Fuehrt eine Befehlszeile in der Visual-Studio-Umgebung aus (cl, dumpbin).
function Invoke-InVsEnvironment([string]$CommandLine) {
    $Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $Vs = & $Vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $Vs) { throw 'Visual Studio mit C++-Werkzeugen nicht gefunden.' }
    $Script = Join-Path $script:SessionDir 'vsenv.cmd'
    Set-Content -Path $Script -Encoding ascii -Value "@call `"$Vs\VC\Auxiliary\Build\vcvars64.bat`" >nul`r`n@$CommandLine"
    & cmd /c "`"$Script`""
}
