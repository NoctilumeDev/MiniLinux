"""Try the unpacked Linux package with only Python, QEMU and shell tools on PATH."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import tarfile
import tempfile
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="MiniLinux Linux play ") as directory:
        work = Path(directory)
        with tarfile.open(args.archive) as archive:
            if any(m.name.startswith("/") or ".." in Path(m.name).parts or not (m.isfile() or m.isdir())
                   for m in archive.getmembers()):
                raise RuntimeError("Unexpected package member")
            archive.extractall(work)
        bundle = work / "MiniLinux-LAB"
        manifest = json.loads((bundle / "package.json").read_text())
        tools = work / "bin"
        tools.mkdir()
        for name in ("python3", "qemu-system-x86_64", "dirname", "sh"):
            executable = shutil.which(name)
            if not executable: raise RuntimeError("Missing runtime test tool: " + name)
            (tools / name).symlink_to(Path(executable).resolve())
        env = dict(os.environ, PATH=str(tools), PYTHONDONTWRITEBYTECODE="1", PYTHONNOUSERSITE="1")
        log = work / "launcher.log"
        with log.open("wb") as output:
            process = subprocess.Popen([str(tools / "sh"), str(bundle / "Start.sh")],
                                       cwd=work, env=env, stdout=output, stderr=output,
                                       start_new_session=True)
        address = None
        try:
            deadline = time.monotonic() + 25
            while time.monotonic() < deadline:
                text = log.read_text(errors="replace")
                match = re.search(r"MiniLinux LAB: (http://127\.0\.0\.1:\d+/)", text)
                if match: address = match[1]; break
                if process.poll() is not None: raise RuntimeError(text)
                time.sleep(.05)
            if not address: raise RuntimeError("Package startup timed out")

            def request(path, body=None):
                data = json.dumps(body).encode() if body is not None else None
                req = urllib.request.Request(address.rstrip("/") + path, data=data,
                                             headers={"Content-Type": "application/json"})
                with urllib.request.urlopen(req, timeout=5) as response:
                    return json.load(response)

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
                raise RuntimeError("Package command timed out: " + text)

            baseline = request("/api/state")
            command("cat hello.txt", "hello from ramfs")
            _, counters = command("run counter-a counter-b", "exited status 0")
            if counters.count("step=5") != 2: raise RuntimeError(counters)
            final, _ = command("run fault", "exited status 142")
            if final["free"] != baseline["free"]: raise RuntimeError("Package did not restore free pages")
            guest_pid = final["guest_pid"]
            process.send_signal(signal.SIGINT)
            if process.wait(timeout=10) != 0: raise RuntimeError("Launcher did not close normally")
            with socket.socket() as probe:
                if probe.connect_ex(("127.0.0.1", int(address.rstrip('/').rsplit(':',1)[1]))) == 0:
                    raise RuntimeError("Package listener survived shutdown")
            try: os.kill(guest_pid, 0)
            except ProcessLookupError: pass
            else: raise RuntimeError("Package QEMU survived launcher shutdown")
            print("LINUX PACKAGE PASS: clean extraction, runtime-only PATH, cat/counters/fault, pages, Ctrl+C and cleanup")
            print("source=" + manifest["source"] + "; " + address)
        finally:
            if process.poll() is None:
                process.send_signal(signal.SIGINT)
                try: process.wait(timeout=10)
                except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
            print(log.read_text(errors="replace"))


if __name__ == "__main__": main()
