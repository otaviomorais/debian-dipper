#!/usr/bin/env python3
"""Factory staging tests using the real host-built keyboard, without mounts."""
import importlib.util
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("factory", PROJECT / "scripts/install_console_rootfs.py")
factory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(factory)


class FactoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = PROJECT / "fbkeyboard/fbkeyboard"
        if not cls.binary.is_file():
            raise RuntimeError("Run make keyboard before the factory staging tests")
        machine = factory.elf_machine(cls.binary)
        cls.architecture = next(name for name, value in factory.MACHINES.items() if value == machine)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name) / "rootfs"
        self.root.mkdir()

    def tearDown(self):
        self.directory.cleanup()

    def stage(self):
        return factory.install(self.root, self.binary, self.architecture)

    def test_factory_defaults_and_idempotence(self):
        count = self.stage()
        self.assertGreater(count, 8)
        self.assertEqual((self.root / "usr/bin/fbkeyboard").read_bytes(), self.binary.read_bytes())
        self.assertEqual((self.root / "usr/local/bin/dipper-menu").stat().st_mode & 0o777, 0o755)
        self.assertTrue((self.root / "etc/dipper-console/autostart").is_file())
        link = self.root / "etc/systemd/system/multi-user.target.wants/fbkeyboard.service"
        self.assertEqual(os.readlink(link), "../fbkeyboard.service")
        self.assertEqual(os.readlink(self.root / "etc/systemd/system/default.target"),
                         "/usr/lib/systemd/system/multi-user.target")
        self.assertEqual(self.stage(), count)
        result = subprocess.run([str(self.root / "usr/bin/fbkeyboard"), "-h"],
                                check=True, text=True, capture_output=True)
        self.assertIn("Six rows", result.stdout)

    def test_wrong_architecture_changes_nothing(self):
        other = "arm64" if self.architecture == "amd64" else "amd64"
        with self.assertRaisesRegex(ValueError, "does not match"):
            factory.install(self.root, self.binary, other)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_parent_symlink_cannot_write_to_host(self):
        outside = Path(self.directory.name) / "outside"
        outside.mkdir()
        (self.root / "usr").mkdir()
        (self.root / "usr/local").symlink_to(outside)
        with self.assertRaisesRegex(ValueError, "escapes rootfs"):
            self.stage()
        self.assertEqual(list(outside.iterdir()), [])
        self.assertFalse((self.root / "usr/bin/fbkeyboard").exists())

    def test_existing_file_symlink_is_replaced_as_an_entry(self):
        outside = Path(self.directory.name) / "original"
        outside.write_text("preserve this")
        (self.root / "usr/bin").mkdir(parents=True)
        (self.root / "usr/bin/fbkeyboard").symlink_to(outside)
        self.stage()
        self.assertEqual(outside.read_text(), "preserve this")
        self.assertFalse((self.root / "usr/bin/fbkeyboard").is_symlink())

    def test_non_elf_is_rejected(self):
        text = Path(self.directory.name) / "not-an-arm-binary"
        text.write_text("#!/bin/sh\nexit 0\n")
        with self.assertRaisesRegex(ValueError, "ELF"):
            factory.install(self.root, text, self.architecture)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_make_rootfs_with_a_prebuilt_keyboard(self):
        result = subprocess.run(["make", "--no-print-directory", "-C", str(PROJECT), "rootfs",
                                 f"ROOTFS_DIR={self.root}", f"FBKEYBOARD={self.binary}",
                                 f"ARCH={self.architecture}"], check=True, text=True, capture_output=True)
        self.assertIn("autostart and fbkeyboard enabled", result.stdout)
        self.assertTrue((self.root / "etc/dipper-console/autostart").exists())

    def test_image_cli_does_not_accept_missing_inputs(self):
        result = subprocess.run(["bash", str(PROJECT / "scripts/build-console-image.sh")],
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("--base", result.stderr)

    def test_compaction_manifest_detects_modified_file(self):
        directory = self.root / "tree"
        directory.mkdir()
        (directory / "file").write_text("original")
        os.link(directory / "file", directory / "hardlink")
        (directory / "symlink").symlink_to("file")
        manifest = Path(self.directory.name) / "manifest.json"
        command = ["python3", str(PROJECT / "scripts/rootfs_manifest.py")]
        subprocess.run(command + ["save", str(directory), str(manifest)], check=True)
        subprocess.run(command + ["check", str(directory), str(manifest)], check=True, capture_output=True)
        (directory / "file").write_text("modified")
        result = subprocess.run(command + ["check", str(directory), str(manifest)], text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Rootfs verification failed", result.stderr)


if __name__ == "__main__":
    unittest.main()
