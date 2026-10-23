#!/usr/bin/env python3
"""Enable the Mi 8 LRA in the exact, known factory boot; never write partitions.

Only status and wave-play-rate-us change in the appended DTB. The original
compressed kernel, initramfs, addresses and command line remain unchanged.
"""
import argparse
import fcntl
import hashlib
import os
from pathlib import Path
import stat
import struct
import sys

BASE_SIZE = 25858048
BASE_SHA256 = "a17e3ea8be5697744e37841e3c5a7da521eca6d7ca23ee45eac6e5b1b1f6c98c"
NODE = "/soc@0/spmi@c440000/pmic@3/haptics@c000"
DTB_OFFSET = 14953845
RATE = 4878
MAGIC = b"\xd0\x0d\xfe\xed"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def align(value, page=4):
    return (value + page - 1) // page * page


def tree(data):
    """Return values plus structure offsets for a version-17 flattened tree."""
    h = struct.unpack_from(">10I", data)
    magic, total, start, strings, reserve, version, compatible, _, ns, nt = h
    require(magic == 0xD00DFEED and total == len(data), "DTB invalido")
    require(version == 17 and compatible <= 17, "Versao DTB inesperada")
    require(40 <= reserve < start < strings and start + nt <= strings and
            strings + ns <= total, "Offsets DTB invalidos")
    stack, values, offsets, ends = [], {}, {}, {}
    pos = start
    while pos < start + nt:
        token_start = pos
        token = struct.unpack_from(">I", data, pos)[0]
        pos += 4
        if token == 1:
            end = data.index(b"\0", pos, start + nt)
            stack.append(data[pos:end].decode("ascii"))
            pos = align(end + 1)
            values["/" + "/".join(stack[1:])] = {}
        elif token == 2:
            require(bool(stack), "DTB sem no pai")
            ends["/" + "/".join(stack[1:])] = token_start
            stack.pop()
        elif token == 3:
            length, nameoff = struct.unpack_from(">2I", data, pos)
            pos += 8
            require(nameoff < ns and pos + length <= start + nt, "Propriedade invalida")
            end = data.index(b"\0", strings + nameoff, strings + ns)
            name = data[strings + nameoff:end].decode("ascii")
            path = "/" + "/".join(stack[1:])
            require(name not in values[path], "Propriedade duplicada")
            values[path][name] = data[pos:pos + length]
            offsets[(path, name)] = (token_start, align(pos + length), nameoff)
            pos = align(pos + length)
        elif token == 4:
            continue
        elif token == 9:
            require(not stack, "DTB nao terminou os nos")
            return values, offsets, ends
        else:
            raise ValueError("Token DTB inesperado")
    raise ValueError("DTB incompleto")


def property_record(nameoff, value):
    return struct.pack(">3I", 3, len(value), nameoff) + value + b"\0" * (align(len(value)) - len(value))


def replace_structure(data, begin, end, replacement):
    h = list(struct.unpack_from(">10I", data))
    delta = len(replacement) - (end - begin)
    h[1] += delta
    h[3] += delta  # strings follow structure
    h[9] += delta
    result = bytearray(data[:begin] + replacement + data[end:])
    struct.pack_into(">10I", result, 0, *h)
    return bytes(result)


def patch_tree(data):
    before, offsets, _ = tree(data)
    require(before.get("/", {}).get("model") == b"Xiaomi Mi 8\0", "Modelo DTB incorreto")
    motor = before.get(NODE, {})
    require(motor.get("status") == b"disabled\0" and
            b"qcom,pmi8998-haptics\0" in motor.get("compatible", b""),
            "Vibrador nao corresponde ao boot original")
    require(motor.get("qcom,play-mode") == struct.pack(">I", 1) and
            "qcom,wave-play-rate-us" not in motor, "Parametros do motor inesperados")
    start, end, nameoff = offsets[(NODE, "status")]
    data = replace_structure(data, start, end, property_record(nameoff, b"okay\0"))
    _, _, ends = tree(data)
    h = list(struct.unpack_from(">10I", data))
    name = b"qcom,wave-play-rate-us\0"
    nameoff = h[8]
    insert = h[3] + h[8]
    data = bytearray(data[:insert] + name + data[insert:])
    h[1] += len(name)
    h[8] += len(name)
    struct.pack_into(">10I", data, 0, *h)
    pos = ends[NODE]
    data = replace_structure(bytes(data), pos, pos,
                             property_record(nameoff, struct.pack(">I", RATE)))
    after, _, _ = tree(data)
    expected = {path: dict(props) for path, props in before.items()}
    expected[NODE]["status"] = b"okay\0"
    expected[NODE]["qcom,wave-play-rate-us"] = struct.pack(">I", RATE)
    require(after == expected, "Modificacao inesperada no DTB")
    return data


def patch_boot(base):
    require(len(base) == BASE_SIZE and hashlib.sha256(base).hexdigest() == BASE_SHA256,
            "Boot diferente da base conhecida. Nenhuma particao foi alterada.")
    require(base[:8] == b"ANDROID!", "Cabecalho de boot invalido")
    ks, _, rs, _, second, _, _, page, version, _ = struct.unpack_from("<10I", base, 8)
    require(page == 4096 and version == 0 and second == 0, "Formato de boot inesperado")
    kernel = base[page:page + ks]
    roff = page + align(ks, page)
    ramdisk = base[roff:roff + rs]
    require(kernel[DTB_OFFSET:DTB_OFFSET + 4] == MAGIC, "DTB nao encontrado")
    size = struct.unpack_from(">I", kernel, DTB_OFFSET + 4)[0]
    newtree = patch_tree(kernel[DTB_OFFSET:DTB_OFFSET + size])
    kernel = kernel[:DTB_OFFSET] + newtree + kernel[DTB_OFFSET + size:]
    header = bytearray(base[:page])
    struct.pack_into("<I", header, 8, len(kernel))
    # Android boot v0 ID: each blob followed by its little-endian size,
    # including a zero size for the absent second stage.
    digest = hashlib.sha1()
    for blob in (kernel, ramdisk, b""):
        digest.update(blob)
        digest.update(struct.pack("<I", len(blob)))
    header[576:608] = digest.digest() + b"\0" * 12
    return (bytes(header) + kernel + b"\0" * (align(len(kernel), page) - len(kernel)) +
            ramdisk + b"\0" * (align(len(ramdisk), page) - len(ramdisk)))


def prepare_device(output):
    require(os.geteuid() == 0, "Execute pelo ADB como root")
    require(b"xiaomi,dipper\0" in Path("/proc/device-tree/compatible").read_bytes(),
            "Este aparelho nao e um Xiaomi Mi 8")
    candidates = ("/dev/disk/by-partlabel/boot", "/dev/block/bootdevice/by-name/boot",
                  "/dev/block/by-name/boot")
    device = next((Path(p) for p in candidates if Path(p).exists()), None)
    require(device is not None and stat.S_ISBLK(device.stat().st_mode),
            "Particao boot nao encontrada")
    with device.open("rb", buffering=0) as stream:
        size = struct.unpack("Q", fcntl.ioctl(stream.fileno(), 0x80081272, b"\0" * 8))[0]
        require(BASE_SIZE <= size <= 128 * 1024 * 1024, "Tamanho da particao inesperado")
        base = stream.read(BASE_SIZE)
    patched = patch_boot(base)  # validate BEFORE creating any output
    write_images(output, base, patched)
    print("Particao lida somente para leitura; nada foi gravado no boot.")


def write_images(output, base, patched):
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    for name, data in (("boot-original.img", base), ("boot-vibracao.img", patched)):
        with (output / name).open("xb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
    sums = "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n"
                   for name, data in (("boot-original.img", base), ("boot-vibracao.img", patched)))
    (output / "SHA256SUMS").write_text(sums, encoding="ascii")
    print(sums, end="")


def verify_running():
    live = Path("/sys/firmware/devicetree/base") / NODE.lstrip("/")
    require((live / "status").read_bytes() == b"okay\0" and
            (live / "qcom,wave-play-rate-us").read_bytes() == struct.pack(">I", RATE),
            "O aparelho nao esta usando o boot de teste")
    # Linux EV_FF is bit 21; FF_RUMBLE is bit 80. Sysfs prints bitmap words
    # highest first, and the target is a 64-bit kernel.
    for entry in Path("/sys/class/input").glob("event*"):
        capabilities = entry / "device/capabilities/ff"
        name = (entry / "device/name").read_text().strip()
        if capabilities.exists() and ("haptic" in name.lower() or "vibra" in name.lower()):
            words = capabilities.read_text().split()
            bits = sum(int(word, 16) << (64 * i) for i, word in enumerate(reversed(words)))
            if bits & (1 << 80):
                print(f"Vibrador disponivel: {name}, {entry.name}")
                return
    raise ValueError("Driver de vibracao ainda nao registrou FF_RUMBLE")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--base", type=Path, help="Preparar offline a partir do boot conhecido")
    group.add_argument("--prepare-device", action="store_true")
    group.add_argument("--verify-running", action="store_true")
    parser.add_argument("--output", type=Path, default=Path("/tmp/dipper-haptics-v013"))
    args = parser.parse_args()
    if args.verify_running:
        verify_running()
    elif args.prepare_device:
        prepare_device(args.output)
    else:
        base = args.base.read_bytes()
        write_images(args.output, base, patch_boot(base))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, struct.error) as error:
        print(f"ERRO: {error}", file=sys.stderr)
        sys.exit(1)
