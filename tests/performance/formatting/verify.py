"""N03 new-only gate: Wio formatting/containers/snapshots, native O0/O2 + VM."""
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
    shutil.copyfile(HERE / "stream_probe.h", folder / "stream_probe.h")
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
    native_probe = folder / ("format-probe.exe" if os.name == "nt" else "format-probe")
    run([args.cxx, "-std=c++20", "-O2", ROOT / "tests/floating_value_format_test.cpp",
         f"-I{ROOT / 'runtime/include'}", "-o", native_probe])
    run([native_probe])
    run([wio, folder / "main.wio", "--emit-cpp", "--include-dir", folder])
    generated = folder / "main.wio.cpp"
    source = generated.read_text(encoding="utf-8")
    if "AppendBasicFormatted(target, value)" not in source or "StringifyFloating(value)" not in source:
        raise RuntimeError("New formatting helpers were not emitted")
    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    for optimization in ("-O0", "-O2"):
        program = folder / ("native.exe" if os.name == "nt" else "native")
        run([args.cxx, "-std=c++20", optimization, "-ffp-contract=off", generated,
             f"-I{folder}", f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--include-dir", folder, "--output", bridge,
         "--bytecode-sidecar", bytecode])
    run([wio, "--vm-run-bytecode", bytecode, "--native-library", bridge])
    shutil.copyfile(HERE / "custom.wio", folder / "custom.wio")
    run([wio, folder / "custom.wio", "--emit-cpp"])
    for optimization in ("-O0", "-O2"):
        program = folder / ("custom.exe" if os.name == "nt" else "custom")
        run([args.cxx, "-std=c++20", optimization, folder / "custom.wio.cpp",
             f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(),
             *links, "-o", program])
        run([program])
    custom_bridge = folder / ("custom.dll" if os.name == "nt" else "custom.dylib" if sys.platform == "darwin" else "custom.so")
    custom_bytecode = folder / "custom.wiob"
    run([wio, folder / "custom.wio", "--target", "shared", "--native-bridge-only", "--output", custom_bridge,
         "--bytecode-sidecar", custom_bytecode])
    run([wio, "--vm-run-bytecode", custom_bytecode, "--native-library", custom_bridge])
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print("N03 native O0/O2 + VM: float precision/signed zero, scalar/nested containers, char/NUL and builder snapshots")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--wio", type=Path, required=True)
    p.add_argument("--runtime", type=Path, required=True)
    p.add_argument("--cxx", required=True)
    p.add_argument("--output", type=Path, required=True)
    verify(p.parse_args())
