#!/usr/bin/env python3
"""Validate the exact boot transformation, including unchanged kernel/initrd."""
import hashlib
import importlib.util
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("hboot", Path(__file__).with_name("prepare-haptics-boot.py"))
hboot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hboot)


class BootTest(unittest.TestCase):
    def test_reject_unknown_boot(self):
        for data in (b"", b"ANDROID!" + b"\0" * 2000):
            with self.assertRaisesRegex(ValueError, "base conhecida"):
                hboot.patch_boot(data)

    @unittest.skipUnless(os.environ.get("DIPPER_TEST_BOOT"), "Set DIPPER_TEST_BOOT to the verified original boot")
    def test_original_boot(self):
        base = Path(os.environ["DIPPER_TEST_BOOT"]).read_bytes()
        patched = hboot.patch_boot(base)
        self.assertEqual(hashlib.sha256(patched).hexdigest(),
                         "294a5472fc7d73d1549639849f6136d1bc9e94f7116947892160ca7cbfddeaac")
        ks, _, rs, _, _, _, _, page, _, _ = struct.unpack_from("<10I", base, 8)
        newks = struct.unpack_from("<I", patched, 8)[0]
        self.assertEqual(base[page:page+hboot.DTB_OFFSET], patched[page:page+hboot.DTB_OFFSET])
        initrd = base[page+hboot.align(ks,page):page+hboot.align(ks,page)+rs]
        self.assertEqual(initrd, patched[page+hboot.align(newks,page):page+hboot.align(newks,page)+rs])
        oldhdr, newhdr = bytearray(base[:page]), bytearray(patched[:page])
        for start, end in ((8,12),(576,608)):
            oldhdr[start:end] = newhdr[start:end]
        self.assertEqual(oldhdr,newhdr)
        kernel = patched[page:page+newks]
        digest = hashlib.sha1()
        for blob in (kernel,initrd,b""):
            digest.update(blob); digest.update(struct.pack("<I",len(blob)))
        self.assertEqual(patched[576:608],digest.digest()+b"\0"*12)
        offset=page+hboot.DTB_OFFSET
        oldsize=struct.unpack_from(">I",base,offset+4)[0]
        newsize=struct.unpack_from(">I",patched,offset+4)[0]
        oldtree=hboot.tree(base[offset:offset+oldsize])[0]
        newtree=hboot.tree(patched[offset:offset+newsize])[0]
        oldtree[hboot.NODE]["status"]=b"okay\0"
        oldtree[hboot.NODE]["qcom,wave-play-rate-us"]=struct.pack(">I",4878)
        self.assertEqual(oldtree,newtree)
        self.assertEqual(base[offset+oldsize:page+ks],patched[offset+newsize:page+newks])
        with self.assertRaises(ValueError):
            hboot.patch_boot(patched)  # cannot accidentally re-patch a modified boot
        corrupted=bytearray(base);corrupted[page+100]^=1
        with self.assertRaises(ValueError):
            hboot.patch_boot(corrupted)
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp)/"images"
            hboot.write_images(out,base,patched)
            with self.assertRaises(FileExistsError):
                hboot.write_images(out,base,patched)
            if shutil.which("dtc") and shutil.which("fdtget"):
                dtb=Path(tmp)/"patched.dtb";dtb.write_bytes(patched[offset:offset+newsize])
                value=subprocess.check_output(["fdtget",str(dtb),hboot.NODE,"status"],text=True)
                self.assertEqual(value.strip(),"okay")
                value=subprocess.check_output(["fdtget","-t","u",str(dtb),hboot.NODE,"qcom,wave-play-rate-us"],text=True)
                self.assertEqual(value.strip(),"4878")
                subprocess.run(["dtc","-q","-I","dtb","-O","dtb","-o",str(Path(tmp)/"roundtrip.dtb"),str(dtb)],check=True)


if __name__ == "__main__":
    unittest.main()
