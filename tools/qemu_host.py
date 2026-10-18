"""One virtual CPU profile for the single-core PIC/PIT teaching kernel."""
import os
from pathlib import Path
import re
import shutil
import subprocess

CPU = "qemu64,-apic"


def qemu_command(tool_root):
    if os.name == "nt":
        return [str(tool_root / "qemu-20260811/qemu-system-x86_64.exe"),
                "-L", "qemu-20260811/share", "-cpu", CPU]
    executable = shutil.which("qemu-system-x86_64")
    if not executable:
        raise RuntimeError("Missing qemu-system-x86_64; see docs/LINUX.md")
    return [executable, "-cpu", CPU]


def check_qemu(root):
    executable = qemu_command(root)[0]
    print("qemu=" + str(Path(executable).resolve()), flush=True)
    try:
        result = subprocess.run([executable, "--version"], check=True, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=5)
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError("QEMU --version timed out after 5 seconds; no guest started") from exc
    print(result.stdout.strip(), flush=True)
    match = re.search(r"^QEMU emulator version (\d+)\.(\d+)\.(\d+)(?:\s|[-(]|$)",
                      result.stdout, re.MULTILINE)
    if not match:
        raise RuntimeError("Cannot identify the QEMU version; see docs/LINUX.md")
    if tuple(int(part) for part in match.groups()) < (6, 2, 0):
        raise RuntimeError("This entry requires QEMU >= 6.2.0 with the explicit PIC CPU profile; "
                           "see docs/LINUX.md; no guest started")
    print(f"cpu={CPU}; one CPU, legacy PIC/PIT. Version prerequisite met; "
          "this does not verify guest execution. Use run/check to test this machine.", flush=True)
