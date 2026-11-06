@echo off
rem Installiert die Coop-Mod. Doppelklick genuegt, Bypass gilt nur fuer diesen einen Aufruf.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-DDCoop.ps1" %*
if errorlevel 1 (echo. & echo Installation abgebrochen, siehe Meldung oben.)
pause
