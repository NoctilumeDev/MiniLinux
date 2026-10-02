"""Record real LAB runs for the explicitly labelled static replay. No simulated kernel."""
import argparse
import hashlib
import json
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path
from lab_guest import Guest


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(project, tool_root, output):
    source = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=project, text=True).strip()
    guest = Guest(project, tool_root)
    clips = []

    def snapshot(after):
        state = guest.snapshot()
        state.pop("guest_pid", None)
        state["events"] = [event for event in state["events"] if event["id"] > after]
        state["guest_memory"] = 64
        return state

    try:
        guest.start()
        clips.append({"command": "boot", "frames": [{"at": 0, "state": snapshot(0)}]})
        commands = ["help", "ls", "cat hello.txt", "ps", "run hello", "run reader",
                    "run counter-a counter-b", "run fault", "check", "mem"]
        for command in commands:
            frames = [{"at": 0, "state": snapshot(0)}]
            cursor = frames[0]["state"]["sequence"]
            with guest.lock:
                before = len(guest.console)
            started = time.monotonic()
            guest.send(command + "\r")
            while True:
                state = snapshot(cursor)
                frames.append({"at": round((time.monotonic() - started) * 1000), "state": state})
                cursor = state["sequence"]
                if "mini$ " in state["console"][before:]:
                    break
                if state["status"] != "running" or time.monotonic() - started > 20:
                    raise RuntimeError(f"Recording failed at {command}: {state['console'][-1500:]}")
                time.sleep(0.12)
            clips.append({"command": command, "frames": frames})
    finally:
        guest.close()
    # These are observed outcomes, never substitute generated console strings.
    assert "hello from ramfs" in clips[3]["frames"][-1]["state"]["console"]
    assert any(e["kind"] == "fault" for frame in clips[8]["frames"] for e in frame["state"]["events"])
    data = {"format": 1, "mode": "recorded-replay", "source": source,
            "recorded_at": datetime.now(timezone.utc).isoformat(),
            "kernel_sha256": digest(project / "build/kernel.elf"),
            "user_sha256": digest(project / "build/user.elf"),
            "iso_sha256": digest(tool_root / "images/minilinux-lab.iso"), "clips": clips}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(data, separators=(",", ":")), encoding="utf-8")
    print(f"Recorded {len(clips)} real clips from {source}: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool-root", type=Path, default=Path("D:/DevTools/MiniLinux"))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    record(Path(__file__).resolve().parents[1], args.tool_root, args.output)
