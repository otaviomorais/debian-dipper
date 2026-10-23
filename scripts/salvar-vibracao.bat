@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0haptics-windows.ps1" -Mode Save > "diagnostico-vibracao-salvar.txt" 2>&1
set "DIPPER_RESULT=%ERRORLEVEL%"
type "diagnostico-vibracao-salvar.txt"
echo.
pause
exit /b %DIPPER_RESULT%
