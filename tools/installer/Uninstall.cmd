@echo off
rem Removes the co-op mod again. Logs and ddcoop.ini stay; with -Purge they go too.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-DDCoop.ps1" -Uninstall %*
if errorlevel 1 (echo. & echo Removal aborted, see message above.)
pause
