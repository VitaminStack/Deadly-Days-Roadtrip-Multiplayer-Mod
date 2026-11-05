# Nur eigene Sitzung: beide F8-Diagnosen messen, danach absichtliche Client-Abweichung mit Ruecksetzung.
param([ValidateRange(10, 3600)][int]$Seconds = 600)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$HostId = Get-RolePid Host
$ClientId = Get-RolePid Client
$ReportDir = Join-Path $script:SessionDir ('desync-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $ReportDir -Force | Out-Null
$Journal = Join-Path $ReportDir 'observations.jsonl'
function Read-Diagnostics {
    $H = (Get-OverlayState $HostId).Diagnostics
    $C = (Get-OverlayState $ClientId).Diagnostics
    @{ at = (Get-Date).ToUniversalTime().ToString('o'); host = $H; client = $C } |
        ConvertTo-Json -Compress | Add-Content -Encoding UTF8 $Journal
    @($H, $C)
}
function Test-BothOk($Texts) {
    $Texts.Count -eq 2 -and @($Texts | Where-Object { $_ -notmatch '^Desync: OK Tag, Phase, Spieler, HP, Inventar\r?\nAusgenommen: Gegner$' }).Count -eq 0
}
Wait-Condition { Test-BothOk (Read-Diagnostics) } 'alle fuenf Diagnosefelder auf beiden Seiten gleich' 60 @($HostId, $ClientId) 1000
Write-Output "Desync: Gleichheitsmessung fuer $Seconds s gestartet"
$Start = Get-Date
$End = $Start.AddSeconds($Seconds)
$Samples = 0
$Comparable = 0
$LastComparable = $Start
$LongestOpen = 0.0
do {
    if (-not (Test-GameProcess $HostId) -or -not (Test-GameProcess $ClientId)) { throw 'Testinstanz beendet' }
    $Texts = Read-Diagnostics
    if (@($Texts | Where-Object { $_ -match '^Desync: ABWEICHUNG' }).Count) { throw "Unerwarteter Alarm in der Gleichheitsmessung: $($Texts -join ' | ')" }
    if (Test-BothOk $Texts) {
        ++$Comparable
        $LastComparable = Get-Date
    } else {
        # Echte Zustandswechsel (Loot, Levelaufstieg, Pause) halten Felder offen, sind aber kein Desync-Alarm.
        $OpenSeconds = ((Get-Date) - $LastComparable).TotalSeconds
        $LongestOpen = [math]::Max($LongestOpen, $OpenSeconds)
        if ($OpenSeconds -gt 60) { throw "Mehr als 60 Sekunden ohne volle Messabdeckung: $($Texts -join ' | ')" }
    }
    ++$Samples
    Start-Sleep -Seconds 1
} while ((Get-Date) -lt $End)
$Elapsed = ((Get-Date) - $Start).TotalSeconds
if ($Comparable -lt 0.9 * $Samples) { throw 'Weniger als 90 Prozent vollstaendig vergleichbare Messungen' }
Wait-Condition { Test-BothOk (Read-Diagnostics) } 'vor Abweichungsprobe wieder alle Felder gleich' 10 @($HostId, $ClientId) 1000
Write-Output "Desync: $Samples Messungen, $([math]::Round($Elapsed, 1)) s ohne Fehlalarm, $Comparable vollstaendig vergleichbar"
$Helper = Join-Path $PSScriptRoot 'desync_difference.py'
$Out = Join-Path $ReportDir 'difference.txt'
$Err = Join-Path $ReportDir 'difference-error.txt'
$Process = Start-Process py -PassThru -WindowStyle Hidden -ArgumentList '-B', "`"$Helper`"", '--hold-seconds', '14' -RedirectStandardOutput $Out -RedirectStandardError $Err
$HostAlert = $false
$ClientAlert = $false
try {
    $Deadline = (Get-Date).AddSeconds(22)
    do {
        $Texts = Read-Diagnostics
        $HostAlert = $HostAlert -or $Texts[0] -match '^Desync: ABWEICHUNG Inventar\r?\n'
        $ClientAlert = $ClientAlert -or $Texts[1] -match '^Desync: ABWEICHUNG Inventar\r?\n'
        Start-Sleep -Milliseconds 500
    } while (-not $Process.HasExited -and (Get-Date) -lt $Deadline)
} finally {
    # Nicht abschiessen: finally im Helfer muss den Originalwert zuruecksetzen koennen.
    if (-not $Process.WaitForExit(25000)) { throw 'Abweichungshelfer noch aktiv; Originalwert-Ruecksetzung unbekannt' }
}
if ($Process.ExitCode -ne 0) { throw "Abweichungshelfer fehlgeschlagen: $(Get-Content $Err -Raw)" }
if (-not $HostAlert -or -not $ClientAlert) { throw 'Absichtliche Inventarabweichung nicht auf beiden Seiten bestaetigt' }
Wait-Condition { Test-BothOk (Read-Diagnostics) } 'Diagnose nach Ruecksetzung wieder OK' 30 @($HostId, $ClientId) 1000
@{ seconds = $Elapsed; samples = $Samples; comparable = $Comparable; longestOpenSeconds = $LongestOpen; hostAlert = $HostAlert; clientAlert = $ClientAlert; recovered = $true } |
    ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $ReportDir 'result.json')
Write-Output "Desync: Inventarabweichung auf beiden Seiten erkannt und nach Ruecksetzung wieder OK. Bericht: $ReportDir"
