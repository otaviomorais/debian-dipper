#!/bin/bash
# Restore only the known files replaced by install-console.sh.
set -euo pipefail
if [ "${1:-}" = --help ]; then echo 'sudo bash scripts/restore-console.sh /var/backups/dipper-console.XXXXXXXX'; exit 0; fi
if [ "$(id -u)" != 0 ] || [ "$#" != 1 ]; then echo 'Informe o backup e execute com sudo.' >&2; exit 2; fi
backup=$(realpath -- "$1")
case "$backup" in /var/backups/dipper-console.*) ;; *) echo 'Backup fora do diretório esperado.' >&2; exit 2 ;; esac
[[ -f "$backup/manifest.txt" ]] || { echo 'Manifesto de backup ausente.' >&2; exit 2; }
# Validate the complete manifest before replacing anything.
while read -r state file extra; do
    [[ "$state" = existed || "$state" = new ]] && [[ -z "${extra:-}" ]] || exit 2
    case "$file" in
        usr/bin/fbkeyboard|usr/local/bin/dipper-console|usr/local/bin/dipper-menu|usr/local/bin/dipper-status|etc/dipper-console/tmux.conf|etc/dipper-console/autostart|etc/profile.d/dipper-console.sh|etc/systemd/system/fbkeyboard.service|etc/systemd/system/fbkeyboard.service.d/fbkeyboard-font.conf) ;;
        *) echo 'Entrada inesperada no manifesto.' >&2; exit 2 ;;
    esac
    if [[ "$state" = existed ]]; then [[ -e "$backup/$file" || -L "$backup/$file" ]] || exit 2; fi
done < "$backup/manifest.txt"
systemctl stop fbkeyboard.service
while read -r state file; do
    if [[ "$state" = existed ]]; then cp -a --remove-destination -- "$backup/$file" "/$file"
    else rm -f -- "/$file"; fi
done < "$backup/manifest.txt"
systemctl daemon-reload
if grep -qx active "$backup/service-state.txt"; then systemctl start fbkeyboard.service; fi
echo 'Arquivos restaurados. Encerre a sessão tmux nova se ela ainda estiver aberta.'
