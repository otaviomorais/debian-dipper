#!/bin/sh
# Apply a prebuilt keyboard to the installed factory console, without flashing.
set -eu
[ "$(id -u)" = 0 ] || { echo 'Execute como root pelo ADB ou sudo.' >&2; exit 1; }
[ "$(uname -m)" = aarch64 ] || { echo 'Este pacote exige o Mi 8/ARM64.' >&2; exit 1; }
payload=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
[ -f /etc/dipper-console/tmux.conf ] && [ -f /usr/bin/fbkeyboard ] || {
    echo 'O Dipper Console original precisa estar instalado.' >&2; exit 1;
}
cd "$payload"
sha256sum -c SHA256SUMS
mkdir -p /var/backups
backup=$(mktemp -d /var/backups/dipper-keyboard.XXXXXXXX)
cp -a /usr/bin/fbkeyboard "$backup/fbkeyboard"
systemctl status fbkeyboard --no-pager -l > "$backup/status-before.txt" 2>&1 || true
journalctl -b -u fbkeyboard --no-pager -n 40 > "$backup/journal-before.txt" 2>&1 || true
echo "Backup do teclado anterior: $backup/fbkeyboard"
staged=$(mktemp /usr/bin/.fbkeyboard-fix.XXXXXXXX)
changed=0
cleanup() {
    result=$?
    trap - EXIT HUP INT TERM
    if [ "$result" != 0 ] && [ "$changed" = 1 ]; then
        echo 'Falha ao iniciar; restaurando o teclado anterior.' >&2
        systemctl stop fbkeyboard.service || true
        cp -a "$backup/fbkeyboard" "$staged"
        mv -f "$staged" /usr/bin/fbkeyboard
        systemctl start fbkeyboard.service || true
    fi
    rm -f "$staged"
    exit "$result"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
install -m755 ./fbkeyboard-arm64 "$staged"
"$staged" -h
systemctl stop fbkeyboard.service
changed=1
mv -f "$staged" /usr/bin/fbkeyboard
systemctl reset-failed fbkeyboard.service
systemctl start fbkeyboard.service
sleep 1
systemctl is-active --quiet fbkeyboard.service
systemctl status fbkeyboard --no-pager -l
journalctl -b -u fbkeyboard --no-pager -n 15
echo 'DIPPER_KEYBOARD_FIX_OK'
echo 'Atualizacao aplicada. Confira se o teclado apareceu no aparelho.'
echo 'Nenhuma particao foi formatada; os dados e as configuracoes foram preservados.'
