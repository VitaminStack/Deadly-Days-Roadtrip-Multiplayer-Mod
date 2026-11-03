# Baut Dumper-7 im getesteten Stand fuer UE 5.8 aus dem Submodul third_party\Dumper-7.
# Das Submodul zeigt auf den Fork mit dem FFieldClass-Fix. Gebaut wird eine Kopie in research\local\dumper7,
# damit die Einstellungen unten das Submodul nicht veraendern.
param(
    # Wohin Dumper-7 das SDK schreibt. Standard ist der von Dumper-7.
    [string]$OutputDir = 'C:/Dumper-7'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\spike\Common.ps1')
Initialize-SessionDir

$Submodule = Join-Path $script:RepoRoot 'third_party\Dumper-7'
$Source = Join-Path $script:RepoRoot 'research\local\dumper7'
$OutputDir = $OutputDir.Replace('\', '/').TrimEnd('/')

# Holt das Submodul nach, wenn ohne --recurse-submodules geklont wurde oder in einem neuen Worktree.
git -C $script:RepoRoot submodule update --init third_party/Dumper-7
if ($LASTEXITCODE -ne 0) { throw 'Submodul third_party/Dumper-7 konnte nicht geladen werden.' }

# Immer frisch aus dem Submodul, damit alle denselben Stand bauen.
if (Test-Path $Source) { Remove-Item -Recurse -Force $Source }
Copy-Item -Recurse (Join-Path $Submodule 'Dumper') (Join-Path $Source 'Dumper')

# Ausgabeordner und automatisches Entladen nach dem Dump, sonst wartet Dumper-7 auf F6.
$Settings = Join-Path $Source 'Dumper\Settings.h'
$Text = [IO.File]::ReadAllText($Settings).Replace('C:/Dumper-7', $OutputDir).Replace(
    'inline bool bShouldAutoUnload = false;', 'inline bool bShouldAutoUnload = true;')
[IO.File]::WriteAllText($Settings, $Text)
$SettingsCpp = Join-Path $Source 'Dumper\Settings.cpp'
[IO.File]::WriteAllText($SettingsCpp, [IO.File]::ReadAllText($SettingsCpp).Replace('C:/Dumper-7', $OutputDir))

$Project = Join-Path $Source 'Dumper\Dumper.vcxproj'
$Out = Join-Path $Source 'out'
# Doppelter Backslash, sonst maskiert er das schliessende Anfuehrungszeichen.
Invoke-InVsEnvironment "msbuild `"$Project`" /p:Configuration=Release /p:Platform=x64 /p:OutDir=`"$Out\\`" /m /v:minimal /nologo" |
    Where-Object { $_ -match '\berror\b' }
if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen.' }
Join-Path $Out 'Dumper-7.dll'
