"""Try the documented source LAB entry, or a fresh runtime-only Linux package."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, nargs="?", help="Omit to test tools/linux.py lab")
    parser.add_argument("--tool-root", type=Path, default=Path(".tools/linux"))
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="MiniLinux Linux play ") as directory:
        work = Path(directory)
        env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", PYTHONNOUSERSITE="1")
        if args.archive:
            with tarfile.open(args.archive) as archive:
                if any(m.name.startswith("/") or ".." in Path(m.name).parts or not (m.isfile() or m.isdir())
                       for m in archive.getmembers()):
                    raise RuntimeError("Unexpected package member")
                archive.extractall(work)
            bundle = work / "MiniLinux-LAB"
            source = json.loads((bundle / "package.json").read_text())["source"]
            tools = work / "bin"
            tools.mkdir()
            for name in ("python3", "qemu-system-x86_64", "dirname", "sh"):
                executable = shutil.which(name)
                if not executable: raise RuntimeError("Missing runtime test tool: " + name)
                (tools / name).symlink_to(Path(executable).resolve())
            env["PATH"] = str(tools)
            launch = [str(tools / "sh"), str(bundle / "Start.sh")]
            directory, expected_exit = work, 0
            label = "LINUX PACKAGE"
        else:
            bundle = Path(__file__).resolve().parents[1]
            source = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=bundle, text=True).strip()
            launch = [sys.executable, "-B", str(bundle / "tools/linux.py"), "lab",
                      "--tool-root", str(args.tool_root.resolve())]
            directory, expected_exit = bundle, 130
            label = "LINUX SOURCE ENTRY"
        log = work / "launcher.log"
        with log.open("wb") as output:
            process = subprocess.Popen(launch, cwd=directory, env=env, stdout=output, stderr=output,
                                       start_new_session=True)
        address = None
        try:
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                text = log.read_text(errors="replace")
                match = re.search(r"MiniLinux (?:LAB|laboratory): (http://127\.0\.0\.1:\d+/)", text)
                if match: address = match[1]; break
                if process.poll() is not None: raise RuntimeError(text)
                time.sleep(.05)
            if not address: raise RuntimeError("LAB startup timed out")
            if address.endswith(":0/"): raise RuntimeError("Printed URL contains port 0: " + address)

            def request(path, body=None):
                data = json.dumps(body).encode() if body is not None else None
                req = urllib.request.Request(address.rstrip("/") + path, data=data,
                                             headers={"Content-Type": "application/json"})
                with urllib.request.urlopen(req, timeout=5) as response:
                    content = response.read()
                    return json.loads(content) if path.startswith("/api/") else content

            def command(text, expected):
                before = len(request("/api/state")["console"])
                request("/api/input", {"text": text + "\r"})
                deadline = time.monotonic() + 12
                while time.monotonic() < deadline:
                    state = request("/api/state")
                    suffix = state["console"][before:]
                    if "mini$ " in suffix:
                        if expected not in suffix: raise RuntimeError(suffix)
                        return state, suffix
                    time.sleep(.05)
                raise RuntimeError("LAB command timed out: " + text)

            if b"MiniLinux Console" not in request("/"): raise RuntimeError("Printed URL did not serve LAB")
            if request("/api/manual")["text"] != (bundle / "docs/USERLAND.md").read_text(encoding="utf-8"):
                raise RuntimeError("Manual response differs from the included manual")
            baseline = request("/api/state")
            command("cat hello.txt", "hello from ramfs")
            _, counters = command("run counter-a counter-b", "exited status 0")
            if counters.count("step=5") != 2 or counters.count("exited status 0") != 2:
                raise RuntimeError(counters)
            final, _ = command("run fault", "exited status 142")
            command("ls", "/hello.txt")
            if final["free"] != baseline["free"]: raise RuntimeError("Package did not restore free pages")
            guest_pid = final["guest_pid"]
            os.killpg(process.pid, signal.SIGINT)
            if process.wait(timeout=10) != expected_exit: raise RuntimeError("LAB did not close normally")
            with socket.socket() as probe:
                if probe.connect_ex(("127.0.0.1", int(address.rstrip('/').rsplit(':',1)[1]))) == 0:
                    raise RuntimeError("Package listener survived shutdown")
            try: os.kill(guest_pid, 0)
            except ProcessLookupError: pass
            else: raise RuntimeError("Package QEMU survived launcher shutdown")
            print(label + " PASS: printed URL reachable, manual, cat/counters/fault, shell survives, pages, Ctrl+C and cleanup")
            print("source=" + source + "; " + address)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGINT)
                try: process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=5)
            try: os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError: pass
            print(log.read_text(errors="replace"))


if __name__ == "__main__": main()
