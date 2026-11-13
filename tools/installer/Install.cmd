@echo off
rem Installs the co-op mod. Double-click is enough; the bypass applies to this one call only.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-DDCoop.ps1" %*
if errorlevel 1 (echo. & echo Installation aborted, see message above.)
pause
