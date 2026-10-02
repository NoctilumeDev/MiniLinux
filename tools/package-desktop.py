"""Build a small Windows launch ZIP; QEMU is fetched from its vendor on first use."""
import argparse
import hashlib
import json
import shutil
import subprocess
import zipfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package(output, iso, extractor, extractor_source, limine_license):
    root = output / "MiniLinux-LAB"
    if root.exists():
        raise ValueError("Choose a fresh output directory; existing packages are preserved")
    (root / "runtime/images").mkdir(parents=True)
    (root / "runtime/extractor").mkdir()
    (root / "tools").mkdir()
    shutil.copytree(PROJECT / "web", root / "web")
    for name in ("lab_guest.py", "lab-server.py", "desktop-launch.py", "prepare-desktop.ps1"):
        shutil.copy2(PROJECT / "tools" / name, root / "tools" / name)
    shutil.copy2(iso, root / "runtime/images/minilinux-lab.iso")
    for name in ("7z.exe", "7z.dll", "License.txt"):
        shutil.copy2(extractor / name, root / "runtime/extractor" / name)
    shutil.copy2(extractor_source, root / "runtime/extractor" / extractor_source.name)
    shutil.copy2(limine_license, root / "runtime/images/LIMINE-LICENSE.txt")
    source = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=PROJECT, text=True).strip()
    manifest = {"format": 1, "source": source, "iso_sha256": digest(iso),
                "app_sha256": digest(root / "web/app.js"),
                "extractor_exe_sha256": digest(extractor / "7z.exe"),
                "extractor_dll_sha256": digest(extractor / "7z.dll")}
    (root / "package.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    launcher = '@echo off\r\ncd /d "%~dp0"\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\\prepare-desktop.ps1"\r\nif errorlevel 1 goto failed\r\n"%~dp0runtime\\python\\python.exe" -I "%~dp0tools\\desktop-launch.py"\r\nif errorlevel 1 goto failed\r\nexit /b 0\r\n:failed\r\necho LAB could not start. Keep this message for troubleshooting.\r\npause\r\n'
    (root / "Start.cmd").write_bytes(launcher.encode("ascii"))
    (root / "READ-ME.txt").write_text(
        "MiniLinux / Windows LAB preview\n\n"
        "1. Extract this entire ZIP to a writable folder on Windows 10/11 x64.\n"
        "2. Double-click Start.cmd. The first start downloads about 220 MB of official\n"
        "   Python/QEMU archives and unpacks them inside runtime/. Internet is required.\n"
        "   No compiler, GDB, WSL or existing Python/QEMU installation is needed.\n"
        "3. A browser opens the actual 64 MiB QEMU guest. Try help, cat hello.txt,\n"
        "   run reader, run counter-a counter-b, then run fault.\n\n"
        "Keep the launcher window open. Ctrl+C closes LAB and its owned QEMU.\n"
        "stop guest stops QEMU; restart guest starts a fresh guest.\n"
        "This is a teaching preview with fixed programs and files, not a Linux distro.\n"
        "If downloads need a proxy, run in PowerShell:\n"
        "  .\\tools\\prepare-desktop.ps1 -Proxy http://127.0.0.1:7897\n"
        "then start again. Runtime downloads are cached for later offline starts.\n\n"
        "Python: PSF license, included in the vendor archive.\n"
        "QEMU: downloaded directly from qemu.weilnetz.de; upstream notices are retained.\n"
        "7-Zip: Igor Pavlov, see runtime/extractor/License.txt and the included source.\n"
        "Limine: runtime/images/LIMINE-LICENSE.txt. Tux: web/assets/TUX-README.md.\n",
        encoding="utf-8")
    archive = output / "MiniLinux-LAB-Windows-x64.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package_zip:
        for path in sorted(root.rglob("*")):
            if path.is_file(): package_zip.write(path, path.relative_to(output))
    print(f"{archive} SHA256 {digest(archive)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("output", "iso", "extractor", "extractor-source", "limine-license"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    package(args.output, args.iso, args.extractor, args.extractor_source, args.limine_license)
