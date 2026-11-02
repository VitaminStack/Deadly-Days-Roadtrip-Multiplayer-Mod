# Beendet den CrashReportClient des Spiels sofort, damit Testabstuerze nicht bei Pixelsplit landen.
# Laeuft weiter, bis das Spiel seit einer Minute nicht mehr laeuft.
$IdleTicks = 0
while ($IdleTicks -lt 600) {
    Get-Process CrashReportClient* -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -like '*Deadly Days Roadtrip*' } |
        Stop-Process -Force -ErrorAction SilentlyContinue
    if (Get-Process DDSurvivors-Win64-Shipping -ErrorAction SilentlyContinue) { $IdleTicks = 0 } else { $IdleTicks++ }
    Start-Sleep -Milliseconds 100
}
