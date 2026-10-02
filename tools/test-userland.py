"""Attack the real serial shell and retain a small mechanism witness."""
import argparse
import json
import time
from pathlib import Path
from lab_guest import Guest

PROJECT = Path(__file__).resolve().parents[1]


def check(condition, message):
    if not condition: raise AssertionError(message)


def main(tool_root):
    report = []
    for memory in (64, 256):
        guest = Guest(PROJECT, tool_root, memory)
        try:
            guest.start()
            check("PID 1 entered user mode" in guest.console, "init did not reach user mode")
            check("help | ls | cat" in guest.command("help"), "help missing")
            check("/hello.txt  17 bytes" in guest.command("ls"), "file enumeration differs")
            check("hello from ramfs" in guest.command("cat hello.txt"), "file bytes differ")
            check("file not found" in guest.command("cat missing"), "missing file did not return")
            check("unknown command" in guest.command("nonsense"), "unknown command did not return")
            check("unknown embedded" in guest.command("run nonexistent"), "unknown program accepted")
            check("line too long" in guest.command("x" * 120), "line overflow not rejected")
            check("too many arguments" in guest.command("run hello reader fault hello"), "extra arguments accepted")
            check("hello from ramfs" in guest.command("cat hello.txX\bt"), "backspace broke input")
            checked = guest.command("check")
            check("user boundaries passed" in checked and "process capacity and reclamation passed" in checked,
                  "new syscall boundaries or process capacity differ")
            check("waiting  init" in guest.command("ps"), "init is not waiting for shell")
            guest.command("mem")
            baseline = guest.snapshot()["free"]
            check(baseline > 0, "no memory observation")
            for _ in range(12):
                check("hello from user mode" in guest.command("run hello"), "slot reuse failed")
                check("hello from ramfs" in guest.command("run reader"), "file slot reuse failed")
            guest.command("mem")
            check(guest.snapshot()["free"] == baseline, "sequential programs leaked pages")

            start_sequence = guest.snapshot()["sequence"]
            pair = guest.command("run counter-a counter-b")
            check("counter-a" in pair and "counter-b" in pair and "ISOLATION FAILED" not in pair,
                  "counter markers changed")
            events = [e for e in guest.snapshot()["events"] if e["id"] > start_sequence]
            spawns = [e for e in events if e["kind"] == "spawn"]
            ids = [int(e["fields"][0]) for e in spawns]
            check(len(ids) == 2, "counter instances were not both created")
            transitions = {(int(e["fields"][0]), int(e["fields"][1])) for e in events if e["kind"] == "schedule"}
            check((ids[0], ids[1]) in transitions and (ids[1], ids[0]) in transitions,
                  "timer did not switch between both programs")
            data = {pid: [e for e in events if e["kind"] == "data" and int(e["fields"][0]) == pid
                          and int(e["fields"][3], 16) != 0] for pid in ids}
            check(all(data.values()), "physical data markers not observed")
            a, b = data[ids[0]][-1]["fields"], data[ids[1]][-1]["fields"]
            check(a[1] == b[1] and a[2] != b[2] and a[3] != b[3], "address isolation witness differs")
            guest.command("mem")
            check(guest.snapshot()["free"] == baseline, "counter pair leaked pages")
            check("status 142" in guest.command("run fault"), "user fault was not contained")
            check("hello from ramfs" in guest.command("cat hello.txt"), "shell did not retake after fault")
            guest.command("mem")
            check(guest.snapshot()["free"] == baseline, "fault path leaked pages")
            check({p["program"] for p in guest.snapshot()["processes"]} == {"init", "shell"},
                  "exited process remained after wait")
            report.append({"memory_mib": memory, "free_before_after": baseline,
                           "counter_pids": ids, "data_a": a, "data_b": b,
                           "timer_switched_both_directions": True, "fault_status": 142,
                           "sequential_program_runs": 24, "input_and_pointer_rejections": True})
            print(f"USERLAND PASS: {memory} MiB, slots reused, pages restored, timer and isolation witnessed")
        finally:
            guest.close()
            check(not guest.process or guest.process.poll() is not None, "QEMU remained running")
    (PROJECT / "build/userland-results.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--tool-root", type=Path, default=Path("D:/DevTools/MiniLinux"))
    main(parser.parse_args().tool_root)
