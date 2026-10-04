"""Check local Markdown links and disposable outputs before the manual closeout."""
import re
import os
import subprocess
from pathlib import Path
from urllib.parse import unquote, urlsplit

PROJECT = Path(__file__).resolve().parents[1]


def main():
    paths = subprocess.check_output(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=PROJECT).decode("utf-8").split("\0")
    errors = []
    checked = 0
    for name in sorted(set(paths)):
        path = PROJECT / name
        if path.suffix != ".md" or not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        text = re.sub(r"(?ms)^(```|~~~).*?^\1[^\n]*$", "", text)
        for target in re.findall(r"!?\[[^\]\n]*\]\(([^)\n]+)\)", text):
            target = target.strip()
            if target.startswith("<"):
                target = target.split(">", 1)[0][1:]
            else:
                target = target.split(' "', 1)[0]
            url = urlsplit(target)
            if url.scheme or url.netloc or not url.path:
                continue
            local = path.parent / unquote(url.path)
            checked += 1
            if not local.is_file() and not local.is_dir():
                errors.append(f"{name}: missing {target}")

    build = PROJECT / "build"
    if build.exists() and any(build.iterdir()):
        errors.append("build/ still contains outputs; classify and close them out first")
    for parent, directories, _ in os.walk(PROJECT, followlinks=False):
        scan = []
        for name in directories:
            path = Path(parent) / name
            if name == "__pycache__":
                errors.append(f"disposable cache: {path.relative_to(PROJECT)}")
            if name != ".git" and not path.is_symlink() and not (
                    getattr(path.lstat(), "st_file_attributes", 0) & 1024):
                scan.append(name)
        directories[:] = scan
    print(f"Local inline Markdown links: {checked} checked.")
    if errors:
        raise SystemExit("\n".join(errors))
    print("Build/cache residue: none.")
    print("Owner, proof coverage, unique local state, and external environments still need manual review.")


if __name__ == "__main__":
    main()
