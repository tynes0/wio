"""Only the new indexed-format scratch and precision/error contract."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def verify(args):
    folder = args.output.resolve() / uuid.uuid4().hex
    folder.mkdir(parents=True)
    rows = []
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    for optimization in ("-O0", "-O2"):
        program = folder / ("contract.exe" if os.name == "nt" else "contract")
        for command in ([args.cxx, "-std=c++20", optimization, HERE / "contract.cpp",
                         f"-I{ROOT / 'runtime/include'}", args.runtime.resolve(), *links, "-o", program], [program]):
            row = dict(command=[str(item) for item in command])
            rows.append(row)
            result = subprocess.run(row["command"], cwd=folder, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=300)
            row.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
            (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
            if result.returncode:
                raise RuntimeError(str(row))
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print("Indexed format O0/O2: numeric extrema, padding/alignment, custom patterns, NUL, errors and poisoned scratch")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
