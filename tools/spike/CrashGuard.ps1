# Beendet den CrashReportClient des Spiels sofort, damit Testabstuerze nicht bei Pixelsplit landen.
# Laeuft weiter, bis das Spiel seit einer Minute nicht mehr laeuft. Vor dem ersten Spielstart wartet er bis zu StartupGraceSeconds,
# beim Start ueber Steam (Start-Session.ps1 -SteamLaunch) so lange, bis jemand die Einladung annimmt.
param([int]$StartupGraceSeconds = 60)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$GamePrefix = (Get-GameDir).TrimEnd('\', '/') + '\'
$IdleTicks = 0
$IdleLimit = [Math]::Max(600, $StartupGraceSeconds * 10)
while ($IdleTicks -lt $IdleLimit) {
    Get-Process CrashReportClient* -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and $_.Path.StartsWith($GamePrefix, [StringComparison]::OrdinalIgnoreCase) } |
        Stop-Process -Force -ErrorAction SilentlyContinue
    if (Get-Process DDSurvivors-Win64-Shipping -ErrorAction SilentlyContinue) { $IdleTicks = 0; $IdleLimit = 600 } else { $IdleTicks++ }
    Start-Sleep -Milliseconds 100
}
