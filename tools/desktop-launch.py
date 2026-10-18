"""Portable LAB launcher: bundled Python + QEMU, loopback UI, standard library only."""
import argparse
import hashlib
import json
import runpy
import subprocess
import sys
import webbrowser
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "tools"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-browser", action="store_true")
    parser.add_argument("--port", type=int, default=0, help="0 chooses a free loopback port")
    args = parser.parse_args()
    if sys.platform.startswith("linux"):
        if sys.version_info < (3, 10): raise SystemExit("Use Python 3.10 or newer; see READ-ME.txt")
        from qemu_host import check_qemu
        manifest = json.loads((PROJECT / "package.json").read_text(encoding="utf-8"))
        iso = PROJECT / "runtime/images/minilinux-lab.iso"
        if hashlib.sha256(iso.read_bytes()).hexdigest() != manifest["iso_sha256"]:
            raise SystemExit("LAB image differs from package.json; unpack the original package again.")
        check_qemu(PROJECT / "runtime")
    module = runpy.run_path(str(PROJECT / "tools/lab-server.py"))
    server = module["create_server"](args.port)
    server.lab = module["Laboratory"](PROJECT / "runtime", 64)
    try:
        server.lab.restart()
        address = f"http://127.0.0.1:{server.server_port}/"
        print(f"MiniLinux LAB: {address}", flush=True)
        print("Real QEMU guest. Leave this window open. Ctrl+C closes LAB and QEMU.", flush=True)
        if not args.no_browser: webbrowser.open(address)
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        server.lab.close()


if __name__ == "__main__":
    try: main()
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as exc:
        raise SystemExit(f"LAB could not start: {exc}\nSee READ-ME.txt.") from None
