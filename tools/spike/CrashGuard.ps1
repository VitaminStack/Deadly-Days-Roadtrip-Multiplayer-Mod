# Beendet den CrashReportClient des Spiels sofort, damit Testabstuerze nicht bei Pixelsplit landen.
# Laeuft weiter, bis das Spiel seit einer Minute nicht mehr laeuft.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$GamePrefix = (Get-GameDir).TrimEnd('\', '/') + '\'
$IdleTicks = 0
while ($IdleTicks -lt 600) {
    Get-Process CrashReportClient* -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and $_.Path.StartsWith($GamePrefix, [StringComparison]::OrdinalIgnoreCase) } |
        Stop-Process -Force -ErrorAction SilentlyContinue
    if (Get-Process DDSurvivors-Win64-Shipping -ErrorAction SilentlyContinue) { $IdleTicks = 0 } else { $IdleTicks++ }
    Start-Sleep -Milliseconds 100
}
