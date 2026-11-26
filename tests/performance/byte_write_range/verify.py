"""Check borrowed range writes in native O0/O2 and VM bridges, plus heap and bounds contracts."""
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
    for source, destination in (("semantics.wio", "main.wio"), ("probe.h", "probe.h"), ("contract.cpp", "contract.cpp")):
        shutil.copyfile(HERE / source, folder / destination)
    rows = []

    def run(command, expected=0, contains=None):
        row = dict(command=[str(item) for item in command], expectedExitCode=expected)
        rows.append(row)
        result = subprocess.run(row["command"], cwd=folder, env=dict(os.environ, WIO_ROOT=str(ROOT)),
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
        row.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
        (folder / "results.json").write_text(json.dumps(dict(passed=False, processes=rows), indent=2) + "\n")
        if result.returncode != expected or (contains and contains not in result.stdout + result.stderr):
            raise RuntimeError(str(row))

    links = ["-lws2_32", "-lshell32", "-Wa,-mbig-obj"] if os.name == "nt" else ["-pthread"]
    static = ["-static-libstdc++", "-static-libgcc"] if os.name == "nt" else []
    wio = args.wio.resolve()
    run([wio, folder / "main.wio", "--emit-cpp", "--include-dir", folder])
    for optimization in ("-O0", "-O2"):
        program = folder / ("native.exe" if os.name == "nt" else "native")
        run([args.cxx, "-std=c++20", optimization, folder / "main.wio.cpp", f"-I{folder}",
             f"-I{ROOT / 'runtime/include'}", f"-I{ROOT / 'sdk/include'}", args.runtime.resolve(), *links, "-o", program])
        run([program], contains="byte-write-range-ok")
        for mode, message in (("range", "outside the input"), ("overflow", "outside the input"), ("closed", None)):
            run([program, mode], expected=1, contains=message)
        contract = folder / ("allocation-probe.exe" if os.name == "nt" else "allocation-probe")
        run([args.cxx, "-std=c++20", optimization, *static, folder / "contract.cpp", f"-I{folder}",
             f"-I{ROOT / 'runtime/include'}", args.runtime.resolve(), *links, "-o", contract])
        run([contract], contains="byte-write-range-contract-ok")
    bridge = folder / ("bridge.dll" if os.name == "nt" else "bridge.dylib" if sys.platform == "darwin" else "bridge.so")
    bytecode = folder / "main.wiob"
    for optimization in ("-O0", "-O2"):
        run([wio, folder / "main.wio", "--target", "shared", "--native-bridge-only", "--include-dir", folder,
             "--output", bridge, "--bytecode-sidecar", bytecode, "--backend-arg=" + optimization,
             "--backend-arg=-ffp-contract=off"])
        run([args.vm.resolve(), bytecode, "--native-library", bridge], contains="byte-write-range-ok")
        for mode, message in (("range", "outside the input"), ("overflow", "outside the input"), ("closed", None)):
            run([args.vm.resolve(), bytecode, "--native-library", bridge, "--", mode], expected=1, contains=message)
    (folder / "results.json").write_text(json.dumps(dict(passed=True, processes=rows), indent=2) + "\n")
    print(folder / "results.json")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wio", type=Path, required=True)
    parser.add_argument("--vm", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--output", type=Path, required=True)
    verify(parser.parse_args())
