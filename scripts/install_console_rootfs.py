#!/usr/bin/env python3
"""Install the console into a staging rootfs, without starting host services."""
import argparse
import os
from pathlib import Path
import shutil
import struct

PROJECT = Path(__file__).resolve().parents[1]
ASSETS = {
    "usr/local/bin/dipper-console": 0o755,
    "usr/local/bin/dipper-menu": 0o755,
    "usr/local/bin/dipper-status": 0o755,
    "etc/dipper-console/tmux.conf": 0o644,
    "etc/profile.d/dipper-console.sh": 0o644,
    "etc/systemd/system/fbkeyboard.service": 0o644,
    "etc/systemd/system/fbkeyboard.service.d/fbkeyboard-font.conf": 0o644,
}
MACHINES = {"arm64": 183, "amd64": 62}


def elf_machine(path):
    with Path(path).open("rb") as stream:
        header = stream.read(20)
    if len(header) < 20 or header[:4] != b"\x7fELF" or header[4:6] != b"\x02\x01":
        raise ValueError(f"Expected a 64-bit little-endian ELF: {path}")
    return struct.unpack_from("<H", header, 18)[0]


def destination(root, relative):
    path = root / relative
    # A staging tree may contain absolute symlinks; never follow them onto host.
    if not path.parent.resolve().is_relative_to(root):
        raise ValueError(f"Destination escapes rootfs: {relative}")
    return path


def install(root, binary, architecture="arm64", factory=True):
    root = Path(root).resolve(strict=True)
    binary = Path(binary).resolve(strict=True)
    if root == Path("/") or not root.is_dir():
        raise ValueError("Use a staging rootfs, not the host root directory")
    machine = MACHINES[architecture]
    if elf_machine(binary) != machine:
        raise ValueError(f"fbkeyboard does not match {architecture}; compile for the target first")
    dpkg = root / "usr/bin/dpkg"
    if dpkg.exists() and elf_machine(dpkg) != machine:
        raise ValueError("Target rootfs and fbkeyboard have different architectures")
    files = [(binary, "usr/bin/fbkeyboard", 0o755)]
    files += [(PROJECT / "rootfs" / relative, relative, mode) for relative, mode in ASSETS.items()]
    if factory:
        files.append((PROJECT / "rootfs/etc/dipper-console/autostart", "etc/dipper-console/autostart", 0o644))
    links = {}
    if factory:
        links = {
            "etc/systemd/system/multi-user.target.wants/fbkeyboard.service": "../fbkeyboard.service",
            "etc/systemd/system/default.target": "/usr/lib/systemd/system/multi-user.target",
        }
    # Validate the complete plan before writing anything.
    for source, relative, _ in files:
        if not source.is_file():
            raise ValueError(f"Missing source asset: {source}")
        path = destination(root, relative)
        if path.exists() and path.is_dir():
            raise ValueError(f"Expected a file: {relative}")
    for relative in links:
        path = destination(root, relative)
        if path.exists() and path.is_dir():
            raise ValueError(f"Expected a symlink: {relative}")
    for source, relative, mode in files:
        path = destination(root, relative)
        path.parent.mkdir(parents=True, exist_ok=True)
        # Replace a symlink as an entry rather than following its target.
        path.unlink(missing_ok=True)
        shutil.copyfile(source, path)
        path.chmod(mode)
        if os.geteuid() == 0:
            os.chown(path, 0, 0)
    for relative, target in links.items():
        path = destination(root, relative)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.unlink(missing_ok=True)
        path.symlink_to(target)
    return len(files)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--keyboard", required=True, type=Path)
    parser.add_argument("--architecture", choices=MACHINES, default="arm64")
    args = parser.parse_args()
    try:
        count = install(args.root, args.keyboard, args.architecture)
    except (OSError, ValueError) as error:
        parser.exit(1, f"rootfs: {error}\n")
    print(f"Console installed: {count} files; autostart and fbkeyboard enabled; text target selected.")


if __name__ == "__main__":
    main()
