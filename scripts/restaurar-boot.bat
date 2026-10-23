@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0haptics-windows.ps1" -Mode Restore > "diagnostico-vibracao-restaurar.txt" 2>&1
set "DIPPER_RESULT=%ERRORLEVEL%"
type "diagnostico-vibracao-restaurar.txt"
echo.
pause
exit /b %DIPPER_RESULT%
