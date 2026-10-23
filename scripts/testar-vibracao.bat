@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0haptics-windows.ps1" -Mode Test > "diagnostico-vibracao-teste.txt" 2>&1
set "DIPPER_RESULT=%ERRORLEVEL%"
type "diagnostico-vibracao-teste.txt"
echo.
pause
exit /b %DIPPER_RESULT%
