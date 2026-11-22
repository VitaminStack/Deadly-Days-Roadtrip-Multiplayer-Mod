# live-protokoll: Sammelt nach einem Live-Test alles, was die Auswertung braucht, in einen Ordner und ein Zip unter research\local\livetest.
# Mod-Logs der letzten Stunden, Absturzberichte der Testinstanzen (Saved\Crashes) und Absturzereignisse ohne Bericht (Windows-Ereignis 1000).
# Jeder Tester ruft es auf seinem PC auf und schickt das Zip. Nichts davon ins Repo: Logs und Berichte koennen IDs und Pfade enthalten.
#   Collect-Logs.ps1              die letzten 12 Stunden
#   Collect-Logs.ps1 -Hours 3     kuerzerer Zeitraum
param([int]$Hours = 12)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$Since = (Get-Date).AddHours(-$Hours)
$Target = Join-Path $script:RepoRoot ("research\local\livetest\" + (Get-Date -Format 'yyyyMMdd-HHmm'))
New-Item -ItemType Directory -Force -Path (Join-Path $Target 'logs'), (Join-Path $Target 'crashes') | Out-Null

$LogDir = Join-Path (Split-Path (Get-GameExe)) 'DDCoop\logs'
$Logs = @(Get-ChildItem -LiteralPath $LogDir -Filter 'ddcoop-*.log' -ErrorAction SilentlyContinue | Where-Object LastWriteTime -ge $Since)
$Logs | Copy-Item -Destination (Join-Path $Target 'logs')

$Crashes = @(Get-ChildItem -Path (Join-Path $script:SessionDir 'ud_*\Saved\Crashes\*') -Directory -ErrorAction SilentlyContinue |
    Where-Object LastWriteTime -ge $Since)
foreach ($Crash in $Crashes) {
    # ...\ud_<rolle>\Saved\Crashes\<bericht>
    $Role = (Split-Path -Leaf $Crash.Parent.Parent.Parent.FullName) -replace '^ud_', ''
    Copy-Item -LiteralPath $Crash.FullName -Destination (Join-Path $Target "crashes\$Role-$($Crash.Name)") -Recurse
}

# Manche Abstuerze legen keinen Bericht an; Adresse und Modul stehen dann nur im Ereignisprotokoll.
$Events = @(Get-WinEvent -FilterHashtable @{ LogName = 'Application'; Id = 1000; StartTime = $Since } -ErrorAction SilentlyContinue |
    Where-Object { $_.Message -match 'DDSurvivors' })
if ($Events) { $Events | Format-List TimeCreated, Message | Out-File -Encoding utf8 (Join-Path $Target 'crash-events.txt') }

$Zip = "$Target.zip"
Compress-Archive -Path (Join-Path $Target '*') -DestinationPath $Zip -Force
"{0} Log(s), {1} Absturzbericht(e), {2} Absturzereignis(se) seit {3:HH:mm}" -f $Logs.Count, $Crashes.Count, $Events.Count, $Since
"Zip: research\local\livetest\$(Split-Path $Zip -Leaf)"
