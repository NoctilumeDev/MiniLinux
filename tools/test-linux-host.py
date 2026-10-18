"""Host preflight regression tests; no compiler, download or guest is started."""
import contextlib
import io
import os
from pathlib import Path
import subprocess
import socket
import sys
import unittest
from unittest.mock import patch

import linux
import qemu_host


class QemuPreflight(unittest.TestCase):
    def check_banner(self, banner):
        result = subprocess.CompletedProcess(["qemu", "--version"], 0, banner)
        with patch.object(qemu_host, "qemu_command", return_value=["qemu"]), \
                patch.object(linux.subprocess, "run", return_value=result), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            linux.check_qemu(Path("unused-tools"))
        return output.getvalue()

    def test_old_versions_are_rejected(self):
        for version in ("5.2.0", "6.1.1"):
            with self.subTest(version=version), self.assertRaisesRegex(RuntimeError, ">= 6.2.0"):
                self.check_banner("QEMU emulator version " + version)

    def test_new_versions_are_prerequisites_not_guest_passes(self):
        for version in ("6.2.0 (Debian 1:6.2+dfsg-2ubuntu6.31)", "8.2.2", "9.2.2 (custom build)", "11.1.0 (v11.1.0-12130)"):
            with self.subTest(version=version):
                output = self.check_banner("QEMU emulator version " + version + "\nCopyright")
                self.assertIn("does not verify guest execution", output)

    def test_unknown_version_is_rejected(self):
        for banner in ("", "qemu development build", "QEMU emulator version 9.2"):
            with self.subTest(banner=banner), self.assertRaisesRegex(RuntimeError, "Cannot identify"):
                self.check_banner(banner)

    def test_version_timeout_is_bounded(self):
        timeout = subprocess.TimeoutExpired(["qemu", "--version"], 5)
        with patch.object(qemu_host, "qemu_command", return_value=["qemu"]), \
                patch.object(linux.subprocess, "run", side_effect=timeout), \
                contextlib.redirect_stdout(io.StringIO()), \
                self.assertRaisesRegex(RuntimeError, "no guest started"):
            linux.check_qemu(Path("unused-tools"))

    def test_failed_version_command_is_not_accepted(self):
        error = subprocess.CalledProcessError(1, ["qemu", "--version"])
        with patch.object(qemu_host, "qemu_command", return_value=["qemu"]), \
                patch.object(linux.subprocess, "run", side_effect=error), \
                contextlib.redirect_stdout(io.StringIO()), self.assertRaises(subprocess.CalledProcessError):
            linux.check_qemu(Path("unused-tools"))

    def test_missing_executable_is_not_accepted(self):
        with patch.object(qemu_host, "qemu_command", side_effect=RuntimeError("Missing QEMU")), \
                self.assertRaisesRegex(RuntimeError, "Missing QEMU"):
            linux.check_qemu(Path("unused-tools"))

    def test_old_qemu_blocks_all_execution_entries_before_build(self):
        for command in (("doctor",), ("run", "--stage", "M0"), ("run",), ("lab",), ("check",)):
            with self.subTest(command=command), \
                    patch.object(sys, "argv", ["linux.py", *command]), \
                    patch.object(qemu_host, "qemu_command", return_value=["qemu"]), \
                    patch.object(linux.subprocess, "run", return_value=subprocess.CompletedProcess(
                        ["qemu", "--version"], 0, "QEMU emulator version 5.2.0")), \
                    patch.object(linux.subprocess, "check_output", return_value=""), \
                    patch.object(linux, "tool") as compiler, \
                    patch.object(linux, "build") as build, \
                    patch.object(linux, "image") as image, \
                    patch.object(linux, "serial_check") as guest, \
                    contextlib.redirect_stdout(io.StringIO()), \
                    self.assertRaisesRegex(RuntimeError, ">= 6.2.0"):
                try:
                    linux.main()
                finally:
                    compiler.assert_not_called()
                    build.assert_not_called()
                    image.assert_not_called()
                    guest.assert_not_called()

    def test_build_and_prepare_do_not_require_qemu(self):
        for command in ("build", "prepare"):
            with self.subTest(command=command), \
                    patch.object(sys, "argv", ["linux.py", command]), \
                    patch.object(linux, "check_qemu") as preflight, \
                    patch.object(linux, "build"), patch.object(linux, "image"), \
                    patch.object(linux, "prepare"):
                linux.main()
                preflight.assert_not_called()

    def test_pic_profile_is_explicit(self):
        root = Path("unused-tools")
        for platform in ("nt", "posix"):
            with self.subTest(platform=platform), patch.object(qemu_host.os, "name", platform), \
                    patch.object(qemu_host.shutil, "which", return_value="qemu"):
                command = qemu_host.qemu_command(root)
                self.assertEqual(command[command.index("-cpu") + 1], "qemu64,-apic")

    @unittest.skipIf(os.name == "nt", "Linux bind error regression")
    def test_busy_port_explains_recovery_before_guest_start(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0)); listener.listen()
            result = subprocess.run([sys.executable, "-B", str(linux.PROJECT / "tools/lab-server.py"),
                                     "--port", str(listener.getsockname()[1])],
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=5)
        self.assertEqual(result.returncode, 1)
        self.assertIn("no guest started", result.stdout)
        self.assertIn("--port 0", result.stdout)
        self.assertNotIn("Traceback", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
