"""Package an already built LAB for Linux; playing needs no compiler or pip."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile

PROJECT = Path(__file__).resolve().parents[1]


def package(output, iso, limine_license):
    import pycdlib
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=PROJECT).strip():
        raise RuntimeError("Commit source changes before packaging")
    source = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=PROJECT, text=True).strip()
    root = output / "MiniLinux-LAB"
    archive = output / "MiniLinux-LAB-Linux-x86_64.tar.gz"
    if root.exists() or archive.exists(): raise RuntimeError("Choose a fresh package output directory")
    image = pycdlib.PyCdlib()
    image.open(str(iso))
    try:
        payload = io.BytesIO()
        image.get_file_from_iso_fp(payload, rr_path="/boot/kernel.elf")
        if payload.getvalue() != (PROJECT / "build/kernel.elf").read_bytes():
            raise RuntimeError("ISO kernel differs from the current build; rebuild LAB before packaging")
    finally:
        image.close()
    (root / "runtime/images").mkdir(parents=True)
    (root / "tools").mkdir()
    (root / "docs").mkdir()
    shutil.copy2(PROJECT / "docs/USERLAND.md", root / "docs/USERLAND.md")
    shutil.copytree(PROJECT / "web", root / "web")
    for name in ("qemu_host.py", "lab_guest.py", "lab-server.py", "desktop-launch.py"):
        shutil.copy2(PROJECT / "tools" / name, root / "tools" / name)
    shutil.copy2(iso, root / "runtime/images/minilinux-lab.iso")
    shutil.copy2(limine_license, root / "runtime/images/LIMINE-LICENSE.txt")
    manifest = {"format": 1, "source": source, "platform": "Linux x86_64",
                "iso_sha256": hashlib.sha256(iso.read_bytes()).hexdigest()}
    (root / "package.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (root / "Start.sh").write_text(
        '#!/bin/sh\nset -eu\ncd "$(dirname "$0")"\n'
        'if ! command -v python3 >/dev/null 2>&1; then\n'
        '  echo "Install Python 3.10+, QEMU 6.2+ and SeaBIOS; see READ-ME.txt." >&2\n'
        '  exit 1\nfi\n'
        'exec python3 -B tools/desktop-launch.py --no-browser "$@"\n', encoding="utf-8")
    (root / "Start.sh").chmod(0o755)
    (root / "READ-ME.txt").write_text(
        "MiniLinux / Linux x86_64 LAB preview\n\n"
        "1. Ubuntu/Debian 先准备运行工具（已有则不需重复安装）：\n"
        "   sudo apt-get update\n"
        "   sudo apt-get install --no-install-recommends python3 qemu-system-x86 seabios\n"
        "   需要 Python 3.10+、QEMU 6.2+；不需要 Clang、LLD、GDB、pip 或 venv。\n"
        "2. 解压整个档案，进入 MiniLinux-LAB，运行：sh Start.sh\n"
        "3. 手动打开终端显示的本次 http://127.0.0.1:端口/ 网址。\n"
        "   端口自动选择，不会弹出或切换你的浏览器。\n"
        "4. 试 cat hello.txt、run counter-a counter-b、run fault；故障后 shell 仍可操作。\n"
        "   保持启动窗口；Ctrl+C 关闭 LAB 与其 QEMU。关闭网页不会关闭客体。\n\n"
        "包内已经有启动映像；首次启动也不下载、编译或安装东西。\n"
        "QEMU 使用 qemu64,-apic、1 CPU、64 MiB、TCG；不需要 KVM 或 root。\n"
        "系统 QEMU 和 Python 由发行版管理，本包不会修改它们。\n"
        "云主机需要 SSH 端口转发：ssh -N -L 8088:127.0.0.1:远端显示的端口 用户@主机\n"
        "然后在自己的浏览器打开 http://127.0.0.1:8088/ 。\n\n"
        "This is a real teaching guest, not a replay or a Linux distribution.\n"
        "Install Python 3.10+, qemu-system-x86_64 6.2+ and SeaBIOS using your distribution.\n"
        "Run sh Start.sh and open the printed loopback URL. Ctrl+C closes LAB and its QEMU.\n"
        "The included ISO needs no compiler, Python packages or network access at runtime.\n"
        "Source: https://github.com/NoctilumeDev/MiniLinux/tree/" + source + "\n"
        "Limine license: runtime/images/LIMINE-LICENSE.txt; Tux: web/assets/TUX-README.md.\n",
        encoding="utf-8")
    with tarfile.open(archive, "w:gz") as target:
        target.add(root, arcname=root.name)
    print(f"{archive} sha256={hashlib.sha256(archive.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("output", "iso", "limine-license"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    package(args.output, args.iso, args.limine_license)
