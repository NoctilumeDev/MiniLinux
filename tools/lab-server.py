"""Loopback-only HTTP bridge to one real guest; no frameworks or third-party packages."""
import argparse
import json
import mimetypes
import secrets
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse, parse_qs
from lab_guest import Guest

PROJECT = Path(__file__).resolve().parents[1]
WEB = PROJECT / "web"


class Laboratory:
    def __init__(self, tool_root, memory):
        self.lock = threading.RLock()
        self.tool_root, self.memory = tool_root, memory
        self.guest = None
        self.epoch = ""

    def restart(self):
        with self.lock:
            if self.guest: self.guest.close()
            self.epoch = secrets.token_hex(8)
            self.guest = Guest(PROJECT, self.tool_root, self.memory)
            self.guest.start()

    def close(self):
        with self.lock:
            if self.guest: self.guest.close()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_): pass

    def response(self, status, body, mime="application/json"):
        if not isinstance(body, bytes): body = json.dumps(body).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; style-src 'self'; img-src 'self'; script-src 'self'; frame-ancestors 'none'")
        self.end_headers()
        try: self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError): pass

    def local_request(self):
        allowed = {f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}"}
        host = self.headers.get("Host", "")
        origin = self.headers.get("Origin")
        return host in allowed and (origin is None or origin in {"http://" + name for name in allowed})

    def do_GET(self):
        if not self.local_request(): self.response(403, {"error": "Local laboratory only"}); return
        url = urlparse(self.path)
        if url.path == "/api/state":
            try: after = int(parse_qs(url.query).get("after", ["0"])[0])
            except ValueError: self.response(400, {"error": "Invalid event cursor"}); return
            with self.server.lab.lock:
                state = self.server.lab.guest.snapshot()
                state["events"] = [e for e in state["events"] if e["id"] > after]
                state["epoch"] = self.server.lab.epoch
                state["guest_memory"] = self.server.lab.memory
            self.response(200, state)
            return
        if url.path == "/api/manual":
            self.response(200, {"text": (PROJECT / "docs/USERLAND.md").read_text(encoding="utf-8")})
            return
        relative = "index.html" if url.path == "/" else url.path.lstrip("/")
        path = (WEB / relative).resolve()
        if not path.is_relative_to(WEB.resolve()) or not path.is_file():
            self.response(404, {"error": "No such laboratory page"}); return
        mime = mimetypes.guess_type(str(path))[0] or "application/octet-stream"
        if mime.startswith("text/"): mime += "; charset=utf-8"
        self.response(200, path.read_bytes(), mime)

    def do_POST(self):
        if not self.local_request(): self.response(403, {"error": "Local laboratory only"}); return
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if size < 1 or size > 1024: raise ValueError("Request too large or empty")
            if self.headers.get("Content-Type", "").split(";")[0] != "application/json":
                raise ValueError("Expected JSON")
            body = json.loads(self.rfile.read(size))
            if not isinstance(body, dict): raise ValueError("Expected a JSON object")
            if self.path == "/api/input":
                text = body.get("text")
                if not isinstance(text, str) or not text or len(text) > 256 or any(
                    ord(c) > 126 or (ord(c) < 32 and c not in "\r\n\b\t") for c in text):
                    raise ValueError("Use at most 256 ASCII terminal characters")
                with self.server.lab.lock: self.server.lab.guest.send(text)
            elif self.path == "/api/restart": self.server.lab.restart()
            elif self.path == "/api/stop": self.server.lab.close()
            else: self.response(404, {"error": "Unknown operation"}); return
            self.response(200, {"okay": True})
        except (ValueError, TypeError, json.JSONDecodeError) as exc: self.response(400, {"error": str(exc)})
        except (OSError, RuntimeError, TimeoutError) as exc: self.response(503, {"error": str(exc)})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--memory", type=int, choices=(64, 256), default=64)
    parser.add_argument("--tool-root", type=Path, default=Path("D:/DevTools/MiniLinux"))
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    server.lab = Laboratory(args.tool_root, args.memory)
    try:
        server.lab.restart()
        print(f"MiniLinux laboratory: http://127.0.0.1:{args.port}/", flush=True)
        print("One real QEMU guest. Ctrl+C stops the bridge and its guest.", flush=True)
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt: pass
    finally:
        server.server_close()
        server.lab.close()


if __name__ == "__main__": main()
