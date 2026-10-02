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
    pending = subprocess.check_output(["git", "status", "--porcelain"], cwd=PROJECT, text=True)
    if pending.strip():
        raise ValueError("Commit changes before packaging so the manifest names the actual source")
    source = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=PROJECT, text=True).strip()
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
    manifest = {"format": 1, "source": source, "iso_sha256": digest(iso),
                "app_sha256": digest(root / "web/app.js"),
                "extractor_exe_sha256": digest(extractor / "7z.exe"),
                "extractor_dll_sha256": digest(extractor / "7z.dll")}
    (root / "package.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    # The native Windows PowerShell must load its own modules even when started
    # from a terminal that inherited a different PowerShell's module path.
    launcher = "\r\n".join([
        "@echo off", "setlocal",
        'set "PSModulePath=%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\Modules"',
        'cd /d "%~dp0"',
        'set "LAB_PROXY=%~1"',
        'set "LAB_BROWSER_OPTION="',
        'if "%~1"=="--no-browser" set "LAB_PROXY="',
        'if "%~1"=="--no-browser" set "LAB_BROWSER_OPTION=--no-browser"',
        'if "%~2"=="--no-browser" set "LAB_BROWSER_OPTION=--no-browser"',
        'if "%LAB_PROXY%"=="" (',
        '  "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\\prepare-desktop.ps1"',
        ') else (',
        '  "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\\prepare-desktop.ps1" -Proxy "%LAB_PROXY%"',
        ')',
        "if errorlevel 1 goto failed",
        '"%~dp0runtime\\python\\python.exe" -I "%~dp0tools\\desktop-launch.py" %LAB_BROWSER_OPTION%',
        "if errorlevel 1 goto failed", "exit /b 0", ":failed",
        "echo LAB could not start. See READ-ME.txt for retry and proxy instructions.",
        "echo Keep the error above when reporting a problem.", "pause", ""
    ])
    (root / "Start.cmd").write_bytes(launcher.encode("ascii"))
    (root / "READ-ME.txt").write_text(
        "MiniLinux / Windows LAB preview\n\n"
        "中文快速开始\n"
        "1. 需要 Windows 10/11 x64。解压整个 ZIP，进入 MiniLinux-LAB 文件夹。\n"
        "2. 双击 Start.cmd。首次联网下载约 220 MB 的官方 Python/QEMU 档案，\n"
        "   核验后只解包到本文件夹的 runtime；以后启动会复用这里的缓存。\n"
        "   无需预装 Python/QEMU、编译器、GDB 或 WSL。阶段提示会显示当前准备步骤。\n"
        "3. 浏览器会打开真实 QEMU 客体。先输入 help、cat hello.txt、run reader，\n"
        "   再输入 run counter-a counter-b；结束后点“查看最近 counter 快照”比较两个PID。\n"
        "   快照只固定观察窗，不会暂停客体。run fault 只退出演示程序，shell 仍能接受命令。\n"
        "4. stop guest 停止客体；restart guest 从头启动。关闭网页不会关闭 LAB。\n"
        "   保留启动窗口，用 Ctrl+C 关闭 LAB 和它自己的 QEMU。\n\n"
        "如需代理：在本文件夹（能看到 Start.cmd）的资源管理器地址栏输入 powershell，\n"
        "按 Enter，然后输入下列命令，把地址替换成你的代理：\n"
        "  .\\Start.cmd http://127.0.0.1:7897\n"
        "下载失败可以按同样方法重试；不修改全局代理或系统模块配置。\n"
        "如果没有自动打开浏览器，请复制启动窗口显示的 http://127.0.0.1:端口/ 网址。\n"
        "下次打开的端口可能变化，请使用这次显示的新网址。\n"
        "若不想自动打开默认浏览器，可运行 .\\Start.cmd --no-browser，手动打开显示的网址。\n"
        "同时需要代理时：.\\Start.cmd http://127.0.0.1:7897 --no-browser。\n"
        "这是固定程序和文件的教学体验，不是 Linux 发行版。\n\n"
        "English quick start\n"
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
        "  .\\Start.cmd http://127.0.0.1:7897\n"
        "Replace that URL with your own proxy. No global setting is changed.\n"
        "Append --no-browser to leave the default browser alone and open the printed URL yourself.\n"
        "A failed download can be retried with the same command. Verified archives\n"
        "are cached for later offline starts.\n\n"
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
