# Gefuehrter Spieltest: Ein Mensch bedient das Spiel, das Skript fuehrt durch die Schritte und haelt fest, was passiert.
# Der Mensch uebernimmt nur Spielaktionen (Figur laufen lassen, kaempfen, im Spiel handeln). Alles, was sich ueber Skripte
# lesen oder steuern laesst, laeuft als automatischer Schritt, auch die Pruefungen.
#   Guided-Test.ps1 -Steps tools\spike\guided\net-ende.txt [-Start] [-Shots]
# Schrittdatei, eine Zeile je Schritt, # fuer Kommentare:
#   Aktion | Erwartung                     Mensch handelt im Spiel, bestaetigt mit Enter
#   > Overlay.ps1 Host leave               fuehrt ein Skript aus tools\spike aus, ohne Rueckfrage
#   > Overlay.ps1 Client status | Im Spiel  wie oben, wiederholt bis zu -CheckSeconds, bis die Ausgabe zum Muster passt
# Je Schritt: Enter = wie erwartet, Text = Abweichung (zaehlt als Fehlschlag), s = uebersprungen, q = abbrechen.
# Notiert werden Dauer, neue Zeilen der Mod-Logs (Sitzung, Transport, Lobby, Welt, Warnungen, Fehler), Overlay-Zustand,
# neue Abstuerze und mit -Shots Bilder beider Fenster. Bericht unter research\local\guided\<Name>-<Zeit>\report.md.
param(
    [Parameter(Mandatory)][string]$Steps,
    # Startet vorher Start-Session.ps1 -Mod -Tunnel Lobby -Restart.
    [switch]$Start,
    # Nach jedem Schritt ein Bild beider Spielfenster (Game-Input.ps1 shot).
    [switch]$Shots,
    # Wie lange ein automatischer Schritt mit Erwartung wiederholt wird.
    [int]$CheckSeconds = 30
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
if (-not (Test-Path $Steps)) { throw "Schrittdatei fehlt: $Steps" }
$Plan = @(Get-Content $Steps -Encoding utf8 | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') })
if (-not $Plan.Count) { throw "Keine Schritte in $Steps." }
if ($Start) { & (Join-Path $PSScriptRoot 'Start-Session.ps1') -Mod -Tunnel Lobby -Restart | Out-Host }
if (-not (Test-OwnSessionRunning)) { throw 'Keine laufende Sitzung aus diesem Ordner. Mit -Start oder vorher Start-Session.ps1.' }

$Name = [IO.Path]::GetFileNameWithoutExtension($Steps)
$RunDir = Join-Path $script:RepoRoot ("research\local\guided\{0}-{1}" -f $Name, (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
$Logs = Join-Path (Split-Path (Get-GameExe)) 'DDCoop\logs'
$Roles = @(Get-LocalRoles)
function Get-LogCount([string]$Role) {
    $Log = Join-Path $Logs "ddcoop-$(Get-RolePid $Role).log"
    if (Test-Path $Log) { @(Get-Content $Log).Count } else { 0 }
}
# Neue Logzeilen seit dem Stand in $Since, nur die fuer Tests aussagekraeftigen.
function Get-NewLogLines([string]$Role, [int]$Since) {
    $Log = Join-Path $Logs "ddcoop-$(Get-RolePid $Role).log"
    if (-not (Test-Path $Log)) { return @() }
    @(Get-Content $Log -Encoding utf8 | Select-Object -Skip $Since | Where-Object { $_ -match '\[(WARN|ERROR)\]|\] (Sitzung|Transport|Lobby|Welt|Protokoll):' })
}
function Get-StateLine([string]$Role) {
    $ProcessId = Get-RolePid $Role
    if (-not (Test-GameProcess $ProcessId)) { return "$Role beendet" }
    $Net = Get-NetState $ProcessId
    $Overlay = try { $O = Get-OverlayState $ProcessId; "$($O.State), $($O.Message)" } catch { 'Overlay nicht lesbar' }
    "$Role $($Net.world), $($Net.players) Spieler | $Overlay"
}
function Get-CrashLine { (& py (Join-Path $PSScriptRoot 'crashes.py') --session | Select-Object -First 1) -join '' }

$Report = @("# Gefuehrter Test: $Name", '', "Start: $(Get-Date -Format 'dd.MM.yyyy HH:mm'), Schritte aus ``$Steps``", '',
    '| Nr. | Schritt | Erwartung | Ergebnis | Dauer | Zustand danach |', '|---|---|---|---|---|---|')
$Details = @()
$Failed = 0
$Crashes = Get-CrashLine
for ($Index = 0; $Index -lt $Plan.Count; $Index++) {
    $Line = $Plan[$Index]
    $Number = $Index + 1
    $Since = @{}
    foreach ($Role in $Roles) { $Since[$Role] = Get-LogCount $Role }
    $Timer = [Diagnostics.Stopwatch]::StartNew()
    if ($Line.StartsWith('>')) {
        # > Skript.ps1 args | Erwartung: Mit Erwartung (regulaerer Ausdruck) wird bis zu $CheckSeconds s wiederholt, bis die Ausgabe passt.
        $Parts = $Line.Substring(1) -split '\s\|\s', 2
        $Command = $Parts[0].Trim()
        $Action = $Command
        $Expect = if ($Parts.Count -gt 1) { $Parts[1].Trim() } else { '-' }
        $Script = ($Command -split '\s+')[0]
        if ($Script -notmatch '^[\w-]+\.ps1$' -or -not (Test-Path (Join-Path $PSScriptRoot $Script))) {
            $Result = "Skript $Script nicht in tools\spike"; $Failed++
        }
        else {
            Write-Host "[$Number/$($Plan.Count)] automatisch: $Command" -ForegroundColor Cyan
            $Deadline = (Get-Date).AddSeconds($CheckSeconds)
            do {
                $Output = powershell -NoProfile -ExecutionPolicy Bypass -Command "& '$(Join-Path $PSScriptRoot $Script)' $($Command.Substring($Script.Length))" 2>&1 | Out-String
                $Code = $LASTEXITCODE
                $Matched = $Expect -eq '-' -or $Output -match $Expect
                if ($Matched -or $Code) { break }
                Start-Sleep -Seconds 1
            } while ((Get-Date) -lt $Deadline)
            Write-Host $Output.TrimEnd()
            $Result = if ($Code) { $Failed++; "Exitcode $Code" } elseif (-not $Matched) { $Failed++; "Erwartung nicht erfuellt nach $CheckSeconds s" } else { 'ok' }
            Write-Host "    $Result" -ForegroundColor $(if ($Result -eq 'ok') { 'Green' } else { 'Red' })
            $Details += @("### $Number. ``$Command``", '', '```text', $Output.TrimEnd(), '```', '')
        }
    }
    else {
        $Parts = $Line -split '\|', 2
        $Action = $Parts[0].Trim()
        $Expect = if ($Parts.Count -gt 1) { $Parts[1].Trim() } else { '-' }
        Write-Host ''
        Write-Host "[$Number/$($Plan.Count)] $Action" -ForegroundColor Yellow
        if ($Expect -ne '-') { Write-Host "    Erwartet: $Expect" }
        $Answer = Read-Host '    Enter = wie erwartet, Text = Abweichung, s = ueberspringen, q = abbrechen'
        $Result = switch -Regex ($Answer) {
            '^$' { 'ok' }
            '^s$' { 'uebersprungen' }
            '^q$' { 'abgebrochen' }
            default { $Failed++; "Abweichung: $Answer" }
        }
    }
    $Seconds = '{0:n0} s' -f $Timer.Elapsed.TotalSeconds
    $States = @($Roles | ForEach-Object { Get-StateLine $_ })
    $NewLines = @(); foreach ($Role in $Roles) { $NewLines += Get-NewLogLines $Role $Since[$Role] | ForEach-Object { "$Role $_" } }
    if ($Shots) {
        foreach ($Role in $Roles) {
            try { & (Join-Path $PSScriptRoot 'Game-Input.ps1') $Role shot (Join-Path $RunDir ("{0:d2}-{1}.png" -f $Number, $Role.ToLower())) | Out-Null } catch {}
        }
    }
    $Report += "| $Number | $($Action -replace '\|', '/') | $($Expect -replace '\|', '/') | $($Result -replace '\|', '/') | $Seconds | $(($States -join '<br>') -replace '\|', '/') |"
    if ($NewLines.Count) { $Details += @("### $Number. Mod-Logs", '', '```text') + $NewLines + @('```', '') }
    if ($Result -eq 'abgebrochen') { break }
}
$CrashesAfter = Get-CrashLine
$Report += @('', "Fehlschlaege: $Failed. Abstuerze: $(if ($CrashesAfter -eq $Crashes) { 'keine neuen' } else { $CrashesAfter })", '', '## Einzelheiten', '') + $Details
Set-Content -Path (Join-Path $RunDir 'report.md') -Value $Report -Encoding utf8
Write-Host ''
Write-Output "Bericht: $(Join-Path $RunDir 'report.md')"
if ($Failed) { exit 1 }
