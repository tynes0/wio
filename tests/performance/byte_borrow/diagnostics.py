"""Backend failures must preserve long physical lines instead of crashing the reporter."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[3]


def verify(args):
    folder = args.output.resolve() / uuid.uuid4().hex
    folder.mkdir(parents=True)
    source = folder / "main.wio"
    source.write_text('using cpp::header("failure.h");\nfn Entry() -> i32\n{\n    return 0;\n}\n', encoding="utf-8")
    version = subprocess.run([args.cxx, "--version"], capture_output=True, text=True, check=True).stdout.lower()
    limit = "-ferror-limit=1" if "clang" in version else "-fmax-errors=1"
    rows = []
    for length in (12, 16384):
        marker = "n04-backend-diagnostic-start-" + "x" * length + "-end"
        (folder / "failure.h").write_text("#error " + marker + "\n", encoding="utf-8")
        command = [str(args.wio.resolve()), str(source), "--include-dir", str(folder),
                   "--output", str(folder / "unused.exe"), "--backend-arg=" + limit,
                   "--backend-arg=-fmessage-length=0"]
        result = subprocess.run(command, cwd=folder, env=dict(os.environ, WIO_ROOT=str(ROOT)),
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
        output = result.stdout + result.stderr
        row = dict(command=command, markerLength=len(marker), exitCode=result.returncode,
                   stdout=result.stdout, stderr=result.stderr)
        rows.append(row)
        passed = result.returncode == 1 and marker in output and "Unhandled compiler failure" not in output
        (folder / "results.json").write_text(json.dumps(dict(passed=passed, processes=rows), indent=2) + "\n")
        if not passed:
            raise RuntimeError(str(row))
    print(folder / "results.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
