"""Export the same vanilla UI with a recording transport for static hosting."""
import argparse
import json
import shutil
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]


def export(recording, output):
    data = json.loads(recording.read_text(encoding="utf-8"))
    if data["format"] != 1 or data["mode"] != "recorded-replay":
        raise ValueError("Expected a real LAB recording")
    output.mkdir(parents=True, exist_ok=True)
    for name in ("app.js", "replay.js", "style.css"):
        shutil.copy2(PROJECT / "web" / name, output / name)
    shutil.copytree(PROJECT / "web/assets", output / "assets", dirs_exist_ok=True)
    index = (PROJECT / "web/index.html").read_text(encoding="utf-8")
    index = index.replace('src="live.js"', 'src="replay.js"')
    index = index.replace("MiniLinux — mechanism laboratory", "MiniLinux — recorded replay")
    (output / "index.html").write_text(index, encoding="utf-8")
    shutil.copy2(recording, output / "recording.json")
    (output / ".nojekyll").touch()
    print(f"Static replay exported to {output}; source {data['source']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--recording", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    export(args.recording, args.output)
