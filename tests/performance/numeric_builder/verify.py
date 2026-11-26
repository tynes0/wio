"""New N03 numeric builder gate: exact types, snapshots, native O0/O2 and VM."""
import argparse
import json
import os
import re
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
    shutil.copyfile(HERE / "reject.wio", folder / "reject.wio")
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
    unit = folder / ("numeric-unit.exe" if os.name == "nt" else "numeric-unit")
    # The allocator diagnostic and its custom C++ locale must share one runtime
    # image on MinGW; do not mix replacement new/delete with a foreign CRT DLL.
    unit_links = ["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else []
    run([args.cxx, "-std=c++20", "-O2", ROOT / "tests/numeric_builder_test.cpp",
         f"-I{ROOT / 'runtime/include'}", *unit_links, "-o", unit])
    run([unit])
    lowered = folder / "numeric.lwir"
    run([wio, folder / "main.wio", "--emit-lowered-wir", "--ir-output", lowered])
    bodies = re.findall(r'^  func [^\n]*"std_builders::Append\$specialized[^\n]*\n(.*?)^  }',
                        lowered.read_text(encoding="utf-8"), re.M | re.S)
    if len(bodies) < 13 or any("interpolate " in body or 'intrinsic-call string::"Append"' not in body for body in bodies):
        raise RuntimeError("Numeric builder specializations retained interpolation or lost direct append")
    run([wio, folder / "main.wio", "--emit-cpp"])
    generated = folder / "main.wio.cpp"
    if "wio::intrinsics::StringAppend" not in generated.read_text(encoding="utf-8"):
        raise RuntimeError("Direct numeric append was not emitted")
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    for optimization in ("-O0", "-O2"):
        program = folder / ("native.exe" if os.name == "nt" else "native")
        run([args.cxx, "-std=c++20", optimization, "-ffp-contract=off", generated,
             f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--output", bridge,
         "--bytecode-sidecar", bytecode])
    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge])
    rejected = run([wio, "file", "check", folder / "reject.wio"], expected=1)
    if "rejects type arguments <string>" not in rejected.stdout + rejected.stderr:
        raise RuntimeError("Expected semantic numeric constraint diagnostic")
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print("N03 numeric builder: exact primitive types, snapshots/capacity, native O0/O2 + VM, semantic rejection")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
