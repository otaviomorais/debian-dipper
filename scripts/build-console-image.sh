#!/bin/bash
# Build a new console image from an existing, compatible Debian dipper image.
# Never mounts a block device or writes to a connected phone.
set -euo pipefail
export LC_ALL=C
if [[ -z "${http_proxy:-}" && -n "${HTTP_PROXY:-}" ]]; then export http_proxy=$HTTP_PROXY; fi
if [[ -z "${https_proxy:-}" && -n "${HTTPS_PROXY:-}" ]]; then export https_proxy=$HTTPS_PROXY; fi
base= boot= output=
max_mib=2150
usage() {
    echo 'sudo bash scripts/build-console-image.sh --base ROOTFS.img --boot BOOT.img --output NEW_DIRECTORY [--max-size-mib 2150]'
    echo 'ROOTFS may be raw ext4 or Android sparse. The matching boot image is preserved.'
}
while (( $# )); do
    case "$1" in
        --base|--boot|--output|--max-size-mib)
            (( $# >= 2 )) || { usage >&2; exit 2; }
            case "$1" in --base) base=$2 ;; --boot) boot=$2 ;; --output) output=$2 ;; --max-size-mib) max_mib=$2 ;; esac
            shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done
die() { echo "image: $*" >&2; exit 1; }
[[ -n "$base" && -n "$boot" && -n "$output" ]] || { usage >&2; exit 2; }
[[ "$max_mib" =~ ^[1-9][0-9]{2,4}$ ]] && (( max_mib >= 512 && max_mib <= 65536 )) || die 'Invalid image size limit'
[[ -f "$base" && ! -b "$base" && -f "$boot" && ! -b "$boot" ]] || die 'Provide regular input image files'
[[ ! -e "$output" && ! -L "$output" ]] || die 'Output directory already exists; choose a new one'
(( EUID == 0 )) || die 'Run as root on a Linux image-build host (not on the phone)'
for command in python3 mount umount mountpoint chroot e2fsck resize2fs dumpe2fs mke2fs truncate sha256sum md5sum realpath comm; do
    command -v "$command" >/dev/null || die "Missing build host tool: $command"
done
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
base=$(realpath -- "$base")
boot=$(realpath -- "$boot")
python3 - "$boot" <<'PY'
import sys
with open(sys.argv[1], 'rb') as stream:
    if stream.read(8) != b'ANDROID!':
        raise SystemExit('Not an Android boot image')
PY
mkdir -p -- "$(dirname -- "$output")"
mkdir -- "$output"
output=$(realpath -- "$output")
work=$(mktemp -d "$output/.build.XXXXXXXX")
root=$work/root
raw=$work/rootfs.raw.img
mkdir "$root" "$work/saved"
temporary=()
source_dir=
restore_temporary() {
    local relative
    for relative in "${temporary[@]}"; do
        rm -f -- "$root/$relative"
        if [[ -e "$work/saved/$relative" || -L "$work/saved/$relative" ]]; then
            cp -a -- "$work/saved/$relative" "$root/$relative"
        fi
    done
    temporary=()
}
cleanup() {
    local status=$?
    trap - EXIT
    set +e
    if mountpoint -q "$root"; then
        restore_temporary
        if [[ -n "$source_dir" ]]; then rm -rf -- "$source_dir"; fi
        if mountpoint -q "$root/proc"; then umount "$root/proc"; fi
        if mountpoint -q "$root/dev"; then umount -R "$root/dev"; fi
        umount "$root"
    fi
    if mountpoint -q "$root"; then
        echo "image: mount still active; retained $work for cleanup" >&2
        status=1
    elif (( status == 0 )); then
        rm -rf -- "$work"
    else
        echo "image: failed build retained in $work; input images were not modified" >&2
    fi
    exit "$status"
}
trap cleanup EXIT
kind=$(python3 - "$base" <<'PY'
import sys
with open(sys.argv[1], 'rb') as stream:
    magic = stream.read(4)
    stream.seek(1080)
    ext4 = stream.read(2)
if magic == bytes.fromhex('3aff26ed'):
    print('sparse')
elif ext4 == bytes.fromhex('53ef'):
    print('raw')
else:
    raise SystemExit('Base image is neither Android sparse nor raw ext4')
PY
)
if [[ "$kind" = sparse ]]; then
    command -v simg2img >/dev/null || die 'Install simg2img on the build host'
    simg2img "$base" "$raw"
else
    cp --reflink=auto --sparse=always -- "$base" "$raw"
fi
fsck_image() {
    local result=0
    e2fsck -pf "$raw" || result=$?
    (( result == 0 || result == 1 )) || die "Filesystem check failed: $result"
}
fsck_image
# Temporary headroom for native toolchain installation. Final output is shrunk.
truncate -s +1024M "$raw"
resize2fs "$raw"
mount -o loop,nodev,nosuid "$raw" "$root"
python3 - "$root" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1]).resolve()
for relative in ('etc', 'usr/sbin', 'etc/ssl/certs', 'tmp', 'dev', 'proc'):
    path = root / relative
    if not path.resolve().is_relative_to(root):
        raise SystemExit(f'Unsafe rootfs path: {relative}')
    path.mkdir(parents=True, exist_ok=True)
release = (root / 'etc/os-release').read_text()
if 'ID=debian' not in release or 'VERSION_ID="13"' not in release:
    raise SystemExit('Use a compatible Debian 13 dipper base image')
PY
chroot "$root" /bin/true || die 'Cannot execute target ARM64 binaries; configure qemu-user-static/binfmt on an x86_64 build host'
[[ "$(chroot "$root" dpkg --print-architecture)" = arm64 ]] || die 'The base rootfs is not arm64'
mount --rbind /dev "$root/dev"
mount --make-rslave "$root/dev"
mount -t proc proc "$root/proc"
replace_temporary() {
    local relative=$1 source=$2
    mkdir -p "$work/saved/$(dirname "$relative")"
    if [[ -e "$root/$relative" || -L "$root/$relative" ]]; then
        cp -a -- "$root/$relative" "$work/saved/$relative"
    fi
    temporary+=("$relative")
    rm -f -- "$root/$relative"
    cp -- "$source" "$root/$relative"
}
# Avoid starting services in the chroot. Restore originals before packaging.
printf '#!/bin/sh\nexit 101\n' > "$work/policy-rc.d"
replace_temporary usr/sbin/policy-rc.d "$work/policy-rc.d"
chmod 755 "$root/usr/sbin/policy-rc.d"
replace_temporary etc/resolv.conf /etc/resolv.conf
# Preserve build-session HTTPS trust, including a configured egress proxy CA.
if [[ -r /etc/ssl/certs/ca-certificates.crt ]]; then
    replace_temporary etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt
fi
mapfile -t runtime_packages < <(sed '/^[[:space:]]*#/d; /^[[:space:]]*$/d' "$project/config/console-packages.list")
mapfile -t build_packages < <(sed '/^[[:space:]]*#/d; /^[[:space:]]*$/d' "$project/config/console-build-packages.list")
chroot "$root" apt-get update
chroot "$root" env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${runtime_packages[@]}"
query_packages() {
    chroot "$root" dpkg-query -W '-f=${Package} ${db:Status-Status}\n' | awk '$2 == "installed" {print $1}' | sort -u
}
query_packages > "$work/packages-before-build.txt"
chroot "$root" env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${build_packages[@]}"
query_packages > "$work/packages-after-build.txt"
source_dir=$(mktemp -d "$root/tmp/dipper-source.XXXXXXXX")
mkdir "$source_dir/fbkeyboard"
cp -- "$project/fbkeyboard/Makefile" "$project/fbkeyboard/"*.c "$project/fbkeyboard/"*.h "$source_dir/fbkeyboard/"
target_source=${source_dir#"$root"}
chroot "$root" env -u PKG_CONFIG_PATH -u PKG_CONFIG_SYSROOT_DIR -u PKG_CONFIG_LIBDIR \
    make -C "$target_source/fbkeyboard" -j2 CC=cc PKG_CONFIG=pkg-config
chroot "$root" env -u PKG_CONFIG_PATH -u PKG_CONFIG_SYSROOT_DIR -u PKG_CONFIG_LIBDIR \
    make -C "$target_source/fbkeyboard" check CC=cc PKG_CONFIG=pkg-config
python3 "$project/scripts/install_console_rootfs.py" --root "$root" --keyboard "$source_dir/fbkeyboard/fbkeyboard"
comm -13 "$work/packages-before-build.txt" "$work/packages-after-build.txt" > "$work/build-only.txt"
mapfile -t added < "$work/build-only.txt"
if (( ${#added[@]} )); then
    chroot "$root" apt-get -s purge "${added[@]}" > "$work/purge-plan.txt"
    awk '$1 == "Remv" || $1 == "Purg" {print $2}' "$work/purge-plan.txt" | sort -u > "$work/to-remove.txt"
    comm -23 "$work/to-remove.txt" "$work/build-only.txt" > "$work/unexpected-removals.txt"
    [[ ! -s "$work/unexpected-removals.txt" ]] || die 'Refusing to purge pre-existing or runtime packages'
    chroot "$root" env DEBIAN_FRONTEND=noninteractive apt-get purge -y "${added[@]}"
fi
chroot "$root" apt-get clean
rm -rf -- "$source_dir"
source_dir=
restore_temporary
umount "$root/proc"
umount -R "$root/dev"
umount "$root"
fsck_image
resize2fs -M "$raw"
blocks=$(dumpe2fs -h "$raw" 2>/dev/null | awk -F: '$1 == "Block count" {gsub(/ /,"",$2); print $2}')
block_size=$(dumpe2fs -h "$raw" 2>/dev/null | awk -F: '$1 == "Block size" {gsub(/ /,"",$2); print $2}')
[[ "$blocks" =~ ^[0-9]+$ && "$block_size" =~ ^[0-9]+$ ]] || die 'Cannot read final filesystem size'
final_mib=$(( (blocks * block_size + 1048575) / 1048576 + 64 ))
if (( final_mib > max_mib )); then
    # resize2fs may leave large free regions below its conservative minimum.
    # Repack only when necessary, and verify all file data/metadata afterwards.
    echo "Compacting ext4 into the selected ${max_mib} MiB image limit."
    details=$work/filesystem.txt
    dumpe2fs -h "$raw" > "$details" 2>/dev/null
    uuid=$(awk -F: '$1 == "Filesystem UUID" {gsub(/ /,"",$2); print $2}' "$details")
    label=$(sed -n 's/^Filesystem volume name: *//p' "$details")
    features=$(sed -n 's/^Filesystem features: *//p' "$details" | xargs | tr ' ' ',')
    inodes=$(awk -F: '$1 == "Inode count" {gsub(/ /,"",$2); print $2}' "$details")
    inode_size=$(awk -F: '$1 == "Inode size" {gsub(/ /,"",$2); print $2}' "$details")
    reserved=$(awk -F: '$1 == "Reserved block count" {gsub(/ /,"",$2); print $2}' "$details")
    reserved_pct=$(python3 - "$reserved" "$blocks" <<'PY'
import sys
print(f'{100 * int(sys.argv[1]) / int(sys.argv[2]):.6f}')
PY
)
    mount -o loop,ro,nodev,nosuid "$raw" "$root"
    python3 "$project/scripts/rootfs_manifest.py" save "$root" "$work/rootfs-before.json"
    compact=$work/compact.raw.img
    truncate -s "${max_mib}M" "$compact"
    mke2fs -q -t ext4 -b "$block_size" -I "$inode_size" -N "$inodes" \
        -U "$uuid" -L "$label" -O "$features" -m "$reserved_pct" -d "$root" "$compact"
    umount "$root"
    mount -o loop,ro,nodev,nosuid "$compact" "$root"
    python3 "$project/scripts/rootfs_manifest.py" check "$root" "$work/rootfs-before.json"
    umount "$root"
    mv -f -- "$compact" "$raw"
else
    truncate -s "${final_mib}M" "$raw"
    resize2fs "$raw"
fi
fsck_image
mkdir "$output/images"
python3 "$project/scripts/mk_sparse_fill.py" "$raw" "$output/images/xiaomi-dipper.img" --verify
cp -- "$boot" "$output/images/boot.img"
chmod 644 "$output/images/boot.img"
cp -- "$project/flash.sh" "$output/flash.sh"
chmod 755 "$output/flash.sh"
(
    cd "$output/images"
    sha256sum boot.img xiaomi-dipper.img > SHA256SUMS
    md5sum boot.img > boot.img.md5
    md5sum xiaomi-dipper.img > xiaomi-dipper.img.md5
)
sha256sum "$base" "$boot" > "$output/BASE-INPUTS.sha256"
mv -- "$raw" "$output/xiaomi-dipper.raw.img"
chmod 644 "$output/xiaomi-dipper.raw.img"
printf '%s\n' 'Factory console image: log in normally on tty1. No post-install script is required.' \
    'Linux flashing: install adb/fastboot on the PC and use flash.sh (erases userdata).' \
    'Hardware boot/touch/stability still require validation on a Xiaomi Mi 8.' > "$output/README.txt"
printf 'Built factory image in %s\n' "$output"
