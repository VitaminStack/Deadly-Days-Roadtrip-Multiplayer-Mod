@echo off
setlocal
rem One-click installation of the locally compiled Release mod into the Steam game.
rem Uses the existing build and validated installer. No GitHub release download.
set "TOOLS=%~dp0"
if not exist "%TOOLS%Build-Mod.ps1" (
    echo Bitte Install-Release.cmd in den tools-Ordner des Projekts kopieren.
    set "RESULT=2"
    goto failed
)

echo Baue die aktuelle DDCoop-Mod als Release ...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%TOOLS%Build-Mod.ps1" -Configuration Release
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto failed

echo.
echo Installiere den Release-Build in die Steam-Installation ...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%TOOLS%installer\Install-DDCoop.ps1" -Source "%TOOLS%..\bin\Release" %*
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto failed

echo.
echo DDCoop Release wurde installiert. Das Spiel jetzt normal ueber Steam starten.
echo Entfernen: tools\installer\Uninstall.cmd doppelklicken.
pause
exit /b 0

:failed
echo.
echo Build oder Installation fehlgeschlagen. Bitte die Meldungen oben pruefen.
pause
exit /b %RESULT%
