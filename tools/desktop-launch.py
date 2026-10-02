"""Portable LAB launcher: bundled Python + QEMU, loopback UI, standard library only."""
import argparse
import runpy
import sys
import webbrowser
from http.server import ThreadingHTTPServer
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "tools"))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()
    module = runpy.run_path(str(PROJECT / "tools/lab-server.py"))
    server = ThreadingHTTPServer(("127.0.0.1", 0), module["Handler"])
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
    main()
