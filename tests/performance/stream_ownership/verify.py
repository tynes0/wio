"""N03 stream-result ownership: Wio native O0/O2 and VM native-bridge parity."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def verify(args):
    folder = args.output.resolve() / uuid.uuid4().hex
    folder.mkdir(parents=True)
    shutil.copyfile(HERE / "semantics.wio", folder / "main.wio")
    shutil.copyfile(HERE / "probe.h", folder / "probe.h")
    rows = []

    def run(command):
        row = dict(command=[str(item) for item in command])
        rows.append(row)
        result = subprocess.run(row["command"], cwd=folder, env=dict(os.environ, WIO_ROOT=str(ROOT)),
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
        row.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
        if result.returncode:
            raise RuntimeError(str(row))

    wio = args.wio.resolve()
    run([wio, folder / "main.wio", "--emit-cpp", "--include-dir", folder])
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    for optimization in ("-O0", "-O2"):
        program = folder / ("native.exe" if os.name == "nt" else "native")
        run([args.cxx, "-std=c++20", optimization, folder / "main.wio.cpp",
             f"-I{folder}", f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--include-dir", folder,
         "--output", bridge, "--bytecode-sidecar", bytecode])
    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge])
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print("N03 stream ownership: native O0/O2 + VM bridge, counted NUL/Unicode, snapshots and i64")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
