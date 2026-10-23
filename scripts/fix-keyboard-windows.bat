@echo off
setlocal
cd /d "%~dp0"
set "DIPPER_ADB=%~dp0tools\windows\adb.exe"
if not exist "%DIPPER_ADB%" (
  echo Extraia todo o ZIP antes de executar este arquivo.
  pause
  exit /b 1
)
echo Conecte o Mi 8 ligado na tela de login, pelo cabo USB.
echo Esta atualizacao nao formata o aparelho.
"%DIPPER_ADB%" get-state >nul 2>&1
if errorlevel 1 (
  "%DIPPER_ADB%" devices
  echo Nao foi possivel acessar um unico aparelho pelo ADB.
  echo Confira o cabo e o driver ADB. O aparelho deve estar no Debian, fora do fastboot.
  pause
  exit /b 1
)
"%DIPPER_ADB%" shell "systemctl status fbkeyboard --no-pager -l; journalctl -b -u fbkeyboard --no-pager -n 40" > diagnostico-antes.txt 2>&1
"%DIPPER_ADB%" shell "mkdir -p /tmp/dipper-keyboard-fix-v0.1.3"
if errorlevel 1 goto failed
for %%F in (fbkeyboard-arm64 SHA256SUMS apply-keyboard-fix.sh tmux.conf safe-area.conf issue fbkeyboard.service) do (
  "%DIPPER_ADB%" push "payload\%%F" /tmp/dipper-keyboard-fix-v0.1.3/
  if errorlevel 1 goto failed
)
"%DIPPER_ADB%" shell "chmod 755 /tmp/dipper-keyboard-fix-v0.1.3/fbkeyboard-arm64; sh /tmp/dipper-keyboard-fix-v0.1.3/apply-keyboard-fix.sh" > diagnostico-correcao.txt 2>&1
type diagnostico-correcao.txt
findstr /x /c:"DIPPER_KEYBOARD_FIX_OK" diagnostico-correcao.txt >nul
if errorlevel 1 goto failed
echo.
echo Atualizacao aplicada. Confira a tela do aparelho; nao precisa reiniciar.
echo Se o teclado ainda nao aparecer, envie diagnostico-antes.txt e diagnostico-correcao.txt.
pause
exit /b 0
:failed
echo.
echo A atualizacao nao foi concluida. Envie os arquivos diagnostico-antes.txt e diagnostico-correcao.txt, se existirem.
pause
exit /b 1
