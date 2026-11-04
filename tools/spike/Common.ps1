# Gemeinsame Helfer der Spike-Skripte. Pfade kommen aus config\Local.props.
$script:SupportedBuild = '26187885'
$script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:SessionDir = Join-Path $script:RepoRoot 'research\local\session'
$script:CommandDir = Join-Path $env:TEMP 'ddspike'
$script:LockFile = Join-Path $script:CommandDir 'session.lock'
$script:GameProcess = 'DDSurvivors-Win64-Shipping'

# In einem Git-Worktree fehlt die lokale Datei, dann gilt die aus dem Hauptordner.
function Find-LocalProps {
    $Props = Join-Path $script:RepoRoot 'config\Local.props'
    if (Test-Path $Props) { return $Props }
    try { $GitDir = & git -C $script:RepoRoot rev-parse --path-format=absolute --git-common-dir 2>$null } catch { $GitDir = $null }
    if ($GitDir) {
        $Props = Join-Path (Split-Path $GitDir) 'config\Local.props'
        if (Test-Path $Props) { return $Props }
    }
    throw 'config\Local.props fehlt. config\Local.props.example kopieren und GameDir eintragen.'
}

function Get-GameDir {
    $Values = ([xml](Get-Content (Find-LocalProps) -Raw)).Project.PropertyGroup
    $GameDir = if ($Values.PinnedGameDir) { [string]$Values.PinnedGameDir } else { [string]$Values.GameDir }
    if (-not (Test-Path -LiteralPath $GameDir -PathType Container)) { throw "Aktiver Spielordner aus config\Local.props fehlt: $GameDir" }
    $GameDir
}

function Get-GameExe {
    Join-Path (Get-GameDir) 'DDSurvivors\Binaries\Win64\DDSurvivors-Win64-Shipping.exe'
}

# Offsets und Patches gelten nur fuer einen Build. Auf einem anderen laufen sie ins Leere oder stuerzen ab.
function Assert-SupportedBuild {
    $Values = ([xml](Get-Content (Find-LocalProps) -Raw)).Project.PropertyGroup
    if ($Values.PinnedGameDir) {
        & py (Join-Path $script:RepoRoot 'tools\game_build.py') verify --directory (Get-GameDir) --build $script:SupportedBuild
        if ($LASTEXITCODE -ne 0) { throw 'Entwicklungskopie ungueltig. Kein Rueckfall auf die Steam-Installation.' }
        return
    }
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

function Test-GameProcess([int]$ProcessId) {
    $Process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    [bool]($Process -and $Process.ProcessName -eq $script:GameProcess)
}

function Test-OwnSessionRunning {
    $File = Join-Path $script:SessionDir 'session.json'
    if (-not (Test-Path $File)) { return $false }
    $Session = Get-Content $File -Raw | ConvertFrom-Json
    (Test-GameProcess $Session.Host) -or (Test-GameProcess $Session.Client)
}

# Eine Testsitzung pro PC: Sie belegt Port 7777, Fenster, Maus und Tastatur. Die Sperre gilt ueber alle Klone und Worktrees.
function Test-LockActive($Lock) {
    if ($Lock.Host -eq 0) { return [bool](Get-Process -Id $Lock.Starter -ErrorAction SilentlyContinue) }
    (Test-GameProcess $Lock.Host) -or (Test-GameProcess $Lock.Client)
}

function Write-SessionLock([int]$HostId, [int]$ClientId) {
    @{ Owner = $script:RepoRoot; Starter = $PID; Host = $HostId; Client = $ClientId } | ConvertTo-Json
}

function Enter-SessionLock {
    foreach ($Attempt in 1, 2) {
        try {
            $Bytes = [Text.Encoding]::ASCII.GetBytes((Write-SessionLock 0 0))
            $Stream = [IO.File]::Open($script:LockFile, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
            $Stream.Write($Bytes, 0, $Bytes.Length)
            $Stream.Close()
            return
        }
        catch [Management.Automation.MethodInvocationException] {
            $Lock = Get-Content $script:LockFile -Raw | ConvertFrom-Json
            if (Test-LockActive $Lock) {
                throw "Testsitzung laeuft bereits aus $($Lock.Owner). Warten, bis sie dort mit Stop-Session.ps1 beendet ist."
            }
            Remove-Item $script:LockFile -Force
        }
    }
    throw 'Sperre konnte nicht gesetzt werden.'
}

function Exit-SessionLock {
    if (-not (Test-Path $script:LockFile)) { return }
    $Lock = Get-Content $script:LockFile -Raw | ConvertFrom-Json
    if ($Lock.Owner -eq $script:RepoRoot) { Remove-Item $script:LockFile -Force }
}

function Get-RolePid([string]$Role) {
    $Session = Get-Session
    if ($Role -eq 'Host') { [int]$Session.Host } else { [int]$Session.Client }
}

# Der Spike liest die Befehlsdatei einmal pro Sekunde. Ein zweiter Befehl darf sie erst ersetzen, wenn der erste abgeholt ist,
# sonst geht der erste still verloren.
function Send-SpikeCommand([int]$ProcessId, [string]$Command, [int]$TimeoutSeconds = 5) {
    $Pending = Join-Path $script:CommandDir "$ProcessId.cmd"
    $Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Test-Path $Pending) -and (Get-Date) -lt $Deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path $Pending) { throw "Spike in $ProcessId holt Befehle nicht ab, '$Command' nicht gesendet." }
    $Temp = Join-Path $script:CommandDir "$ProcessId.tmp"
    Set-Content -Path $Temp -Value $Command -NoNewline -Encoding ascii
    Move-Item $Temp (Join-Path $script:CommandDir "$ProcessId.cmd") -Force
}

# Fuehrt eine Befehlszeile in der Visual-Studio-Umgebung aus (cl, dumpbin).
function Invoke-InVsEnvironment([string]$CommandLine) {
    $Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $Vs = & $Vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $Vs) { throw 'Visual Studio mit C++-Werkzeugen nicht gefunden.' }
    # Eigene Datei je Prozess, damit parallele Builds sich nicht ueberschreiben. vcvars ruft vswhere ohne Pfad auf.
    New-Item -ItemType Directory -Force -Path $script:SessionDir | Out-Null
    $Script = Join-Path $script:SessionDir "vsenv-$PID.cmd"
    Set-Content -Path $Script -Encoding ascii -Value ("@set `"PATH=$(Split-Path $Vswhere);%PATH%`"`r`n" +
        "@call `"$Vs\VC\Auxiliary\Build\vcvars64.bat`" >nul`r`n@$CommandLine")
    try { & cmd /c "`"$Script`"" }
    finally { Remove-Item $Script -Force -ErrorAction SilentlyContinue }
}
