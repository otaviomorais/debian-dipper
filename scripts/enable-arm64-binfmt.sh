#!/bin/bash
# Use the distribution's QEMU registration, pinned outside a future chroot (F).
set -euo pipefail
if (( EUID != 0 )); then echo 'Run as root on the Linux build host.' >&2; exit 1; fi
directory=/proc/sys/fs/binfmt_misc
if [[ ! -e "$directory/status" ]]; then mount -t binfmt_misc binfmt_misc "$directory"; fi
if [[ ! -e "$directory/qemu-aarch64" ]]; then
    if [[ -r /usr/share/qemu/binfmt.d/qemu-aarch64.conf ]]; then
        cat /usr/share/qemu/binfmt.d/qemu-aarch64.conf > "$directory/register"
    elif [[ -r /usr/lib/binfmt.d/qemu-aarch64.conf ]]; then
        cat /usr/lib/binfmt.d/qemu-aarch64.conf > "$directory/register"
    else
        update-binfmts --enable qemu-aarch64
    fi
fi
grep -q '^enabled$' "$directory/qemu-aarch64"
grep -q '^flags: .*F' "$directory/qemu-aarch64" || {
    echo 'QEMU registration lacks the F flag required for chroot. Check the installed QEMU package.' >&2
    exit 1
}
echo 'ARM64 binfmt registration is enabled for chroot builds.'
