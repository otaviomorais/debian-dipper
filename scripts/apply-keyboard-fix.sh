#!/bin/sh
# Update the installed factory console over ADB, without flashing partitions.
set -eu
[ "$(id -u)" = 0 ] || { echo 'Execute como root pelo ADB ou sudo.' >&2; exit 1; }
[ "$(uname -m)" = aarch64 ] || { echo 'Este pacote exige o Mi 8/ARM64.' >&2; exit 1; }
payload=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
[ -f /etc/dipper-console/tmux.conf ] && [ -f /usr/bin/fbkeyboard ] || {
    echo 'O Dipper Console original precisa estar instalado.' >&2; exit 1;
}
cd "$payload"
sha256sum -c SHA256SUMS
for file in fbkeyboard-arm64 tmux.conf safe-area.conf issue; do
    [ -f "$file" ] || { echo "Arquivo ausente no pacote: $file" >&2; exit 1; }
done
mkdir -p /var/backups
backup=$(mktemp -d /var/backups/dipper-keyboard.XXXXXXXX)
files='usr/bin/fbkeyboard etc/dipper-console/tmux.conf etc/dipper-console/safe-area.conf etc/issue'
for file in $files; do
    if [ -e "/$file" ] || [ -L "/$file" ]; then
        mkdir -p "$backup/$(dirname "$file")"
        cp -a "/$file" "$backup/$file"
        printf 'existed %s\n' "$file" >> "$backup/manifest.txt"
    else
        printf 'new %s\n' "$file" >> "$backup/manifest.txt"
    fi
done
systemctl status fbkeyboard --no-pager -l > "$backup/status-before.txt" 2>&1 || true
journalctl -b -u fbkeyboard --no-pager -n 40 > "$backup/journal-before.txt" 2>&1 || true
echo "Backup dos arquivos anteriores: $backup"
staged=$(mktemp /usr/bin/.fbkeyboard-fix.XXXXXXXX)
changed=0
cleanup() {
    result=$?
    trap - EXIT HUP INT TERM
    if [ "$result" != 0 ] && [ "$changed" = 1 ]; then
        echo 'Falha na atualizacao; restaurando os arquivos anteriores.' >&2
        systemctl stop fbkeyboard.service || true
        while read -r state file; do
            if [ "$state" = existed ]; then
                restore=$(mktemp "/$file.restore.XXXXXXXX")
                cp -a "$backup/$file" "$restore"
                mv -f "$restore" "/$file"
            else
                rm -f "/$file"
            fi
        done < "$backup/manifest.txt"
        systemctl start fbkeyboard.service || true
    fi
    [ -z "$staged" ] || rm -f "$staged"
    exit "$result"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
install -m755 ./fbkeyboard-arm64 "$staged"
"$staged" -h
systemctl stop fbkeyboard.service
changed=1
mv -f "$staged" /usr/bin/fbkeyboard
staged=
for file in etc/dipper-console/tmux.conf etc/dipper-console/safe-area.conf etc/issue; do
    mkdir -p "/$(dirname "$file")"
    staged=$(mktemp "/$file.update.XXXXXXXX")
    install -m644 "$payload/$(basename "$file")" "$staged"
    mv -f "$staged" "/$file"
    staged=
done
systemctl reset-failed fbkeyboard.service
systemctl start fbkeyboard.service
sleep 1
systemctl is-active --quiet fbkeyboard.service
# Reload existing factory tmux servers without closing any terminals.
for socket in /tmp/tmux-*/dipper-console; do
    [ -S "$socket" ] || continue
    owner=$(stat -c %u "$socket")
    username=$(getent passwd "$owner" | cut -d: -f1)
    [ -n "$username" ] || continue
    runuser -u "$username" -- tmux -S "$socket" source-file /etc/dipper-console/tmux.conf ||
        echo 'Se a margem nao mudar, execute na sessao: tmux source-file /etc/dipper-console/tmux.conf'
done
systemctl status fbkeyboard --no-pager -l
journalctl -b -u fbkeyboard --no-pager -n 15
echo 'DIPPER_KEYBOARD_FIX_OK'
echo 'Atualizacao aplicada. Confira o teclado e a margem superior no aparelho.'
echo 'O prompt de login usara a nova margem no proximo login/inicio do getty.'
echo 'Nenhuma particao foi formatada; os dados e os terminais abertos foram preservados.'
