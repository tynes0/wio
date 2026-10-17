"""New-only scalar formatting-call gate: WIR dispatch, native O0/O2 and VM."""
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
    rows = []

    def run(command, expected=0):
        row = dict(command=[str(item) for item in command], expectedExitCode=expected)
        rows.append(row)
        result = subprocess.run(row["command"], cwd=folder, env=dict(os.environ, WIO_ROOT=str(ROOT)),
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
        row.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
        if result.returncode != expected:
            raise RuntimeError(str(row))
        return result

    wio = args.wio.resolve()
    lowered = folder / "calls.lwir"
    run([wio, folder / "main.wio", "--emit-lowered-wir", "--ir-output", lowered])
    ir = lowered.read_text(encoding="utf-8")
    for required in ('"format.null"', '"format.call"', '"format.merge"', '::"ToString"',
                     "interface-call", "std::ToString$specialized"):
        if required not in ir:
            raise RuntimeError("Missing shared formatting-call IR: " + required)
    run([wio, folder / "main.wio", "--emit-cpp"])
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    for optimization in ("-O0", "-O2"):
        program = folder / ("native.exe" if os.name == "nt" else "native")
        run([args.cxx, "-std=c++20", optimization, folder / "main.wio.cpp",
             f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--output", bridge,
         "--bytecode-sidecar", bytecode])
    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge])
    # The old object-array fixture deliberately remains unsupported in VM.
    # Its scalar hook now succeeds; the array hole must not be called parity.
    shutil.copyfile(HERE.parent / "formatting/custom.wio", folder / "container.wio")
    container_bridge = folder / ("container.dll" if os.name == "nt" else "container.dylib" if sys.platform == "darwin" else "container.so")
    container_bytecode = folder / "container.wiob"
    run([wio, folder / "container.wio", "--target", "shared", "--native-bridge-only", "--output", container_bridge,
         "--bytecode-sidecar", container_bytecode])
    rejected = run([wio, "--vm-run-bytecode", container_bytecode, "--native-library", container_bridge], expected=1)
    if "WVM1062" not in rejected.stderr:
        raise RuntimeError("Expected explicit remaining user-container formatting boundary")
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print("N03 scalar ToString: ordered/null-safe dispatch, view/interface/generic calls, native O0/O2 + VM")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
