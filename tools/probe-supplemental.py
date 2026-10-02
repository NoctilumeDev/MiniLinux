"""Bounded supplemental probes. Run only after the root agent grants RUN."""
import argparse
import json
import shutil
import traceback
from pathlib import Path
from lab_guest import Guest

PROJECT = Path(__file__).resolve().parents[1]
EVIDENCE = PROJECT / "build/supplemental"


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def save_case(name, guest):
    for source in ("lab-console.log", "lab-trace.log", "lab-qemu.log"):
        path = PROJECT / "build" / source
        if path.exists():
            shutil.copyfile(path, EVIDENCE / f"{name}-{source}")
    (EVIDENCE / f"{name}-snapshot.json").write_text(
        json.dumps(guest.snapshot(), indent=2), encoding="utf-8")


def quiet_baseline(guest):
    guest.command("mem")
    check(guest.snapshot()["free"] > 0, "free-page baseline missing")
    return guest.snapshot()["free"]


def restored(guest, baseline):
    guest.command("mem")
    guest.wait_for(lambda: guest.snapshot()["free"] == baseline and
                   {p["program"] for p in guest.snapshot()["processes"]} == {"init", "shell"})
    snap = guest.snapshot()
    check(snap["free"] == baseline, "free pages did not return to baseline")
    check({p["program"] for p in snap["processes"]} == {"init", "shell"},
          "a completed child remained after wait")
    check("hello from ramfs" in guest.command("cat hello.txt"), "shell did not retake")


def mixed_fault(guest, command, baseline):
    start = guest.snapshot()["sequence"]
    output = guest.command(command, timeout=12)
    check("status 142" in output, "fault status 142 absent")
    for step in range(1, 6):
        check(f" step={step} " in output, f"survivor step {step} absent")
    check("counter-a pid=" in output and "ISOLATION FAILED" not in output,
          "counter survivor differs")
    guest.wait_for(lambda: len([e for e in guest.snapshot()["events"]
                               if e["id"] > start and e["kind"] == "exit"]) == 2)
    events = [e for e in guest.snapshot()["events"] if e["id"] > start]
    spawns = [e for e in events if e["kind"] == "spawn"]
    check(len(spawns) == 2, "expected exactly two children")
    by_program = {int(e["fields"][2]): int(e["fields"][0]) for e in spawns}
    fault_pid, survivor_pid = by_program[6], by_program[4]
    faults = [e for e in events if e["kind"] == "fault" and int(e["fields"][0]) == fault_pid]
    check(len(faults) == 1 and faults[0]["fields"][1] == "14" and faults[0]["fields"][5] == "3",
          "expected a single CPL3 page-fault witness")
    exits = {int(e["fields"][0]): int(e["fields"][1]) for e in events if e["kind"] == "exit"}
    check(exits.get(fault_pid) == 142 and exits.get(survivor_pid) == 0,
          "both child exit statuses were not observed")
    after_fault = [e for e in events if e["id"] > faults[0]["id"]]
    check(any(e["kind"] == "syscall" and int(e["fields"][0]) == survivor_pid
              and int(e["fields"][1]) == 1 for e in after_fault),
          "survivor did not issue a write after the other child faulted")
    restored(guest, baseline)
    return {"command": command, "fault_pid": fault_pid, "survivor_pid": survivor_pid,
            "survivor_steps": 5, "fault_status": 142, "survivor_status": 0,
            "free_before_after": baseline, "shell_retook": True}


def fixture(guest, baseline):
    output = guest.command("probe")
    expected = ("PROBE PROCESS cross-page rejected intact",
                "PROBE FILE cross-page rejected intact",
                "PROBE MEMORY cross-page rejected intact",
                "PROBE mapped cross-page outputs passed",
                "PROBE wait non-child self reaped rejected -3",
                "SUPPLEMENTAL PROBE PASSED; pages restored")
    for witness in expected:
        check(witness in output, f"missing guest witness: {witness}")
    restored(guest, baseline)
    with guest.lock:
        before = len(guest.console)
    guest.send("probe-input\r")
    guest.wait_for(lambda: "PROBE INPUT READY: send Z" in guest.console[before:])
    guest.send("Z")
    guest.wait_for(lambda: "mini$ " in guest.console[before:], timeout=6)
    with guest.lock:
        input_output = guest.console[before:]
    check("PROBE INPUT bad pointer preserved queued Z" in input_output,
          "bad input pointer consumed queued Z or readback differed")
    restored(guest, baseline)
    return {"struct_cross_page_rejected_intact": ["process", "file", "memory"],
            "mapped_cross_page_success": True, "wait_non_child_self_reaped": -3,
            "bad_input_pointer_preserved_queued_byte": "Z", "free_before_after": baseline}


def main(mode):
    tool_root = PROJECT / "build" / f"probe-tools-{mode}"
    report = []
    for memory in (64, 256):
        guest = Guest(PROJECT, tool_root, memory)
        name = f"{mode}-{memory}m"
        try:
            guest.start()
            baseline = quiet_baseline(guest)
            if mode == "baseline":
                cases = [mixed_fault(guest, command, baseline)
                         for command in ("run fault counter-a", "run counter-a fault")]
            else:
                cases = [fixture(guest, baseline)]
            report.append({"memory_mib": memory, "cases": cases})
            print(f"SUPPLEMENTAL {mode.upper()} PASS: {memory} MiB", flush=True)
        except Exception:
            (EVIDENCE / f"{name}-first-failure.txt").write_text(traceback.format_exc(), encoding="utf-8")
            raise
        finally:
            guest.close()
            save_case(name, guest)
            check(not guest.process or guest.process.poll() is not None, "QEMU remained running")
    (EVIDENCE / f"{mode}-results.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("baseline", "fixture"), required=True)
    main(parser.parse_args().mode)
