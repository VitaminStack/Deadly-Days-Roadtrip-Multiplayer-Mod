# Antwort auf den wartenden Spielschritt eines Guided-Test.ps1, das im Hintergrund laeuft. Der Agent ruft es auf,
# nachdem der Mensch im Chat gemeldet hat, wie der Schritt lief. Der Mensch selbst bedient keine Konsole.
#   Guided-Answer.ps1 status             zeigt den wartenden Schritt oder "kein Schritt wartet"
#   Guided-Answer.ps1 ok                 wie erwartet
#   Guided-Answer.ps1 "<Abweichung>"     zaehlt als Fehlschlag, Text kommt in den Bericht
#   Guided-Answer.ps1 skip | quit        ueberspringen oder Test abbrechen
param([Parameter(Mandatory)][string]$Answer)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Common.ps1')
$GuidedDir = Join-Path $script:RepoRoot 'research\local\guided'
$PendingFile = Join-Path $GuidedDir 'pending.txt'
if (-not (Test-Path $PendingFile)) { Write-Output 'kein Schritt wartet'; exit 1 }
$Pending = (Get-Content $PendingFile -Raw -Encoding utf8).Trim()
if ($Answer -eq 'status') { Write-Output "wartet: $Pending"; return }
# Mit q: markiert Guided-Test.ps1 eigene Abbrueche, ein Text darf damit nicht verwechselt werden.
if ($Answer -like 'q:*') { $Answer = " $Answer" }
Set-Content -Path (Join-Path $GuidedDir 'answer.txt') -Value $Answer.Trim() -Encoding utf8
Write-Output "beantwortet: $Pending -> $Answer"
