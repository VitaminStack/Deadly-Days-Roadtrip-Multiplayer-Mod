@echo off
rem Entfernt die Coop-Mod wieder. Logs und ddcoop.ini bleiben, mit -Purge auch weg.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-DDCoop.ps1" -Uninstall %*
if errorlevel 1 (echo. & echo Entfernen abgebrochen, siehe Meldung oben.)
pause
