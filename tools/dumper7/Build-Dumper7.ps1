# Baut Dumper-7 im getesteten Stand fuer UE 5.8: upstream-Commit 2c1eae3 plus ue58-fieldclass.patch.
# Der Quellcode kommt von GitHub nach research\local\dumper7, im Repo liegt nur der Patch (Dumper-7 hat keine Lizenz).
param(
    # Wohin Dumper-7 das SDK schreibt. Standard ist der von Dumper-7.
    [string]$OutputDir = 'C:/Dumper-7'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\spike\Common.ps1')
Initialize-SessionDir

$Upstream = 'https://github.com/Encryqed/Dumper-7.git'
$Commit = '2c1eae303f3d775f6f59aee6e95990d9ce2e1966'
$Source = Join-Path $script:RepoRoot 'research\local\dumper7'
$Patch = Join-Path $PSScriptRoot 'ue58-fieldclass.patch'
$OutputDir = $OutputDir.Replace('\', '/').TrimEnd('/')

# Immer frisch aus dem festen Commit, damit alle denselben Stand bauen.
if (Test-Path $Source) { Remove-Item -Recurse -Force $Source }
git -c core.autocrlf=false clone --quiet $Upstream $Source
if ($LASTEXITCODE -ne 0) { throw 'Klonen von Dumper-7 fehlgeschlagen.' }
git -C $Source -c advice.detachedHead=false checkout --quiet $Commit
if ($LASTEXITCODE -ne 0) { throw "Commit $Commit nicht gefunden." }
git -C $Source apply $Patch
if ($LASTEXITCODE -ne 0) { throw 'Patch passt nicht auf den Commit.' }

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
