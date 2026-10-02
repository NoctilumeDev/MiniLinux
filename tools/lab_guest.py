"""Own one QEMU guest and read its two real serial channels. Standard library only."""
from __future__ import annotations

import copy
import socket
import subprocess
import threading
import time
from pathlib import Path

PROGRAMS = ["init", "shell", "hello", "reader", "counter-a", "counter-b", "fault"]
SYSCALLS = ["pid", "write", "ticks", "report", "exit", "open", "read", "close",
            "program", "spawn", "wait", "input", "process", "file", "memory", "inspect"]
STATES = ["unused", "ready", "waiting", "exited"]


def available_port() -> int:
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


class Guest:
    def __init__(self, project: Path, tool_root: Path, memory: int = 64):
        self.project, self.tool_root, self.memory = project.resolve(), tool_root.resolve(), memory
        self.lock = threading.RLock()
        self.write_lock = threading.Lock()
        self.process = None
        self.sockets = []
        self.threads = []
        self.closed = False
        self.console = ""
        self.console_base = 0
        self.events = []
        self.processes = {}
        self.maps = {}
        self.data = {}
        self.frames = self.free = self.ticks = 0
        self.sequence = 0
        self.status = "starting"
        self.error = ""

    def start(self):
        tty_port, trace_port = available_port(), available_port()
        while trace_port == tty_port:
            trace_port = available_port()
        build = self.project / "build"
        build.mkdir(exist_ok=True)
        self.stderr = (build / "lab-qemu.log").open("wb")
        args = [str(self.tool_root / "qemu-20260811/qemu-system-x86_64.exe"),
                "-machine", "pc", "-m", str(self.memory), "-smp", "1", "-accel", "tcg,tb-size=32",
                "-display", "none", "-monitor", "none", "-no-reboot",
                "-chardev", f"socket,id=tty,host=127.0.0.1,port={tty_port},server=on,wait=on",
                "-serial", "chardev:tty",
                "-chardev", f"socket,id=observe,host=127.0.0.1,port={trace_port},server=on,wait=on",
                "-serial", "chardev:observe", "-boot", "d", "-cdrom",
                "images/minilinux-lab.iso", "-L", "qemu-20260811/share"]
        try:
            # QEMU's Windows file arguments may lose non-ASCII absolute paths.
            # Keep its working directory at the runtime root and use relative assets.
            self.process = subprocess.Popen(args, cwd=self.tool_root, stdout=self.stderr,
                                            stderr=self.stderr, creationflags=0x08000000)
            for port in (tty_port, trace_port):
                deadline = time.monotonic() + 10
                while True:
                    if self.process.poll() is not None:
                        raise RuntimeError("QEMU stopped before serial connection; see build/lab-qemu.log")
                    try:
                        channel = socket.create_connection(("127.0.0.1", port), timeout=0.25)
                        channel.settimeout(None)
                        self.sockets.append(channel)
                        break
                    except OSError:
                        if time.monotonic() >= deadline:
                            raise RuntimeError("QEMU serial connection timed out")
                        time.sleep(0.05)
            for index, channel in enumerate(self.sockets):
                thread = threading.Thread(target=self._read, args=(index, channel), daemon=True)
                thread.start()
                self.threads.append(thread)
            self.wait_for(lambda: "mini$ " in self.console, timeout=20)
            self.status = "running"
            return self
        except Exception:
            self.close()
            raise

    def _read(self, index, channel):
        pending = ""
        path = self.project / "build" / ("lab-console.log" if index == 0 else "lab-trace.log")
        try:
            with path.open("wb") as log:
                while not self.closed:
                    chunk = channel.recv(4096)
                    if not chunk:
                        break
                    log.write(chunk)
                    log.flush()
                    text = chunk.decode("ascii", errors="replace")
                    with self.lock:
                        if index == 0:
                            for c in text:
                                if c == "\b": self.console = self.console[:-1]
                                elif c != "\r": self.console += c
                            removed = max(0, len(self.console) - 64000)
                            self.console_base += removed
                            self.console = self.console[-64000:]
                            if "MiniLinux PANIC:" in self.console:
                                self.status, self.error = "faulted", "Kernel panic; inspect the console and restart."
                        else:
                            pending += text
                            while "\n" in pending:
                                line, pending = pending.split("\n", 1)
                                self._event(line)
                            if len(pending) > 4096:
                                raise RuntimeError("Unterminated observation record")
        except (OSError, RuntimeError) as exc:
            if not self.closed:
                with self.lock: self.error = str(exc)
        finally:
            if not self.closed:
                with self.lock:
                    self.status = "disconnected"
                    if not self.error: self.error = "QEMU serial channel closed."

    def _event(self, line):
        parts = line.rstrip("\r").split("\t")
        if len(parts) < 3 or parts[0] != "ML": return
        kind, tick, fields = parts[1], int(parts[2]), parts[3:]
        self.ticks = max(self.ticks, tick)
        self.sequence += 1
        event = {"id": self.sequence, "kind": kind, "tick": tick, "fields": fields}
        # Keep useful events after hours of idle snapshots, not just the last
        # few seconds of repeated mapping records.
        interesting = kind in {"boot", "spawn", "schedule", "syscall", "wait", "exit", "fault"}
        if kind == "memory": interesting = int(fields[1]) != self.free
        if kind == "data":
            old = self.data.get(int(fields[0]))
            interesting = not old or old["pa"] != fields[2] or old["marker"] != fields[3]
        if interesting:
            self.events.append(event)
            self.events = self.events[-2000:]
        if kind == "process":
            pid, parent, program, state = map(int, fields[:4])
            if state == 0:
                self.processes.pop(pid, None); self.maps.pop(pid, None); self.data.pop(pid, None)
            else:
                self.processes[pid] = {"pid": pid, "parent": parent, "program": PROGRAMS[program],
                                       "state": STATES[state], "root": fields[4],
                                       "slices": int(fields[5]), "current": fields[6] == "1"}
        elif kind == "map":
            pid = int(fields[0])
            self.maps.setdefault(pid, {})[fields[1]] = {
                "va": fields[1], "pa": fields[2], "flags": int(fields[3]), "name": fields[4]}
        elif kind == "data":
            self.data[int(fields[0])] = {"va": fields[1], "pa": fields[2], "marker": fields[3]}
        elif kind == "memory":
            self.frames, self.free = map(int, fields[:2])
        elif kind == "schedule":
            for process in self.processes.values():
                process["current"] = process["pid"] == int(fields[1])
        elif kind == "syscall":
            for process in self.processes.values():
                process["current"] = process["pid"] == int(fields[0])

    def snapshot(self):
        with self.lock:
            if self.process and self.process.poll() is not None and not self.closed:
                self.status = "disconnected"
            return copy.deepcopy({"status": self.status, "error": self.error, "console": self.console,
                                  "console_base": self.console_base,
                                  "events": self.events, "processes": list(self.processes.values()),
                                  "maps": self.maps, "data": self.data, "frames": self.frames,
                                  "free": self.free, "ticks": self.ticks, "sequence": self.sequence,
                                  "guest_pid": self.process.pid if self.process else None})

    def send(self, text):
        if self.closed or self.status != "running": raise RuntimeError("Guest is not running")
        with self.write_lock:
            for byte in text.encode("ascii", errors="strict"):
                self.sockets[0].sendall(bytes([byte]))
                time.sleep(0.002)

    def wait_for(self, predicate, timeout=8):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with self.lock:
                if predicate(): return
                if self.status in ("faulted", "disconnected"):
                    raise RuntimeError(f"Guest {self.status}: {self.console[-2000:]}")
            time.sleep(0.025)
        raise TimeoutError(f"Guest observation timed out: {self.console[-2000:]}")

    def command(self, command, timeout=8):
        with self.lock: before = len(self.console)
        self.send(command + "\r")
        self.wait_for(lambda: "mini$ " in self.console[before:], timeout)
        with self.lock: return self.console[before:]

    def close(self):
        self.closed = True
        for channel in self.sockets:
            try: channel.shutdown(socket.SHUT_RDWR)
            except OSError: pass
            channel.close()
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try: self.process.wait(timeout=3)
            except subprocess.TimeoutExpired: self.process.kill(); self.process.wait(timeout=3)
        for thread in self.threads: thread.join(timeout=2)
        if hasattr(self, "stderr"): self.stderr.close()
        self.status = "stopped"


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool-root", type=Path, default=Path("D:/DevTools/MiniLinux"))
    args = parser.parse_args()
    guest = Guest(Path(__file__).resolve().parents[1], args.tool_root)
    try:
        guest.start()
        for command in ("help", "ls", "cat hello.txt", "ps", "run hello", "run reader",
                        "run counter-a counter-b", "run fault", "check", "mem"):
            print(guest.command(command))
    finally:
        guest.close()
