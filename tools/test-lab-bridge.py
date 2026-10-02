"""Exercise the loopback bridge and its real guest, using only the standard library."""
import argparse
import json
import runpy
import threading
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from http.server import ThreadingHTTPServer

PROJECT = Path(__file__).resolve().parents[1]


def main(tool_root):
    module = runpy.run_path(str(PROJECT / "tools/lab-server.py"))
    server = ThreadingHTTPServer(("127.0.0.1", 0), module["Handler"])
    lab = module["Laboratory"](tool_root, 64)
    server.lab = lab
    base = f"http://127.0.0.1:{server.server_port}"
    thread = None
    guests = []
    results = []

    def request(path, expected=200, body=None, headers=None):
        data = json.dumps(body).encode() if body is not None else None
        supplied = {"Content-Type": "application/json"} if data else {}
        supplied.update(headers or {})
        query = Request(base + path, data=data, headers=supplied)
        try:
            response = urlopen(query, timeout=25)
        except HTTPError as exc:
            response = exc
        with response:
            assert response.code == expected, (path, response.code, expected)
            content = response.read()
            results.append({"path": path, "status": expected})
            return json.loads(content) if path.startswith("/api/") else content

    try:
        lab.restart()
        guests.append(lab.guest)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        assert b"MiniLinux Console" in request("/")
        initial = request("/api/state")
        assert initial["status"] == "running"
        request("/api/state?after=invalid", 400)
        request("/../README.md", 404)
        request("/api/state", 403, headers={"Host": "outside.example"})
        request("/api/input", 403, {"text": "help\r"}, {"Origin": "http://outside.example"})
        request("/api/input", 400, [])
        request("/api/input", 400, {"text": "x" * 257})
        request("/api/input", 400, {"text": "\u4f60\u597d"})
        request("/api/input", body={"text": "cat hello.txt\r"})
        lab.guest.wait_for(lambda: "hello from ramfs" in lab.guest.console)
        state = request("/api/state")
        assert any(e["kind"] == "syscall" and e["fields"][1] == "6" for e in state["events"])
        request("/api/stop", body={})
        assert lab.guest.process.poll() is not None
        assert request("/api/state")["status"] == "stopped"
        request("/api/input", 503, {"text": "help\r"})
        request("/api/restart", body={})
        guests.append(lab.guest)
        fresh = request("/api/state")
        assert fresh["epoch"] != initial["epoch"] and fresh["status"] == "running"
        assert "hello from ramfs" not in fresh["console"]
        assert {p["pid"] for p in fresh["processes"]} == {1, 2}
    finally:
        if thread:
            server.shutdown()
            thread.join(timeout=3)
        server.server_close()
        lab.close()
        assert all(guest.process.poll() is not None for guest in guests)
    (PROJECT / "build/bridge-results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    print("BRIDGE PASS: real input and trace, invalid requests, stop/restart, fresh state, owned QEMU cleanup")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool-root", type=Path, default=Path("D:/DevTools/MiniLinux"))
    main(parser.parse_args().tool_root)
