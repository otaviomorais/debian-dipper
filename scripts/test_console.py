#!/usr/bin/env python3
"""Offline status tests; no NetworkManager connections or credentials are changed."""
import importlib.machinery
import importlib.util
from pathlib import Path
import tempfile
import unittest
import shutil
import subprocess
import time
import shlex
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
loader = importlib.machinery.SourceFileLoader("status", str(ROOT / "rootfs/usr/local/bin/dipper-status"))
spec = importlib.util.spec_from_loader(loader.name, loader)
status = importlib.util.module_from_spec(spec)
loader.exec_module(status)


class StatusTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, name, value):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value)

    def test_absent_battery(self):
        self.assertEqual(status.battery(self.root), "BAT --")

    def test_charging_and_invalid_capacity(self):
        self.write("bat/type", "Battery")
        self.write("bat/capacity", "76")
        self.write("bat/status", "Charging")
        self.assertEqual(status.battery(self.root), "BAT 76%+")
        self.write("bat/capacity", "unknown")
        self.assertEqual(status.battery(self.root), "BAT --")

    def test_offline(self):
        self.write("wlan0/operstate", "down")
        self.assertEqual(status.network(self.root, self.root / "absent"), "NET offline")

    def test_default_route_and_no_address_permission(self):
        self.write("usb0/operstate", "up")
        self.write("wlan0/operstate", "up")
        (self.root / "wlan0/wireless").mkdir()
        self.write("routes", "Iface Destination Gateway Flags\nwlan0 00000000 12345678 0003\n")
        with patch.object(status.fcntl, "ioctl", side_effect=OSError):
            self.assertEqual(status.network(self.root, self.root / "routes"), "WiFi wlan0")

    def test_tmux_format_characters_removed(self):
        self.write("bad#[name]/operstate", "up")
        with patch.object(status.fcntl, "ioctl", side_effect=OSError):
            text = status.network(self.root, self.root / "absent")
        self.assertEqual(text, "NET badname")


@unittest.skipUnless(shutil.which("tmux"), "tmux não instalado neste host")
class TmuxTests(unittest.TestCase):
    def test_windows_panes_selection_and_paste(self):
        with tempfile.TemporaryDirectory(prefix="dipper-tmux-") as directory:
            base = Path(directory)
            config = (ROOT / "rootfs/etc/dipper-console/tmux.conf").read_text()
            config = config.replace("/usr/local/bin/", str(ROOT / "rootfs/usr/local/bin") + "/")
            config = config.replace("/etc/dipper-console/safe-area.conf", str(ROOT / "rootfs/etc/dipper-console/safe-area.conf"))
            (base / "tmux.conf").write_text(config)
            command = ["tmux", "-S", str(base / "socket"), "-f", str(base / "tmux.conf")]

            def run(*args):
                return subprocess.run(command + list(args), check=True, text=True,
                                      capture_output=True, timeout=5).stdout

            try:
                run("new-session", "-d", "-s", "test", "-x", "67", "-y", "45",
                    "printf 'hello-copy\\n'; exec cat")
                for _ in range(30):
                    if "hello-copy" in run("capture-pane", "-p", "-t", "test:1.1"):
                        break
                    time.sleep(0.05)
                else:
                    self.fail("terminal did not produce its expected output")
                bindings = run("list-keys", "-T", "copy-mode")
                self.assertIn("F10", bindings)
                self.assertIn("copy-selection-and-cancel", bindings)
                self.assertEqual(run("show-options", "-gv", "prefix").strip(), "C-a")
                self.assertEqual(run("show-options", "-gv", "set-clipboard").strip(), "off")
                self.assertEqual(run("show-options", "-gv", "status").strip(), "5")
                for line in range(4):
                    self.assertEqual(run("show-options", "-gv", f"status-format[{line}]").strip(), "")
                self.assertIn("status-left", run("show-options", "-gv", "status-format[4]"))
                run("copy-mode", "-t", "test:1.1")
                for action in ("history-top", "start-of-line", "begin-selection",
                               "cursor-right", "cursor-right", "cursor-right", "cursor-right", "cursor-right",
                               "copy-selection-and-cancel"):
                    run("send-keys", "-t", "test:1.1", "-X", action)
                self.assertEqual(run("show-buffer"), "hello")
                pasted = base / "pasted.txt"
                run("new-window", "-t", "test", "-n", "Paste", "cat > " + shlex.quote(str(pasted)))
                # Wait for the shell to exec cat before sending the buffer.
                for _ in range(30):
                    if run("display-message", "-p", "-t", "test:2.1", "#{pane_current_command}").strip() == "cat":
                        break
                    time.sleep(0.05)
                run("paste-buffer", "-p", "-t", "test:2.1")
                run("send-keys", "-t", "test:2.1", "Enter")
                for _ in range(30):
                    if pasted.exists() and pasted.read_text() == "hello\n":
                        break
                    time.sleep(0.05)
                self.assertEqual(pasted.read_text(), "hello\n")
                run("split-window", "-t", "test:2.1", "-v", "cat")
                self.assertEqual(len(run("list-panes", "-t", "test:2").splitlines()), 2)
                run("resize-pane", "-t", "test:2.1", "-Z")
                self.assertEqual(run("display-message", "-p", "-t", "test:2.1", "#{window_zoomed_flag}").strip(), "1")
            finally:
                subprocess.run(command + ["kill-server"], capture_output=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
