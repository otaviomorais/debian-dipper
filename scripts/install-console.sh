#!/bin/bash
# Native installer for an already booted Debian Mi 8. No flash or partition writes.
set -euo pipefail
packages=1
restart=1
autostart=0
for arg in "$@"; do
    case "$arg" in
        --no-packages) packages=0 ;;
        --no-restart) restart=0 ;;
        --autostart) autostart=1 ;;
        -h|--help)
            echo 'sudo bash scripts/install-console.sh [--autostart] [--no-packages] [--no-restart]'
            echo 'Executar no Mi 8 após iniciar o Debian. Preserva backup dos arquivos substituídos.'
            exit 0 ;;
        *) echo "Opção desconhecida: $arg" >&2; exit 2 ;;
    esac
done
if [ "$(id -u)" != 0 ]; then echo 'Execute com sudo no Mi 8.' >&2; exit 1; fi
if [ "$(uname -m)" != aarch64 ]; then
    echo 'Este instalador compila nativamente no Mi 8/aarch64, não no computador.' >&2
    exit 1
fi
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if (( packages )); then
    mapfile -t runtime_packages < <(sed '/^[[:space:]]*#/d; /^[[:space:]]*$/d' "$project/config/console-packages.list")
    mapfile -t build_packages < <(sed '/^[[:space:]]*#/d; /^[[:space:]]*$/d' "$project/config/console-build-packages.list")
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        "${runtime_packages[@]}" "${build_packages[@]}"
fi
make -C "$project/fbkeyboard" clean
make -C "$project/fbkeyboard" -j2
make -C "$project/fbkeyboard" check
mkdir -p /var/backups
backup=$(mktemp -d /var/backups/dipper-console.XXXXXXXX)
files=(usr/bin/fbkeyboard usr/local/bin/dipper-console usr/local/bin/dipper-menu \
       usr/local/bin/dipper-status etc/dipper-console/tmux.conf \
       etc/dipper-console/autostart etc/profile.d/dipper-console.sh \
       etc/systemd/system/fbkeyboard.service \
       etc/systemd/system/fbkeyboard.service.d/fbkeyboard-font.conf)
for file in "${files[@]}"; do
    if [ -e "/$file" ] || [ -L "/$file" ]; then
        mkdir -p "$backup/$(dirname "$file")"
        cp -a -- "/$file" "$backup/$file"
        printf 'existed %s\n' "$file" >> "$backup/manifest.txt"
    else
        printf 'new %s\n' "$file" >> "$backup/manifest.txt"
    fi
done
systemctl is-active fbkeyboard.service > "$backup/service-state.txt" || true
# Restore replaced files if installation fails after this point.
trap 'echo "Falha na instalação; restaurando $backup" >&2; bash "$project/scripts/restore-console.sh" "$backup" || true' ERR
systemctl stop fbkeyboard.service
make -C "$project/fbkeyboard" install
for program in dipper-console dipper-menu dipper-status; do
    install -Dm755 "$project/rootfs/usr/local/bin/$program" "/usr/local/bin/$program"
done
install -Dm644 "$project/rootfs/etc/dipper-console/tmux.conf" /etc/dipper-console/tmux.conf
install -Dm644 "$project/rootfs/etc/profile.d/dipper-console.sh" /etc/profile.d/dipper-console.sh
install -Dm644 "$project/rootfs/etc/systemd/system/fbkeyboard.service" /etc/systemd/system/fbkeyboard.service
install -Dm644 "$project/rootfs/etc/systemd/system/fbkeyboard.service.d/fbkeyboard-font.conf" \
    /etc/systemd/system/fbkeyboard.service.d/fbkeyboard-font.conf
if (( autostart )); then touch /etc/dipper-console/autostart; fi
systemctl daemon-reload
if (( restart )); then systemctl restart fbkeyboard.service; fi
trap - ERR
printf '\nInstalado. Backup: %s\n' "$backup"
echo 'Como usuário normal, execute: dipper-console'
echo 'Menu/F1: configurações; New/F2: terminal; setas sempre na última linha.'
echo 'Para desfazer: sudo bash scripts/restore-console.sh CAMINHO_DO_BACKUP'
