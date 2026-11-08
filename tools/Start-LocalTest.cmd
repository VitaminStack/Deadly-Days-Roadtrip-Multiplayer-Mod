@echo off
setlocal
rem One-click local multiplayer test: two game instances, one Steam lobby.
rem Uses the existing session script (including its lock and crash guard).
set "TOOLS=%~dp0"
if not exist "%TOOLS%spike\Start-Session.ps1" (
    echo Bitte Start-LocalTest.cmd in den tools-Ordner des Projekts kopieren.
    set "RESULT=2"
    goto failed
)

echo Starte lokalen DDCoop-Test mit Host und Client ...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%TOOLS%spike\Start-Session.ps1" -Role Both -Mod -Tunnel Lobby -NoSpike -Brief %*
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto failed

echo.
echo Host und Client sind gestartet und automatisch verbunden.
echo Spiele jetzt in den beiden Fenstern.
echo Danach J druecken, um beide Testinstanzen sicher zu beenden.
echo N schliesst nur diesen Starter; die Testinstanzen laufen weiter.
choice /C JN /N /M "Nach dem Test: J = stoppen, N = weiterlaufen lassen [J/N]: "
if errorlevel 2 exit /b 0

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%TOOLS%spike\Stop-Session.ps1"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto failed
echo.
echo Stop-Session ausgefuehrt. Hinweise zur Mod-Entfernung stehen oben.
pause
exit /b 0

:failed
echo.
echo Vorgang fehlgeschlagen. Bitte die Meldungen oben pruefen.
pause
exit /b %RESULT%
