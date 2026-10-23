#!/usr/bin/env python3
"""Check file contents, ownership, permissions, links and xattrs after repacking."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import stat


def manifest(root):
    root = Path(root)
    records, hardlinks = {}, {}

    def visit(path):
        relative = path.relative_to(root).as_posix()
        info = path.lstat()
        item = {"mode": info.st_mode, "uid": info.st_uid, "gid": info.st_gid}
        item["xattrs"] = {
            name: hashlib.sha256(os.getxattr(path, name, follow_symlinks=False)).hexdigest()
            for name in sorted(os.listxattr(path, follow_symlinks=False))
        }
        if stat.S_ISREG(info.st_mode):
            with path.open("rb") as stream:
                item["sha256"] = hashlib.file_digest(stream, "sha256").hexdigest()
            item["mtime"] = int(info.st_mtime)
            identity = (info.st_dev, info.st_ino)
            if info.st_nlink > 1:
                item["hardlink"] = hardlinks.setdefault(identity, relative)
        elif stat.S_ISLNK(info.st_mode):
            item["target"] = os.readlink(path)
        elif stat.S_ISCHR(info.st_mode) or stat.S_ISBLK(info.st_mode):
            item["device"] = [os.major(info.st_rdev), os.minor(info.st_rdev)]
        elif not stat.S_ISDIR(info.st_mode) and not stat.S_ISFIFO(info.st_mode):
            raise ValueError(f"Unsupported entry in offline rootfs: {relative}")
        records[relative] = item
        if stat.S_ISDIR(info.st_mode):
            for child in sorted(path.iterdir()):
                visit(child)

    visit(root)
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("save", "check"))
    parser.add_argument("root", type=Path)
    parser.add_argument("file", type=Path)
    args = parser.parse_args()
    current = manifest(args.root)
    if args.operation == "save":
        args.file.write_text(json.dumps(current, sort_keys=True))
    else:
        previous = json.loads(args.file.read_text())
        differences = [name for name in sorted(previous.keys() | current.keys())
                       if previous.get(name) != current.get(name)]
        if differences:
            raise SystemExit("Rootfs verification failed: " + ", ".join(differences[:10]))
        print(f"Rootfs verified: {len(current)} entries; contents, ownership, modes, links and xattrs match.")


if __name__ == "__main__":
    main()
