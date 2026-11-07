# Gemeinsame Helfer der Spike-Skripte. Pfade kommen aus config\Local.props.
# Standard fuer neue Entwicklungskopien. Alle Builds mit Offsets/Patches in der Mod stehen in SupportedBuilds,
# gleich gehalten mit tools/game_build.py (REFERENCES) und src/Core/Version.cpp.
$script:SupportedBuild = '26187885'
$script:SupportedBuilds = @('26187885', '26495302')
# Nach Assert-SupportedBuild der tatsaechlich verwendete Build.
$script:ActiveBuild = $null
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

# Start ueber Steam (Start-Session.ps1 -SteamLaunch): Steam startet immer seine eigene Installation, nie die eingefrorene Kopie.
# Die Umgebungsvariable erben Deploy-Mod.ps1 und der Crash-Schutz.
function Test-UsePinnedGameDir($Values) {
    [bool]$Values.PinnedGameDir -and $env:DDCOOP_STEAM_INSTALL -ne '1'
}

function Get-GameDir {
    $Values = ([xml](Get-Content (Find-LocalProps) -Raw)).Project.PropertyGroup
    $GameDir = if (Test-UsePinnedGameDir $Values) { [string]$Values.PinnedGameDir } else { [string]$Values.GameDir }
    if (-not (Test-Path -LiteralPath $GameDir -PathType Container)) { throw "Aktiver Spielordner aus config\Local.props fehlt: $GameDir" }
    $GameDir
}

function Get-GameExe {
    Join-Path (Get-GameDir) 'DDSurvivors\Binaries\Win64\DDSurvivors-Win64-Shipping.exe'
}

# Offsets und Patches gelten nur fuer einen Build. Auf einem anderen laufen sie ins Leere oder stuerzen ab.
function Assert-SupportedBuild {
    $Values = ([xml](Get-Content (Find-LocalProps) -Raw)).Project.PropertyGroup
    if (Test-UsePinnedGameDir $Values) {
        # Der Build steht im Beleg der Kopie; game_build.py prueft ihn danach gegen die feste Referenz.
        $Receipt = Join-Path (Get-GameDir) '.ddcoop-build.json'
        $Build = try { [string](Get-Content -LiteralPath $Receipt -Raw | ConvertFrom-Json).build_id } catch { '' }
        if ($script:SupportedBuilds -notcontains $Build) { throw "Entwicklungskopie mit unbekanntem Build '$Build'. Kein Rueckfall auf die Steam-Installation." }
        & py (Join-Path $script:RepoRoot 'tools\game_build.py') verify --directory (Get-GameDir) --build $Build
        if ($LASTEXITCODE -ne 0) { throw 'Entwicklungskopie ungueltig. Kein Rueckfall auf die Steam-Installation.' }
        $script:ActiveBuild = $Build
        return
    }
    $Manifest = Join-Path (Get-GameDir) '..\..\appmanifest_3026450.acf'
    if (-not (Test-Path $Manifest)) { throw "Steam-Manifest nicht gefunden: $Manifest" }
    $Match = [regex]::Match((Get-Content $Manifest -Raw), '"buildid"\s+"(\d+)"')
    if ($script:SupportedBuilds -notcontains $Match.Groups[1].Value) {
        throw "Spiel-Build $($Match.Groups[1].Value) statt $($script:SupportedBuilds -join ' oder '). Siehe CONTRIBUTING.md, Abschnitt Spiel-Updates."
    }
    $script:ActiveBuild = $Match.Groups[1].Value
}

function Initialize-SessionDir {
    New-Item -ItemType Directory -Force -Path $script:SessionDir, $script:CommandDir | Out-Null
}

function Get-Session {
    $File = Join-Path $script:SessionDir 'session.json'
    if (-not (Test-Path $File)) { throw 'Keine Sitzung gefunden. Zuerst tools\spike\Start-Session.ps1 ausfuehren.' }
    Get-Content $File -Raw | ConvertFrom-Json
}

# Laeuft die Sitzung mit Spike? Nein nur bei Start-Session.ps1 -NoSpike, also wie im Release allein mit der Mod.
function Test-SessionSpike {
    $File = Join-Path $script:SessionDir 'session.json'
    if (-not (Test-Path $File)) { return $true }
    $Session = Get-Content $File -Raw | ConvertFrom-Json
    -not ($Session.PSObject.Properties['NoSpike'] -and $Session.NoSpike)
}

# Wirft, falls spike.dll im Prozess geladen ist. Beleg fuer Laeufe mit -NoSpike.
function Assert-NoSpikeLoaded([int]$ProcessId) {
    $Modules = (Get-Process -Id $ProcessId -ErrorAction Stop).Modules | ForEach-Object { $_.ModuleName }
    if ($Modules -contains 'spike.dll') { throw "spike.dll ist in $ProcessId geladen, obwohl die Sitzung ohne Spike laeuft." }
    if ($Modules -notcontains 'DDCoop.dll') { throw "DDCoop.dll ist in $ProcessId nicht geladen." }
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
    # Beide 0: Die Sitzung startet noch. Nur eine 0: Test auf zwei PCs, die andere Rolle laeuft woanders.
    if ($Lock.Host -eq 0 -and $Lock.Client -eq 0) { return [bool](Get-Process -Id $Lock.Starter -ErrorAction SilentlyContinue) }
    (Test-GameProcess $Lock.Host) -or (Test-GameProcess $Lock.Client)
}

function Write-SessionLock([int]$HostId, [int]$ClientId) {
    @{ Owner = $script:RepoRoot; Starter = $PID; Host = $HostId; Client = $ClientId } | ConvertTo-Json
}

# Mit WaitSeconds > 0 wartet der Aufruf, bis eine fremde Sitzung endet, statt abzubrechen. Fremde Instanzen bleiben unberuehrt.
function Enter-SessionLock([int]$WaitSeconds = 0) {
    $Deadline = (Get-Date).AddSeconds($WaitSeconds)
    $Announced = $false
    while ($true) {
        try {
            $Bytes = [Text.Encoding]::ASCII.GetBytes((Write-SessionLock 0 0))
            $Stream = [IO.File]::Open($script:LockFile, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
            $Stream.Write($Bytes, 0, $Bytes.Length)
            $Stream.Close()
            return
        }
        catch [Management.Automation.MethodInvocationException] {
            $Lock = $null
            try { $Lock = Get-Content $script:LockFile -Raw -ErrorAction Stop | ConvertFrom-Json } catch { continue }
            if (-not (Test-LockActive $Lock)) { Remove-Item $script:LockFile -Force -ErrorAction SilentlyContinue; continue }
            if ((Get-Date) -ge $Deadline) {
                $Hint = if ($WaitSeconds) { "Nach $WaitSeconds s noch belegt." } else { 'Mit -Wait <Sekunden> warten oder ohne Spiel weiterarbeiten.' }
                throw "Testsitzung laeuft bereits aus $($Lock.Owner). $Hint"
            }
            if (-not $Announced) { Write-Output "Warte auf die Sitzung aus $($Lock.Owner) ..."; $Announced = $true }
            Start-Sleep -Seconds 5
        }
    }
}

function Exit-SessionLock {
    if (-not (Test-Path $script:LockFile)) { return }
    $Lock = Get-Content $script:LockFile -Raw | ConvertFrom-Json
    if ($Lock.Owner -eq $script:RepoRoot) { Remove-Item $script:LockFile -Force }
}

function Get-RolePid([string]$Role) {
    $Session = Get-Session
    $ProcessId = if ($Role -eq 'Host') { [int]$Session.Host } else { [int]$Session.Client }
    if (-not $ProcessId) { throw "$Role laeuft nicht auf diesem PC (Test auf zwei PCs, Sitzung mit -Role $($Session.Role))." }
    $ProcessId
}

# Rollen, die auf diesem PC laufen. Beim Test auf zwei PCs ist es nur eine.
function Get-LocalRoles {
    $Session = Get-Session
    @('Host', 'Client') | Where-Object { [int]$Session.$_ }
}

# Der Spike liest die Befehlsdatei einmal pro Sekunde. Ein zweiter Befehl darf sie erst ersetzen, wenn der erste abgeholt ist,
# sonst geht der erste still verloren.
function Send-SpikeCommand([int]$ProcessId, [string]$Command, [int]$TimeoutSeconds = 5) {
    if (-not (Test-SessionSpike)) { throw "Sitzung ohne Spike (-NoSpike): '$Command' braucht den Spike." }
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

# Netzzustand einer Instanz aus netstate.py, etwa @{ world = 'MetaBase'; players = 2; clients = 1 }. Leer, solange nichts lesbar ist.
function Get-NetState([int]$ProcessId) {
    $Line = & py (Join-Path $PSScriptRoot 'netstate.py') $ProcessId 2>$null | Select-Object -Last 1
    $State = @{ line = $Line }
    foreach ($Match in [regex]::Matches([string]$Line, "'(\w+)': (?:'([^']*)'|(-?\d+))")) {
        $State[$Match.Groups[1].Value] = if ($Match.Groups[2].Success) { $Match.Groups[2].Value } else { [int]$Match.Groups[3].Value }
    }
    $State
}

# Wartet, bis die Bedingung gilt, statt fest zu schlafen. Bricht ab, wenn eine der Instanzen endet.
function Wait-Condition([scriptblock]$Condition, [string]$What, [int]$TimeoutSeconds, [int[]]$ProcessIds = @(), [int]$PollMilliseconds = 1000) {
    $Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $Deadline) {
        foreach ($ProcessId in $ProcessIds) {
            if ($ProcessId -and -not (Test-GameProcess $ProcessId)) { throw "Instanz $ProcessId ist beendet ($What). Siehe tools\spike\crashes.py." }
        }
        if (& $Condition) { return }
        Start-Sleep -Milliseconds $PollMilliseconds
    }
    throw "Zeitablauf nach $TimeoutSeconds s: $What"
}

# Bereit fuer den Spike: Das Hauptmenue ist geladen. Frueher galt dafuer ein fester Schlaf von 20 s.
# Die Routenkarte erkennt man am gruenen Banner "Felder" oben rechts im Host-Fenster.
function Test-RouteMap {
    $Color = & (Join-Path $PSScriptRoot 'Game-Input.ps1') Host pixel 700 13
    $R, $G, $B = [Convert]::ToInt32($Color.Substring(0, 2), 16), [Convert]::ToInt32($Color.Substring(2, 2), 16), [Convert]::ToInt32($Color.Substring(4, 2), 16)
    [Math]::Abs($R - 0x3C) -lt 16 -and [Math]::Abs($G - 0x5F) -lt 16 -and [Math]::Abs($B - 0x3C) -lt 16
}

# Beendet den Tag: !finish beim Host, dann bei jeder lokalen Instanz Fortsetzen (Rundenuebersicht) und Bestaetigen (Lager).
# Seit bestaetigen-rundenende wartet der Host im Lager, bis auch der Client bestaetigt hat; erst dann oeffnet er die Routenkarte.
# Der Host bestaetigt zuerst, der Client loest dann fuer beide aus. Beim Test auf zwei PCs bestaetigt der Client am anderen PC.
# Liefert $true, sobald der Host auf der Routenkarte ist, $false nach -TimeoutSeconds.
function Complete-RoundEnd([int]$TimeoutSeconds = 30) {
    $GameInput = Join-Path $PSScriptRoot 'Game-Input.ps1'
    $Roles = @(Get-LocalRoles)
    Send-SpikeCommand (Get-RolePid 'Host') '!finish'
    Start-Sleep -Seconds 6
    foreach ($Role in $Roles) { & $GameInput $Role click 410 426 | Out-Null }
    Start-Sleep -Seconds 4
    foreach ($Role in $Roles) {
        & $GameInput $Role click 633 422 | Out-Null
        Start-Sleep -Seconds 1
    }
    $Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $Deadline) {
        if (Test-RouteMap) { return $true }
        Start-Sleep -Seconds 1
    }
    $false
}

# Host ist schon in der Mission oder reist dorthin. Ein frischer Spielstand startet den ersten Run ohne Routenkarte.
function Test-HostInMission {
    $State = Get-NetState (Get-RolePid 'Host')
    $State.world -eq 'DDDynamic_Map' -or $State.package -like '*DDDynamic_Map'
}

function Wait-MainMenu([int]$ProcessId, [int]$TimeoutSeconds = 90) {
    Wait-Condition { (Get-NetState $ProcessId).world -eq 'MainMenu' } "Hauptmenue von $ProcessId" $TimeoutSeconds @($ProcessId)
}

# Overlay der Mod (Fenster DDCoop.Overlay der Instanz). Knoepfe und Felder wie in src\UI\UI.cpp.
$script:OverlayIds = @{ Create = 101; Join = 102; Invite = 103; Leave = 104; Lobby = 105 }
if (-not ('DDCoopOverlay' -as [type])) {
    Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public static class DDCoopOverlay {
    delegate bool EnumProc(IntPtr Window, IntPtr Data);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc Proc, IntPtr Data);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr Window, out uint Process);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr Window, StringBuilder Name, int Size);
    [DllImport("user32.dll")] static extern IntPtr GetDlgItem(IntPtr Window, int Id);
    [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr Window, uint Command);
    [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr Window);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr Window, uint Message, IntPtr W, StringBuilder L);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr Window, uint Message, IntPtr W, string L);
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr Window, uint Message, IntPtr W, IntPtr L);
    public static IntPtr Find(uint Process) {
        IntPtr Found = IntPtr.Zero;
        EnumWindows((Window, Data) => {
            uint Owner; GetWindowThreadProcessId(Window, out Owner);
            if (Owner != Process) return true;
            var Name = new StringBuilder(64); GetClassName(Window, Name, 64);
            if (Name.ToString() != "DDCoop.Overlay") return true;
            Found = Window; return false;
        }, IntPtr.Zero);
        return Found;
    }
    static string Text(IntPtr Window) { var Buffer = new StringBuilder(4096); SendMessage(Window, 0x0D, (IntPtr)4096, Buffer); return Buffer.ToString(); }
    // Texte aller Steuerelemente in Reihenfolge: Status, Meldung, Debug, Hinweis, Knoepfe.
    public static string[] Texts(IntPtr Window) {
        var Result = new List<string>();
        for (IntPtr Child = GetWindow(Window, 5); Child != IntPtr.Zero; Child = GetWindow(Child, 2)) Result.Add(Text(Child));
        return Result.ToArray();
    }
    public static bool Enabled(IntPtr Window, int Id) { return IsWindowEnabled(GetDlgItem(Window, Id)); }
    // Wie ein Klick: WM_COMMAND mit BN_CLICKED an das Overlay, ohne Maus und ohne sichtbares Fenster.
    public static void Click(IntPtr Window, int Id) { SendMessage(Window, 0x111, (IntPtr)Id, GetDlgItem(Window, Id)); }
    public static void SetText(IntPtr Window, int Id, string Value) { SendMessage(GetDlgItem(Window, Id), 0x0C, IntPtr.Zero, Value); }
}
"@
}

function Get-OverlayWindow([int]$ProcessId, [int]$TimeoutSeconds = 0) {
    $Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        $Window = [DDCoopOverlay]::Find([uint32]$ProcessId)
        if ($Window -ne [IntPtr]::Zero) { return $Window }
        Start-Sleep -Milliseconds 500
    } while ((Get-Date) -lt $Deadline)
    throw "Kein Overlay in $ProcessId. Laeuft die Mod, und steht overlay_enabled in ddcoop.ini nicht auf false?"
}

# Zustand des Overlays: State, Message, Role, LobbyId, Ping und welche Knoepfe frei sind.
function Get-OverlayState([int]$ProcessId) {
    $Window = Get-OverlayWindow $ProcessId 5
    $Texts = [DDCoopOverlay]::Texts($Window)
    $Debug = [string]$Texts[2]
    $Lobby = [regex]::Match($Debug, 'Lobby: (\d+)')
    [pscustomobject]@{
        State = ([string]$Texts[0]) -replace '^DDCoop \| ', ''
        Message = [string]$Texts[1]
        Diagnostics = if ($Texts.Length -gt 4) { [string]$Texts[4] } else { '' }
        Role = [regex]::Match($Debug, 'Rolle: (\w+)').Groups[1].Value
        LobbyId = if ($Lobby.Success) { $Lobby.Groups[1].Value } else { '' }
        Ping = [regex]::Match($Debug, 'Ping: ([^\r\n]+?)(\r|\n|$)').Groups[1].Value
        CanCreate = [DDCoopOverlay]::Enabled($Window, $script:OverlayIds.Create)
        CanJoin = [DDCoopOverlay]::Enabled($Window, $script:OverlayIds.Join)
        CanLeave = [DDCoopOverlay]::Enabled($Window, $script:OverlayIds.Leave)
    }
}

# Loest einen Overlay-Knopf aus wie ein Klick. Join setzt vorher die Lobby-ID ins Feld.
function Invoke-OverlayAction([int]$ProcessId, [ValidateSet('Create', 'Join', 'Invite', 'Leave')][string]$Action, [string]$LobbyId) {
    $Window = Get-OverlayWindow $ProcessId 5
    if ($Action -eq 'Join') {
        if ($LobbyId -notmatch '^\d{1,20}$') { throw 'Join braucht eine Lobby-ID.' }
        [DDCoopOverlay]::SetText($Window, $script:OverlayIds.Lobby, $LobbyId)
        Start-Sleep -Milliseconds 300
    }
    [DDCoopOverlay]::Click($Window, $script:OverlayIds[$Action])
}

# Erstellt beim Host eine Lobby ueber das Overlay und liefert ihre ID.
function New-OverlayLobby([int]$HostId, [int]$TimeoutSeconds = 30) {
    Wait-Condition { (Get-OverlayState $HostId).CanCreate } 'Overlay des Hosts bereit zum Erstellen' 30 @($HostId) 500
    Invoke-OverlayAction $HostId Create
    $script:NewLobby = $null
    Wait-Condition { $script:NewLobby = Get-OverlayState $HostId; $script:NewLobby.LobbyId -and $script:NewLobby.State -ne 'Verbindung wird aufgebaut' } 'Lobby beim Host erstellt' $TimeoutSeconds @($HostId) 500
    $script:NewLobby.LobbyId
}

# Tritt beim Client ueber das Overlay bei und wartet, bis die Sitzung steht oder scheitert. Liefert den Overlay-Zustand.
function Join-OverlayLobby([int]$ClientId, [string]$LobbyId, [int]$TimeoutSeconds = 60) {
    Get-OverlayWindow $ClientId 30 | Out-Null
    Invoke-OverlayAction $ClientId Join $LobbyId
    Start-Sleep -Milliseconds 500
    $script:JoinState = $null
    Wait-Condition {
        $script:JoinState = Get-OverlayState $ClientId
        $script:JoinState.State -in 'Im Spiel', 'Verbindungsfehler', 'Offline'
    } 'Beitritt des Clients' $TimeoutSeconds @($ClientId) 500
    $script:JoinState
}

# Bringt den Client in die Lobby des Hosts, beide auf diesem PC. Hat der Host keine Lobby oder ist sie weg, erstellt er
# ueber sein Overlay eine neue. Ein Konto auf einem PC: Verlaesst der Client, loest Steam die Lobby auf (Antwort 2).
function Join-HostLobby([int]$HostId, [int]$ClientId) {
    $LobbyId = (Get-OverlayState $HostId).LobbyId
    if ($LobbyId) {
        $Joined = Join-OverlayLobby $ClientId $LobbyId
        if ($Joined.State -eq 'Im Spiel') { return $LobbyId }
        Write-Host "Beitritt abgelehnt: $($Joined.Message). Host erstellt eine neue Lobby."
        if ((Get-OverlayState $HostId).CanLeave) {
            Invoke-OverlayAction $HostId Leave
            Wait-Condition { (Get-OverlayState $HostId).CanCreate } 'Host wieder frei' 20 @($HostId) 500
        }
    }
    $LobbyId = New-OverlayLobby $HostId
    $Joined = Join-OverlayLobby $ClientId $LobbyId
    if ($Joined.State -ne 'Im Spiel') { throw "Beitritt gescheitert: $($Joined.Message)" }
    $LobbyId
}

# Kurzform fuer -Brief: eine Zeile je Instanz statt der ganzen Zustaende aus netstate.py.
function Format-NetBrief([int]$ProcessId, [string]$Role) {
    if (-not (Test-GameProcess $ProcessId)) { return "$Role ${ProcessId}: beendet" }
    $State = Get-NetState $ProcessId
    if (-not $State.world) { return "$Role ${ProcessId}: nicht lesbar" }
    $Clients = if ($State.role -eq 'host') { ", $($State.clients) Client(s)" } else { '' }
    "$Role ${ProcessId}: $($State.world), $($State.players) Spieler, $($State.role)$Clients"
}

# Inventarvergleich in einer Zeile.
function Format-InventoryBrief([int]$HostId, [int]$ClientId) {
    $Text = (& py (Join-Path $PSScriptRoot 'inventory.py') $HostId $ClientId) -join "`n"
    $Complete = $Text -match '"complete": true'
    $Equal = $Text -match '"equal": true'
    "Inventar: $(if ($Complete) { 'vollstaendig' } else { 'UNVOLLSTAENDIG' }), $(if ($Equal) { 'gleich' } else { 'UNGLEICH' })"
}

# Neue Abstuerze seit Sitzungsstart in einer Zeile.
function Format-CrashBrief {
    $Lines = @(& py (Join-Path $PSScriptRoot 'crashes.py') --session)
    if (-not $Lines.Count -or $Lines[0] -match 'Keine') { return 'Abstuerze: keine' }
    "Abstuerze: $(@($Lines | Where-Object { $_ -match '^UECC|^\S+ (host|client) ' }).Count), neuester: $($Lines[0].Substring(0, [Math]::Min(120, $Lines[0].Length)))"
}

# Netzprofile fuer -NetProfile: Ping hin und zurueck in ms, Schwankung +- ms, Verlust je Richtung in Prozent.
# Wirkt nur im Tunnel der Mod (-Mod mit -Tunnel Lobby oder Mod), siehe docs\TESTING.md, Abschnitt Latenz.
$script:NetProfiles = [ordered]@{
    Internet = '120,30,2'
    Schlecht = '250,80,5'
}

# Liefert den Wert fuer -ddcoop_netsim= oder $null ohne Profil. Name aus $NetProfiles oder eigene Werte "Ping,Schwankung,Verlust".
function Resolve-NetProfile([string]$Name) {
    if (-not $Name -or $Name -eq 'Aus') { return $null }
    foreach ($Key in $script:NetProfiles.Keys) { if ($Key -eq $Name) { return $script:NetProfiles[$Key] } }
    if ($Name -match '^\d+(\.\d+)?(,\d+(\.\d+)?){0,2}$') { return $Name }
    throw "Unbekanntes Netzprofil '$Name'. Bekannt: $($script:NetProfiles.Keys -join ', '), Aus oder Werte wie 120,30,2."
}

# Letzte Messzeile der Netzsimulation aus dem Mod-Log, eine Zeile je Instanz.
function Format-NetSimBrief([int]$ProcessId, [string]$Role) {
    $Log = Join-Path (Split-Path (Get-GameExe)) "DDCoop\logs\ddcoop-$ProcessId.log"
    $Hit = if (Test-Path $Log) { Select-String -Path $Log -Pattern 'Netzsimulation: (\d+ Pakete.*)$' | Select-Object -Last 1 }
    if ($Hit) { "$Role Netzsimulation: $($Hit.Matches[0].Groups[1].Value)" } else { "$Role Netzsimulation: noch keine Messung" }
}
