"""Linux source build and QEMU entry. Guest code and LAB checks stay shared."""
from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
import zipfile

from lab_guest import qemu_command

PROJECT = Path(__file__).resolve().parents[1]
LIMINE_SHA256 = "04f922a9265d2f02756417254eb746e7687692ff3f42c8a354b8dcfbda9ef571"
LIMINE_URL = "https://github.com/Limine-Bootloader/Limine/releases/download/v12.9.0/limine-binary.zip"
LIMINE_FILES = {
    "limine-bios-cd.bin": "0a0f509cd2e8b0f7ea4cfca6eac5b88a4ac477cd102cc90fb7e2c21ba80b2a22",
    "limine-bios.sys": "8e432db7b4721f906c9136b3854d3d9fec7b337d6f6d049c0f2a1ba8ea591507",
}
STAGES = tuple(f"M{i}" for i in range(7)) + ("LAB",)
CHECKS = ("", "physical page", "address mapping", "timer preemption",
          "user boundary", "file byte", "process isolation")
ATTACKS = ("", "counterexample: syscall parent permission rejected",
           "counterexample: all ancestor permissions checked; pages restored",
           "counterexample: table limit rejected new branch; existing branch and cleanup passed",
           "counterexample: pointer endpoints checked without consuming file bytes",
           "counterexample: DF and nine registers survived timer and syscall entry")
FAULTS = ("", "", "vector=14 error=0x0000000000000007",
          "vector=14 error=0x0000000000000015", "vector=14 error=0x0000000000000004",
          "user exception: task=0 vector=6 error=0x0000000000000000 CS=0x0000000000000023",
          "user exception: task=0 vector=13 error=0x0000000000000000 CS=0x0000000000000023",
          "user exception: task=0 vector=0 error=0x0000000000000000 CS=0x0000000000000023")


def run(args, project=PROJECT):
    subprocess.run([str(arg) for arg in args], cwd=project, check=True)


def tool(name, llvm_bin):
    if os.name == "nt": name += ".exe"
    found = shutil.which(str(llvm_bin / name) if llvm_bin else name)
    if not found:
        raise RuntimeError(f"Missing {name}; install LLVM or set --llvm-bin. See docs/LINUX.md")
    return found


def prepare(root, archive):
    root.mkdir(parents=True, exist_ok=True)
    if archive:
        data = archive.read_bytes()
    else:
        print(f"Downloading {LIMINE_URL}", flush=True)
        with urllib.request.urlopen(LIMINE_URL, timeout=60) as response:
            data = response.read()
    if hashlib.sha256(data).hexdigest() != LIMINE_SHA256:
        raise RuntimeError("Limine archive SHA256 differs from the retained M0 version")
    # Extract only known assets; no host installer, shell command or ZIP path is run.
    with zipfile.ZipFile(io.BytesIO(data)) as package:
        for name in ("limine-bios-cd.bin", "limine-bios.sys", "LICENSE"):
            destination = root / "limine-12.9.0/limine-binary" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(package.read("limine-binary/" + name))
    print(f"Limine 12.9.0 prepared at {root}")


def doctor(root, llvm_bin):
    print("source=" + subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=PROJECT, text=True).strip())
    dirty = subprocess.check_output(["git", "status", "--porcelain"], cwd=PROJECT, text=True)
    print(f"working-tree-dirty={bool(dirty)}; host={sys.platform}; Python={sys.version.split()[0]}")
    for name in ("clang", "ld.lld", "llvm-objcopy", "llvm-readelf", "llvm-nm"):
        executable = tool(name, llvm_bin)
        print(executable, flush=True)
        run([executable, "--version"])
    run([qemu_command(root)[0], "--version"])
    version = importlib.metadata.version("pycdlib")
    if version != "1.20.0":
        raise RuntimeError(f"Expected pycdlib 1.20.0, found {version}")
    print("pycdlib=" + version)
    for name, expected in LIMINE_FILES.items():
        asset = root / "limine-12.9.0/limine-binary" / name
        digest = hashlib.sha256(asset.read_bytes()).hexdigest()
        if digest != expected: raise RuntimeError(f"Limine asset changed: {asset}")
        print(str(asset) + " sha256=" + digest)


def build(project, llvm_bin, stage, fault=1, attack=0):
    if stage != "M6" and (fault != 1 or attack != 0):
        raise RuntimeError("Additional faults and counterexamples belong to M6")
    level = 7 if stage == "LAB" else int(stage[1:])
    output = project / "build"
    output.mkdir(parents=True, exist_ok=True)
    clang, linker = tool("clang", llvm_bin), tool("ld.lld", llvm_bin)
    # Same freestanding recipe as build.ps1; .S uses Clang's integrated assembler.
    flags = ["--target=x86_64-unknown-none-elf", "-Wall", "-Wextra", "-Werror",
             "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-pic", "-fno-pie",
             "-mno-red-zone", "-mno-sse", "-mno-mmx", "-mno-80387", "-O0", "-g",
             "-I", str(project / "include")]
    defines = [f"-DMINILINUX_LEVEL={level}", f"-DMINILINUX_PROBE={fault}",
               f"-DMINILINUX_ATTACK={attack}"]

    def compile_source(source, destination, model="kernel", frame_pointer=True, definitions=defines):
        language = ["-std=c11"] if source.suffix == ".c" else []
        frame = ["-fno-omit-frame-pointer"] if frame_pointer else []
        run([clang, *language, *flags, *frame, f"-mcmodel={model}", *definitions,
             "-c", source, "-o", destination], project)

    sources = ["kernel/main.c"]
    if attack: sources += ["kernel/counterexample.c"]
    if level >= 1: sources += ["kernel/memory/page.c", "kernel/memory/page_test.c"]
    if level >= 2: sources += ["kernel/bytes.c", "kernel/memory/vm.c", "kernel/memory/vm_test.c"]
    if level >= 3:
        sources += ["kernel/cpu.c", "kernel/interrupt.c", "kernel/entry.S"]
        if level != 7: sources += ["kernel/task.c"]
    if level >= 4:
        source = project / ("user/laboratory.c" if level == 7 else "user/program.c")
        user_objects = [output / "user-program.o"]
        compile_source(source, user_objects[0], "small")
        if level == 7:
            user_objects += [output / "user-bytes.o"]
            compile_source(project / "kernel/bytes.c", user_objects[-1], "small", False, [])
        run([linker, "-m", "elf_x86_64", "-nostdlib", "-static", "-T", project / "user/linker.ld",
             "-o", output / "user.elf", *user_objects], project)
        run([tool("llvm-objcopy", llvm_bin), "-O", "binary", output / "user.elf", output / "user.bin"], project)
        data = (output / "user.bin").read_bytes()
        if not 0 < len(data) <= (65536 if level == 7 else 8192):
            raise RuntimeError("User image exceeds its fixed code page limit")
        values = ",".join(f"0x{byte:02x}" for byte in data)
        (output / "user-image.c").write_text(
            "#include <stdint.h>\nconst unsigned char user_image[] = {" + values + "};\n"
            + f"const uint64_t user_image_size = {len(data)};\n", encoding="ascii")
        sources += ["kernel/usercopy.c", "build/user-image.c"]
        sources += ["kernel/laboratory.c"] if level == 7 else ["kernel/user.c", "kernel/syscall.c"]
    if level >= 5: sources += ["kernel/ramfs.c"]
    objects = []
    for relative in sources:
        source = project / relative
        objects.append(output / (source.stem + ".o"))
        compile_source(source, objects[-1])
    run([linker, "-m", "elf_x86_64", "-nostdlib", "-static", "-z", "max-page-size=0x1000",
         "-T", project / "linker.ld", "-o", output / "kernel.elf", *objects], project)
    for name in ("kernel.elf", "user.elf") if level >= 4 else ("kernel.elf",):
        elf = output / name
        undefined = subprocess.check_output([tool("llvm-nm", llvm_bin), "--undefined-only", str(elf)])
        segments = subprocess.check_output([tool("llvm-readelf", llvm_bin), "-l", str(elf)])
        if undefined.strip() or b"INTERP" in segments or b"DYNAMIC" in segments:
            raise RuntimeError(f"Unexpected runtime dependency in {name}")
    digest = hashlib.sha256((output / "kernel.elf").read_bytes()).hexdigest()
    print(f"Built {stage} fault={fault} attack={attack}: {output / 'kernel.elf'} sha256={digest}", flush=True)


def image(project, root, stage):
    import pycdlib
    import runpy
    make_image = runpy.run_path(str(PROJECT / "tools/make-image.py"))["main"]
    make_image(project, root, stage)
    # Compare bytes in memory: a familiar serial message cannot identify the ELF.
    iso = pycdlib.PyCdlib()
    iso.open(str(root / "images" / f"minilinux-{stage.lower()}.iso"))
    try:
        for path, source in (("/boot/kernel.elf", project / "build/kernel.elf"),
                             ("/limine.conf", project / "boot/limine.conf"),
                             ("/limine-bios.sys", root / "limine-12.9.0/limine-binary/limine-bios.sys")):
            payload = io.BytesIO()
            iso.get_file_from_iso_fp(payload, rr_path=path)
            if payload.getvalue() != source.read_bytes():
                raise RuntimeError(f"ISO payload differs from this build: {source.name}")
    finally:
        iso.close()
    print("ISO payload matches this build.", flush=True)


def serial_check(root, stage, memory, marker="", case=None):
    output = PROJECT / "build/linux-checks"
    output.mkdir(parents=True, exist_ok=True)
    label = f"{(case or stage).lower()}-{memory}m"
    serial = output / f"{label}-serial.log"
    serial.write_bytes(b"")
    args = qemu_command(root) + ["-machine", "pc", "-m", str(memory), "-smp", "1", "-accel", "tcg",
            "-display", "none", "-serial", f"file:{serial}", "-monitor", "none", "-no-reboot",
            "-boot", "d", "-cdrom", str(root / "images" / f"minilinux-{stage.lower()}.iso")]
    with (output / f"{label}-qemu.log").open("wb") as errors:
        process = subprocess.Popen(args, cwd=root, stdout=errors, stderr=errors,
                                   creationflags=0x08000000 if os.name == "nt" else 0)
        try:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                text = serial.read_text(encoding="ascii", errors="replace")
                if "MiniLinux PANIC:" in text or process.poll() is not None: break
                time.sleep(0.05)
        finally:
            if process.poll() is None:
                process.terminate()
                try: process.wait(timeout=3)
                except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=3)
    text = serial.read_text(encoding="ascii", errors="replace")
    base_stage = stage
    witnesses = [f"MiniLinux {base_stage}: entered kernel_main",
                 f"MiniLinux PANIC: {base_stage} reached its intentional stop"]
    level = int(base_stage[1:])
    if level: witnesses.append(f"MiniLinux {base_stage}: {CHECKS[level]} checks passed")
    if marker: witnesses.append(marker)
    if any(witness not in text for witness in witnesses):
        raise RuntimeError(f"Serial check failed: {label}; inspect {serial}")
    print(f"SERIAL PASS: {label}", flush=True)


def check(root, llvm_bin):
    doctor(root, llvm_bin)
    for stage in STAGES[:-1]:
        build(PROJECT, llvm_bin, stage)
        image(PROJECT, root, stage)
        for memory in (64, 256): serial_check(root, stage, memory)
    for kind, cases in (("attack", range(1, 6)), ("fault", range(2, 8))):
        for case in cases:
            build(PROJECT, llvm_bin, "M6", fault=case if kind == "fault" else 1,
                  attack=case if kind == "attack" else 0)
            image(PROJECT, root, "M6")
            # Distinct logs keep earlier witnesses while the image is rebuilt.
            label = f"M6-{kind}-{case}"
            for memory in (64, 256):
                serial_check(root, "M6", memory, ATTACKS[case] if kind == "attack" else FAULTS[case], label)
                if kind == "attack" and case == 3:
                    text = (PROJECT / f"build/linux-checks/{label.lower()}-{memory}m-serial.log").read_text()
                    if any(f"OOM with spare pages={spare} rejected" not in text for spare in range(3)):
                        raise RuntimeError("An OOM depth was not checked")
    build(PROJECT, llvm_bin, "M6")
    image(PROJECT, root, "M6")
    build(PROJECT, llvm_bin, "LAB")
    image(PROJECT, root, "LAB")
    for script in ("test-userland.py", "test-lab-bridge.py"):
        run([sys.executable, "-B", PROJECT / "tools" / script, "--tool-root", root])
    output = PROJECT / "build/supplemental"
    script = PROJECT / "tools/test-lab-boundaries.py"
    run([sys.executable, "-B", script, "--mode", "baseline", "--project", PROJECT,
         "--tool-root", root, "--output", output])
    # The retained probe changes only a disposable copy, never the real userland.
    with tempfile.TemporaryDirectory(prefix="linux-boundaries-", dir=PROJECT / "build") as temporary:
        fixture = Path(temporary)
        for name in ("boot", "include", "kernel", "user"):
            shutil.copytree(PROJECT / name, fixture / name)
        shutil.copyfile(PROJECT / "linker.ld", fixture / "linker.ld")
        patch = PROJECT / "docs/evidence/experience-regression/fixture.patch"
        original = (fixture / "user/laboratory.c").read_bytes()
        # git apply inside a repository subdirectory can silently skip all hunks.
        # Run from the root and explicitly name the disposable target directory.
        run(["git", "apply", "--no-index", "--exclude=tools/*",
             "--directory=" + fixture.relative_to(PROJECT).as_posix(), patch])
        if (fixture / "user/laboratory.c").read_bytes() == original:
            raise RuntimeError("The retained user probe patch did not change the fixture")
        runtime = fixture / "runtime"
        shutil.copytree(root / "limine-12.9.0", runtime / "limine-12.9.0")
        build(fixture, llvm_bin, "LAB")
        image(fixture, runtime, "LAB")
        run([sys.executable, "-B", script, "--mode", "fixture", "--project", fixture,
             "--tool-root", runtime, "--output", output])
    print("LINUX CHECK PASS: M0–M6 serial, attacks/faults, shared LAB and supplemental probes.\n"
          "GDB mutation checks and monitor components are outside this command.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("prepare", "doctor", "build", "run", "lab", "check"))
    parser.add_argument("--tool-root", type=Path, default=PROJECT / ".tools/linux")
    parser.add_argument("--llvm-bin", type=Path)
    parser.add_argument("--limine-zip", type=Path, help="Offline prepare with the same verified archive")
    parser.add_argument("--stage", choices=STAGES, default="LAB")
    parser.add_argument("--fault", type=int, choices=range(1, 8), default=1)
    parser.add_argument("--attack", type=int, choices=range(6), default=0)
    parser.add_argument("--memory", type=int, choices=(64, 256), default=64)
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    root = args.tool_root.resolve()
    if args.command == "prepare": prepare(root, args.limine_zip)
    elif args.command == "doctor": doctor(root, args.llvm_bin)
    elif args.command == "check": check(root, args.llvm_bin)
    else:
        build(PROJECT, args.llvm_bin, args.stage, args.fault, args.attack)
        image(PROJECT, root, args.stage)
        if args.command == "run":
            if args.stage == "LAB":
                run([sys.executable, "-B", PROJECT / "tools/lab_guest.py", "--tool-root", root,
                     "--memory", args.memory])
            else: serial_check(root, args.stage, args.memory)
        elif args.command == "lab":
            if args.stage != "LAB": raise RuntimeError("The browser LAB needs --stage LAB")
            run([sys.executable, "-B", PROJECT / "tools/lab-server.py", "--tool-root", root,
                 "--memory", args.memory, "--port", args.port])


if __name__ == "__main__":
    try: main()
    except KeyboardInterrupt: raise SystemExit(130)
    except (OSError, RuntimeError, subprocess.CalledProcessError,
            importlib.metadata.PackageNotFoundError) as exc:
        raise SystemExit(f"{type(exc).__name__}: {exc}\nSee docs/LINUX.md; keep the first failure log.")
